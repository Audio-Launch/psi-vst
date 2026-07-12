#include "PluginEditor.h"

#if ! PSI_DEV_SERVER
 #include "BinaryData.h"
 #include <optional>

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
        .withEventListener ("openaccount", [ed] (juce::var)   { ed->openAccount(); });      // manage in the browser
   #if ! PSI_DEV_SERVER
    opts = opts.withResourceProvider ([] (const juce::String& url) { return getWebResource (url); });
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
}

PsiEditor::~PsiEditor() { stopTimer(); deviceConnect.stop(); }

juce::File PsiEditor::credsFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("AudioLab").getChildFile ("PsiSpectral").getChildFile ("account.json");
}

void PsiEditor::handleConnect() { deviceConnect.start (PSI_CONNECT_BASE); }

void PsiEditor::onKeyReceived (juce::String key, juce::String email)
{
    // ponytail: plaintext in the user's app-data dir (user-scoped, and the key is a revocable
    // free-tier credential). Upgrade path = Windows DPAPI (CryptProtectData) for higher-value tokens.
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("key", key);
    o->setProperty ("email", email);
    const auto f = credsFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::JSON::toString (juce::var (o.get())));
    pushAccountState();
}

void PsiEditor::pushAccountState()
{
    const auto v = juce::JSON::parse (credsFile().loadFileAsString());
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
    web.emitEventIfBrowserIsVisible ("meter", juce::var (o.get()));
}

void PsiEditor::resized() { web.setBounds (getLocalBounds()); }
