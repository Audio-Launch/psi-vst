#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

// Ψ Spectral — v0.2: pass-through audio + real output metering pushed to the web
// UI over the JUCE↔JS bridge. DSP (EQ bank + morph) wired in next.
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

    // Output meter — read from the editor's timer (message thread), written on the audio thread.
    struct MeterData { float rms = -100.0f, peak = -100.0f, lufs = -70.0f; };
    MeterData getMeter() const { return { rmsDb.load(), peakDb.load(), lufsDb.load() }; }

private:
    // K-weighting biquads (BS.1770-ish: high-pass ~38 Hz + high-shelf +4 dB @ 1.5 kHz),
    // per channel, for a momentary loudness estimate. Coeffs computed in prepareToPlay.
    double hp[5] {1, 0, 0, 0, 0};   // b0,b1,b2,a1,a2 (a0-normalised)
    double sh[5] {1, 0, 0, 0, 0};
    double zHp[2][2] {};            // [ch][z1,z2]
    double zSh[2][2] {};
    double msAcc[2] { 1e-12, 1e-12 };
    double msAlpha = 0.001;         // ~400 ms leaky integrator

    std::atomic<float> rmsDb { -100.0f }, peakDb { -100.0f }, lufsDb { -70.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsiProcessor)
};
