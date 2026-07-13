# Sessie 2026-07-13 — capture + RFC 8252 + Labs-tab-amendement

## Gedaan
- **RFC 8252 device-connect compleet** (server e87f0ec + web 6c64ae9 + C++ b632552): PKCE-code
  i.p.v. key in de loopback-URL; e2e bevestigd in FL door Nathan.
- **Account-consolidatie:** jdrenting@gmail verwijderd (wegwerp, 0 calls); nathanrenting = hoofd;
  Google her-linken kan via "+ Link Google" op /account (manual linking aan).
- **DAW-bus capture (iteratie 6):** C++ tap → /__capture.f32 → ingest in terrain/Viz; kosten-chip.
  Adversariële review (7 bevindingen, 3 wortels) gefixt; **rebuild klaar (03:5x-binary), instal-
  latie + FL-test pending (FL was open).**
- **Scorecard catch-up:** iteraties 4–6 bijgeschreven; gemiddelde 3.4 → 3.8.

## Labs-tab (sota-loop amendement, PDD v1.1-CONCEPT)
- Intake-antwoorden Nathan: labs **1:1 met het platform, samen uitbreidend**; resultaat
  **in-plugin + /account-history**; kostenlabel **'1 call · ~MB' op de knop**.
- Architectuur gecacht in PDD §4: /v1-endpoints bestaan al; upload via /api/upload-url signed-PUT
  (key/<hash>/-namespace); **key blijft in C++**, editor doet de cloud-keten, UI rendert.
- Nieuwe rubriekdimensie "Labs / platform-integratie" (streef 4.5), score 0.
- **Status: WACHT OP NATHAN'S PDD-VERIFICATIE** (de verplichte gate) + evt. rapport-referentie-
  screenshots (Youlean / Loudness Penalty-achtig als extern ijkpunt genoteerd).

## Openstaand
1. Nathan: PDD v1.1 akkoord (of bijstellen).
2. Plugin installeren (FL dicht) + FL-test: capture → terrain/Viz; connect-regressie.
3. Commits capture (web + vst) — berichten staan klaar in scratchpad.
4. Web-deploy niet nodig voor capture (plugin-only bundel), wel t.z.t. voor /plugin-pariteit.
