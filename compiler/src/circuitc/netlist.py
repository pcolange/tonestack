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
