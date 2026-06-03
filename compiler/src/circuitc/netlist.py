"""Tube Screamer (TS808/TS9) component values.

A typed component set the discretizer consumes directly. The symbolic Lcapy MNA front-end
(which will parse ``circuits/ts9.net`` into this same set) lands later.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class DriveSection:
    """Op-amp non-inverting drive filter (gain/clipping stage feedback network)."""

    rpot: float = 550e3  # drive pot, full value
    rf: float = 51e3  # series feedback resistor
    cf: float = 51e-12  # feedback cap
    r1: float = 4700.0
    c1: float = 0.047e-6


@dataclass(frozen=True)
class ToneSection:
    """Tone control + recovery stage."""

    rf: float = 1e3
    rpot: float = 20e3  # tone pot, full value
    r10k: float = 10e3
    r1k: float = 1e3
    r220: float = 220.0
    c4: float = 0.22e-6
    ctone: float = 0.22e-6


@dataclass(frozen=True)
class Clipper:
    """Asinh diode/op-amp soft-clipping saturation stage."""

    saturation_current: float = 1e-14  # Is
    thermal_voltage: float = 26e-3  # nvt


@dataclass(frozen=True)
class TS9:
    drive: DriveSection = DriveSection()
    tone: ToneSection = ToneSection()
    clipper: Clipper = Clipper()
