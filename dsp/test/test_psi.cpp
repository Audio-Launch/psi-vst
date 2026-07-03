// Golden-reference test for the C++ phase-vocoder port (dsp/PsiVocoder.h).
// Reproduces the JS oracle's setup and checks the output matches golden.json (values
// hardcoded below to avoid a JSON dependency). Build+run:
//
//   c++ -std=c++17 -O2 -I../ test_psi.cpp -o test_psi && ./test_psi
//   (MSVC: cl /std:c++17 /O2 /I.. test_psi.cpp)
//
// Exit 0 = port matches the verified JS algorithm; non-zero = drift/regression.
#include "../PsiVocoder.h"
#include <cstdio>
#include <cmath>
#include <vector>

static constexpr double TWO_PI = 6.283185307179586476925286766559;

// Must match makeChirp() in dsp/reference/psi-dsp.mjs exactly (double math, float store).
static std::vector<float> makeChirp(int sr = 48000, double dur = 2.0) {
    const int len = static_cast<int>(sr * dur);
    std::vector<float> d(len);
    double p = 0.0;
    for (int i = 0; i < len; i++) {
        const double t = (double)i / sr, f = 200.0 * std::pow(4.0, t / dur);
        p += (TWO_PI * f) / sr;
        const double trem = 0.6 + 0.4 * std::sin(TWO_PI * 5.0 * t);
        d[i] = static_cast<float>(std::sin(p) * trem * 0.7);
    }
    return d;
}

static double rms(const std::vector<float>& a) {
    double s = 0.0; for (float v : a) s += (double)v * v;
    return a.empty() ? 0.0 : std::sqrt(s / a.size());
}
static bool finite_all(const std::vector<float>& a) {
    for (float v : a) if (!std::isfinite(v)) return false;
    return true;
}

int main() {
    int fails = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok) fails++;
    };

    const auto input = makeChirp(48000, 2.0);
    const auto a = psi::analyze(input);
    std::printf("analyze: %d frames, %d bins\n", a.F, a.BINS);
    check(a.F == 184, "frame count == 184");
    check(finite_all(a.mag) && finite_all(a.phase), "analysis finite");

    // Golden reference (from the passing JS oracle, dsp/reference/golden.json).
    // Config: rate=0.5, dir=1, smear=0, frozen=false, 4096 samples.
    const double GOLD_RMS = 0.41439571;
    const int    gi[5]    = {0, 512, 1024, 2048, 4095};
    const double gv[5]    = {0.0, 0.0819587, 0.06879304, -0.5582304, -0.58729386};
    const double SAMPLE_TOL = 2e-3;  // float-storage + double libm; a correct port lands well inside
    const double RMS_TOL_REL = 0.01;

    {
        psi::PsiSynth s;
        s.setAnalysis(a);
        s.setRate(0.5); s.setDir(1); s.setSmear(0); s.setFrozen(false);
        std::vector<float> out(4096);
        s.render(out.data(), (int)out.size());

        check(finite_all(out), "golden render finite");
        const double r = rms(out);
        std::printf("  rms %.8f (golden %.8f)\n", r, GOLD_RMS);
        check(std::fabs(r - GOLD_RMS) <= GOLD_RMS * RMS_TOL_REL, "golden rms within 1%");
        for (int j = 0; j < 5; j++) {
            const double diff = std::fabs((double)out[gi[j]] - gv[j]);
            std::printf("  sample[%d] = %.8f (golden %.8f, |d|=%.2e)\n", gi[j], (double)out[gi[j]], gv[j], diff);
            check(diff <= SAMPLE_TOL, "golden sample within tol");
        }
    }

    // Each creative mode must be finite + non-silent (mirrors the JS oracle test).
    struct Mode { const char* name; double rate; bool frozen; int dir; int smear; };
    const Mode modes[] = {
        {"identity (rate=1)", 1.0, false, 1, 0},
        {"stretch  (rate=0.5)", 0.5, false, 1, 0},
        {"freeze   (frozen)", 1.0, true, 1, 0},
        {"reverse  (dir=-1)", 1.0, false, -1, 0},
        {"smear    (smear=4)", 0.5, false, 1, 4},
    };
    for (const auto& m : modes) {
        psi::PsiSynth s; s.setAnalysis(a);
        s.setRate(m.rate); s.setFrozen(m.frozen); s.setDir(m.dir); s.setSmear(m.smear);
        std::vector<float> out(4096);
        s.render(out.data(), (int)out.size());
        char buf[96];
        std::snprintf(buf, sizeof buf, "%s finite+non-silent (rms %.4f)", m.name, rms(out));
        check(finite_all(out) && rms(out) > 1e-4, buf);
    }

    // Freeze holds the read position.
    {
        psi::PsiSynth s; s.setAnalysis(a);
        s.setRate(1.0); s.setFrozen(true);
        std::vector<float> out(8192);
        s.render(out.data(), (int)out.size());
        check(s.readPos() == 0.0, "freeze holds read position at 0");
    }

    std::printf("\n%s (%d failure%s)\n", fails == 0 ? "ALL C++ PORT CHECKS PASSED" : "FAILURES", fails, fails == 1 ? "" : "s");
    return fails == 0 ? 0 : 1;
}
