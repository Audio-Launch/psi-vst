# Project Design Document (PDD) — Ψ Spectral

> Bron van waarheid voor het hele bouwproces. De loop meet elke iteratie tegen dit document.
> Versie: 1.1 · Datum: 2026-07-13 · Status: geverifieerd (Nathan akkoord: v1.0 2026-07-10, v1.1 2026-07-13). Rapport-referentie-screenshot: niet geleverd → huisstijl /account-stat-tiles is het ijkpunt (genoteerd als gat, gecompenseerd met Youlean/Loudness-Penalty-patronen uit onderzoek).

## 1. Visie & probleem
**Ψ Spectral** = een gratis, beeldschone, tactiele VST die je je sound laat **zien, meten én boetseren** — "control time & space in audio." Wint niet als "weer een EQ" (dat veld is bloedig), maar op wat niemand heeft: **live meten (LUFS/dBTP) in de EQ zelf** + een **screenshot-waardige visualizer**, in een **hardware-jasje**. Doel: AudioLab bekend maken (de viz = de deel-haak) en funnelen naar de betaalde platform-laag.

**Het "één ding":** de gebruiker ziet + meet + vormt zijn geluid in één tactiel instrument — en wil er een screenshot van delen.

## 2. Doelgroep & kernscenario's
- **Primair:** producers/mixers (bedroom → pro) in een DAW (FL Studio eerst).
- **Scenario's (jobs-to-be-done):**
  1. Op de master: zie + meet je mix (LUFS/dBTP), spot een probleem in de viz, sculpt/tame het.
  2. Record een stuk (het "lint") → skyline-viz → scrub + bewerk per frame.
  3. Pass-thru: live EQ + meten als insert op de bus.
  4. Deel een screenshot van je track-skyline (de funnel).

## 3. SOTA-benchmark
**De lat = onze eigen hardware-standaard** (Nathan's keuze): premium tactiel gevoel, screenshot-waardige gebrande viz, uitblinkend live-meten. Niet FabFilter c.s. naäpen — wél de technische baselines evenaren, en op viz + meten er duidelijk bovenuit.

- **Referenties (feel/visueel):** de glow-VU-render + MPC-controllers (hardware-gewicht, koper/glow), de arc-meter, de voxel-city-viz. Brand: audiolab.tools OKLCH-tokens (ink/signal/warm), Geist + JetBrains Mono, cyaan accent + oranje pass-thru.
- **Technische SOTA-baselines (onderzoek — gecacht, niet opnieuw doen):**
  - **Bridge:** JUCE 8 WebView native integration — `Options::withNativeIntegrationEnabled()` + `withNativeFunction(name, cb)` (JS→C++) + `WebBrowserComponent::emitEventIfBrowserIsVisible(eventId, var)` (C++→JS). In de WebView: `window.__JUCE__` (of het `juce` frontend-module). Bewezen pad.
  - **EQ-DSP:** peaking biquad-bank (juce::dsp::IIR), zero-latency, click-free via param-smoothing; M/S-routing zoals de web-versie.
  - **Metering:** BS.1770-4 — momentary (400ms) / short-term (3s) / integrated LUFS + true-peak via ≥4× oversampling. Web-versie heeft nu een K-weighted momentary benadering; C++ doet 't accuraat op de DAW-stream.
  - **Morph-DSP:** phase-vocoder (`dsp/PsiVocoder.h`, STFT N=2048/hop 512, geverifieerd tegen golden). Realtime op live input = rolling analyse + inherente latency (block-based).
  - **Bekende valkuilen:** WebView per-instance geheugen (~50–150MB); realtime-audio-thread veiligheid (géén allocaties/locks/mutexen in processBlock); clicks bij param-sprongen (smoothing verplicht); bridge-throughput (meters ~30–60/s pushen, niet per sample); WebView leeg als dev-server down (v1 dev-afhankelijkheid, opgelost in v0.2 door bundelen).

## 4. Scope
**In scope (v1 SOTA-instrument, end-to-end in de DAW):**
- UI/UX naar SOTA: hardware-skin-finesse, tool-HUD, arc-meter, mode-systeem, **viz screenshot-waardig + gebrand**, plus de gestagede items (nav-in-scherm, brush-unificatie over 2D/3D/Viz, granulariteit-QoL, viewer-pivot, value-entry).
- **JS↔C++ bridge:** params ↔ DSP, live meters/analyse ↔ UI.
- **EQ-DSP:** de biquad-bank EQ't de DAW-audio echt (per M/S), click-free.
- **Metering:** echte DAW-loudness in de meter (BS.1770 momentary + true-peak; integrated indien haalbaar).
- **Morph-DSP:** de time-engine morpht echt (freeze/reverse/stretch/smear).
- **Record/Pass-thru:** Record cap't een lint (live of import) → viz; Pass-thru = live insert; zichtbare cyaan/oranje-modus.
- **Robuustheid:** geen crashes (ook in FL-scan/verify), edge-cases, empty/loading-states, geen audio-glitches/clicks.

**In scope sinds v1.1 (amendement 2026-07-13 — Labs-tab):**
- **Labs-tab** naast EQ/Viz: de AudioLab.tools-labs 1:1 in de plugin, samen met het platform
  uitbreidend (config-gedreven lab-lijst, nieuw lab = nieuwe entry, geen verbouwing).
- **v1-lab-acties op de capture/import:** MixLab loudness-analyse + target-check (Spotify/EBU/…),
  spectrum; VoiceLab QA + segments; SignalLab index; A/B-compare (twee captures) zodra het
  e2e-pad staat. Rapporten in huisstijl, in-plugin.
- **Kosten-transparantie:** elke cloud-knop toont '1 call · ~X MB' + resterend tegoed; lokale
  acties ongemarkeerd (= gratis). Principe: **gewoon EQ/viz/capture-gebruik kost nooit een call.**
- **Resultaat-bestemming:** in-plugin rapport én meetellend in /account-stats + history
  (de account-koppeling die de device-connect al legde).
- **Architectuur (gecacht onderzoek, 2026-07-13):** de hosted API bestaat al — `POST /v1/mixlab/analyze
  | check-target | spectrum | timeseries`, `/v1/voicelab/qa | segments`, `/v1/signallab/index`
  (JSON `{url|objectPath}` of raw ≤4,5MB). Uploadpad = `/api/upload-url` signed-PUT naar Supabase
  Storage (`key/<hash>/`-namespace, zelfde meters, bucket-cap 50MB) → `/v1 {objectPath}`.
  **De device-key blijft in C++**: de editor doet upload-url → PUT (WAV 16-bit stereo van de
  capture) → /v1-call op een background-thread en emit het rapport als bridge-event "lab";
  de UI rendert alleen. Vermijd voorlopig `timeseries`/`segments` op lange audio
  (bekende payload-bug) — capture is ≤60s, dus veilig.

**Uitgesteld / buiten scope:**
- ~~v0.2 asset-bundeling~~ → **GEDAAN** (webui.zip + resource-provider, 2026-07-12).
- ~~Account-hooks~~ → **GEDAAN** (RFC 8252 device-connect + usage in de UI, 2026-07-13); Pro-unlock/upsell-flows blijven uitgesteld.
- De familie van plugins (andere losse plugins).
- Distributie / licensing / marketplace.
- AU/AAX en andere formaten (VST3 + Standalone eerst).
- Node-mode brushes (FabFilter-stijl) — Could, geen Must.

## 5. Functionele specificatie (MoSCoW)
- **Must:** bridge werkt; EQ EQ't echt (click-free, M/S); echte meter (momentary LUFS + true-peak); mode-systeem functioneel; geen crashes; UI-finesse-pass; viz screenshot-waardig. **v1.1:** Labs-tab met MixLab loudness+target-check e2e op de capture, met kostenlabels en nette foutpaden.
- **Should:** morph-DSP live; ~~Record-lint-capture~~ (GEDAAN 2026-07-13); integrated LUFS; nav-in-scherm; brush-unificatie over views; value-entry. **v1.1:** spectrum + voice-QA + signal-index in de tab; compare (2 captures); zichtbare /account-history-koppeling.
- **Could:** granulariteit-QoL (horizontaal/verticaal); viewer-pivot; node-mode brushes; viz-export + watermerk-toggle. **v1.1:** rapport-export/share (de funnel), per-lab deep-link naar het platform-lab.
- **Won't (nu):** ~~asset-bundeling~~ (gedaan); Pro-unlock/upsell-flows; andere formaten; plugin-familie (losse plugins).

## 6. UX & schermflow
- **Schermen/states:** leeg (Open audio / test clip) · geladen 2D-curve · 3D-landschap · Viz-city · Pass-thru (viz grijs). Menubalk (File/Edit/Presets/View/Help) + mode-toggle + A/B + undo/redo. Links tool-HUD, midden scherm, rechts arc-meter, onder transport + time-engine.
- **Kerninteracties:** sleep-sculpt (2D-curve, 3D-landschap), band-select + clean, hover-readout (Hz·dB + footprint), Viz zoom/scrub/tilt, knobs (drag + dubbelklik-reset), mode-swap.
- **Micro-UX (feel):** detent-knobs met glow-LED, keys met press + cyaan/oranje glow, vloeiende arc-meter met peak-hold, intro-cinematic (reveal → fold), hot-reload-live-in-DAW.

## 7. Visuele richting
- **Sfeer:** premium tactiele hardware — geborsteld donker metaal, bronzen/koperen bezels, ingelaten schermen, glow. Dark-committed.
- **Kleuren:** OKLCH ink-schaal; accent cyaan (`signal-400`) in Record, oranje (`oklch 0.71 0.185 52`) in Pass-thru; amber = "hot"; rood = over.
- **Typografie:** Geist Variable (labels), JetBrains Mono (readouts/tabular).
- **Kerncomponenten:** `psi/Controls.tsx` (PsiBtn/PsiSeg/PsiSlider/PsiKnob/PsiMenuBar) + `.psi-panel/.psi-bezel/.psi-recess/.psi-key(-on)` in global.css.

## 8. Technische architectuur
- **UI:** Astro/React/CSS/SVG/WebGL, route `/plugin` (full-bleed), in `E:\New audiolab.dev`. Dev: hot-reload van dev-server (:4321). Ship (v0.2): gebundeld + door JUCE-resource-provider geserveerd.
- **Plugin:** JUCE 8 VST3 + Standalone in `E:\psi-vst`. WebView2 (static link). DSP native: EQ biquad-bank (M/S) + `psi::PsiSynth` morph. Bridge via native integration.
- **Datamodel:** UI-params (M/S EQ-curves als filterbank-gains, brush/depth/Q, mode, morph: rate/freeze/dir/smear, A/B-bypass) → C++ DSP. C++ → UI: meters (rms/peak/LUFS/true-peak), evt. analyse-frames voor de live-viz.
- **Realtime-contract:** processBlock alloc-vrij, lock-vrij; UI↔DSP via atomics/lock-free FIFO; meters gepusht op timer (~30–60Hz), niet per sample.

## 9. Niet-functionele eisen
- **Performance:** geen dropouts/clicks; lage CPU; geen allocaties in processBlock; param-smoothing. UI 60fps; WebGL efficiënt.
- **Robuustheid:** geen crashes in scan/load/gebruik; graceful zonder audio/zonder dev-server (nette melding i.p.v. wit).
- **Toegankelijkheid:** toetsenbord (undo/redo aanwezig; uitbreiden), focus-states, contrast, reduced-motion (aanwezig).
- **Security/privacy:** lokaal, geen upload, geen data-opslag.

## 10. Kwaliteitsrubriek (de meetlat van de loop)
Dimensies gewogen op Nathan's lat — **Meten** en **Viz/deel-haak** zijn de differentiators en wegen zwaar (streef = 5).

| Dimensie | SOTA-criterium | Streefscore |
|---|---|---|
| Functionaliteit (audio end-to-end) | EQ + morph + meten + modi werken écht in de DAW, click-free | 4.5 |
| **Meten (differentiator)** | Live BS.1770 momentary LUFS + true-peak, accuraat én mooi in de arc-meter | 5 |
| **Viz / deel-haak (differentiator)** | Screenshot-waardig, gebrand, leest muzikaal (riser/wobble/kick herkenbaar) | 5 |
| UX & flow | Logische plaatsing, pro-feel, snel, geen frictie; nav in scherm | 4.5 |
| Visuele getrouwheid (hardware-feel) | Voelt als premium tactiele hardware (metaal/koper/glow/detent) | 4.5 |
| Architectuur & codekwaliteit (web + C++) | Schoon, realtime-safe, onderhoudbaar, geen debt | 4 |
| Performance | Geen dropouts/clicks, lage CPU, 60fps UI | 4.5 |
| Robuustheid | Geen crashes; edge/empty/loading/geen-server netjes | 4.5 |
| **Labs / platform-integratie (v1.1)** | Capture → 1 klik → rapport in huisstijl; kosten per knop expliciet ('1 call · ~MB'); resultaat ook op /account; nieuw lab = config-entry; foutpaden (offline/geen tegoed/geen account) netjes | 4.5 |

**Meetmethode:** UI/visueel/robuustheid meet ik zelf (browser + screenshots + jouw oog); **audio-kwaliteit deels via jouw oren** — ik bouw + run de Standalone, jij beoordeelt het geluid (dropouts/clicks/morph-kwaliteit). Dat markeer ik expliciet op de scorekaart.

## 11. Definition of Done & convergentie
- **Klaar wanneer:** alle dimensies ≥ streefscore, met **Meten + Viz op 5**. Evenaart/verslaat de eigen hardware-lat; de differentiators steken er duidelijk bovenuit.
- **Convergentie:** stop als twee opeenvolgende iteraties < ~5% totale vooruitgang geven (dan rapporteren i.p.v. doorpolijsten).
- **Iteratieplafond:** 15 (hoger dan default vanwege de brede "heel het instrument"-scope; DSP-iteraties zijn duur).

## 12. Open vragen & risico's
- **Risico — DSP-iteratie traag/duur** (elke wijziging = C++ rebuild + FL-test). *Mitigatie:* UI/visueel-gaten eerst (hot-reload, goedkoop), DSP-gaten gebatcht; binnen "heel het instrument" sequence ik zo.
- **Risico — realtime morph latency/kwaliteit.** *Mitigatie:* begin met EQ + meten (zero-latency, hoge zekerheid); morph daarna.
- **Risico — audio hoor ik niet volledig autonoom.** *Mitigatie:* jouw oren zijn het meetpunt op de audio-dimensies.
- **Open — plugin-naam** (Psi Spectral / Ψ / TerraEQ). Geparkeerd, niet blokkerend.

## 13. Changelog
| Versie | Datum | Wat veranderde | Waarom |
|---|---|---|---|
| 1.0 | 2026-07-10 | Eerste versie (wacht op verificatie) | Start SOTA-loop na v0.1-in-FL |
