#include "PluginEditor.h"
#include <optional>
#include <cstring>

#if JUCE_WINDOWS
 #include <dpapi.h> // CryptProtectData/CryptUnprotectData — user-scoped key-at-rest (crypt32)
#endif

#if ! PSI_DEV_SERVER
 #include "BinaryData.h"

// ── Serve the embedded web bundle (webui.zip) straight to the WebView, so the plugin
//    needs no dev server. Root "/" -> the plugin page; every other path maps 1:1 to a
//    zip entry (_astro/foo.js, etc.). ──────────────────────────────────────────────────
static juce::String mimeForExtension (const juce::String& ext)
{
    if (ext == "html")                 return "text/html";
    if (ext == "js" || ext == "mjs")   return "text/javascript";
    if (ext == "css")                  return "text/css";
    if (ext == "json" || ext == "map") return "application/json";
    if (ext == "svg")                  return "image/svg+xml";
    if (ext == "woff2")                return "font/woff2";
    if (ext == "woff")                 return "font/woff";
    if (ext == "png")                  return "image/png";
    if (ext == "webp")                 return "image/webp";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "ico")                  return "image/vnd.microsoft.icon";
    return "application/octet-stream";
}

static juce::ZipFile& webUiZip()
{
    // Opened once from the embedded bytes; the resource provider only runs on the message thread.
    static juce::MemoryInputStream stream { BinaryData::webui_zip, (size_t) BinaryData::webui_zipSize, false };
    static juce::ZipFile zip { &stream, false };
    return zip;
}

static std::optional<juce::WebBrowserComponent::Resource> getWebResource (const juce::String& url)
{
    const auto path = (url == "/") ? juce::String ("plugin/index.html")
                                   : url.fromFirstOccurrenceOf ("/", false, false);
    auto& zip = webUiZip();
    const auto index = zip.getIndexOfFileName (path);
    if (index < 0)
        return std::nullopt;

    std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (index));
    if (in == nullptr)
        return std::nullopt;

    std::vector<std::byte> bytes ((size_t) in->getTotalLength());
    in->setPosition (0);
    in->read (bytes.data(), (int) bytes.size());

    const auto ext = path.fromLastOccurrenceOf (".", false, false).toLowerCase();
    return juce::WebBrowserComponent::Resource { std::move (bytes), mimeForExtension (ext) };
}
#endif // ! PSI_DEV_SERVER

// Where the browser "Click 2 Connect" page lives (dev: the Vite server; release: prod).
#if PSI_DEV_SERVER
 #define PSI_CONNECT_BASE "http://localhost:4321"
#else
 #define PSI_CONNECT_BASE "https://audiolab.tools"
#endif

// Serve the finished DAW-bus capture to the UI as raw float32, planar (ch0 then ch1) —
// no chunked bridge events, just one fetch of /__capture.f32. Message thread only.
static std::optional<juce::WebBrowserComponent::Resource> captureResource (PsiProcessor* pp)
{
    // Lock vs prepareToPlay's setSize (a sample-rate change mid-read would free the block
    // under this memcpy — prepareToPlay isn't guaranteed to run on the message thread).
    const juce::ScopedLock sl (pp->captureGuard());
    const auto& buf = pp->captureBuffer();
    const int m = juce::jmin (pp->captureSamples(), buf.getNumSamples());
    std::vector<std::byte> bytes ((size_t) m * 2 * sizeof (float));
    if (m > 0)
    {
        std::memcpy (bytes.data(),                              buf.getReadPointer (0), (size_t) m * sizeof (float));
        std::memcpy (bytes.data() + (size_t) m * sizeof (float), buf.getReadPointer (1), (size_t) m * sizeof (float));
    }
    return juce::WebBrowserComponent::Resource { std::move (bytes), "application/octet-stream" };
}

static juce::WebBrowserComponent::Options makeOptions (PsiProcessor* pp, PsiEditor* ed)
{
    auto opts = juce::WebBrowserComponent::Options{}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withNativeIntegrationEnabled()
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
            .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)))
        .withEventListener ("eq",          [pp] (juce::var v) { pp->applyEqEvent (v); })   // EQ curve -> DSP
        .withEventListener ("connect",     [ed] (juce::var)   { ed->handleConnect(); })    // start account link
        .withEventListener ("account-req", [ed] (juce::var)   { ed->pushAccountState(); }) // UI asks on mount
        .withEventListener ("disconnect",  [ed] (juce::var)   { ed->disconnectAccount(); })
        .withEventListener ("openaccount", [ed] (juce::var)   { ed->openAccount(); })       // manage in the browser
        .withEventListener ("rec",         [ed] (juce::var v) { ed->handleRec (v); })       // DAW-bus capture
        .withEventListener ("lab",         [ed] (juce::var v) { ed->handleLab (v); })       // Labs cloud runs
        .withEventListener ("saveviz",     [ed] (juce::var v) { ed->handleSaveViz (v); })   // PNG share hook
        .withEventListener ("opendownload",[ed] (juce::var)   { ed->openDownload(); });      // update chip → /download
   #if PSI_DEV_SERVER
    // Dev: the page is on localhost, so the capture fetch to https://juce.backend/ is
    // cross-origin — allow it. Only the capture endpoint is served here.
    opts = opts.withResourceProvider ([pp] (const juce::String& url)
        -> std::optional<juce::WebBrowserComponent::Resource>
    {
        if (url == "/__capture.f32") return captureResource (pp);
        return std::nullopt;
    }, juce::String ("http://localhost:4321"));
   #else
    opts = opts.withResourceProvider ([pp] (const juce::String& url)
    {
        if (url == "/__capture.f32") return captureResource (pp);
        return getWebResource (url);
    });
   #endif
    return opts;
}

PsiEditor::PsiEditor (PsiProcessor& p)
    : AudioProcessorEditor (&p), proc (p), web (makeOptions (&p, this))
{
    // SafePointer so a torn-down editor makes the deferred connect callback a no-op (the socket
    // thread can fire this after the host has closed the editor window).
    deviceConnect.onConnected = [safe = juce::Component::SafePointer<PsiEditor> (this)] (juce::String key, juce::String email)
    {
        if (safe != nullptr) safe->onKeyReceived (key, email);
    };
    addAndMakeVisible (web);
   #if PSI_DEV_SERVER
    web.goToURL ("http://localhost:4321/plugin");                       // dev: Vite hot-reload
   #else
    web.goToURL (juce::WebBrowserComponent::getResourceProviderRoot()); // release: embedded bundle
   #endif
    setResizable (true, true);
    setResizeLimits (900, 560, 3000, 2000);
    setSize (1360, 760);
    startTimerHz (30); // push the output meter to the UI
    checkForUpdate();  // ask the site whether a newer build is out
}

// A UI-driven capture has no consumer once the editor is gone — stop it, or a reopened
// editor could press Record against a still-running orphan capture.
PsiEditor::~PsiEditor() { stopTimer(); deviceConnect.stop(); proc.stopCapture(); }

juce::File PsiEditor::credsFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("AudioLab").getChildFile ("PsiSpectral").getChildFile ("account.json");
}

void PsiEditor::handleConnect() { deviceConnect.start (PSI_CONNECT_BASE); }

#if JUCE_WINDOWS
// User-scoped encrypt/decrypt of the stored credentials (DPAPI): only this Windows user on
// this machine can read the key back. Returns empty on failure.
static juce::MemoryBlock dpapi (const void* data, size_t size, bool protect)
{
    DATA_BLOB in { (DWORD) size, (BYTE*) data }, out {};
    const bool ok = protect
        ? CryptProtectData   (&in, L"PsiSpectral", nullptr, nullptr, nullptr, 0, &out)
        : CryptUnprotectData (&in, nullptr, nullptr, nullptr, nullptr, 0, &out);
    if (! ok) return {};
    juce::MemoryBlock r (out.pbData, out.cbData);
    LocalFree (out.pbData);
    return r;
}
#endif

juce::var PsiEditor::readCreds() const
{
    juce::MemoryBlock raw;
    if (! credsFile().loadFileAsData (raw) || raw.isEmpty()) return {};
   #if JUCE_WINDOWS
    const auto plain = dpapi (raw.getData(), raw.getSize(), false);
    if (! plain.isEmpty())
        return juce::JSON::parse (plain.toString());
   #endif
    // Legacy plaintext account.json (pre-DPAPI installs) still parses; the next write encrypts.
    return juce::JSON::parse (raw.toString());
}

void PsiEditor::writeCreds (const juce::var& v) const
{
    const auto json = juce::JSON::toString (v);
    const auto f = credsFile();
    f.getParentDirectory().createDirectory();
   #if JUCE_WINDOWS
    const auto enc = dpapi (json.toRawUTF8(), json.getNumBytesAsUTF8(), true);
    if (! enc.isEmpty()) { f.replaceWithData (enc.getData(), enc.getSize()); return; }
   #endif
    f.replaceWithText (json); // non-Windows / DPAPI failure: fall back to plaintext
}

void PsiEditor::onKeyReceived (juce::String key, juce::String email)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("key", key);
    o->setProperty ("email", email);
    writeCreds (juce::var (o.get()));
    pushAccountState();
}

void PsiEditor::pushAccountState()
{
    const auto v = readCreds();
    const auto email = v.getProperty ("email", juce::String()).toString();
    const auto key = v.getProperty ("key", juce::String()).toString();
    const bool connected = key.isNotEmpty();
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("connected", connected);
    o->setProperty ("email", email);
    web.emitEventIfBrowserIsVisible ("account", juce::var (o.get()));
    if (connected) fetchAndPushUsage (key, email); // enrich with tier + usage off the message thread
}

void PsiEditor::disconnectAccount() { credsFile().deleteFile(); pushAccountState(); }

void PsiEditor::openAccount()
{
    juce::URL (juce::String (PSI_CONNECT_BASE) + "/account").launchInDefaultBrowser();
}

void PsiEditor::openDownload()
{
    juce::URL (juce::String (PSI_CONNECT_BASE) + "/download").launchInDefaultBrowser();
}

// Dotted-version compare: true if `remote` is strictly newer than `local` (e.g. 0.2.0 > 0.1.9).
static bool versionIsNewer (const juce::String& remote, const juce::String& local)
{
    auto parts = [] (const juce::String& v) {
        juce::Array<int> out;
        for (const auto& s : juce::StringArray::fromTokens (v.upToFirstOccurrenceOf ("-", false, false), ".", ""))
            out.add (s.getIntValue());
        return out;
    };
    const auto r = parts (remote), l = parts (local);
    for (int i = 0; i < juce::jmax (r.size(), l.size()); ++i)
    {
        const int rv = i < r.size() ? r[i] : 0, lv = i < l.size() ? l[i] : 0;
        if (rv != lv) return rv > lv;
    }
    return false;
}

// Ask the site for the latest published build; if it's newer than this compiled one, tell the
// UI (which shows an "update available" chip). Best-effort, off the message thread, never blocks.
void PsiEditor::checkForUpdate()
{
    juce::Component::SafePointer<PsiEditor> safe (this);
    juce::Thread::launch ([safe]
    {
        const juce::URL url (juce::String (PSI_CONNECT_BASE) + "/vst/version.json");
        const auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs (6000);
        std::unique_ptr<juce::InputStream> in (url.createInputStream (opts));
        const auto v = in != nullptr ? juce::JSON::parse (in->readEntireStreamAsString()) : juce::var();
        const auto latest = v.getProperty ("version", juce::String()).toString();
        if (latest.isEmpty() || ! versionIsNewer (latest, PSI_VERSION)) return;
        juce::MessageManager::callAsync ([safe, latest]
        {
            if (safe == nullptr) return;
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            o->setProperty ("version", latest);
            safe->web.emitEventIfBrowserIsVisible ("update", juce::var (o.get()));
        });
    });
}

// POST a JSON body with the device key as bearer; returns the raw response ("" on network fail).
static juce::String postJson (const juce::String& url, const juce::String& body, const juce::String& bearer)
{
    const auto u = juce::URL (url).withPOSTData (body);
    const auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
        .withExtraHeaders ("Content-Type: application/json\r\nAuthorization: Bearer " + bearer)
        .withConnectionTimeoutMs (30000);
    std::unique_ptr<juce::InputStream> in (u.createInputStream (opts));
    return in != nullptr ? in->readEntireStreamAsString() : juce::String();
}

// Labs tab — the plugin half of a cloud run. Two steps, both on a background thread, key
// never leaves this process: "prepare" mints a one-shot signed upload URL for the UI to PUT
// the wav to; "run" fires the /v1 analysis on the uploaded objectPath and relays the report.
void PsiEditor::handleLab (const juce::var& v)
{
    const auto action = v.getProperty ("action", juce::String()).toString();
    const auto key = readCreds().getProperty ("key", juce::String()).toString();

    auto emitError = [this] (const juce::String& msg)
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("action", "error");
        o->setProperty ("error", msg);
        web.emitEventIfBrowserIsVisible ("lab", juce::var (o.get()));
    };
    if (key.isEmpty()) { emitError ("Connect your AudioLab account first."); return; }

    juce::Component::SafePointer<PsiEditor> safe (this);

    if (action == "prepare")
    {
        juce::DynamicObject::Ptr req = new juce::DynamicObject();
        req->setProperty ("contentType", v.getProperty ("contentType", "audio/wav"));
        req->setProperty ("size", v.getProperty ("size", 0));
        const auto body = juce::JSON::toString (juce::var (req.get()), true);
        juce::Thread::launch ([safe, key, body]
        {
            const auto r = juce::JSON::parse (postJson (juce::String (PSI_CONNECT_BASE) + "/api/upload-url", body, key));
            juce::MessageManager::callAsync ([safe, r]
            {
                if (safe == nullptr) return;
                juce::DynamicObject::Ptr o = new juce::DynamicObject();
                if (r.hasProperty ("signedUrl"))
                {
                    o->setProperty ("action", "upload");
                    o->setProperty ("signedUrl",  r.getProperty ("signedUrl",  juce::String()));
                    o->setProperty ("objectPath", r.getProperty ("objectPath", juce::String()));
                }
                else
                {
                    o->setProperty ("action", "error");
                    o->setProperty ("error", r.getProperty ("error", "Could not prepare the upload - check your connection."));
                }
                safe->web.emitEventIfBrowserIsVisible ("lab", juce::var (o.get()));
            });
        });
        return;
    }

    if (action == "run")
    {
        const auto tool = v.getProperty ("tool", juce::String()).toString();
        // Allowlist shape: "<lab>/<tool>", lowercase + dashes only — the URL is built from this.
        if (! tool.containsChar ('/') || ! tool.containsOnly ("abcdefghijklmnopqrstuvwxyz-/") || tool.length() > 40)
            { emitError ("Unknown tool."); return; }

        juce::DynamicObject::Ptr req = new juce::DynamicObject();
        req->setProperty ("objectPath", v.getProperty ("objectPath", juce::String()));
        if (auto* p = v.getProperty ("params", juce::var()).getDynamicObject())
            for (const auto& kv : p->getProperties())
                req->setProperty (kv.name, kv.value);
        const auto body = juce::JSON::toString (juce::var (req.get()), true);

        juce::Thread::launch ([safe, key, tool, body]
        {
            const auto raw = postJson (juce::String (PSI_CONNECT_BASE) + "/v1/" + tool, body, key);
            const auto r = juce::JSON::parse (raw);
            juce::MessageManager::callAsync ([safe, tool, r, raw]
            {
                if (safe == nullptr) return;
                juce::DynamicObject::Ptr o = new juce::DynamicObject();
                if (raw.isEmpty() || ! r.isObject())
                    { o->setProperty ("action", "error"); o->setProperty ("error", "Network error - try again."); }
                else if (r.hasProperty ("error"))
                    { o->setProperty ("action", "error"); o->setProperty ("error", r.getProperty ("error", juce::String())); }
                else
                    { o->setProperty ("action", "result"); o->setProperty ("tool", tool); o->setProperty ("result", r); }
                safe->web.emitEventIfBrowserIsVisible ("lab", juce::var (o.get()));
            });
        });
        return;
    }

    emitError ("Unknown lab action.");
}

// Save the exported screen PNG to the Desktop and reveal it — the share hook: one click,
// one branded image ready to post. Filename is sanitised and never clobbers an earlier export.
void PsiEditor::handleSaveViz (const juce::var& v)
{
    const auto b64 = v.getProperty ("png", juce::String()).toString();
    if (b64.isEmpty()) return;
    juce::MemoryOutputStream bytes;
    if (! juce::Base64::convertFromBase64 (bytes, b64) || bytes.getDataSize() == 0) return;
    const auto name = juce::File::createLegalFileName (v.getProperty ("name", "psi-spectral.png").toString());
    const auto f = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                     .getChildFile (name.endsWith (".png") ? name : name + ".png")
                     .getNonexistentSibling();
    const bool ok = f.replaceWithData (bytes.getData(), bytes.getDataSize());
    // Ack so the UI can confirm loudly — a silent save reads as "nothing happened".
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("saved", ok);
    o->setProperty ("name", f.getFileName());
    web.emitEventIfBrowserIsVisible ("saveviz", juce::var (o.get()));
    if (ok) f.revealToUser();
}

void PsiEditor::handleRec (const juce::var& v)
{
    const auto action = v.getProperty ("action", juce::String()).toString();
    if (action == "start") { proc.startCapture(); return; }
    if (action != "stop") return;
    proc.stopCapture();
    // Ack with what was captured; the UI then fetches /__capture.f32. A processBlock already
    // in flight may still write one trailing block — bounded and inaudible in the terrain.
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("samples",    proc.captureSamples());
    o->setProperty ("sampleRate", proc.captureSampleRate());
    web.emitEventIfBrowserIsVisible ("rec", juce::var (o.get()));
}

void PsiEditor::fetchAndPushUsage (juce::String key, juce::String email)
{
    juce::Component::SafePointer<PsiEditor> safe (this);
    juce::Thread::launch ([safe, key, email]
    {
        const juce::URL url (juce::String (PSI_CONNECT_BASE) + "/api/key-usage");
        const auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
            .withExtraHeaders ("Authorization: Bearer " + key)
            .withConnectionTimeoutMs (8000);
        std::unique_ptr<juce::InputStream> in (url.createInputStream (opts));
        const juce::String body = in != nullptr ? in->readEntireStreamAsString() : juce::String();
        const auto v = juce::JSON::parse (body);
        juce::MessageManager::callAsync ([safe, email, v]
        {
            if (safe == nullptr || ! v.isObject() || ! v.hasProperty ("tier")) return; // network fail → keep state
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            o->setProperty ("connected", true);
            o->setProperty ("email", email);
            o->setProperty ("tier", v.getProperty ("tier", "free"));
            o->setProperty ("callsUsed", v.getProperty ("callsUsed", 0));
            o->setProperty ("callsQuota", v.getProperty ("callsQuota", juce::var()));
            safe->web.emitEventIfBrowserIsVisible ("account", juce::var (o.get()));
        });
    });
}

void PsiEditor::timerCallback()
{
    const auto m = proc.getMeter();
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("rms",  m.rms);
    o->setProperty ("peak", m.peak);
    o->setProperty ("lufs", m.lufs);
    // Live capture progress rides the same 30Hz event — drives the UI's REC strip, and lets
    // it detect the 60s capacity auto-stop (recording flips false while it still shows 'rec').
    o->setProperty ("recording", proc.isCapturing());
    o->setProperty ("recSecs",   proc.captureSampleRate() > 0 ? proc.captureSamples() / proc.captureSampleRate() : 0.0);
    web.emitEventIfBrowserIsVisible ("meter", juce::var (o.get()));
}

void PsiEditor::resized() { web.setBounds (getLocalBounds()); }
