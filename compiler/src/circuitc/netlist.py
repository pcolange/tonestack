"""Tube Screamer (TS9) component values.

A typed component set the discretizer consumes directly. Values follow the factory TS9
schematic (R.G. Keen's TS-9/-10/-808 signal path; see ``circuits/REFERENCE.md`` for the
stage-by-stage ground truth and sources). The symbolic Lcapy MNA front-end (which will parse
``circuits/ts9.net`` into this same set) lands later.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class DriveSection:
    """Op-amp non-inverting drive filter (gain/clipping stage feedback network)."""

    rpot: float = 500e3  # drive pot, full value (factory: 500KA, audio taper)
    rf: float = 51e3  # series feedback resistor
    cf: float = 51e-12  # feedback cap
    r1: float = 4700.0
    c1: float = 0.047e-6


@dataclass(frozen=True)
class ToneSection:
    """Tone control + recovery stage."""

    rf: float = 1e3
    rpot: float = 20e3  # tone pot, full value (factory: 20KG)
    r10k: float = 10e3
    r1k: float = 1e3
    r220: float = 220.0
    c4: float = 0.22e-6
    ctone: float = 0.22e-6


@dataclass(frozen=True)
class Clipper:
    """Asinh diode/op-amp soft-clipping saturation stage.

    Stock TS9 diodes are 1S1588 silicon small-signal (electrically ~1N4148/1N914); constants
    are the standard 1N4148 SPICE model: Is = 2.52 nA, n = 1.752, so n*Vt ~= 45.3 mV at 300 K.
    """

    saturation_current: float = 2.52e-9  # Is
    thermal_voltage: float = 45.3e-3  # n * Vt


@dataclass(frozen=True)
class TS9:
    drive: DriveSection = DriveSection()
    tone: ToneSection = ToneSection()
    clipper: Clipper = Clipper()


@dataclass(frozen=True)
class GuitarSource:
    """Electric-guitar source model feeding the Rangemaster's low input impedance.

    The ``pickup`` axis morphs linearly between a bright single-coil (low L/R, 250k volume
    pot) and a dark humbucker (high L/R, 500k pot); presets are knob settings, not schema.
    Typical published values — refine with measurements.
    """

    single_coil_l: float = 2.2  # H
    single_coil_r: float = 5.8e3
    single_coil_vpot: float = 250e3
    humbucker_l: float = 4.4  # H
    humbucker_r: float = 8.5e3
    humbucker_vpot: float = 500e3
    cable_min: float = 200e-12  # F, short low-capacitance cable
    cable_max: float = 1.0e-9  # F, long vintage coily cable

    def at(self, pickup: float) -> tuple[float, float, float]:
        """(L, R, Rvol) at morph position ``pickup`` in [0, 1]."""
        p = min(max(pickup, 0.0), 1.0)

        def lerp(a: float, b: float) -> float:
            return a + (b - a) * p

        return (
            lerp(self.single_coil_l, self.humbucker_l),
            lerp(self.single_coil_r, self.humbucker_r),
            lerp(self.single_coil_vpot, self.humbucker_vpot),
        )


@dataclass(frozen=True)
class GeTransistor:
    """OC44 germanium PNP (signal magnitudes; polarity handled as a final inversion).

    hFE and Icbo are typical for a low-leakage selected OC44 (ElectroSmash guidance:
    gains 75-100, leakage selected low). Is is *derived* by the bias solver so the
    quiescent point lands on the documented factory ideal (|Vc| = 7.0 V) — the historical
    biasing procedure selected transistors to hit that point, so the operating point, not
    a datasheet Is, is the ground truth. Cob is a typical value for the Miller input
    capacitance approximation.
    """

    hfe: float = 90.0
    nvt: float = 26e-3  # n ~ 1 for germanium
    icbo: float = 0.5e-6  # A, low-leakage selected
    cob: float = 10e-12  # F, collector-base capacitance (typical)
    vce_sat: float = 0.2  # V


@dataclass(frozen=True)
class RangemasterCircuit:
    """Dallas Rangemaster treble booster (see circuits/rangemaster-reference.md)."""

    supply: float = 9.0
    c_in: float = 4.7e-9  # C1, the treble-defining input cap
    r_bias_top: float = 470e3  # base divider from supply
    r_bias_bot: float = 68e3  # base divider to ground
    r_emitter: float = 3.9e3
    c_emitter: float = 47e-6  # emitter bypass
    r_load: float = 10e3  # boost pot full track = DC collector load
    c_out: float = 10e-9  # C4, wiper to output
    r_next: float = 1e6  # downstream amplifier input
    vc_target: float = 7.0  # documented factory-ideal |Vc|
    transistor: GeTransistor = GeTransistor()
    source: GuitarSource = GuitarSource()
