#include "PluginEditor.h"

static juce::WebBrowserComponent::Options makeOptions (PsiProcessor* pp)
{
    return juce::WebBrowserComponent::Options{}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withNativeIntegrationEnabled()
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
            .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)))
        .withEventListener ("eq", [pp] (juce::var v) { pp->applyEqEvent (v); }); // UI EQ curve → DSP
}

PsiEditor::PsiEditor (PsiProcessor& p)
    : AudioProcessorEditor (&p), proc (p), web (makeOptions (&p))
{
    addAndMakeVisible (web);
    web.goToURL ("http://localhost:4321/plugin");
    setResizable (true, true);
    setResizeLimits (900, 560, 3000, 2000);
    setSize (1360, 760);
    startTimerHz (30); // push the output meter to the UI
}

PsiEditor::~PsiEditor() { stopTimer(); }

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
