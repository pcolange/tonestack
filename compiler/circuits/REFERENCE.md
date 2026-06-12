# TS9 Ground Truth

Stage-by-stage reference for the Ibanez TS9 Tube Screamer model: what the real pedal
contains, what `circuitc` models, and why the rest is omitted. Component values and topology
are traced from R.G. Keen's "TS-9, -10, -808 Signal Path" schematic
([geofex.com](http://www.geofex.com/article_folders/tstech/tsxtech.htm), mirrored at
[schematicheaven.net](https://schematicheaven.net/effects/ts9_ts10_ts808.pdf)) and
cross-checked against the
[ElectroSmash Tube Screamer analysis](https://www.electrosmash.com/tube-screamer-analysis).

The drive and tone transfer functions in `discretize.py` have been verified symbolically
(MNA + bilinear transform) against this topology to machine precision.

## Signal path

```
IN → input buffer → clipping stage → tone/recovery stage → level pot → output buffer → OUT
        (omitted)     (modeled)          (modeled)              (omitted)
```

## Clipping stage — modeled (drive biquad + asinh waveshaper)

Non-inverting op-amp (A1, half of an RC4558) with the clipping diodes in the feedback path.

| Component | Value | Role |
|---|---|---|
| R1 / C1 | 4.7k / 0.047 µF | inverting leg to Vref; gain shelf corner 720 Hz |
| Rf | 51k | series feedback resistor |
| Drive pot | **500k, A (audio) taper** | in series with Rf; Rdrive = 51k…551k |
| Cf | 51 pF | feedback cap; HF rolloff 1/(2π·Rdrive·51p) ≈ 5.7 kHz at max drive |
| Diodes | 2× 1S1588 (≈1N4148/1N914) | anti-parallel across feedback |
| Op-amp | RC4558 (some units TA75558) | GBW ≈ 3 MHz |

- LTI part: `H(s) = 1 + s·C1·Rdrive / ((1 + s·Rdrive·Cf)(1 + s·R1·C1))` — gain 1 below
  720 Hz rising to 1 + Rdrive/4.7k (≈118, 41 dB at max drive); this shelf is the TS mid-hump's
  low side.
- Diodes: split out as the asinh waveshaper with the standard 1N4148 SPICE constants
  **Is = 2.52 nA, n = 1.752 (n·Vt = 45.3 mV)**. The previous Is = 1e-14 / 26 mV (n = 1) gave
  roughly the right ~0.6 V knee voltage but an unrealistically hard knee.
- A-taper drive pot modeled as log skew with midpoint 0.1 (≈10% of track at half rotation,
  per R.G. Keen's commercial audio-taper data). If a measured taper curve for the factory
  pot turns up, replace the skew with it.

## Tone / recovery stage — modeled (tone biquad)

A1 out → 1k series → node t; 0.22 µF from t to ground (723 Hz LPF) and 10k bias to Vref.
Tone pot end H = node t = A2's + input; end J = A2's − input, 1k feedback from A2 out;
wiper → 0.22 µF + 220Ω series to ground (treble-boost leg, corner 3.3 kHz, max HF gain
1 + 1k/220 ≈ 5.5 toward the J end).

| Component | Value |
|---|---|
| series R | 1k |
| LPF cap | 0.22 µF (tant in the original) |
| bias R | 10k (to Vref) |
| Tone pot | **20k, G taper** (symmetric S-curve) |
| wiper leg | 0.22 µF + 220Ω |
| feedback R | 1k |

- The G (S-curve) taper passes 50% of track at half rotation; the engine's linear skew
  matches it at the midpoint and is the closest representable two-parameter approximation.
  A custom taper map would be needed to capture the S-curve ends.

## Omitted stages, with rationale

| Stage | Contents | Why omitted |
|---|---|---|
| Input buffer | 0.02 µF → 1k → Q1 (2SC1815) EF, 510k bias, 10k load; 1 µF NP → 10k bias into A1 | High-pass corners ≈ 18 Hz and 16 Hz — below the audio band; unity gain; input Z ≈ 446k only matters for pickup loading, not the wet path |
| Level pot | 1 µF NP → 1k → 100k B-taper pot → 0.1 µF | Pure output attenuation; corner ≈ 3 Hz. Belongs in the module as a plain gain when the TubeScreamerModule lands |
| Output buffer | Q2 EF, 510k bias, 10k load → 470Ω → 10 µF → 100k shunt | Unity gain; shapes output impedance only. **This is where TS9 ≠ TS808** (TS808: 100Ω/10k, RC4558 vs TA75558 in some TS9s) — the modeled stages are identical between the two |
| Op-amp GBW / slew | RC4558, GBW ≈ 3 MHz | At max drive the naive closed-loop corner is ≈ 25 kHz, but Cf already rolls the gain off above ≈ 5.7 kHz, easing GBW demand — a naive one-pole would overstate the effect. Revisit with measurements if HF accuracy at max drive matters |
| JFET bypass switching, power supply | — | No audio-path effect when engaged |

## Known approximations in the modeled stages

1. **Static nonlinearity decomposition**: the diodes interact with Cf and the feedback
   network inside the loop; the filter → memoryless-asinh split ignores that. Dominant
   error source at high drive. The fix (state-space/DK or WDF) is an engine-architecture
   decision, not a constants fix.
2. **Diode constants are typical, not measured**: 1S1588 ≈ 1N4148 is an electrical
   approximation; unit-to-unit Is spread changes the knee by tens of mV.
3. **Pot tapers are nominal curves**, not measured tracks (see above).
