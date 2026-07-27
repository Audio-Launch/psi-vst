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

    // DAW-bus capture ("Record master bus"): the audio thread appends post-EQ stereo into a
    // pre-allocated buffer; the UI pulls it via the /__capture.f32 resource after stop. All
    // local — no audio ever leaves the machine and no API call is involved.
    // The position reset is ARMED here but executed by the audio thread itself (first block
    // after arming), so a message-thread reset can never race an in-flight block's store.
    static constexpr int MAX_CAPTURE_SECONDS = 60;
    void startCapture()             { captureArm.store (true); }
    void stopCapture()              { captureArm.store (false); capturing.store (false); }
    int  captureSamples() const     { return capturePos.load (std::memory_order_acquire); }
    bool isCapturing() const        { return capturing.load(); }
    double captureSampleRate() const { return sr; }
    const juce::AudioBuffer<float>& captureBuffer() const { return capture; }
    // Guards capture buffer IDENTITY (setSize in prepareToPlay) against the message-thread
    // reader (captureResource memcpy). processBlock never takes it — the host already
    // serialises prepareToPlay vs processBlock, and the audio thread must not lock.
    const juce::CriticalSection& captureGuard() const { return captureLock; }

    // EQ update from the UI ("eq" bridge event):
    //   { q, bypass, m:[48 dB], s:[48 dB], freqs?:[48 Hz] }
    // Message thread. `freqs` is optional so an older UI still works; when it is
    // absent the previous band centres are kept.
    void applyEqEvent (const juce::var& v);

private:
    void recomputeEq();  // audio thread, when eqDirty

    double sr = 48000.0;

    // Band centres, double-buffered exactly like the gains below. They used to be
    // computed once here and never touched, which was fine while the web UI used
    // the same formula. It no longer does: the UI now centres each filter on the
    // rows it owns, so it sends its table along and this has to follow. Writing a
    // live array from the message thread while the audio thread reads it is a
    // race, so it rides the same gainIdx flip that already makes the gains safe.
    double filterFreq[2][NUM_FILTERS] {};

    // Peaking biquads: [role 0=Mid,1=Side][filter], coeffs (a0-normalised) + state.
    double eqB[2][NUM_FILTERS][3] {};   // b0,b1,b2
    double eqA[2][NUM_FILTERS][2] {};   // a1,a2
    double eqZ[2][NUM_FILTERS][2] {};   // z1,z2

    // Target gains (double-buffered, same index as filterFreq), Q, bypass —
    // written on the message thread.
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

    // Capture state — buffer allocated in prepareToPlay (never on the audio thread).
    juce::AudioBuffer<float> capture;
    juce::CriticalSection captureLock;
    std::atomic<int>  capturePos { 0 };
    std::atomic<bool> capturing { false };
    std::atomic<bool> captureArm { false };   // set by startCapture; consumed by processBlock

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsiProcessor)
};
