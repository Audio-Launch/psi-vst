# Sessielog — 2026-07-10 · SOTA-loop start

## Context
Ψ Spectral draait als v0.1 VST3 + Standalone in FL Studio (WebView-UI + pass-through audio). Toolchain (VS2022 C++, CMake), WebView2-crash, en VST-vs-VST3-pad zijn opgelost — zie [[vst-direction]] memory. Nu SOTA-loop gestart.

## PDD & scope (geverifieerd)
- Scope = **heel het instrument incl. DSP** (bridge, EQ, morph, Record/Pass-thru), end-to-end in de DAW.
- Lat = **eigen hardware-standaard** (niet FabFilter naäpen); differentiators = **Meten + Viz** (streef 5).
- Aanpak = **goedkope UI/visueel-gaten eerst** (hot-reload), **DSP-blok daarna** (rebuild+FL, duur).
- Audio-kwaliteit = **Nathan's oren** als meetpunt.

## Bestanden
- `E:\psi-vst\docs\PDD.md` — bron van waarheid.
- `E:\psi-vst\docs\SCORECARD.md` — gap-tracker (baseline gemiddelde 3.0).
- UI-code: `E:\New audiolab.dev\src\components\PsiSpectralEQ.tsx` (+ `psi/Controls.tsx`, `styles/global.css`). Plugin-route: `/plugin`.
- Plugin: `E:\psi-vst\src\Plugin*.{h,cpp}`, `CMakeLists.txt`.

## Dev-workflow
- UI-wijziging → save → hot-reload live in de plugin (dev-server :4321 moet draaien).
- C++-wijziging → rebuild (`cmake --build build --config Release`) → kopieer .vst3 naar `C:\Program Files\Common Files\VST3\` (admin/UAC) → FL rescan.

## Voortgang
- Iteratie 0: baseline. Volgende: goedkope differentiator-winst (Viz-branding), dan UX-gaten, dan bridge+DSP.
