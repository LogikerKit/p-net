/- SPDX-License-Identifier: GPL-3.0-or-later
PROVISIONAL reading of IEC 61158-6-10:2019 Table 242 rows 7/8/16.
Entry: parsed DATA+TACK; local confirmation has arrived after an issued request.
Map synchronous raw-send full/zero/negative returns to that confirmation.
No allocation-before-request failure, reentry, peer timer, hardware or C
refinement theorem is supplied. EXPERT_REVIEW.md lists the open premises. -/
import Common

namespace AlarmAck.Recovery

inductive SendResult where | full | zero | negative deriving DecidableEq, Repr

structure State where
  expected : Nat
  previous : Nat
  waiting : Bool
  deriving DecidableEq, Repr

structure Packet where
  sequence : Nat
  status : Status
  localConfirmation : SendResult
  deriving Repr

def next (n : Nat) : Nat := (n + 1) % 32768
def pending (s : State) : Nat := if s.waiting then 1 else 0
def delivered (o : Option Status) : Nat := if o.isSome then 1 else 0

/-- This deliberately models the local-confirmation boundary, not successful
    physical transmission. The status guard's absence is a model premise. -/
def receive (s : State) (p : Packet) : State × Option Status :=
  if p.sequence = s.expected then
    ({expected := next s.expected, previous := s.expected, waiting := false},
     if s.waiting then some p.status else none)
  else (s, none)

theorem fresh_confirms (s : State) (p : Packet)
    (fresh : p.sequence = s.expected) (waiting : s.waiting = true) :
    (receive s p).2 = some p.status := by simp [receive, fresh, waiting]

theorem confirmation_status_independent (s : State) (p : Packet) (outcome : SendResult) :
    receive s {p with localConfirmation := outcome} = receive s p := by rfl

theorem duplicate_never_redelivers (s : State) (p : Packet)
    (duplicate : p.sequence = s.previous) (distinct : s.previous ≠ s.expected) :
    (receive s p).2 = none := by simp [receive, duplicate, distinct]

theorem step_conserves_pending (s : State) (p : Packet) :
    pending (receive s p).1 + delivered (receive s p).2 = pending s := by
  by_cases eq : p.sequence = s.expected
  · cases hw : s.waiting <;> simp [receive, eq, pending, delivered, hw]
  · simp [receive, eq, delivered]

def execute : State → List Packet → State × Nat
  | s, [] => (s, 0)
  | s, p :: ps =>
    let (t, o) := receive s p
    let (u, count) := execute t ps
    (u, delivered o + count)

/-- Any finite receive-only word conserves the one outstanding completion.
    A new application notification is an external operation, not proved here. -/
theorem arbitrary_trace_conserves_pending (s : State) (ps : List Packet) :
    pending (execute s ps).1 + (execute s ps).2 = pending s := by
  induction ps generalizing s with
  | nil => simp [execute]
  | cons p ps ih =>
    have one := step_conserves_pending s p
    cases hr : receive s p with
    | mk t o =>
      rw [hr] at one
      simp only [Prod.fst, Prod.snd] at one
      have rest := ih t
      cases he : execute t ps with
      | mk u n =>
        rw [he] at rest
        simp only [Prod.fst, Prod.snd] at rest
        simp only [execute, hr, he]
        omega

theorem arbitrary_trace_at_most_one (s : State) (ps : List Packet) :
    (execute s ps).2 ≤ 1 := by
  have conservation := arbitrary_trace_conserves_pending s ps
  have bound : pending s ≤ 1 := by
    unfold pending
    split <;> omega
  omega

theorem c_mask_matches_next (n : Nat) : ((n + 1) &&& 32767) = next n := by
  simpa only [next] using Nat.and_two_pow_sub_one_eq_mod (n + 1) 15

def initial : State := {expected := 65535, previous := 65534, waiting := true}
def packet (status : Status) (result : SendResult) : Packet :=
  {sequence := 65535, status, localConfirmation := result}

structure Case where
  status : Status
  separate : Bool
  result : SendResult
  deriving Repr

def cases : List Case := statuses.flatMap fun status =>
  [false, true].flatMap fun separate =>
    [.full, .zero, .negative].map fun result => {status, separate, result}

def first (c : Case) := receive initial (packet c.status c.result)
def afterRetries (c : Case) :=
  execute initial [packet c.status c.result, packet c.status .full, packet c.status .full]

def correct (c : Case) : Bool := decide (
  (first c).1.expected = 0 ∧ (first c).2 = some c.status ∧
  (afterRetries c).2 = 1 ∧ (afterRetries c).1.waiting = false)

theorem all_cases_certified : cases.all correct = true := by decide
theorem corpus_size : cases.length = 36 := by decide

/-- Preserve the old behavior as a counterexample, rather than claiming that
    conservation alone proves progress. The consumed sequence hides the ACK
    from later retries when the original completion is suppressed. -/
def baseline (s : State) (p : Packet) : State × Option Status :=
  if p.sequence = s.expected ∧ p.localConfirmation ≠ .full then
    ({s with expected := next s.expected, previous := s.expected}, none)
  else receive s p

theorem baseline_failure_then_retry_loses_completion :
    let p := packet ⟨0, 0, 0, 0⟩ .negative
    let first := baseline initial p
    let retry := baseline first.1 {p with localConfirmation := .full}
    first.2 = none ∧ retry.2 = none ∧ retry.1.waiting = true := by decide

#print axioms fresh_confirms
#print axioms confirmation_status_independent
#print axioms duplicate_never_redelivers
#print axioms step_conserves_pending
#print axioms arbitrary_trace_conserves_pending
#print axioms arbitrary_trace_at_most_one
#print axioms c_mask_matches_next
#print axioms all_cases_certified
#print axioms corpus_size
#print axioms baseline_failure_then_retry_loses_completion

end AlarmAck.Recovery
