// Verifies the JS phase-vocoder oracle and emits golden reference values that the
// C++ port (dsp/PsiVocoder.h) must reproduce. Run: node dsp/reference/test_psi.mjs
import assert from 'node:assert/strict';
import { writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { analyze, PsiSynth, makeChirp } from './psi-dsp.mjs';

const finite = (arr) => arr.every((v) => Number.isFinite(v));
const rms = (arr) => Math.sqrt(arr.reduce((s, v) => s + v * v, 0) / arr.length) || 0;

const sr = 48000;
const input = makeChirp(sr, 2);
const { mag, phase, F, BINS } = analyze(input);
assert.ok(F > 100, `expected many frames, got ${F}`);
assert.ok(finite(mag) && finite(phase), 'analysis produced non-finite values');
console.log(`analyze: ${F} frames, ${BINS} bins`);

// Each creative mode must yield finite, non-silent audio.
const modes = [
  { name: 'identity  (rate=1)', p: { rate: 1 }, n: 4096 },
  { name: 'stretch   (rate=0.5)', p: { rate: 0.5 }, n: 4096 },
  { name: 'freeze    (frozen)', p: { rate: 1, frozen: true }, n: 4096 },
  { name: 'reverse   (dir=-1)', p: { rate: 1, dir: -1 }, n: 4096 },
  { name: 'smear     (smear=4)', p: { rate: 0.5, smear: 4 }, n: 4096 },
];
for (const m of modes) {
  const s = new PsiSynth();
  s.setAnalysis(mag, phase, F);
  s.setParams(m.p);
  const out = s.render(m.n);
  assert.ok(finite(out), `${m.name}: non-finite output`);
  const r = rms(out);
  assert.ok(r > 1e-4, `${m.name}: output is silent (rms ${r})`);
  console.log(`  ${m.name}: rms ${r.toFixed(5)}  ok`);
}

// Freeze must NOT advance the read position (holds one spectral frame).
{
  const s = new PsiSynth();
  s.setAnalysis(mag, phase, F);
  s.setParams({ rate: 1, frozen: true });
  s.render(8192);
  assert.equal(s.rp, 0, `freeze should hold rp at 0, got ${s.rp}`);
  console.log('  freeze holds read position: ok');
}

// Golden reference: fixed config the C++ test must match (within tolerance).
const g = new PsiSynth();
g.setAnalysis(mag, phase, F);
g.setParams({ rate: 0.5, frozen: false, dir: 1, smear: 0 });
const golden = g.render(4096);
const idx = [0, 512, 1024, 2048, 4095];
const goldenRef = {
  config: { sr, dur: 2, N: 2048, Ha: 512, Hs: 512, rate: 0.5, dir: 1, smear: 0, frozen: false, samples: 4096 },
  frames: F,
  rms: Number(rms(golden).toFixed(8)),
  samplesAt: Object.fromEntries(idx.map((i) => [i, Number(golden[i].toFixed(8))])),
};
const outPath = fileURLToPath(new URL('./golden.json', import.meta.url));
writeFileSync(outPath, JSON.stringify(goldenRef, null, 2) + '\n');
console.log('\ngolden reference written to golden.json:');
console.log(JSON.stringify(goldenRef, null, 2));
console.log('\nALL JS ORACLE CHECKS PASSED');
