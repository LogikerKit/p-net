# Inputs, generated artifacts and trust boundaries

## Pinned source inputs

| Input | Public repository | Revision |
| --- | --- | --- |
| Tested core | rtlabs-com/p-net | `19782d52c56640526156f8db04efe93317f9c530` |
| Linux PNAL only | rtlabs-com/p-net, historical v0.2.0 | `3e650720850e1f31f8a419c8aa135a2e81b44ddb` |
| OSAL | rtlabs-com/osal | `88784fcfff7a1b74e20bdeb71a55c2cdd22b4a59` |
| GoogleTest | google/googletest | `25808659d317cb03409e7949914b274e10e6824f` |
| Lean checker | leanprover/lean4 | `v4.19.0` |

The public core snapshot lacks its CMake build system and ports. The overlay
rebuilds four historical Linux PNAL units against their own matching headers,
plus current public core units, OSAL and GoogleTest. No historical protocol/core
object and no previously built protocol binary is linked. This mixed-source
platform overlay is a regression aid, not an official supported p-net build.

The initial local dependency clones were clean pinned source copies from
earlier work. `fetch-inputs.mjs` supplies a reproducible public-only acquisition
path; the verification report distinguishes any fresh-fetch run.

## Origin

Prior parser/recovery analysis, selected-edition standards paraphrases and the
public core's startup fixture informed this reproducer. The earlier material
was evidence to examine, not a substitute for reviewing the model or a
contributor's authorization to publish.

This package rewrites a shared native fixture, expands the corpus to 288
parser / 36 recovery histories, and introduces a new linked Lean model,
serialization bridge and standalone source-rebuild driver. It is not merely a
rerun of a previously built experiment.

Specification interpretation, formalization, harness and prose received LLM
assistance. Claims must rest on reviewed assumptions and reproducible checks,
not authorship by an LLM. No LLM is executed by the tests.

## Standards source

Selected edition: IEC 61158-6-10:2019 edition 4, DIN EN IEC 61158-6-10:2020-08
English PDF previously supplied by the user.
PDF SHA-256:
`ca3adf581f55ba7d47692c154d41f0347785dfb469c54f316753506b94f8b31d`.

Only clause identifiers and paraphrases are supplied. No licensed PDF, rendered
page or extracted standard text is supplied. The November 9 preparation and
November 10 verification did not repeat the original page-by-page
interpretation audit or compare the newer 2023 edition.

## Private-copy adapters and fixture provenance

`verify.mjs` hashes all tracked core/dependency inputs and copies regular core
files into a private build directory. It permits only `pf_alarm.c` to differ
from the pinned core. It never modifies original source while checking.

In that copy it aligns the Ethernet-init mock's newer receive-type argument,
aligns callback declarations with existing definitions, and hooks the mock send
return to the new fixture observer. Each transformation records before/after
hashes in `support-adaptations.json`; adapted source remains in the build tree.
The upstream UDP-receive mock also leaves its source address/port outputs
uninitialized; the private-copy adapter supplies deterministic peer metadata
192.168.0.1:49153. This fixes test-support input nondeterminism, not protocol
state. The new fixture checks `pnet_init_only`'s return instead of inheriting the
upstream fixture's ignored return. Core source logic is not adapted.

Five exact existing startup arrays (`connect_req`, `write_req`, `prm_end_req`,
`appl_rdy_rsp`, `data_packet`) are extracted mechanically from pinned
`test/test_cmrpc.cpp`; their byte hashes are recorded. The new peer AlarmAck
encoder is independent of the production block encoder.

The new source/Lean/JS files use GPL-3.0-or-later notices consistent with the
p-net-derived harness. OSAL/GoogleTest remain separately fetched inputs under
their own repository licenses. This package is not a claim of employer release
permission or signed contributor terms.

## Model and bridge

Parser roundtrip covers every twelve-byte body and four-byte status in its
specified valid layout. Closed certificates check the 288-case policy and
36-case recovery contract. Recovery additionally proves receive-only finite
trace conservation and at-most-once, not a full Session or protocol model.

Lean's named axiom audit accepts only standard `propext`, `Quot.sound` and
`Classical.choice` (or none); rejects custom assumptions/proof escapes such as
`sorryAx`; and rejects an intentionally false status certificate. The wire
model/standard correspondence is still a human-review obligation.

`Emit.lean` emits both C++ vectors and JSON expectations. JS independently
serializes the header and checks byte equality; it also compares captured
native frame blocks, lengths, fault returns and observations. Twelve corruption
controls test missing/misrouted/duplicate records, wrong status/sequence,
extra bytes, lost/double completion and fault-boundary drift.

Compiler, evaluator/serialization, fixture, adapters and observation bridge
remain trusted/tested components, not a formal C-refinement theorem. The
recovery send-return/confirmation premise and pre-request policy are openly
unreviewed.
