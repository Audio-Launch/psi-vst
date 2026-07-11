#include "PluginEditor.h"

PsiEditor::PsiEditor (PsiProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    addAndMakeVisible (web);
    // v0.1/0.2: load the running dev server. (Ship: bundle + serve from a resource provider.)
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
