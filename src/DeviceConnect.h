#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <functional>
#include <map>
#include <memory>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <bcrypt.h>
#endif

// "Click 2 Connect" — the desktop half of the account-link flow (RFC 8252 loopback + PKCE):
//  1) generate a PKCE verifier/challenge pair, bind a 127.0.0.1 listener on an ephemeral
//     port + a random state nonce,
//  2) open the system browser at audiolab.tools/connect/device?port=..&state=..&name=..&challenge=..,
//  3) the browser (where the user is already logged in) authorizes and redirects a short-lived
//     SINGLE-USE CODE to http://127.0.0.1:<port>/?code=..&state=.. — never the API key,
//  4) we validate the state, then exchange {code, code_verifier} at /api/device-token over a
//     direct back-channel POST; the key exists only in that response, not in any URL/history.
// The host is never taken from the network — only this machine's loopback receives the code.
class DeviceConnect : private juce::Thread
{
public:
    DeviceConnect() : juce::Thread ("psi-connect") {}
    ~DeviceConnect() override { stop(); }

    // Called on the MESSAGE THREAD when a valid key arrives.
    std::function<void (juce::String key, juce::String email)> onConnected;

    // Launch the flow. connectBase = e.g. "https://audiolab.tools".
    void start (const juce::String& connectBase)
    {
        stop();

        juce::Random rng;
        int port = 0;
        listener = std::make_unique<juce::StreamingSocket>();
        if (listener->createListener (0, "127.0.0.1"))
            port = listener->getBoundPort();
        for (int i = 0; i < 20 && port <= 0; ++i)                 // fallback if ephemeral bind is unsupported
        {
            listener = std::make_unique<juce::StreamingSocket>();
            const int p = 49152 + rng.nextInt (16000);
            if (listener->createListener (p, "127.0.0.1"))
                port = p;
        }
        if (port <= 0) { listener.reset(); return; }

        state = juce::Uuid().toDashedString();
        base  = connectBase;

        // PKCE (RFC 7636 S256): verifier = base64url(32 random bytes) -> 43 chars; the
        // challenge rides the front-channel URL, the verifier never leaves this process
        // until the back-channel exchange proves we are the plugin that started the flow.
        unsigned char rnd[32];
        fillSecureRandom (rnd, sizeof (rnd));
        verifier = base64url (rnd, sizeof (rnd));
        const juce::SHA256 sha (verifier.toRawUTF8(), verifier.getNumBytesAsUTF8());
        const auto shaRaw = sha.getRawData();
        const auto challenge = base64url (shaRaw.getData(), shaRaw.getSize());

        const auto name = "Psi VST - " + juce::SystemStats::getComputerName();
        const auto url  = connectBase + "/connect/device?port=" + juce::String (port)
                        + "&state=" + juce::URL::addEscapeChars (state, true)
                        + "&name="  + juce::URL::addEscapeChars (name, true)
                        + "&challenge=" + challenge;                 // base64url = already URL-safe
        juce::URL (url).launchInDefaultBrowser();
        startThread();
    }

    void stop()
    {
        signalThreadShouldExit();
        if (listener != nullptr) listener->close();               // unblocks waitForNextConnection()
        stopThread (2500);
        listener.reset();
    }

private:
    void run() override
    {
        if (listener == nullptr) return;

        // Keep accepting until the real state-matching redirect arrives or we time out — a stray
        // local probe (AV/endpoint scanner, or an empty connection) must not eat the one slot.
        const auto deadline = juce::Time::getCurrentTime() + juce::RelativeTime::seconds (120.0);
        while (! threadShouldExit() && juce::Time::getCurrentTime() < deadline)
        {
            std::unique_ptr<juce::StreamingSocket> conn (listener->waitForNextConnection());
            if (conn == nullptr || threadShouldExit()) return;

            // Read up to the end of the request line (the whole query lives there).
            char buf[8192] = {};
            int total = 0;
            while (total < (int) sizeof (buf) - 1 && ! threadShouldExit())
            {
                if (conn->waitUntilReady (true, 2000) <= 0) break;
                const int n = conn->read (buf + total, (int) sizeof (buf) - 1 - total, false);
                if (n <= 0) break;
                total += n;
                if (juce::String (juce::CharPointer_UTF8 (buf)).contains ("\r\n")) break;
            }

            const juce::String req { juce::CharPointer_UTF8 (buf) }; // braces: avoid the most-vexing-parse
            const auto line   = req.upToFirstOccurrenceOf ("\r\n", false, false);          // GET /?.. HTTP/1.1
            const auto target = line.fromFirstOccurrenceOf (" ", false, false)
                                    .upToFirstOccurrenceOf (" ", false, false);            // /?key=..
            const auto params = parseQuery (target.fromFirstOccurrenceOf ("?", false, false));

            const bool ok = params.count ("state") && params.at ("state") == state
                            && params.count ("code") && params.at ("code").isNotEmpty();

            const juce::String msg = ok ? "Connected. Return to your DAW - you can close this tab."
                                        : "Waiting for authorization - keep this tab open or retry from the plugin.";
            const juce::String body =
                "<!doctype html><meta charset=utf-8><title>AudioLab</title>"
                "<body style=\"background:#0b0f17;color:#e6f0ff;font:16px system-ui,sans-serif;"
                "display:grid;place-items:center;height:100vh;margin:0\"><div>" + msg + "</div>";
            const juce::String resp =
                "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n"
                "Content-Length: " + juce::String (body.getNumBytesAsUTF8()) + "\r\n\r\n" + body;
            conn->write (resp.toRawUTF8(), (int) resp.getNumBytesAsUTF8());
            conn->close();

            if (! ok) continue; // stray/probe connection — keep listening for the real redirect

            // Back-channel exchange (we're on the connect thread, so blocking is fine):
            // {code, code_verifier} -> {key, email}. On failure the plugin simply stays
            // disconnected and the user retries — the code is single-use either way.
            const auto v = juce::JSON::parse (exchangeCode (params.at ("code")));
            const auto key   = v.getProperty ("key",   juce::String()).toString();
            const auto email = v.getProperty ("email", juce::String()).toString();
            if (key.isEmpty()) return;

            // Copy the callback by value so the deferred lambda never touches DeviceConnect `this`
            // (which may be gone before the message runs); the editor half is guarded by a
            // Component::SafePointer in PluginEditor.cpp.
            auto cb = onConnected;
            juce::MessageManager::callAsync ([cb, key, email] { if (cb) cb (key, email); });
            return;
        }
    }

    // POST {code, code_verifier} to /api/device-token; returns the raw JSON body ("" on failure).
    juce::String exchangeCode (const juce::String& code) const
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("code", code);
        o->setProperty ("code_verifier", verifier);
        const auto url = juce::URL (base + "/api/device-token")
                            .withPOSTData (juce::JSON::toString (juce::var (o.get()), true));
        const auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
            .withExtraHeaders ("Content-Type: application/json")
            .withConnectionTimeoutMs (8000);
        std::unique_ptr<juce::InputStream> in (url.createInputStream (opts));
        return in != nullptr ? in->readEntireStreamAsString() : juce::String();
    }

    // Crypto-grade randomness for the PKCE verifier (Windows CNG), falling back to
    // juce::Random only if the OS RNG is unavailable.
    static void fillSecureRandom (void* dest, size_t bytes)
    {
       #if JUCE_WINDOWS
        if (BCryptGenRandom (nullptr, static_cast<PUCHAR> (dest), (ULONG) bytes,
                             BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0)
            return;
       #endif
        juce::Random::getSystemRandom().fillBitsRandomly (dest, bytes);
    }

    static juce::String base64url (const void* data, size_t n)
    {
        return juce::Base64::toBase64 (data, n)
                 .replaceCharacter ('+', '-')
                 .replaceCharacter ('/', '_')
                 .removeCharacters ("=");
    }

    static std::map<juce::String, juce::String> parseQuery (const juce::String& q)
    {
        std::map<juce::String, juce::String> m;
        for (const auto& pair : juce::StringArray::fromTokens (q, "&", ""))
        {
            if (pair.isEmpty()) continue;
            const auto k = pair.upToFirstOccurrenceOf ("=", false, false);
            const auto v = pair.fromFirstOccurrenceOf ("=", false, false);
            m[k] = juce::URL::removeEscapeChars (v);
        }
        return m;
    }

    std::unique_ptr<juce::StreamingSocket> listener;
    juce::String state, verifier, base;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceConnect)
};
