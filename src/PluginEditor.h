#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "DeviceConnect.h"

// One WebView hosting the Ψ web UI. Timer pushes the output meter to the UI
// (C++ → JS "meter"); the UI pushes EQ curves back (JS → C++ "eq"). The account
// "Click 2 Connect" links this install to an AudioLab account (JS "connect"/"disconnect"/
// "account-req" ↔ C++ "account").
class PsiEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PsiEditor (PsiProcessor&);
    ~PsiEditor() override;
    void resized() override;

    // Account "Click 2 Connect" — invoked by the WebView bridge listeners (JS → C++).
    void handleConnect();       // "connect"     → open the browser flow
    void pushAccountState();    // "account-req" → push {connected, email} back to the UI
    void disconnectAccount();   // "disconnect"  → forget the stored key
    void openAccount();         // "openaccount" → open audiolab.tools/account in the browser

private:
    void timerCallback() override;
    void onKeyReceived (juce::String key, juce::String email); // message thread, from DeviceConnect
    void fetchAndPushUsage (juce::String key, juce::String email); // bg fetch → re-emit "account" with usage
    juce::File credsFile() const;

    PsiProcessor& proc;
    DeviceConnect deviceConnect;     // declared before web so the "connect" listener can use it
    juce::WebBrowserComponent web;   // initialised in the ctor (needs proc + this for the listeners)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsiEditor)
};
