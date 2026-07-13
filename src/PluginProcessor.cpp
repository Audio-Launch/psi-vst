#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

PsiProcessor::PsiProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // 48 log-spaced band centres, 30 Hz … 18 kHz (matches the web filter bank).
    const double lo = std::log10 (30.0), hi = std::log10 (18000.0);
    for (int i = 0; i < NUM_FILTERS; ++i)
        filterFreq[i] = std::pow (10.0, lo + (i / (double) (NUM_FILTERS - 1)) * (hi - lo));
}

static void makeHighPass (double* c, double sr, double f0, double Q)
{
    const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / sr;
    const double cw = std::cos (w0), sw = std::sin (w0), alpha = sw / (2.0 * Q);
    const double a0 = 1.0 + alpha;
    c[0] = ((1.0 + cw) / 2.0) / a0; c[1] = (-(1.0 + cw)) / a0; c[2] = ((1.0 + cw) / 2.0) / a0;
    c[3] = (-2.0 * cw) / a0; c[4] = (1.0 - alpha) / a0;
}

static void makeHighShelf (double* c, double sr, double f0, double dB)
{
    const double A = std::pow (10.0, dB / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / sr;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double alpha = sw / 2.0 * std::sqrt (2.0); // S=1
    const double tsa = 2.0 * std::sqrt (A) * alpha;
    const double a0 = (A + 1.0) - (A - 1.0) * cw + tsa;
    c[0] = (A * ((A + 1.0) + (A - 1.0) * cw + tsa)) / a0;
    c[1] = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cw)) / a0;
    c[2] = (A * ((A + 1.0) + (A - 1.0) * cw - tsa)) / a0;
    c[3] = (2.0 * ((A - 1.0) - (A + 1.0) * cw)) / a0;
    c[4] = ((A + 1.0) - (A - 1.0) * cw - tsa) / a0;
}

void PsiProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    makeHighPass  (hp, sr, 38.0, 0.5);
    makeHighShelf (sh, sr, 1500.0, 4.0);
    for (int ch = 0; ch < 2; ++ch) { zHp[ch][0] = zHp[ch][1] = zSh[ch][0] = zSh[ch][1] = 0.0; msAcc[ch] = 1e-12; }
    msAlpha = 1.0 - std::exp (-1.0 / (0.4 * sr));
    for (int r = 0; r < 2; ++r) for (int k = 0; k < NUM_FILTERS; ++k) eqZ[r][k][0] = eqZ[r][k][1] = 0.0;
    eqDirty.store (true);

    // Capture buffer for "Record master bus" — allocated here, written by processBlock.
    // Reset the whole capture state: a re-prepare mid-capture (sample-rate change, host
    // re-init) must never leave a stale capturePos pointing into a reallocated buffer.
    capturing.store (false);
    captureArm.store (false);
    capturePos.store (0);
    {
        const juce::ScopedLock sl (captureLock); // vs a concurrent captureResource memcpy
        capture.setSize (2, (int) (sr * MAX_CAPTURE_SECONDS));
    }
}

void PsiProcessor::applyEqEvent (const juce::var& v)
{
    const int nb = 1 - gainIdx.load();
    auto* mArr = v.getProperty ("m", juce::var()).getArray();
    auto* sArr = v.getProperty ("s", juce::var()).getArray();
    for (int i = 0; i < NUM_FILTERS; ++i)
    {
        gainBuf[nb][0][i] = (mArr != nullptr && i < mArr->size()) ? (float) (double) (*mArr)[i] : 0.0f;
        gainBuf[nb][1][i] = (sArr != nullptr && i < sArr->size()) ? (float) (double) (*sArr)[i] : 0.0f;
    }
    eqQ.store ((float) (double) v.getProperty ("q", 4.5));
    eqBypass.store ((bool) v.getProperty ("bypass", false));
    gainIdx.store (nb);
    eqDirty.store (true);
}

void PsiProcessor::recomputeEq()
{
    const int idx = gainIdx.load();
    const double Q = juce::jlimit (0.3, 24.0, (double) eqQ.load());
    for (int role = 0; role < 2; ++role)
        for (int k = 0; k < NUM_FILTERS; ++k)
        {
            const double A  = std::pow (10.0, (double) gainBuf[idx][role][k] / 40.0);
            const double w0 = 2.0 * juce::MathConstants<double>::pi * filterFreq[k] / sr;
            const double cw = std::cos (w0), sw = std::sin (w0), alpha = sw / (2.0 * Q);
            const double a0 = 1.0 + alpha / A;
            eqB[role][k][0] = (1.0 + alpha * A) / a0;
            eqB[role][k][1] = (-2.0 * cw) / a0;
            eqB[role][k][2] = (1.0 - alpha * A) / a0;
            eqA[role][k][0] = (-2.0 * cw) / a0;
            eqA[role][k][1] = (1.0 - alpha / A) / a0;
        }
}

bool PsiProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void PsiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n  = buffer.getNumSamples();
    const int nc = juce::jmin (2, getTotalNumInputChannels());
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // ── 48-band M/S peaking EQ (Mid = role 0, Side = role 1) ──
    if (! eqBypass.load())
    {
        if (eqDirty.exchange (false)) recomputeEq();
        float* L = buffer.getWritePointer (0);
        float* R = nc > 1 ? buffer.getWritePointer (1) : nullptr;
        for (int i = 0; i < n; ++i)
        {
            double m, s = 0.0;
            if (R != nullptr) { const double l = L[i], r = R[i]; m = (l + r) * 0.5; s = (l - r) * 0.5; }
            else m = L[i];
            for (int k = 0; k < NUM_FILTERS; ++k)
            { const double x = m, y = eqB[0][k][0]*x + eqZ[0][k][0]; eqZ[0][k][0] = eqB[0][k][1]*x - eqA[0][k][0]*y + eqZ[0][k][1]; eqZ[0][k][1] = eqB[0][k][2]*x - eqA[0][k][1]*y; m = y; }
            if (R != nullptr)
            {
                for (int k = 0; k < NUM_FILTERS; ++k)
                { const double x = s, y = eqB[1][k][0]*x + eqZ[1][k][0]; eqZ[1][k][0] = eqB[1][k][1]*x - eqA[1][k][0]*y + eqZ[1][k][1]; eqZ[1][k][1] = eqB[1][k][2]*x - eqA[1][k][1]*y; s = y; }
                L[i] = (float) (m + s); R[i] = (float) (m - s);
            }
            else L[i] = (float) m;
        }
    }

    // ── DAW-bus capture (post-EQ, what you hear) — appends into the pre-allocated buffer ──
    // The audio thread owns the position reset: consume the arm flag here so the reset can
    // never race one of our own in-flight stores.
    if (captureArm.exchange (false)) { capturePos.store (0); capturing.store (true); }
    if (capturing.load (std::memory_order_relaxed))
    {
        const int cap = capture.getNumSamples();
        const int pos = capturePos.load (std::memory_order_relaxed);
        const int m   = juce::jmin (n, cap - pos);
        if (m > 0)
        {
            capture.copyFrom (0, pos, buffer, 0, 0, m);
            capture.copyFrom (1, pos, buffer, nc > 1 ? 1 : 0, 0, m);
            capturePos.store (pos + m, std::memory_order_release);
        }
        if (pos + m >= cap) capturing.store (false); // buffer full — auto-stop
    }

    // ── output metering (post-EQ): peak / RMS / K-weighted momentary loudness ──
    float blockPeak = 0.0f; double blockSumSq = 0.0;
    for (int ch = 0; ch < nc; ++ch)
    {
        const float* d = buffer.getReadPointer (ch);
        double z1h = zHp[ch][0], z2h = zHp[ch][1], z1s = zSh[ch][0], z2s = zSh[ch][1], ms = msAcc[ch];
        for (int i = 0; i < n; ++i)
        {
            const double x = (double) d[i];
            const float ax = std::abs (d[i]); if (ax > blockPeak) blockPeak = ax;
            blockSumSq += x * x;
            const double yhp = hp[0] * x   + z1h; z1h = hp[1] * x   - hp[3] * yhp + z2h; z2h = hp[2] * x   - hp[4] * yhp;
            const double ysh = sh[0] * yhp + z1s; z1s = sh[1] * yhp - sh[3] * ysh + z2s; z2s = sh[2] * yhp - sh[4] * ysh;
            ms += (ysh * ysh - ms) * msAlpha;
        }
        zHp[ch][0] = z1h; zHp[ch][1] = z2h; zSh[ch][0] = z1s; zSh[ch][1] = z2s; msAcc[ch] = ms;
    }
    const int total = juce::jmax (1, n * nc);
    rmsDb.store  (blockSumSq > 0.0 ? (float) (10.0 * std::log10 (blockSumSq / total)) : -100.0f);
    peakDb.store (blockPeak > 0.0f ? (float) (20.0 * std::log10 ((double) blockPeak)) : -100.0f);
    double msSum = 0.0; for (int ch = 0; ch < nc; ++ch) msSum += msAcc[ch];
    lufsDb.store (msSum > 1e-12 ? (float) (-0.691 + 10.0 * std::log10 (msSum)) : -70.0f);
}

juce::AudioProcessorEditor* PsiProcessor::createEditor() { return new PsiEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PsiProcessor(); }
