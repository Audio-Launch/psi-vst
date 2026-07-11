#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// The editor is one WebView hosting the Ψ web UI, plus a timer that pushes the
// output meter to the UI over the native bridge (C++ → JS "meter" event).
class PsiEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PsiEditor (PsiProcessor&);
    ~PsiEditor() override;
    void resized() override;

private:
    void timerCallback() override;

    PsiProcessor& proc;
    juce::WebBrowserComponent web {
        juce::WebBrowserComponent::Options{}
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withNativeIntegrationEnabled()
            .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)))
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsiEditor)
};
