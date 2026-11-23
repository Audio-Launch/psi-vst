# Psi Spectral

An experimental Windows VST3 effect: a 48-band mid/side EQ, a loudness meter and a
capture-and-visualize view, drawn by a web UI inside the plugin (C++17, JUCE 8, WebView2).

**Status: early alpha, v0.1.0.** Developed and tried in FL Studio; other hosts are untested.
Not production software.

## What v0.1.0 does

- **EQ.** A 48-band peaking EQ, applied to mid and side separately (mono input: mid only).
  The curve is drawn in the UI and sent to the audio thread over a JS-to-C++ bridge
  ([`src/PluginProcessor.cpp`](src/PluginProcessor.cpp)).
- **Loudness meter.** K-weighted, ungated, momentary-style loudness with 0.4 s one-pole smoothing,
  plus sample peak and RMS, measured after the EQ. On sine tones at 44.1, 48 and 96 kHz the reading
  stays within about 0.05 dB of the K-weighting in libebur128 (not tested with the EBU test files).
- **Capture and visualizer.** "Record DAW bus" keeps up to 60 s of the post-EQ output in memory.
  The UI draws it as a 2D curve, a 3D landscape or a "Viz" skyline and can export a PNG to the Desktop.
- **Optional AudioLab account link** (browser sign-in, RFC 8252 loopback redirect with PKCE; the key
  is stored encrypted with Windows DPAPI, see [`src/DeviceConnect.h`](src/DeviceConnect.h)).
  Only the Labs tab needs it. Labs uploads the captured or opened clip (WAV) to AudioLab's cloud,
  through a signed upload URL, for a loudness analysis or target check; each action shows its cost
  up front.
- **Network use.** On opening the editor the plugin fetches `audiolab.tools/vst/version.json` to
  look for a newer build. Apart from that it makes network requests only for the account link and Labs.

## What it is not

- **Not a morpher in the DAW.** `dsp/` holds a phase vocoder (freeze, stretch, reverse, smear):
  a Node reference and a header-only C++ port. It is not part of the plugin build. The UI has
  Freeze/Reverse/Stretch/Smear controls, but they run in the UI's own Web Audio engine on an opened
  or captured clip, not on the DAW signal.
- **Not a certified loudness meter.** No gating, no integrated or short-term loudness, no true-peak
  in the live meter. The Labs report shows those values, computed by the hosted API.
- **No host integration beyond audio.** No automatable parameters, and no preset or project recall
  (the plugin saves no state).
- **Windows x64 VST3 only.** No macOS, Linux, AU, AAX or CLAP. The v0.1.0 binary is not code-signed,
  so Windows may show a warning.
- **No demo limiter, license key or purchase flow** exists in the plugin.

## Install (v0.1.0)

1. Download `Psi-Spectral-0.1.0-win-vst3.zip` from [Releases](https://github.com/Audio-Launch/psi-vst/releases/tag/v0.1.0).
   SHA-256: `e04469893fab5e870ae6c217a5cce0a115ad5dc6fe5fec1cddfb4de44f31f034`
2. Unzip and copy the `Psi Spectral.vst3` folder to `C:\Program Files\Common Files\VST3\`.
3. Rescan plugins in your DAW. The Microsoft WebView2 Runtime must be installed (it is part of
   Windows 11; on Windows 10 it may need a separate install).

## Build

Needs Visual Studio 2022 (C++ workload), CMake 3.22 or newer and Git. JUCE 8.0.4 is fetched by CMake.

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

**This repository does not build a working plugin on its own.** The web UI is developed outside
this repository and is not published here. The release build embeds a prebuilt bundle as
`webui.zip` (git-ignored; `CMakeLists.txt` stops without it unless configured with
`-DPSI_DEV_SERVER=ON`, which loads the UI from `http://localhost:4321/plugin` instead). The
WebView2 SDK (NuGet package `Microsoft.Web.WebView2`) must be unpacked into `third_party/webview2`
(also git-ignored). The DSP tests below need none of this.

## DSP tests

```
node dsp/reference/test_psi.mjs        # JS reference; also rewrites dsp/reference/golden.json
cd dsp/test
g++ -std=c++17 -O2 -D_USE_MATH_DEFINES -I.. test_psi.cpp -o test_psi && ./test_psi
```

Both print `ALL ... CHECKS PASSED` (last run with Node 24 and g++ 16.2 from MSYS2). The C++ port
matches the reference to within 1e-8 on the checked samples.

## Layout

- `src/` plugin: audio processor, editor and WebView bridge, account link
- `dsp/` phase-vocoder reference and port, with tests (not wired into the plugin)
- `scripts/` helper for bundling the UI
- `docs/RELEASE.md` the maintainer's release checklist (contains local paths)

## License

Not decided yet. Until a license is added, no right to copy, modify or redistribute the source is
granted. The v0.1.0 binary is free to use.
