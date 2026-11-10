# AlarmAck recovery: review before proposing protocol behavior

Status: **unreviewed**. This is a question for a PROFINET/IEC specialist, not an
expert endorsement. The parser contribution can proceed independently.

## Observed implementation failure

In `pf_alarm_apmr_a_data_ind`, receipt of a fresh AlarmAck acknowledges APMS and
consumes the receive sequence before `pf_alarm_apmr_send_ack` returns. A zero or
negative raw-send return then suppresses the ALPMI indication. Successful
retransmissions of that same peer sequence only re-ACK: ALPMI remains in W_ACK
and cannot send the next AlarmNotification.

The recovery proposal removes that success guard only for a parsed AlarmAck.
It leaves peer retries and duplicate suppression intact. Its tests inject
full-length, zero and -1 returns **after a complete TACK is constructed**.

## Selected text and provisional interpretation

IEC 61158-6-10:2019, edition 4, as supplied in DIN EN IEC 61158-6-10:2020-08.
These references are paraphrases carried forward from the earlier analysis,
not a new line-by-line standards audit on November 9. No licensed text is
redistributed.

| Reference | Reading to review | Printed / PDF page |
| --- | --- | --- |
| Table 242 row 7 | Fresh DATA+TACK in OPEN: store, advance ExpectedSeq, request ACK, enter WCNF. | 268 / 286 |
| Table 242 row 8 | Previous-sequence duplicate in OPEN: re-ACK, no new upper indication. | 268 / 286 |
| Table 242 row 16 | A_Data_cnf in WCNF: retrieve stored data, indicate upward, return OPEN; no status guard shown. | 269 / 287 |
| Table 242 row 17 | Non-ERR data indication while WCNF is ignored. | 269 / 287 |
| Table 243 | A_Data_cnf maps the LMPM confirmation, including LMPM_status. | 269 / 287 |
| 4.17.2.3 / Table 409 row 5 | Request validation and local confirmation for an issued RTA request. | 389 / 407 |
| Table 886 rows 13, 15 | AlarmAck indication completes W_ACK; another notification is rejected while waiting. | 667 / 685 |
| Table 104 | Positive/negative confirmation guards are explicit elsewhere; this is supporting context, not proof of intent. | approximately 125 |

The conditional argument is: **if** the synchronous raw-send return corresponds
to A_Data_cnf for an issued request, **and if** row 16 really is unguarded, its
stored AlarmAck indication must not depend on that return being successful.

## Decisions needed

1. Is this selected-edition reading correct, including the absence of a status
   guard and the duplicate behavior? Do IEC 61158-6-10:2023 or the PI maintenance
   edition used by current p-net change it? The analysis has not compared them;
   the 2019 edition is a selected source, not a claim to use the newest text.
2. Does the synchronous PNAL return represent this asynchronous confirmation,
   or should p-net retain pending data and deliver on a different event?
3. Is reentry possible during the synchronous send? The proposed ordering relies
   on no intervening input before the local-confirmation handling.
4. What happens when allocation fails **before a request exists**? The small
   candidate currently delivers in this case too. That is a separate recovery
   policy, not established by row 16 and not covered by the post-construction
   fault harness. Should it instead preserve a pending indication, fail the AR,
   or use a dedicated negative-confirmation path?
5. Should the proposal remain separate from any broader APMS or notification
   send-failure changes? Its current scope is AlarmAck receive completion only.

Please record the reviewed edition, each accepted/rejected premise and the
chosen pre-request policy before calling this standards-conforming or opening
a ready-for-review behavioral PR.

## What Lean establishes, and what it does not

`Recovery.lean` proves fresh delivery, send-status independence, duplicate
suppression and conservation/at-most-once for arbitrary finite receive-only
words in the explicit model. It also preserves the old lost-completion path as
a checked counterexample. The 36 generated histories compare this model with
native startup/notification/ACK/retry exchanges.

LLM assistance was used to carry earlier standards analysis into this model,
the harness and the drafts. Lean checks the formal statements; it does not
certify the automatic interpretation, event mapping, pre-request policy or C
implementation. New application notifications are exercised natively but not
included in the receive-only trace theorem. No hardware, timer-liveness, full
PROFINET conformance or C-refinement proof is supplied.
