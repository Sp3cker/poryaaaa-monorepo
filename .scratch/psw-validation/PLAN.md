# Programmable-Wave Validation Harness Plan

Updated: 2026-08-11
Status: planned
Scope: `packages/poryaaaa` programmable-wave voices (`VOICE_PROGRAMMABLE_WAVE` and `VOICE_PROGRAMMABLE_WAVE_ALT`)
Reference: pinned mGBA `afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9` running the compiled ROM MP2K driver

## 1. Goal

Build an opt-in differential harness that identifies the first programmable-wave (PSW) discrepancy at one of three independent seams:

1. **Driver transaction semantics:** Does poryaaaa emit the same ordered PSW bus transactions as the ROM's `CgbSound` for the same voice and control scenario?
2. **Hardware interpretation:** Given either transaction stream, do poryaaaa and pinned mGBA produce the same native post-bias stereo PCM16 at the same sample cycles?
3. **Fixture identity:** Did both paths use the same voice type and 16 waveform bytes?

The harness must fail on the current known write-order discrepancy and report the first differing transaction. It must not hide a mismatch with waveform alignment, gain fitting, normalization, or cycle substitution.

## 2. Current evidence

The existing hardware matrix proves that poryaaaa can replay the pinned ROM's `wave.trace` exactly. That does not validate poryaaaa's PSW driver emissions.

The pinned ROM emits this start sequence in `.scratch/arch-fix/evidence/matrix/wave.trace`:

```text
NR30 = 0x40
WAVE_RAM0..3 = four 32-bit writes
NR30 = 0x00
NR31
NR33
NR34 = trigger
NR51
NR32
NR30 = 0x80
NR34 = trigger
```

Current `plugin/m4a/m4a_cgb.c` emits a different sequence. `test_v2_wave_ram_events()` also asserts the incorrect condition that `NR30=0x80` immediately follows the wave data.

This plan validates the behavior before changing it.

## 3. Non-goals

- DirectSound/PCM voice mixing in `m4a_pcm.c`.
- PSG square or noise channels.
- General ROM-driver ARM7TDMI instruction timing parity.
- Frontend sinc, WAV alignment, or listening tests.
- Fixing PSW behavior as part of the harness commits.
- Checking in ROMs, ELFs, generated WAVs, PCM, or trace captures.

## 4. Target architecture

```text
                         one fixture + one scenario
                                   |
                    +--------------+--------------+
                    |                             |
          Reference adapter                 Candidate adapter
       full ROM + pinned mGBA              poryaaaa driver calls
        mgba_mp2k_reference                poryaaaa_psw_trace
                    |                             |
             reference.trace                 candidate.trace
                    |                             |
                    +-------- psw_compare --------+
                    |                             |
          pinned mGBA replay              pinned mGBA replay
          poryaaaa replay                 poryaaaa replay
                    |                             |
             exact native gate              exact native gate
```

The harness module has one orchestration interface. The two capture adapters are internal seams because the same fixture/scenario must be expressed once through the compiled ROM and once through poryaaaa.

### 4.1 User interface

Add:

```text
python3 tools/mgba-reference/validate_psw.py \
  --decomp PATH \
  --voicegroup NAME \
  --voice INDEX \
  --scenario start \
  --output-dir PATH
```

Optional fixture controls remain narrow: `--note`, `--velocity`, `--volume`, and `--pan`, matching `record_voice.sh`. Build executable locations may be overridden, but default to package-local `build/` targets.

The orchestrator owns temporary paths, subprocess execution, hashing, artifact publication, and exit status. Callers must not manually sequence individual tools for a normal validation run.

### 4.2 Candidate adapter

Add a focused executable:

```text
poryaaaa_psw_trace PROJECT_ROOT VOICEGROUP VOICE_INDEX \
  --scenario NAME --note N --velocity N --volume N --pan N \
  --trace-output FILE
```

Implementation location: `cmd/poryaaaa_psw_trace.c`.

It must:

- load the existing project voicegroup through `voicegroup_loader`;
- reject a selected slot that is not type `0x03` or `0x0B`;
- drive `M4ADriver` directly, without MIDI parsing or the renderer;
- advance a fixed number of logical vblanks for the selected scenario;
- serialize the existing `M4ARegWriteBatch` stream through `m4a_driver_trace`;
- write a small manifest containing the resolved type, waveform SHA-256, controls, scenario, and trace SHA-256.

Do not expose new production driver functions solely for this command. Use existing public driver operations and accessors.

### 4.3 Reference adapter

Extend `mgba_mp2k_reference` and `record_voice.sh` with `--psw-scenario NAME`.

The adapter must continue copying the real 12-byte ROM `ToneData` and using its real waveform pointer. Scenario commands are injected into the existing one-track EWRAM fixture; the ROM's compiled `MPlayMain` and `CgbSound` remain the implementation under test.

The reference manifest must record:

- ROM and ELF SHA-256;
- voicegroup symbol, zero-based index, and ROM voice address;
- copied 12-byte `ToneData` SHA-256;
- resolved type and 16 waveform bytes SHA-256;
- pinned mGBA revision and observation-patch SHA-256;
- scenario and controls;
- trace/native artifact hashes.

Reject non-PSW voices before capture.

### 4.4 Fixed scenarios

Use a fixed enum, not a general event scripting language:

1. `start` — one tied note; captures wave load, DAC enable, first pitch, pan, volume, and trigger ordering.
2. `envelope` — sustain long enough to cross the voice's natural NR32 volume-table transitions.
3. `pitch` — one sustained note followed by a pitch change without retrigger.
4. `volume-pan` — one sustained note followed by track volume and pan changes.
5. `retrigger` — stop and restart the same voice, proving DAC shutdown, waveform cache behavior, and trigger count.
6. `release` — note stop through release and final channel disable.

Both adapters must have behavior tests proving that each scenario issues the intended high-level controls. Do not share generated expected bus transactions between adapters; that would let one wrong model certify the other.

## 5. Trace projection and comparison contract

Add `tools/mgba-reference/psw_compare.py`.

Input:

```text
psw_compare.py REFERENCE.trace CANDIDATE.trace --output RESULT.json
```

The comparator parses the existing version-1 trace format and retains writes touching:

- `0x04000070..0x04000075` — NR30 through NR34;
- `0x04000080` — combined NR50/NR51 halfword;
- `0x04000090..0x0400009F` — wave RAM.

Only records inside `BEGIN`/`END` participate. `SAMPLE`, FIFO, timer, DirectSound, square, and noise records are excluded.

### 5.1 Required transaction gate

Compare retained records by ordinal, preserving:

- event kind;
- address;
- access width;
- value;
- sequence order.

Absolute cycle values are excluded from this gate because the software driver does not execute the ROM's ARM instructions. The result must be named `transaction_exact`, not `driver_parity`.

Any missing, extra, reordered, differently sized, or differently valued write fails. Four 32-bit ROM wave writes therefore do not silently equal sixteen byte writes.

### 5.2 Required fixture gate

Expand wave-RAM writes according to little-endian GBA bus semantics and compare the resulting 16 bytes. Report:

- `wave_payload_exact`;
- first differing byte and nibble;
- reference and candidate waveform hashes.

This gate is separate from transaction equality. Equal final bytes cannot excuse the wrong bus width or order.

### 5.3 Required logical-state gate

Replay only the retained PSW writes into a tiny diagnostic register model and compare state after every ordinal event:

- DAC/bank state from NR30;
- length from NR31;
- volume code from NR32;
- 11-bit frequency and trigger/length bits from NR33/NR34;
- wave routing bits from NR51;
- both wave-RAM banks.

This model is diagnostic and must not synthesize audio. It reports the first state divergence and the causal input record.

### 5.4 Timing report

Report cycle and same-cycle-order differences without using them to pass or fail `transaction_exact`:

- first cycle mismatch;
- per-record cycle delta;
- first same-cycle-order mismatch;
- writes that cross a native `SAMPLE` event in only one trace.

A write crossing a sample boundary is high severity because it can change the first audible waveform samples. Do not shift candidate cycles to match the oracle.

### 5.5 Comparator result

`RESULT.json` must contain:

```json
{
  "transaction_exact": false,
  "wave_payload_exact": true,
  "logical_state_exact": false,
  "cycle_exact": false,
  "reference_event_count": 0,
  "candidate_event_count": 0,
  "first_divergence": {},
  "hashes": {}
}
```

The command exits nonzero unless the three required gates pass: transaction, waveform payload, and logical state.

## 6. Native hardware gates

For each scenario, replay both traces through both hardware adapters:

```text
reference.trace -> mgba_audio_trace_replay -> reference-mgba
reference.trace -> poryaaaa_audio_trace    -> reference-pory
candidate.trace -> mgba_audio_trace_replay -> candidate-mgba
candidate.trace -> poryaaaa_audio_trace    -> candidate-pory
```

Run `native_compare.py` on each same-input replay pair and on the full-ROM `reference-native` capture versus `reference-mgba`.

Required:

- exact frame count;
- exact sample cycles;
- exact signed left PCM16;
- exact signed right PCM16.

This proves that the oracle trace reproduces the full-ROM native capture and answers whether either transaction stream is interpreted differently by the two hardware implementations. Do not compare reference-native directly with a candidate replay as a gate while their event cycles differ.

## 7. Artifact contract

Each run publishes atomically under the requested output directory:

```text
manifest.json
reference.trace
reference-native.{pcm,cycles,json}
reference-mgba.{pcm,cycles,json}
reference-pory.{pcm,cycles,json}
candidate.trace
candidate-mgba.{pcm,cycles,json}
candidate-pory.{pcm,cycles,json}
psw-compare.json
reference-hardware-compare.json
candidate-hardware-compare.json
```

`manifest.json` records exact commands, executable hashes, input hashes, revisions, controls, scenario, and every artifact hash. A failed run keeps complete temporary diagnostics but must not publish a manifest claiming success.

Generated artifacts belong under `build/` or another user-selected ignored directory. Only compact reviewed evidence JSON may be copied into `.scratch/`.

## 8. Execution plan

Every phase has a hard gate. Do not change PSW implementation behavior until Phase 4 proves that the harness detects the existing discrepancy.

### Phase 0 — Freeze the oracle fixture

Role: dispatch a `task` implementer; require a `reviewer` for ROM/mGBA provenance.

1. Select the currently discrepant real PSW voice and record its project, voicegroup, index, type, and waveform symbol.
2. Capture `start` twice through the pinned full-ROM recorder.
3. Require byte-identical traces, native artifacts, and manifests.
4. Record the known ROM start transaction sequence and artifact hashes as review evidence.

Gate: two independent reference runs are byte-identical and contain one unambiguous PSW start sequence.

### Phase 1 — Build the PSW comparator

Role: dispatch a `task` implementer; use a `reviewer` for GBA bus-width and little-endian semantics.

1. Implement the trace parser/projection in `psw_compare.py`.
2. Add self-tests covering:
   - identical traces pass;
   - cycle-only differences leave transaction equality intact but fail `cycle_exact`;
   - reordered NR30/NR32 writes fail;
   - 32-bit versus byte wave writes fail transaction equality;
   - equal expanded wave bytes still pass only the payload gate;
   - swapped nibbles and reversed words identify the first byte/nibble;
   - missing, duplicate, and extra NR34 triggers fail;
   - malformed traces fail closed;
   - writes crossing `SAMPLE` boundaries are reported.
3. Emit deterministic JSON and useful one-line failure output.

Gate: comparator self-tests pass and the comparator identifies the known oracle-versus-current sequence divergence.

### Phase 2 — Build the poryaaaa candidate adapter

Role: dispatch a `task` implementer; require a `reviewer` for lifetime and trace-order correctness.

1. Add `cmd/poryaaaa_psw_trace.c` and its CMake target.
2. Load the selected voice using the existing loader and reject non-PSW slots.
3. Implement `start` only.
4. Emit candidate trace and fixture manifest.
5. Prove deterministic repeat captures.

Gate: two candidate runs are byte-identical, contain the selected waveform bytes, and fail against the current oracle at the expected first transaction rather than at fixture identity.

### Phase 3 — Add the orchestration module

Role: dispatch a `task` implementer; use a `reviewer` for subprocess failure and artifact-publication behavior.

1. Add `validate_psw.py` with the narrow interface in §4.1.
2. Verify required binaries, pinned recorder provenance, ROM/ELF pairing, and writable output directory before capture.
3. Run both capture adapters, the PSW comparator, four replays, and two native comparisons.
4. Write hashes and commands to a temporary manifest; rename outputs into place only after validation finishes.
5. Return nonzero on any required-gate failure while preserving diagnostic artifacts.

Gate: one command reproduces the current PSW failure and localizes it to driver transactions while both hardware replay pairs remain exact.

### Phase 4 — Prove harness sensitivity

Role: use a `reviewer` independent of the implementer.

Run mutation checks against copied candidate traces:

1. remove `NR30=0x00`;
2. move NR32 before NR51;
3. replace four word writes with byte writes;
4. swap high/low waveform nibbles;
5. remove the second triggered NR34;
6. add a pitch-only NR34;
7. move one write across a `SAMPLE` record.

Gate: every mutation fails the intended gate and reports the exact causal record. No mutation may pass because final register state happens to match.

### Phase 5 — Add PSW lifecycle scenarios

Role: dispatch a `task` implementer for each adapter together; require a `reviewer` to compare the scenario's high-level intent, not shared expected bus output.

1. Add `envelope`, `pitch`, `volume-pan`, `retrigger`, and `release` to both adapters.
2. Keep scenario names and controls identical at the orchestration interface.
3. Capture normal and `_alt` PSW voices.
4. Exercise low, middle, and high notes; center and hard pan; and waveforms with non-symmetric bytes.
5. Require deterministic captures and run every gate for every case.

Gate: the fixture matrix has an explicit result for every scenario and reports no unexercised PSW register or waveform byte.

### Phase 6 — Convert oracle behavior into permanent regression tests

Role: dispatch a `task` implementer; require a `reviewer` to reject tests that merely restate implementation code.

1. Replace the incorrect `test_v2_wave_ram_events()` expectation with the exact reviewed ROM transaction contract.
2. Add behavioral tests for start, pitch-only update, envelope-volume update, retrigger, and disable.
3. Keep the oracle-derived expected event sequences independent from `m4a_cgb.c` helpers.
4. Run the focused tests and complete package unit suite.

Gate: each test fails under at least one Phase 4 mutation and passes only when the observed ROM contract is implemented.

### Phase 7 — Document the opt-in workflow

Role: use a `sonic` agent for the mechanical README update and a `reviewer` for command accuracy.

1. Add one concise PSW validation section to `tools/mgba-reference/README.md`.
2. Document required pinned source, external ROM/ELF inputs, command, artifacts, and gate meanings.
3. State explicitly that `transaction_exact` excludes absolute ARM instruction cycles and is not whole-driver parity.
4. Do not add the external full-ROM harness to default unit tests.

Gate: a clean package build plus documented external inputs can reproduce the same manifest and results.

## 9. Suggested commit slices

1. `feat(poryaaaa): compare programmable-wave transactions`
2. `feat(poryaaaa): capture isolated programmable-wave driver traces`
3. `feat(poryaaaa): orchestrate PSW differential validation`
4. `test(poryaaaa): prove PSW harness mutation sensitivity`
5. `test(poryaaaa): lock ROM programmable-wave contracts`
6. `docs(poryaaaa): document PSW validation workflow`

Each commit must build and pass its affected focused checks. Raw captures never enter a commit.

## 10. Completion criteria

The harness is complete when:

1. One command captures the same selected real PSW voice through the ROM and poryaaaa paths.
2. Fixture manifests prove equal voice type and waveform payload.
3. The comparator preserves address, width, value, and ordinal ordering and reports the first mismatch.
4. Reference and candidate traces each replay identically through pinned mGBA and poryaaaa hardware.
5. The current known NR30/NR32/NR34 ordering discrepancy is detected before any implementation fix.
6. Every mutation in Phase 4 fails for the intended reason.
7. Normal and `_alt` PSW lifecycle scenarios have deterministic results.
8. Focused comparator/driver tests and the complete `poryaaaa` unit suite pass.
9. Documentation states the exact claim: the harness validates PSW transaction semantics and hardware interpretation, not absolute ROM instruction timing or whole-engine parity.
