# p-net AlarmAck: optional model-linked regression companion

Sources prepared November 9, 2026; supported debug checks rerun November 10.
This standalone reproducer is separate from the proposed upstream source fixes.

| Package | Contents | Readiness |
| --- | --- | --- |
| Parser | `parser/0001-alarmack-parser.patch`, generated cases and native harness | Debug regression passes; selected-edition acceptance policy remains reviewable. |
| Recovery | Incremental `recovery/0002-alarmack-recovery.patch`, model and fault harness | Unreviewed proposal; confirmation mapping and pre-request-failure policy unresolved. |

`VERIFICATION.md` summarizes dated results. `EXPERT_REVIEW.md` exposes the recovery
interpretation and implementation mapping. `PROVENANCE.md` records origins,
pins, adapters and trust boundaries. Private machine logs and licensed standards
documents are not included.

## Recommended upstream footprint

Offer the small source fix and the relevant native regression first. Keep this
standalone Linux overlay and Lean/Node companion external unless maintainers
want them. Do not propose a workflow or new project-wide tooling requirement.
The generated headers let native CTest run without Lean, Node or an LLM.

The patches modify `src/common/pf_alarm.c` only. Harness files are supplied
separately because the public snapshot has no build system/ports and its test
support requires small compatibility adapters. A maintainer can integrate the
new tests into their complete tree rather than adopting this overlay.

## Prerequisites

Linux, Git, CMake >= 3.16, Ninja, C/C++ compilers, Node.js >= 18, `prlimit`
(util-linux), and Lean/Lake 4.19.0 for the combined check. The existing locally
available 4.19.0 checker is pinned for reproducibility, not claimed to be the
newest Lean release. Install/use that toolchain through your normal process.

The network step below reads public source only; it requires no GitHub account.
It downloads no executable artifacts, creates no fork and posts nothing.
Run commands from this package's root. Directory names must be new for fetch
and evidence; build directories may be reused.

## Parser reproduction and Lean-linked check

```sh
node companion/fetch-inputs.mjs inputs
git -C inputs/source apply --check ../../parser/0001-alarmack-parser.patch
git -C inputs/source apply ../../parser/0001-alarmack-parser.patch
cd companion
lake test -- --scope parser \
  --source ../inputs/source --dependencies ../inputs/dependencies \
  --build ../build-parser --evidence ../evidence/reproduced-parser
```

The driver rebuilds pinned source, checks the named Lean theorems and their
axioms, rejects a false certificate, regenerates case headers, audits twelve
bridge corruption controls, runs all histories and compares exact injected
blocks/status/callback/sequence observations. It fails on drift or disagreement.
No model expectation is updated merely to make changed C pass.

For a baseline counterexample, use a fresh unpatched source checkout and add
`--expect-counterexample --scope parser`. The command succeeds only if all 288
histories produce records and a semantic counterexample is witnessed, not if
the executable crashes or skips cases.

## Recovery reproduction

First run `--scope recovery --expect-counterexample` against parser-only source.
This isolates the send-failure problem from the parser defect.

Then, from this package's root:

```sh
git -C inputs/source apply --check ../../recovery/0002-alarmack-recovery.patch
git -C inputs/source apply ../../recovery/0002-alarmack-recovery.patch
cd companion
lake test -- --scope both \
  --source ../inputs/source --dependencies ../inputs/dependencies \
  --build ../build-combined --evidence ../evidence/reproduced-combined
```

All 288 parser and 36 recovery histories should agree with the explicit model.
Passing does not resolve the standards interpretation or allocation policy:
read `EXPERT_REVIEW.md` before treating this candidate as deployable.

## Plain native rerun

After the combined check has created its snapshot/configured build:

```sh
ctest --test-dir build-combined/native --output-on-failure
```

Use `-R '^parser$'` for a parser-only build: its recovery test intentionally
fails. For an independently configured overlay, CMake accepts `SOURCE`, `PNAL`,
`OSAL`, `GTEST` and `GENERATED` absolute paths; source test support must first be
adapted as recorded in `support-adaptations.json`. The runner does that in a
private copy, never in the original checkout. Frozen vectors are in `generated/`.

If behavior legitimately changes, review the selected specification and model,
regenerate the headers, run the negative controls and native comparison, and
review both changes. A green Lean check alone is insufficient.

## Limits and publication checks

The chosen cases are not every test derivable from the standard. They use
high-priority AlarmAck, one unpadded block and six opaque status patterns.
Recovery faults are post-construction sends, not allocation faults. Hardware,
other ports, low-priority paths and a whole-project suite are not covered.
Lean proves this model, not automatic interpretation or full C refinement.
An extra UBSan profile still fails during startup before AlarmAck; only the
documented debug configuration has passed. See `VERIFICATION.md`.

Recovery is supplied for discussion, not deployment or a ready-for-review
behavioral PR. Human review must resolve its event mapping and allocation
policy. No contributor-agreement signing or employer authorization is claimed.
