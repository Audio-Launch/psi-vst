# Ψ VST — audio-morphing plugin (JUCE)

The first AudioLab product beyond the browser + hosted API: a **creative real-time
morpher** as a cross-DAW plugin. Not a meter (every DAW ships those) — a phase-vocoder
that freezes, stretches, reverses, and smears audio. See `docs/product-vision.md` in the
web repo for the strategy and the unique selling points.

## Releasing

New build to ship? Follow [`docs/RELEASE.md`](docs/RELEASE.md). Short version: bump
`CMakeLists.txt` VERSION, rebuild, zip the `.vst3`, `gh release create v<X.Y.Z>` with the
zip, then update `audiolab.tools/public/vst/version.json` (version + url + size + notes) and
redeploy. The site's `/download` and the in-plugin update chip both read from
`audiolab.tools/vst/version.json`, so wiring that one file is what actually ships the update.

## Status

- ✅ **DSP core ported + verified.** The phase-vocoder algorithm from the browser lab
  (`PsiMorpher.tsx`) is extracted to a Node-runnable oracle and a header-only C++ port.
  The JS oracle passes all checks; the C++ port is a line-for-line mirror, golden-referenced.
- ⏳ **JUCE wrapper: next.** Needs Nathan's naming + licensing decisions (below) and a
  C++ toolchain (not available in the authoring environment, so the C++ is compile-pending
  — build the golden test first, see below).

## Layout

```
dsp/
  reference/psi-dsp.mjs     # JS oracle — the verified reference implementation
  reference/test_psi.mjs    # runs the oracle, emits golden.json   ✅ passing
  reference/golden.json     # golden output values the C++ must reproduce
  PsiVocoder.h              # header-only C++ port (analyze + PsiSynth)
  test/test_psi.cpp         # golden test for the C++ port
README.md
```

### Verify the DSP

JS oracle (works today, no toolchain needed):
```
node dsp/reference/test_psi.mjs
```
It prints 184 frames, confirms all five modes (identity/stretch/freeze/reverse/smear) are
finite + non-silent, confirms freeze holds position, and writes `golden.json`.

C++ port (once you have a compiler):
```
cd dsp/test
c++ -std=c++17 -O2 -I../ test_psi.cpp -o test_psi && ./test_psi
# MSVC:  cl /std:c++17 /O2 /I.. test_psi.cpp
```
Exit 0 = the port reproduces the oracle. If a sample drifts past tolerance, the port
diverged from the verified algorithm.

## DSP overview

Phase vocoder, STFT N=2048 / hop 512, Hann window, WOLA overlap-add. Four creative params
drive resynthesis:

| Param | Effect | Range (proposed) |
|-------|--------|------------------|
| `rate` | time-stretch (frame advance speed); <1 = slower/longer, pitch unchanged | 0.05 … 4.0 |
| `frozen` | hold the current spectral frame → endless evolving pad | bool |
| `dir` | playback direction | +1 / −1 |
| `smear` | spectral blur radius in frames (Paulstretch-style cloud) | 0 … 16 |

`PsiVocoder.h` exposes `psi::analyze(buffer)` → magnitude/phase frames, and a
`psi::PsiSynth` that you feed the analysis + params and pull output samples from
(`render(out, n)`), synthesizing frames on demand via an internal ring buffer.

## JUCE wrapper — the plan

The DSP is done; the plugin is a thin shell around it.

1. **Project (CMake + JUCE).** `juce_add_plugin` with formats `VST3` (+ `AU` on macOS,
   `AAX` later). Fetch JUCE via CPM or a submodule. Windows/VST3 first (per decision 3).
2. **Parameters.** Five `juce::AudioProcessorValueTreeState` params mapping to the DSP:
   `stretch` (rate), `freeze` (bool), `reverse` (dir), `smear`, plus a **`morph` macro**
   (PoS #4) that crossfades the transforms from one knob for automatable build-ups.
3. **Analysis source.** MVP: analyze a captured window of incoming audio (grab-and-hold),
   like the lab's "freeze the current moment". Re-analyze on demand or on a rolling buffer.
4. **processBlock.** Feed the live params to `PsiSynth`, `render()` into the output buffer.
   Tempo-sync `rate` to the host BPM (PoS #3) via `getPlayHead()`.
5. **Editor.** Port the lab's UI feel; keep it minimal for v0.1.

Deferred to v0.2+: AI-intent presets (PoS #5), the AudioLab API/MCP bridge (PoS #6), and
phase-locking (the known "phasiness" quality upgrade noted in the lab code).

## Licensing — simple with upselling (decision 4)

- **Free / demo:** full sound, with a gentle limiter (periodic silence blips or a save
  lock) so it's usable to evaluate but not to ship — the standard plugin-demo pattern.
- **Paid unlock (one-off):** the base plugin, perpetual.
- **Upsell tiers:** preset packs, the AI-intent presets (v0.2), and the AudioLab-ecosystem
  bridge as add-ons or a subscription layer on top of the one-off base.

Keep the base purchase simple; put recurring value in the AI + ecosystem layer.

## Decisions still needed (Nathan)

1. **Plugin name** (marketing name vs the "Ψ" mark) — needed for the bundle, and to name
   the GitHub repo (kept local + private for now until named).
2. **Vendor / manufacturer:** JUCE needs a 4-char manufacturer code + plugin code, a
   company name, and a bundle identifier (e.g. `tools.audiolab.psi`).
3. **Param ranges** — confirm the proposed ranges above (esp. max stretch and smear).
4. **Licensing mechanic** — which demo limiter, and which unlock system (e.g. a key file,
   an online check against the AudioLab account, or a marketplace like Plugin Boutique).
5. **Distribution** — own store (ties to the AudioLab account) vs a marketplace vs both.
