# Apr 30 5-oclock findings

Review target: `HW_AUDIO_SCAFFOLD_PLAN.md`

## Plan coherency issues

1. `HW_AUDIO_SCAFFOLD_PLAN.md:68` still says "Steps 1-2 audit" is pending, but the top status says step 1 and step 2 are closed at lines 12 and 16. Pick one status. Based on the rest of the document, this should probably become a narrower note about remaining parity validation, not a pending step-1/2 audit.

2. `HW_AUDIO_SCAFFOLD_PLAN.md:130` says the MODT/LFO test is still a smoke test "until LFO/modM advancement lands," but step 1 says LFO advancement landed and lists LFO tests at lines 1192-1208. This is stale.

3. `HW_AUDIO_SCAFFOLD_PLAN.md:1166` says "Still open before calling Layer 1 complete," then lists MPlayMain, LFO, and PCM/reverb audit. Step 1 later says MPlayMain song-walk is deliberately out of scope for DAW use, LFO is present, and step 2 is closed. This section now contradicts the numbered plan.

4. `HW_AUDIO_SCAFFOLD_PLAN.md:1151` and line 869 still describe production/event rendering as "host-rate placeholder PSG + PCM." That is stale after steps 8/9. The current implementation is described elsewhere as chip-internal rate plus polyphase resampler.

5. `HW_AUDIO_SCAFFOLD_PLAN.md:1284` says noise is "Still at host-rate placeholder" and that SOUNDBIAS/polyphase will land at steps 8/9. Steps 8/9 are already closed. Remove this or mark it explicitly as historical.

6. `HW_AUDIO_SCAFFOLD_PLAN.md:331` says the remaining parity gate is §12.10b, then immediately says the remaining parity gates include steps 8, 9, and 10. Since 8 and 9 are closed, this reads like stale pasted text.

7. `HW_AUDIO_SCAFFOLD_PLAN.md:1366` marks DirectSound FIFO sample-hold boundary tests as "out-of-scope," but DirectSound crackle is an active observed problem. This should not be out-of-scope now. Move it into an immediate validation/blocker subsection.

8. The "Blocking gates before parity claims" table at line 1407 only lists §12.10b. Given the current DAW crackle, it should also list the wave-trigger/phase contract and DirectSound PCM timing/chunk-invariance validation as open blockers.

## Suggested coherent status

- Steps 1-9 and 10a are closed.
- Step 10b remains open.
- Current DAW crackle investigation is a blocker before parity claims.
- DirectSound FIFO timing tests should be treated as immediate validation work, not out-of-scope.
- The wave trigger/phase contract should be documented as an open blocker until `hw_psg.h`, `hw_psg.c`, `m4a_cgb.c`, and tests agree with mGBA/hardware behavior.
