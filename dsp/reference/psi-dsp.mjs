// Ψ phase vocoder — standalone, Node-runnable REFERENCE implementation.
//
// Math-identical to the browser AudioWorklet in the web repo
// (src/components/PsiMorpher.tsx). This module is the ORACLE: the C++ port in
// dsp/PsiVocoder.h must reproduce its golden outputs bit-for-bit-close (see
// dsp/test/). Pure math, no dependencies, so it ports cleanly to C++.
//
// Pipeline: analyze() runs a windowed STFT on the main thread and returns per-frame
// magnitude + phase. PsiSynth resynthesizes with the four creative params:
//   rate   — time-stretch factor (advance speed through frames; <1 = slower/longer)
//   frozen — hold the current frame (endless spectral pad)
//   dir    — playback direction (+1 forward, -1 reverse)
//   smear  — spectral blur radius in frames (Paulstretch-style cloud)

const TWO_PI = 2 * Math.PI;

// In-place iterative radix-2 FFT (decimation-in-time). inv=true → inverse (scaled by 1/n).
export function fft(re, im, inv) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { const tr = re[i]; re[i] = re[j]; re[j] = tr; const ti = im[i]; im[i] = im[j]; im[j] = ti; }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = (inv ? 2 : -2) * Math.PI / len, wr = Math.cos(ang), wi = Math.sin(ang);
    for (let i = 0; i < n; i += len) {
      let cr = 1, ci = 0; const h = len >> 1;
      for (let k = 0; k < h; k++) {
        const ur = re[i + k], ui = im[i + k], xr = re[i + k + h], xi = im[i + k + h];
        const vr = xr * cr - xi * ci, vi = xr * ci + xi * cr;
        re[i + k] = ur + vr; im[i + k] = ui + vi; re[i + k + h] = ur - vr; im[i + k + h] = ui - vi;
        const ncr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = ncr;
      }
    }
  }
  if (inv) for (let i = 0; i < n; i++) { re[i] /= n; im[i] /= n; }
}

// Principal-argument wrap into (-π, π].
const princ = (x) => x - TWO_PI * Math.round(x / TWO_PI);

const hann = (N) => { const w = new Float32Array(N); for (let i = 0; i < N; i++) w[i] = 0.5 - 0.5 * Math.cos((TWO_PI * i) / N); return w; };

// Windowed STFT. Returns Float32Array mag/phase laid out as [frame * BINS + bin].
export function analyze(mono, N = 2048, Ha = 512) {
  const BINS = (N >> 1) + 1;
  const win = hann(N);
  const F = Math.min(2400, Math.floor((mono.length - N) / Ha) + 1);
  const mag = new Float32Array(F * BINS), phase = new Float32Array(F * BINS);
  const re = new Float32Array(N), im = new Float32Array(N);
  for (let f = 0; f < F; f++) {
    const off = f * Ha;
    for (let i = 0; i < N; i++) { re[i] = (mono[off + i] || 0) * win[i]; im[i] = 0; }
    fft(re, im, false);
    for (let k = 0; k < BINS; k++) { mag[f * BINS + k] = Math.hypot(re[k], im[k]); phase[f * BINS + k] = Math.atan2(im[k], re[k]); }
  }
  return { mag, phase, F, BINS, N, Ha };
}

// Phase-vocoder resynthesis. Mirrors the PV AudioWorkletProcessor: synth() produces
// Hs samples per call into a ring buffer; render() pulls them out.
export class PsiSynth {
  constructor(N = 2048, Ha = 512, Hs = 512) {
    this.N = N; this.Ha = Ha; this.Hs = Hs; this.B = (N >> 1) + 1;
    this.win = hann(N);
    this.wola = 1.5;
    this.mag = null; this.phase = null; this.F = 0; this.ready = false;
    this.rp = 0; this.rate = 1; this.frozen = false; this.dir = 1; this.smear = 0;
    this.psi = new Float32Array(this.B);
    this.re = new Float32Array(N); this.im = new Float32Array(N);
    this.acc = new Float32Array(N);
    this.outQ = new Float32Array(N * 4); this.qr = 0; this.qw = 0; this.qc = 0;
  }

  setAnalysis(mag, phase, F) {
    this.mag = mag; this.phase = phase; this.F = F; this.rp = 0;
    for (let k = 0; k < this.B; k++) this.psi[k] = phase[k];
    this.ready = true;
  }
  setParams({ rate, frozen, dir, smear } = {}) {
    if (rate !== undefined) this.rate = rate;
    if (frozen !== undefined) this.frozen = frozen;
    if (dir !== undefined) this.dir = dir;
    if (smear !== undefined) this.smear = smear;
  }

  synth() {
    const N = this.N, half = N >> 1, B = this.B, F = this.F, re = this.re, im = this.im, mag = this.mag, phase = this.phase, psi = this.psi;
    let ia = Math.floor(this.rp); if (ia < 0) ia = 0; if (ia > F - 1) ia = F - 1;
    const ib = Math.min(F - 1, ia + 1), frac = this.rp - ia;
    const ex = TWO_PI * this.Ha / N, sm = this.smear | 0;
    for (let k = 0; k <= half; k++) {
      const ka = ia * B + k, kb = ib * B + k;
      let m;
      if (sm > 0) { let s = 0, c = 0; for (let ff = Math.max(0, ia - sm); ff <= Math.min(F - 1, ia + sm); ff++) { s += mag[ff * B + k]; c++; } m = s / c; }
      else m = mag[ka] + (mag[kb] - mag[ka]) * frac;
      const omega = ex * k + princ(phase[kb] - phase[ka] - ex * k);
      psi[k] += omega;
      re[k] = m * Math.cos(psi[k]); im[k] = m * Math.sin(psi[k]);
      if (k > 0 && k < half) { re[N - k] = re[k]; im[N - k] = -im[k]; }
    }
    im[0] = 0; im[half] = 0;
    fft(re, im, true);
    const acc = this.acc, win = this.win, Q = this.outQ, L = Q.length;
    for (let i = 0; i < N; i++) acc[i] += re[i] * win[i];
    for (let i = 0; i < this.Hs; i++) { Q[this.qw] = acc[i] / this.wola; this.qw = (this.qw + 1) % L; this.qc++; }
    acc.copyWithin(0, this.Hs); acc.fill(0, N - this.Hs);
    this.rp += this.dir * (this.frozen ? 0 : this.rate);
    if (this.rp > F - 1) { this.rp = 0; for (let k = 0; k < B; k++) psi[k] = phase[k]; }
    else if (this.rp < 0) { this.rp = F - 1; for (let k = 0; k < B; k++) psi[k] = phase[(F - 1) * B + k]; }
  }

  // Pull `numSamples` output samples, synthesizing frames on demand.
  render(numSamples) {
    const out = new Float32Array(numSamples), Q = this.outQ, L = Q.length;
    if (!this.ready) return out;
    for (let i = 0; i < numSamples; i++) {
      if (this.qc <= 0) this.synth();
      out[i] = Q[this.qr]; this.qr = (this.qr + 1) % L; this.qc--;
    }
    return out;
  }
}

// Deterministic test input shared by the JS oracle and the C++ test: an exponential
// sine sweep with tremolo (no RNG, so both languages produce identical input).
export function makeChirp(sr = 48000, dur = 2) {
  const len = Math.floor(sr * dur), d = new Float32Array(len);
  let p = 0;
  for (let i = 0; i < len; i++) {
    const t = i / sr, f = 200 * Math.pow(4, t / dur);
    p += (TWO_PI * f) / sr;
    const trem = 0.6 + 0.4 * Math.sin(TWO_PI * 5 * t);
    d[i] = Math.sin(p) * trem * 0.7;
  }
  return d;
}
