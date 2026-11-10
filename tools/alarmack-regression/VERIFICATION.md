# Local verification: November 10, 2026

## Publication rerun

The supported debug configuration was rerun November 10 against the same
pinned core and dependency sources. Artifact hashes match the source and
generated vectors supplied here.

| Source / scope | Complete histories | Disagreeing histories | Result | Wall time |
| --- | ---: | ---: | --- | ---: |
| Unchanged public / parser | 288 | 288 | Expected counterexample | 11.27 s |
| Parser-only / parser | 288 | 0 | PASS | 13.63 s |
| Parser-only / recovery | 36 | 24 | Expected counterexample | 10.78 s |
| Parser + recovery / both | 288 + 36 | 0 | PASS | 13.65 s |

These times include proof and bridge checks with cached dependencies. They
are not cold-download or CI benchmarks. The two pairs ran concurrently.
`verification-results-20261110.json` copies results, phase exit codes and
artifact hashes from the driver receipts, omitting machine paths and command
arguments. The private raw logs are not part of this public source companion.

The November 9 sanitizer failure below remains unresolved and was not rerun
November 10. Fresh debug success does not imply sanitizer-clean validation.

## November 9 results

Pinned public core: `19782d52c56640526156f8db04efe93317f9c530`.
The final harness checks initialization and supplies deterministic peer metadata
in the private-copy UDP mock. Each run completes all expected histories; a
crash or incomplete corpus is rejected, even in counterexample mode.

| Source / native scope | Histories | Model-disagreeing histories | Result | Local-only receipt directory |
| --- | ---: | ---: | --- | --- |
| Unchanged public / parser | 288 | 288 | Expected counterexample | `evidence/parser-baseline-final` |
| Parser-only / parser | 288 | 0 | PASS | `evidence/parser-final-deterministic` |
| Parser-only / recovery | 36 | 24 | Expected counterexample | `evidence/recovery-parser-only-final` |
| Parser + recovery / parser | 288 | 0 | PASS | `evidence/combined-final-deterministic` |
| Parser + recovery / recovery | 36 | 0 | PASS | `evidence/combined-final-deterministic` |

The 24 recovery failures are exactly the zero/-1-send cases. All twelve
full-send cases complete with parser-only code. The combined candidate
completes all 36, preserves the received status, re-ACKs both duplicates
without a second callback, and sends the next notification.

The final combined run uses all four inputs freshly fetched from their public
repositories by `fetch-inputs.mjs`; it does not reuse a previous protocol binary.
Both exported incremental patches were checked and applied in order to that
fresh source. Resulting `pf_alarm.c` bytes match the combined candidate, as
recorded in `patch-provenance.json` and the run's source hashes.

Both local candidate trees pass `git diff --check`. Plain CTest on the final
combined build passes its two targets (288 + 36 internal GoogleTest cases).
No hosted CI, whole-project suite, hardware or non-Linux port was run.

## Concrete counterexamples

Parser case 48 injects status `da 81 3c 00`:

```text
unchanged source callback: 00 02 00 00
parser-fixed callback:    da 81 3c 00
```

The old bytes are AlarmType 2 plus leading API bytes, not the status at block
offset 18. Exact frame bytes are retained in `parser-observations.json`.

Recovery case 1 injects a zero-return local TACK after parsing a fresh ACK:

```text
parser-only: receive counter 0, callbacks 0, next notification not sent
duplicates:  two complete 32-byte TACK attempts succeed, callbacks still 0
combined:    receive counter 0, callbacks exactly 1, next notification sent
```

The -1 cases exhibit the same lost-completion mechanism. Faults select the
complete matching TACK frame, not startup traffic or the AlarmNotification.
They do not exercise allocation failure.

## Lean and bridge checks

Lean 4.19.0 kernel checks **16 named theorems/corpus certificates**: six parser
and ten recovery statements. These include arbitrary-body/status wire
roundtrip, shape rejection, 288-/36-case certification, fresh completion,
duplicate suppression, send-status independence, finite receive-only
conservation/at-most-once and a checked old-behavior counterexample.

The axiom audit allows only the checker library's standard `propext`,
`Quot.sound`, `Classical.choice`, or none; no `sorryAx`, custom axioms,
admissions or native-decide proof escapes are accepted. An intentionally false
status certificate is rejected. Twelve JS corruption controls pass.

Generated header text is checked against an independent serialization of
model JSON, and every captured native parser block/advertised length is
checked against the model input. Native status/sequence/callback/retry results
are checked separately. Input hashes, commands, timing, logs, JUnit XML,
observations and semantic differences are retained per receipt.

This is a model-to-native regression bridge, not a proof of its compiler,
serialization or C implementation. The standard-to-model translation and
recovery event mapping remain review obligations.

## Time and environment

Final parser combined-check command: **18.46 s**, including proof regeneration,
input hashing, current-core rebuild and 288 native histories.
Final combined command: **19.19 s**, including approximately **6.44 s** checking
the three Lean modules, **8.01 s** rebuild, **0.70 s** parser native and
**0.09 s** recovery native. These use already-fetched inputs and dependency
build caches; they are not cold-download or CI benchmarks.

A subsequent plain CTest rerun took approximately **0.58 s** total. Timings vary
with concurrent jobs. Local environment: Linux, GCC 13.3.0, Node.js 24.22.1,
Lean/Lake 4.19.0. Driver subprocesses have 4 GiB virtual-address, 45/46-second
CPU and normally 60-second wall limits; address-space limit is not peak RSS.

## Failed probes and limitations

The optional `-O2 -fsanitize=undefined -fno-sanitize-recover=all` profile
**does not pass**. It completes the executable's 288 test attempts but startup
never reaches the expected DATA exchange; zero AlarmAck observation rows are
produced, so the driver rejects it as incomplete, not a semantic counterexample.
The private `evidence/combined-final-ubsan` receipt retains the final probe
(61.62 s total); it is not included in this source companion.
An earlier O0/UBSan probe also failed at startup.

Debugging exposed uninitialized peer outputs in the upstream UDP mock; these
were corrected in the private-copy adapter and the final debug controls were
rerun. That correction did **not** resolve the sanitizer startup failure.
Its remaining cause has not been isolated. Do not claim sanitizer-clean,
optimized-build validation or universal harness portability.

Earlier receipts are development iterations, not additional coverage: the
initial 1 GiB virtual-address limit terminated Common's check, corrected to
4 GiB; the Lake test driver needed its explicit `test_driver` annotation.
Only the final deterministic runs above describe the current supported debug
configuration. Historical October/November 2 experiments are not fresh results.

## Recovery gate

Native success and Lean proofs do not approve the selected IEC interpretation.
In particular, post-construction zero/-1 send faults do not establish how to
handle allocation failure before any ACK request exists. The candidate treats
that as completion too, but this remains a separate unreviewed policy.
Read `EXPERT_REVIEW.md` before pursuing the recovery PR.
