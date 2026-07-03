// Ψ phase vocoder — C++ port of the verified JS oracle (dsp/reference/psi-dsp.mjs).
//
// Header-only, framework-agnostic (no JUCE dependency), so it drops straight into a
// PluginProcessor. The JS reference is the ORACLE: this port mirrors it exactly and is
// checked against golden.json by dsp/test/test_psi.cpp.
//
// Numerics: the JS reference stores mag/phase/psi/acc/re/im/win in Float32Array (float
// storage) and does all intermediate arithmetic in double. This port mirrors that
// precisely — float arrays, double scalars — so it reproduces the golden values to
// within libm rounding. Keep it that way, or the golden test will drift.
//
// NOTE: not yet compiler-verified in the authoring environment (no C++ toolchain there).
// Build + run dsp/test/test_psi.cpp against golden.json to confirm the port. Logic is a
// line-for-line mirror of the passing JS oracle.
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace psi {

constexpr double TWO_PI = 6.283185307179586476925286766559;

// In-place iterative radix-2 FFT over float storage with double intermediate math
// (identical order of operations to the JS oracle). inv=true → inverse, scaled by 1/n.
inline void fft(std::vector<float>& re, std::vector<float>& im, bool inv) {
    const int n = static_cast<int>(re.size());
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (int len = 2; len <= n; len <<= 1) {
        const double ang = (inv ? 2.0 : -2.0) * M_PI / len, wr = std::cos(ang), wi = std::sin(ang);
        for (int i = 0; i < n; i += len) {
            double cr = 1.0, ci = 0.0; const int h = len >> 1;
            for (int k = 0; k < h; k++) {
                const double ur = re[i + k], ui = im[i + k], xr = re[i + k + h], xi = im[i + k + h];
                const double vr = xr * cr - xi * ci, vi = xr * ci + xi * cr;
                re[i + k] = static_cast<float>(ur + vr); im[i + k] = static_cast<float>(ui + vi);
                re[i + k + h] = static_cast<float>(ur - vr); im[i + k + h] = static_cast<float>(ui - vi);
                const double ncr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = ncr;
            }
        }
    }
    if (inv) for (int i = 0; i < n; i++) { re[i] = static_cast<float>(re[i] / n); im[i] = static_cast<float>(im[i] / n); }
}

inline double princ(double x) { return x - TWO_PI * std::round(x / TWO_PI); }

inline std::vector<float> hann(int N) {
    std::vector<float> w(N);
    for (int i = 0; i < N; i++) w[i] = static_cast<float>(0.5 - 0.5 * std::cos((TWO_PI * i) / N));
    return w;
}

struct Analysis {
    std::vector<float> mag, phase;
    int F = 0, BINS = 0, N = 0, Ha = 0;
};

// Windowed STFT. mag/phase laid out as [frame * BINS + bin].
inline Analysis analyze(const std::vector<float>& mono, int N = 2048, int Ha = 512) {
    Analysis a; a.N = N; a.Ha = Ha; a.BINS = (N >> 1) + 1;
    const int BINS = a.BINS;
    const std::vector<float> win = hann(N);
    const int len = static_cast<int>(mono.size());
    a.F = std::min(2400, (len - N) / Ha + 1);
    if (a.F < 0) a.F = 0;
    a.mag.assign(static_cast<size_t>(a.F) * BINS, 0.0f);
    a.phase.assign(static_cast<size_t>(a.F) * BINS, 0.0f);
    std::vector<float> re(N), im(N);
    for (int f = 0; f < a.F; f++) {
        const int off = f * Ha;
        for (int i = 0; i < N; i++) {
            const double s = (off + i < len) ? static_cast<double>(mono[off + i]) : 0.0;
            re[i] = static_cast<float>(s * win[i]); im[i] = 0.0f;
        }
        fft(re, im, false);
        for (int k = 0; k < BINS; k++) {
            a.mag[static_cast<size_t>(f) * BINS + k] = static_cast<float>(std::hypot((double)re[k], (double)im[k]));
            a.phase[static_cast<size_t>(f) * BINS + k] = static_cast<float>(std::atan2((double)im[k], (double)re[k]));
        }
    }
    return a;
}

// Phase-vocoder resynthesis. Mirrors the PV AudioWorkletProcessor.
class PsiSynth {
public:
    explicit PsiSynth(int N = 2048, int Ha = 512, int Hs = 512)
        : N_(N), Ha_(Ha), Hs_(Hs), B_((N >> 1) + 1), win_(hann(N)),
          psi_(B_, 0.0f), re_(N, 0.0f), im_(N, 0.0f), acc_(N, 0.0f), outQ_(static_cast<size_t>(N) * 4, 0.0f) {}

    void setAnalysis(const float* mag, const float* phase, int F) {
        mag_ = mag; phase_ = phase; F_ = F; rp_ = 0.0;
        for (int k = 0; k < B_; k++) psi_[k] = phase[k];
        ready_ = true;
    }
    void setAnalysis(const Analysis& a) { setAnalysis(a.mag.data(), a.phase.data(), a.F); }

    void setRate(double r)    { rate_ = r; }
    void setFrozen(bool f)    { frozen_ = f; }
    void setDir(int d)        { dir_ = d; }   // +1 forward, -1 reverse
    void setSmear(int s)      { smear_ = s; }

    double readPos() const { return rp_; }

    // Pull `numSamples` output samples, synthesizing frames on demand.
    void render(float* out, int numSamples) {
        const int L = static_cast<int>(outQ_.size());
        if (!ready_) { for (int i = 0; i < numSamples; i++) out[i] = 0.0f; return; }
        for (int i = 0; i < numSamples; i++) {
            if (qc_ <= 0) synth();
            out[i] = outQ_[qr_]; qr_ = (qr_ + 1) % L; qc_--;
        }
    }

private:
    void synth() {
        const int N = N_, half = N >> 1, B = B_, F = F_;
        const float* mag = mag_; const float* phase = phase_;
        int ia = static_cast<int>(std::floor(rp_)); if (ia < 0) ia = 0; if (ia > F - 1) ia = F - 1;
        const int ib = std::min(F - 1, ia + 1); const double frac = rp_ - ia;
        const double ex = TWO_PI * Ha_ / N; const int sm = smear_;
        for (int k = 0; k <= half; k++) {
            const int ka = ia * B + k, kb = ib * B + k;
            double m;
            if (sm > 0) {
                double s = 0.0; int c = 0;
                for (int ff = std::max(0, ia - sm); ff <= std::min(F - 1, ia + sm); ff++) { s += mag[ff * B + k]; c++; }
                m = s / c;
            } else {
                m = (double)mag[ka] + ((double)mag[kb] - (double)mag[ka]) * frac;
            }
            const double omega = ex * k + princ((double)phase[kb] - (double)phase[ka] - ex * k);
            psi_[k] = static_cast<float>((double)psi_[k] + omega);
            re_[k] = static_cast<float>(m * std::cos((double)psi_[k]));
            im_[k] = static_cast<float>(m * std::sin((double)psi_[k]));
            if (k > 0 && k < half) { re_[N - k] = re_[k]; im_[N - k] = -im_[k]; }
        }
        im_[0] = 0.0f; im_[half] = 0.0f;
        fft(re_, im_, true);
        const int L = static_cast<int>(outQ_.size());
        for (int i = 0; i < N; i++) acc_[i] = static_cast<float>((double)acc_[i] + (double)re_[i] * (double)win_[i]);
        for (int i = 0; i < Hs_; i++) { outQ_[qw_] = static_cast<float>((double)acc_[i] / wola_); qw_ = (qw_ + 1) % L; qc_++; }
        // acc.copyWithin(0, Hs); acc.fill(0, N-Hs)  — shift left by Hs, zero the tail.
        for (int i = 0; i < N - Hs_; i++) acc_[i] = acc_[i + Hs_];
        for (int i = N - Hs_; i < N; i++) acc_[i] = 0.0f;
        rp_ += dir_ * (frozen_ ? 0.0 : rate_);
        if (rp_ > F - 1) { rp_ = 0.0; for (int k = 0; k < B; k++) psi_[k] = phase[k]; }
        else if (rp_ < 0) { rp_ = F - 1; for (int k = 0; k < B; k++) psi_[k] = phase[(F - 1) * B + k]; }
    }

    int N_, Ha_, Hs_, B_;
    std::vector<float> win_, psi_, re_, im_, acc_, outQ_;
    const float* mag_ = nullptr; const float* phase_ = nullptr;
    int F_ = 0; bool ready_ = false;
    double rp_ = 0.0, rate_ = 1.0; bool frozen_ = false; int dir_ = 1, smear_ = 0;
    double wola_ = 1.5;
    int qr_ = 0, qw_ = 0, qc_ = 0;
};

} // namespace psi
