"""circuitc: ToneStack's offline circuit compiler.

netlist -> discretized coefficient-table IR -> generated C++ header. All symbolic/algebraic
work is offline; the real-time engine only interpolates the emitted tables.
"""

from __future__ import annotations

from .ir import IR_VERSION

__all__ = ["IR_VERSION"]
