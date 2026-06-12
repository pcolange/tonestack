# Rangemaster Ground Truth

Stage-by-stage reference for the Dallas Rangemaster treble booster model: what the real
pedal contains, what `circuitc` models, and the approximations made. Sources:
[Fuzz Dog Range Master schematic/BOM](https://pedalparts.co.uk/docs/RangeMaster-v2.pdf)
(NPN-converted layout of the original), [DIY Fever — Dallas
Rangemaster](https://diy-fever.com/effects/dallas-rangemaster/) (original PNP wiring,
~24 dB boost figure, R.G. Keen's −7 V bias guidance), and the [ElectroSmash Dallas
Rangemaster analysis](https://www.electrosmash.com/dallas-rangemaster) (input-impedance
framing, transistor selection: hFE 75–100, low leakage).

## Circuit

One germanium PNP (OC44) common-emitter stage, positive-ground −9 V supply:

| Component | Value | Role |
|---|---|---|
| C1 | 4.7 nF | input cap — against the stage's low input impedance (~12k) this *is* the treble booster |
| R3 / R2 | 470k / 68k | base bias divider (Thevenin 1.14 V / 59.4k) |
| Re ∥ Ce | 3.9k ∥ 47 µF | emitter bias + bypass (shelf corner ≈ 30 Hz) |
| Boost pot | **10k, A taper** | full track is the DC collector load; the wiper taps the output |
| C4 → load | 10 nF → ~1M | output coupling into the next amplifier |
| Q1 | OC44 | hFE ≈ 90 typical of selected units; leakage selected low (~0.5 µA) |

## Model decomposition

```
guitar source grid (3 axes, 4th-order IIR) -> oversampled germanium shape -> boost tap
            input table                        ge_common_emitter           output table
```

- **Source/input network (modeled, 4th-order direct-form grid table).** Axes: guitar
  **volume** (knob proportion; A-taper folded into the compiler, rows cosine-clustered
  toward the knob ends where the response moves ~20 dB across a few percent of rotation),
  **cable** capacitance (200 pF–1 nF), **pickup** morph (0 = bright single-coil
  2.2 H/5.8k/250k pot, 1 = dark humbucker 4.4 H/8.5k/500k). The network is solved per grid
  point by polynomial MNA: pickup R+L, volume pot split, cable C, C1 against
  `Zin(s) = rπ + (hfe+1)(Re ∥ 1/sCe)`. The Miller input capacitance `Cob(1 + Av)` is
  lumped onto the cable node through C1 rather than kept as a fifth state — a fifth state
  adds a parasitic pole whose root structure reorganizes (real pairs coalescing to complex
  and back) across the grid. That reorganization is also why the table stores **unfactored
  direct-form coefficients** (engine `IirNode`) instead of biquad sections: factored forms
  cannot be interpolated continuously through root breakaways, while the unfactored
  coefficients are polynomial in the component values and blend smoothly (verified: worst
  inter-row blend error ≤ 1.5 dB at 44.1 kHz including bilinear warp, all blends stable).
  Output variable is the junction voltage v_π — exactly the waveshaper's input. The famous
  behaviors fall out: ~22–28 dB net treble boost peaking near 1–1.6 kHz, level + corner
  cleanup as volume drops, darkening with longer cable / hotter pickup. Volume zero is an
  exact-silence row so the floor fades cleanly under interpolation.
- **Germanium stage (modeled, static waveshaper, ×4 oversampled).**
  `y = −Ic_q·RL·(e^(x/nVt) − 1)`, hard-clamped at collector saturation (−6.8 V, reached at
  x ≈ +44 mV) with the exponential's own soft cutoff ceiling (+1.55 V) on the other side —
  the germanium asymmetry. Slope at origin = −gm·RL ≈ −59 (35.5 dB), so the linear regime
  needs no separate gain stage.
- **Boost tap (modeled, 1-axis table).** Wiper divider with `C4` into ~1M:
  `H(s) = b·sC4·Rn / (1 + sC4(Rn + b(1−b)RL))`.

## Operating point

The factory procedure selected transistors until the collector sat at the documented ideal
|Vc| = 7.0 V, so the model pins that point and derives everything else (`bias.py`):
Ic = 200 µA total, of which (hfe+1)·Icbo ≈ 45 µA is leakage; Ic_exp = 155 µA; gm ≈ 5.9 mS;
rπ ≈ 15.1k; mid-band stage gain 35.5 dB; mid-band input impedance ≈ 12k (ElectroSmash
quotes the same order). The implied Vbe/Is are bookkeeping artifacts of the pin — the
audio-relevant quantities don't depend on them.

## Approximations, ranked

1. **Static decomposition (Hammerstein split):** the exponential is separated from the
   reactive elements; the real circuit's nonlinearity interacts with Ce and the Miller
   capacitance. Dominant error when slammed.
2. **Fixed-temperature bias.** Germanium leakage doubles every ~10 °C and audibly moves
   the bias point on real units; the model is pinned at the factory-ideal point.
   Temperature could later become one more table axis if wanted.
3. **Miller capacitance at fixed mid-band gain** (Cob = 10 pF typical, not measured).
4. **Bilinear warping at the base rate:** input-table response error ≤ 0.32 dB to 4 kHz at
   44.1 kHz, rising to ~2.7 dB at 10 kHz (where the response is already ~25 dB down);
   negligible at 88.2k/96k.
5. **Typical, not measured constants:** pickup/cable values, OC44 hFE/Icbo/Cob, pot tapers
   (A-taper ≈ log skew, 10% at center).
