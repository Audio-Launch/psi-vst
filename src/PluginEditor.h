#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// One WebView hosting the Ψ web UI. Timer pushes the output meter to the UI
// (C++ → JS "meter"); the UI pushes EQ curves back (JS → C++ "eq").
class PsiEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PsiEditor (PsiProcessor&);
    ~PsiEditor() override;
    void resized() override;

private:
    void timerCallback() override;

    PsiProcessor& proc;
    juce::WebBrowserComponent web;   // initialised in the ctor (needs proc for the "eq" listener)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsiEditor)
};
