#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

// Ψ Spectral — pass-through + a 48-band M/S peaking EQ driven by the web UI over
// the JUCE↔JS bridge, plus real output metering pushed back to the UI.
class PsiProcessor : public juce::AudioProcessor
{
public:
    static constexpr int NUM_FILTERS = 48;

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

    // Output meter — read from the editor's timer (message thread).
    struct MeterData { float rms = -100.0f, peak = -100.0f, lufs = -70.0f; };
    MeterData getMeter() const { return { rmsDb.load(), peakDb.load(), lufsDb.load() }; }

    // EQ update from the UI ("eq" bridge event): { q, bypass, m:[48 dB], s:[48 dB] }. Message thread.
    void applyEqEvent (const juce::var& v);

private:
    void recomputeEq();  // audio thread, when eqDirty

    double sr = 48000.0;
    double filterFreq[NUM_FILTERS] {};

    // Peaking biquads: [role 0=Mid,1=Side][filter], coeffs (a0-normalised) + state.
    double eqB[2][NUM_FILTERS][3] {};   // b0,b1,b2
    double eqA[2][NUM_FILTERS][2] {};   // a1,a2
    double eqZ[2][NUM_FILTERS][2] {};   // z1,z2

    // Target gains (double-buffered), Q, bypass — written on the message thread.
    float gainBuf[2][2][NUM_FILTERS] {};   // [bufIdx][role][filter] (dB)
    std::atomic<int>   gainIdx { 0 };
    std::atomic<float> eqQ { 4.5f };
    std::atomic<bool>  eqBypass { true };  // flat until the UI sends a curve
    std::atomic<bool>  eqDirty { true };

    // K-weighting biquads for a momentary loudness estimate (per channel).
    double hp[5] { 1, 0, 0, 0, 0 };
    double sh[5] { 1, 0, 0, 0, 0 };
    double zHp[2][2] {};
    double zSh[2][2] {};
    double msAcc[2] { 1e-12, 1e-12 };
    double msAlpha = 0.001;

    std::atomic<float> rmsDb { -100.0f }, peakDb { -100.0f }, lufsDb { -70.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsiProcessor)
};
