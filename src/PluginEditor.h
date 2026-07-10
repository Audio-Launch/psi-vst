#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// The whole editor is one WebView filling the window, hosting the Ψ web UI.
class PsiEditor : public juce::AudioProcessorEditor
{
public:
    explicit PsiEditor (PsiProcessor&);
    void resized() override;

private:
    // WebView2 (Chromium) backend so the modern React/WebGL/CSS UI renders.
    juce::WebBrowserComponent web {
        juce::WebBrowserComponent::Options{}
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)))
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsiEditor)
};
