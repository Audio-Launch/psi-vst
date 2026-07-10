#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Ψ Spectral — v0.1 shell: pass-through audio + a WebView editor that hosts the
// web UI. The DSP (EQ filter bank + the PsiVocoder morph in dsp/PsiVocoder.h) is
// wired in next; for now we prove the UI-in-DAW pipeline with clean pass-through.
class PsiProcessor : public juce::AudioProcessor
{
public:
    PsiProcessor();
    ~PsiProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Psi Spectral"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsiProcessor)
};
