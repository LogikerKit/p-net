/- SPDX-License-Identifier: GPL-3.0-or-later
IEC 61158-6-10:2019 5.1.2/Table 411: AlarmAck layout. High-priority block
0x8001, version 1.0; BlockLength counts after the first four header octets.
The strict no-trailing-data policy is an explicit interpretation choice.
This is not a proof of the C reader or arbitrary malformed Ethernet input. -/
import Common

set_option maxRecDepth 4096

namespace AlarmAck.Parser

abbrev Body := Fin 12 → UInt8

def header : List UInt8 := [0x80, 1, 0, 18, 1, 0]

def encode (b : Body) (s : Status) : List UInt8 :=
  header ++ [
    b 0, b 1, b 2, b 3, b 4, b 5, b 6, b 7, b 8, b 9, b 10, b 11] ++ s.bytes

def valid (block : List UInt8) (advertised : Nat) : Prop :=
  block.length = 22 ∧ advertised = 22 ∧ block.take 6 = header

instance (block : List UInt8) (n : Nat) : Decidable (valid block n) :=
  inferInstanceAs (Decidable (block.length = 22 ∧ n = 22 ∧ block.take 6 = header))

def parse (block : List UInt8) (advertised : Nat) : Option Status :=
  if valid block advertised then
    some ⟨block[18]!, block[19]!, block[20]!, block[21]!⟩
  else none

/-- Every possible twelve-byte body and four status octets are covered.
    In particular the status is not taken from the body's leading bytes. -/
theorem roundtrip (body : Body) (status : Status) :
    parse (encode body status) 22 = some status := by
  rfl

theorem encoded_size (body : Body) (status : Status) :
    (encode body status).length = 22 := by rfl

theorem rejects_bad_shape (block : List UInt8) (n : Nat) (h : ¬valid block n) :
    parse block n = none := by simp [parse, h]

theorem accepted_requires_shape (block : List UInt8) (n : Nat) (s : Status)
    (h : parse block n = some s) : valid block n := by
  by_cases hv : valid block n
  · exact hv
  · simp [parse, hv] at h

/-- Concrete full-startup fixture: process alarm type 2, API 0, slot/subslot 1,
    specifier 0. The roundtrip theorem above is not restricted to this body. -/
def fixtureBody : Body := fun i => ([0, 2, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0] : List UInt8)[i.val]!

structure Case where
  status : Status
  separate : Bool
  mutation : Nat
  deriving Repr

/-- IDs 1..16 retain the entire six-byte header and truncate body/status.
    Shorter headers, wrong peers, UDP, low priority and padding are outside
    this selected corpus. IDs 22/23 vary RTA length independently of bytes. -/
def mutate (bytes : List UInt8) (id : Nat) : List UInt8 × Nat :=
  if 1 ≤ id ∧ id ≤ 16 then (bytes.take (bytes.length - id), 22)
  else if id = 17 then (bytes.set 3 17, 22)
  else if id = 18 then (bytes.set 3 19, 22)
  else if id = 19 then (bytes.set 4 0, 22)
  else if id = 20 then (bytes.set 5 1, 22)
  else if id = 21 then (bytes ++ [0xaa], 23)
  else if id = 22 then (bytes, 21)
  else if id = 23 then (bytes, 23)
  else (bytes, 22)

def input (c : Case) : List UInt8 × Nat := mutate (encode fixtureBody c.status) c.mutation

def accepted (c : Case) : Bool := c.mutation == 0

def cases : List Case := statuses.flatMap fun status =>
  [false, true].flatMap fun separate => (List.range 24).map fun mutation =>
    {status, separate, mutation}

/-- Independently specified literal acceptance policy versus executable
    decoder, for every case emitted to the native regression harness. -/
def correct (c : Case) : Bool :=
  decide (parse (input c).1 (input c).2 =
    if accepted c then some c.status else none)

theorem all_cases_certified : cases.all correct = true := by decide
theorem corpus_size : cases.length = 288 := by decide

#print axioms roundtrip
#print axioms encoded_size
#print axioms rejects_bad_shape
#print axioms accepted_requires_shape
#print axioms all_cases_certified
#print axioms corpus_size

end AlarmAck.Parser
