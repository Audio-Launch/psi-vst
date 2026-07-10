#include "PluginEditor.h"

PsiEditor::PsiEditor (PsiProcessor& p)
    : AudioProcessorEditor (&p)
{
    addAndMakeVisible (web);

    // v0.1: load the running dev server (npm run dev in the web repo).
    // v0.2: bundle the built assets (astro build) and serve them from a local
    // resource provider so it works without the dev server. See BUILD.md.
    web.goToURL ("http://localhost:4321/plugin");

    setResizable (true, true);
    setResizeLimits (900, 560, 3000, 2000);
    setSize (1360, 760);
}

void PsiEditor::resized()
{
    web.setBounds (getLocalBounds());
}
