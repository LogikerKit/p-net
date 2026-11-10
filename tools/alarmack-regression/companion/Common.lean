/- SPDX-License-Identifier: GPL-3.0-or-later
LLM-assisted formalization of selected IEC 61158-6-10:2019 clauses.
Kernel checking establishes the mathematical claims, not the interpretation. -/
import Lean

namespace AlarmAck

structure Status where
  code : UInt8
  decode : UInt8
  code1 : UInt8
  code2 : UInt8
  deriving DecidableEq, Repr

def Status.bytes (s : Status) : List UInt8 := [s.code, s.decode, s.code1, s.code2]

def statuses : List Status := [
  ⟨0, 0, 0, 0⟩, ⟨0xda, 0x81, 0x3c, 0⟩, ⟨1, 2, 3, 4⟩,
  ⟨255, 255, 255, 255⟩, ⟨0, 0, 0, 1⟩, ⟨128, 0, 128, 0⟩]

/-- Test coverage includes opaque byte patterns; their PNIO meanings are not
    asserted to be legal error classes by this companion. -/
def statusJson (s : Status) : Lean.Json := Lean.toJson (s.bytes.map UInt8.toNat)

end AlarmAck
