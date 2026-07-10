# Build Ψ Spectral (v0.1 — WebView shell) and load it in FL Studio

v0.1 goal: prove the pipeline — the web UI running **inside FL Studio**, audio
passing through. DSP (EQ bank + the PsiVocoder morph) lands next. Claude can't
compile/test this — you build, we iterate on any errors together.

## Prerequisites (Windows)

- **Visual Studio 2022** with the *Desktop development with C++* workload.
- **CMake** ≥ 3.22 (bundled with VS 2022, or install standalone).
- **Git** (CMake fetches JUCE 8 automatically on first configure — a few minutes).
- **WebView2 Runtime** — already present on virtually all Win10/11. If the plugin
  window is blank, install the Evergreen runtime from Microsoft.

## 1. Keep the web UI running

The plugin's WebView loads `http://localhost:4321/plugin`. In the web repo:

```
cd "E:\New audiolab.dev"
npm run dev
```

Leave it running while FL is open. (v0.2 bundles the assets so this isn't needed.)

## 2. Build the plugin

```
cd E:\psi-vst
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

First configure downloads JUCE (slow, once). The VST3 lands at:

```
E:\psi-vst\build\PsiSpectral_artefacts\Release\VST3\Psi Spectral.vst3
```

## 3. Load in FL Studio

1. Copy `Psi Spectral.vst3` to your VST3 folder, e.g.
   `C:\Program Files\Common Files\VST3\` — **or** add `E:\psi-vst\build\...\VST3`
   to FL's plugin search paths (Options → Manage plugins → add path).
2. FL → **Manage plugins → Find installed plugins** (rescan).
3. Add **Psi Spectral** on a channel or the master. The web UI appears; audio
   passes through untouched (v0.1).

Tip: build the **Standalone** target first (`Psi Spectral.exe` in
`...\Standalone\`) to sanity-check the UI without a DAW.

## Likely iteration points (tell me the exact error)

- **WebView2 SDK not found at compile** — JUCE needs the WebView2 headers. If the
  build errors on `WebView2.h` / the loader, add the `Microsoft.Web.WebView2`
  NuGet package to the generated VS project (Visual Studio auto-restores it), or
  point JUCE at the WebView2 SDK. Paste me the error and I'll wire it in CMake.
- **Blank plugin window** — dev server not running, or WebView2 Runtime missing,
  or it fell back to the legacy backend. Check `http://localhost:4321/plugin`
  opens in Edge first.
- **JUCE 8 API drift** — if `WebBrowserComponent::Options` / `Backend::webview2`
  names differ in your JUCE tag, send the error; the enum moved across versions.

## Next (v0.2+)

- Bundle the built web assets (`npm run build`) + serve them from a JUCE
  resource provider → no dev server needed.
- JS↔C++ bridge (`WebBrowserComponent` native integration): UI knobs → params →
  DSP, and DSP meters/analysis → the UI.
- DSP: the EQ biquad bank + the `psi::PsiSynth` morph (dsp/PsiVocoder.h).
- Record/Import ↔ Pass-thru mode wiring (the capture buffer = the "lint").
