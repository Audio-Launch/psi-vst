# TerraEQ — the analysis-guided spectral sculptor (working name)

> The "upgrade of the EQ." You see the track as terrain (time × frequency × loudness)
> and sculpt it with brushes, like terraforming in Rollercoaster Tycoon — but the
> **AudioLab engine already knows where the problems and the interesting features are**,
> so it marks the terrain and hands you the right tool. RX makes you hunt; TerraEQ points.
>
> Shares the STFT analysis + phase-vocoder resynthesis core with Ψ (`dsp/PsiVocoder.h`).
> Positioning: its own product/story (everyone uses EQ); technique: the same shared engine.

## The one idea that makes it different

A normal EQ applies **one static curve to the whole track**. RX/SpectraLayers let you edit
the spectrogram but you must **find** every problem yourself with your eyes and ears.

TerraEQ is EQ that acts **only where and when you paint** (surgical in time *and*
frequency), **and** the engine pre-annotates the terrain: "harsh ridge at 3 kHz in the
chorus", "mud at 250 Hz throughout", "sibilant peak at 0:45", "clip at 1:12". One click on
a marker loads the right brush over the right region. That guidance layer — built from
analysis AudioLab already computes — is the moat.

## The 4D terrain

- **x = time, y = frequency, z = magnitude (height).** A literal spectrogram-as-heightmap.
- **4th D = it plays while you sculpt** — real-time inverse-STFT resynthesis (the phase
  vocoder already does this) + automatable over the timeline. Live, tactile feedback is
  what makes it feel like terraforming, not spreadsheet-editing.
- Rendered on the GPU (WebGL / a plugin GL view); the raw data is the same `mag[frame][bin]`
  grid `psi::analyze()` produces.

## Engine knowledge → terrain intelligence (the core of "apply the engine optimally")

Every one of these AudioLab engine outputs becomes a terrain annotation + a suggested brush.
The engine runs its existing feature detectors **per time-region** on the same STFT, turning
track-level labels into localized hotspots:

| Engine output (already computed) | Terrain annotation | Suggested brush |
|---|---|---|
| `harshness` (Low/Med/High) | harsh ridge, ~2–5 kHz, where/when it spikes | Flatten / Scoop |
| `muddiness` (Low/Med/High) | low-mid buildup, ~150–400 Hz | Scoop |
| `bandEnergy` (7 bands) + `dominantBand` | tonal-balance heatmap: hot vs dull bands | Level / Raise |
| `sibilanceRisk` (voice) | sibilant peaks in the presence band, time-localized | De-ess (dynamic scoop) |
| `clippingRegions` / `hasClipping` | clip markers on the timeline | Heal (repair) |
| `spectralCentroid` / `rolloff` / `flatness` | overall brightness + noisiness tilt | global Tilt guide |
| `contentTypeGuess` (music/voice/…) | picks the default brush preset + targets | preset switch |
| `silenceRegions` | dead-air regions | Erase / trim |
| `stereoCorrelation` / mono-compat | mono-trouble bands (per-channel diff) | width-aware cut |

Result: open a track and the terrain is already lit up with "here's what your ears would
eventually find." Repair engineers save hunting time; creators get pointed at the juicy
features to exploit.

## Brush set v1

Two families, one surface:

**Repair / corrective (clean, phase-safe — magnitude reduction is easy):**
- **Scoop** — carve magnitude down in the painted region (surgical, time-following notch).
- **Flatten** — pull a ridge toward the surrounding level (tame a resonance only when it rings).
- **De-ess** — a scoop keyed to detected sibilance, follows the esses automatically.
- **Heal** — repair a clip/click by interpolating magnitude+phase from neighbours.
- **Erase** — magnitude → 0 in a region (remove a cough, a bleed, dead air).

**Creative (needs care on phase — big boosts are the hard part):**
- **Raise / Boost** — lift a dull region (phase-coherent up to moderate gain; flag beyond).
- **Level** — normalize a band's energy over time (even out a wandering resonance or a riser).
- **Smooth** — average neighbouring frames (Ψ's smear, but painted onto a region).
- **Freeze-paint** — Ψ's freeze applied to selected bins only (hold part of the spectrum).

### Phase approach per brush (the honest engineering)

- **Reduction brushes** (scoop/flatten/de-ess/erase): scale magnitude, keep original phase.
  Clean, low-artifact. Ship these first.
- **Heal**: interpolate both magnitude and phase across the gap from neighbouring frames.
- **Boost/raise**: magnitude up with original phase works to ~+6 dB; beyond that, phase
  incoherence ("phasiness") shows. v1 caps boost + warns; v2 adds phase-locking (the upgrade
  already noted in the Ψ code) for clean large boosts.
- **Freeze-paint/smooth**: reuse Ψ's existing frozen/smear math per-bin.

## Serves both creators and repair

The smart layer is the same; the **preset** differs by `contentTypeGuess`:
- **Voice/podcast** → de-ess, breath/plosive erase, mud scoop, clip heal (repair-leaning).
- **Music** → resonance flatten, mud scoop, creative freeze-paint + level (creative-leaning).

So we do not have to choose the niche up front; the engine picks the toolset per file.

## Architecture

- **Shared core:** `psi::analyze()` gives the `mag/phase` grid; a resynth path (extend
  `PsiSynth`) does inverse-STFT of the *edited* grid. The edit is a per-bin gain/phase mask
  applied before resynthesis — cheap and real-time.
- **Analysis layer:** run the engine's feature detectors per time-window → the annotation
  set. This is the "AudioLab knowledge" applied; it can run locally (WASM) or via the hosted
  API/MCP for heavier passes.
- **Edit model:** a stack of non-destructive brush strokes (region + brush + amount), so
  everything is undoable and automatable. The strokes compile to the per-bin mask.
- **Render:** GPU heightmap of the magnitude grid + overlay for annotations + brush cursor.
- **Delivery:** same choice as Ψ — VST3 (Windows-first) and/or a standalone/web app. The
  spectrogram-GL + brush UX may be nicer as a standalone/web first, then a plugin.

## MVP (v0.1)

1. Load audio → `analyze()` → render the terrain (GPU heightmap).
2. The engine annotation layer: harshness / muddiness / sibilance / clipping hotspots lit
   up, with one-click "load the right brush here".
3. Three reduction brushes: **Scoop, Flatten, De-ess** (phase-safe, ship-clean).
4. Live resynthesis playback of the edited grid + A/B against the original.
5. Non-destructive stroke stack + undo.

Defer: boost/raise with phase-locking, Heal, Level, Freeze-paint, automation lanes,
plugin packaging (prototype as standalone/web first to validate the interaction).

## Decisions needed (Nathan)

1. **Name** — TerraEQ? SculptEQ? Something on-brand with the "Lab" family, or its own mark?
2. **First surface** — standalone/web app (easier GL + UX) or straight to VST3?
3. **Lead niche for the launch demo** — creators (musical, playful) or repair (clean-up),
   knowing the engine serves both. Sets the hero use-case + marketing.
4. **Where the analysis runs** — fully local (WASM) for privacy, or hosted API for heavier
   detectors (ties into the paid API + account).
5. **Relationship to Ψ** — one "spectral studio" product with Morph + Sculpt modes, or two
   sibling products sharing the engine? (Recommendation: TerraEQ leads as its own story;
   Ψ-morph becomes a mode inside the same spectral platform.)

## Decisions locked (2026-07-03)

- **Name:** TerraEQ (working) — no existing plugin by that name; terraeq.io/.app/.audio free (.com taken); can live under audiolab.tools.
- **View:** time left → right; bass bottom/left, highs top/right (standard spectrogram convention).
- **Sequencing:** the Ψ morph VST ships first (one plugin); TerraEQ is the later sculpt product on the shared STFT + phase-vocoder core.

## Feature backlog — from the 5-lens gap analysis (mix, master, sound-design, repair, competitive)

72 candidate gaps → prioritized. A recurring theme: many high-value items are **display/wiring over data the engine already computes** (the analysis-guided angle keeps paying off); the rest are genuinely new DSP.

### Quick wins (do first — value over effort)
1. **Gain-matched (LUFS-normalized) A/B preview** — the sharpened form of the preview-before-apply idea; `integratedLufs`+`truePeakDb` already measured, so it is wiring, and it makes every audition trustworthy (louder always sounds "better").
2. **Numeric readout + hover-to-read Hz/dB/time** — `get_spectrum` knows each bin's exact Hz; a display layer with outsized recall/communication value.
3. **Delta / residual monitor** — hear only what a stroke removes/adds; the mask already resynthesizes, so it is one subtraction on the existing path. The #1 "tool not toy" check.
4. **Per-stroke solo/mute + global wet/dry blend** — small gain/UI controls that make a deep stroke stack workable.
5. **Global tilt/shelf macro as one stroke** — most mastering EQ is one broadband gesture; a draggable tilt over the whole terrain, near-zero DSP.
6. **Calibrated dB color legend (adjustable floor/ceiling)** — turns "height = magnitude" from vibes into quantitative editing.

### Must-have (v1 table-stakes)
- Numeric readout + typed entry per stroke (see quick wins).
- Gain-matched A/B preview (see quick wins).
- Delta / residual monitor (see quick wins).
- **Mid/Side + L/R targeting per stroke, with a M/S terrain view** — de-ess only the Sides, tighten bass only in Mid; genuinely new DSP (core currently collapses to mono).
- **Dynamic / threshold-gated strokes** (bin acts only when the hotspot is actually hot, with attack/release) — the line between a spectral processor and a blunt mask.
- **Loop-region playback** — audition a bar/range cycling in context; nobody sculpts 3 minutes at once.
- **Additive/creative brushes** (Raise/Boost, harmonic/air excite, Smear) on the shared phase-vocoder — a tool that only digs holes is a de-noiser; add-moves are why you open it over a synth+EQ chain.
- **Delivery-grade render/export** — offline resynth to WAV/MP3 at target LUFS/true-peak + bit-depth/dither; no resynth-to-file path exists today.
- **DAW plugin path (VST3/AU/CLAP) with reported latency (PDC) + automation** — the can-I-even-use-it gate.
- **Learn-a-noise-print broadband denoise brush** — the #1 dialogue job; `noiseFloorDb` + a silence region already let it auto-offer "grab the print here".

### Should-have (v2)
- Reference-track spectrum overlay + match-to-reference target curve.
- Log-frequency terrain view with a note/harmonic grid, snap-to-pitch, harmonic-linked strokes.
- Adjustable STFT resolution (FFT/hop) or transient-vs-tonal mode + transient-preservation.
- Adjustable annotation sensitivity + dismiss/pin individual hotspots.
- Linear- vs minimum-phase toggle with honest latency + pre-ring readout.
- Tempo/grid lock (bars/beats ruler + snap) on the time axis.
- Presets/macros: save a stroke stack, apply across tracks/clips.
- De-click / de-plosive heal brush + 50/60 Hz hum notch (auto-flagged).
- De-reverb / room-echo brush wired to the existing `roomEcho` annotation.
- Region/selection scoping (lasso or time+freq box; "process only silences" / "only this speaker" via the voice/silence regions).
- Per-stroke + global wet/dry blend (see quick wins).
- A/B/C/D snapshot slots with morph between two terrain states.

### Nice-to-have (later)
Sidechain / spectral-difference brush · transient/tonal (harmonic-percussive) separation · automatable/keyframed stroke depth over the timeline · de-clip restoration (wired to `clippingRegions`) · breath-control brush · phase/pre-echo artifact meter · randomize/generative + spectral morph · marker/EDL export for video editors · oversampling toggle.
