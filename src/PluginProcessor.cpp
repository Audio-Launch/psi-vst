#include "PluginProcessor.h"
#include "PluginEditor.h"

PsiProcessor::PsiProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void PsiProcessor::prepareToPlay (double, int) {}

bool PsiProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Mono or stereo, input must match output.
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void PsiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // v0.1: clean pass-through. DSP (EQ bank + morph) lands here next.
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* PsiProcessor::createEditor()
{
    return new PsiEditor (*this);
}

// JUCE plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PsiProcessor();
}
