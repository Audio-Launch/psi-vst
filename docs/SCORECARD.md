# Scorecard — Ψ Spectral

> Levend document. Elke iteratie bijgewerkt. Meet de afstand tot SOTA uit het PDD (v1.1-concept).
> Laatste update: catch-up + amendement · 2026-07-13

## Stand per dimensie

| Dimensie | Score | Streef | Bewijs (één regel) |
|---|---|---|---|
| Functionaliteit (audio e2e) | 3.5 | 4.5 | 48-band M/S EQ-DSP LIVE in de DAW (it.4); DAW-bus-capture gebouwd (it.6, FL-test pending); morph-DSP nog niet live |
| Meten (differentiator) | 4 | 5 | Arc-meter volgt de DAW-mix (momentary LUFS + peak); rest = true-peak (oversampling) + integrated LUFS |
| Viz / deel-haak (differentiator) | 4.5 | 5 | Voxel-city + watermerk; capture voedt 'm nu met echte DAW-audio (it.6); rest = in-plugin export |
| UX & flow | 4.0 | 4.5 | Nav in scherm; mode-identiteit (cyaan/oranje) + mode-presets (it.5); rest = brush-unificatie + granulariteit + value-entry |
| Visuele hardware-feel | 4 | 4.5 | Skin/bezels/knobs/arc-meter sterk; finesse rest |
| Architectuur & code | 3 | 4 | Monoliet ~1500 regels; C++ nu netjes gelaagd (DSP/capture/connect) maar web-kant blijft één file |
| Performance | 3.5 | 4.5 | UI 60fps; EQ-DSP live zonder klachten; nog geen systematische DSP-load-meting |
| Robuustheid | 4 | 4.5 | Zelfstandige bundel (geen dev-server-gat meer, it.4); capture-review: 3 race/UB-wortels gefixt (it.6); rest = brede host-matrix |
| **Labs / platform-integratie (v1.1)** | 3 | 4.5 | It.7: tab + MixLab analyse/target-check e2e gebouwd (kostenlabels, gates, rapport-tiles, watchdog); rest = FL-e2e-bevestiging + Should-labs (spectrum/voice/compare) + history-link zichtbaar |

**Totaal / gemiddelde (excl. nieuwe dimensie):** 3.8 · **Vooruitgang sinds it.3:** +0.4 (EQ-DSP + bundel + capture + modes)

## Grootste resterende gaten (volgende keuze)
1. **Labs-tab (nieuwe Must, na PDD-akkoord)** — MixLab loudness+target e2e op de capture; server-side bestaat alles al.
2. **Meten → 5** — true-peak (4× oversampling) + integrated LUFS in C++.
3. **Functionaliteit → 4.5** — morph-DSP live (duurste blok; batchen).

## Iteratielog
- **Iteratie 7 (2026-07-13):** Labs-tab (PDD v1.1 Must) — '⚗ Labs' naast 2D/3D/Viz; MixLab loudness-analyse + target-check op de capture; '1 call · ~MB'-labels; key blijft in C++ (signed-URL-keten). Review-fixes: geneste /v1-fouten → string vóór render; tile-keys = echte engine-keys; 90s-watchdog tegen dode knoppen. Labs 0→3. **Pending: install + FL-e2e.** Processnotitie: reviews voortaan zelf/gericht (tokenfeedback Nathan).
- **Iteratie 6 (2026-07-13):** DAW-bus capture — C++ tap (post-EQ stereo, 60s, arm-flag reset op de audio-thread) + `/__capture.f32` resource + Record-knoppen (File/empty/transport) + kosten-chip "local DSP · 0 API". Adversariële review: 7 bevindingen → 3 wortels gefixt (capturePos-reset in prepareToPlay; CriticalSection om setSize vs memcpy; stopCapture in ~PsiEditor). Functionaliteit 3→3.5, Robuustheid → 4. **Pending: FL-test.**
- **Iteratie 5 (2026-07-12/13):** mode-presets + pass-thru live-surface; RFC 8252 device-connect (PKCE, BCryptGenRandom, back-channel exchange) + in-VST usage-stats; account-koppeling e2e bevestigd in FL.
- **Iteratie 4 (2026-07-11/12):** 48-band M/S EQ-DSP live (biquads, dubbel-gebufferde gains); zelfstandige bundel (webui.zip + resource-provider) — dev-server-gat dicht. Functionaliteit 1.5→3, Robuustheid 3→4.
- **Iteratie 3 (2026-07-11):** JS↔C++ bridge + echte output-metering (K-weighted momentary LUFS + peak) — C++ meet de DAW-stream, editor-timer pusht "meter"-events, UI toont ze i.p.v. de browser-analyser. Meten 2→3. **Pending:** FL-reinstall + Nathan's live-test (speel track → meter beweegt) → dan naar ~4. Volgende: EQ-DSP (Functionaliteit), dan true-peak/integrated.
- **Iteratie 2 (2026-07-10):** 3D-camera + Viz-nav ín het scherm (zwevende onderrand-balk), losse rij weg. UX 3.5→4.0.
- **Iteratie 1 (2026-07-10):** brand-watermerk op het scherm (uitzetbaar via View-menu) — de deel-haak/funnel. Viz 3.5→4.5.
- **Iteratie 0 (2026-07-10):** baseline vastgesteld tegen het geverifieerde PDD. Gemiddelde 3.0. Strategie: goedkope UI/visueel-differentiator-winst eerst (hot-reload), dan het dure bridge+DSP-blok.

## Status
- [ ] Done (alle dimensies ≥ streef, Meten + Viz op 5)
- [ ] Geconvergeerd (< 5% vooruitgang)
- [ ] Iteratieplafond (15) bereikt
- [ ] Gepauzeerd — wacht op gebruiker
