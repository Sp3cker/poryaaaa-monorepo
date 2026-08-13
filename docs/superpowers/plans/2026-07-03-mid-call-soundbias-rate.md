# Chip Audio Fidelity Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the fixable chip-emulation fidelity gaps found in the July 3 audit: same-call `SOUNDBIAS` rate changes, DirectSound FIFO reset register behavior, and PSG length counters.

**Architecture:** Keep each fix at the existing chip seam. `HwAudio` owns in-call rate epochs, `HwPcm` owns DirectSound FIFO state, and `HwPsgSynth` owns PSG length-counter state. Do not add public interfaces.

**Tech Stack:** C11, CMake, `poryaaaa_unit_tests`.

---

## Scope

- Fix only these chip-emulation gaps: same-call `SOUNDBIAS` rate changes, `SOUNDCNT_H` FIFO reset bits, and PSG length counters.
- Do not change the render-cadence strategy. The internal-rate floor remains `max(131072, 32768 << sampling_cycle)`.
- Do not reintroduce the analog output filter.
- Do not implement CPU FIFO writes, timer-selected FIFO drains, hardware envelope progression, or render-cadence changes in this plan.
- Check each chip-behavior fix against the local mGBA/source references in Task 8 before closing it. Unit tests alone are not enough.
- A `SOUNDBIAS` event must still update `HwMixBus` immediately for bias/clipping, as it does now.
- A `SOUNDBIAS` event must also update `HwPcm`'s quirk rate immediately, even when `internal_rate` does not change because the 131072 Hz floor still applies.
- When `internal_rate` changes, flush/rebuild the resampler at the event point. That matches the existing source comment's stated requirement for lifting the boot-time-only restriction.
- Preserve existing public functions in `hw_audio.h`; this is an implementation change behind the current interface.

## Files

- Modify: `packages/poryaaaa/plugin/hw_audio/hw_audio.c`
  - Add a private rate-sync helper.
  - Render event-to-event host spans instead of precomputing one internal span for the whole call.
  - Run rate sync after applying each `M4A_REG_SOUNDBIAS` event.
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_audio.h`
  - Update comments that currently say mid-call `SOUNDBIAS` changes wait until the next call.
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_pcm.c`
  - Handle `SOUNDCNT_H` FIFO reset bits in `hw_pcm_apply_event`.
  - Split held-sample cache sentinels per DirectSound FIFO so resetting A does not disturb B, and vice versa.
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_pcm.h`
  - Add the per-FIFO cache sentinel fields needed by `hw_pcm.c`.
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_psg.c`
  - Add PSG length-counter loads, enables, trigger reloads, and frame-sequencer decrement behavior.
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_psg.h`
  - Add per-channel length-counter and length-enable state.
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_mix.h`
  - Correct stale or misleading `SOUNDCNT_H` bit comments.
- Modify: `packages/poryaaaa/test/test_engine.c`
  - Change the existing `test_chip_canned_soundbias_internal_rate_switches` expectation.
  - Add a one-call-vs-split-call regression for a mid-call `SOUNDBIAS` switch.
  - Add focused DirectSound FIFO reset and PSG length-counter regressions.
- Modify: `packages/poryaaaa/docs/arch-parity-fix-plan.md`
  - Remove stale "boot-time-only SOUNDBIAS target restriction" wording after tests pass.
  - Record the DirectSound reset and PSG length-counter gaps as closed when their tests pass.
  - Record the local source/reference evidence used for each closed chip-behavior fix.

Do not modify `m4a_driver`, `m4a_cgb`, or CMake. The m4a writer already carries enough register intent for these fixes; the work belongs inside the chip-emulation modules.

---

### Task 1: Write the Failing Rate-Switch Test

**Files:**
- Modify: `packages/poryaaaa/test/test_engine.c`

- [ ] **Step 1: Change the same-call rate-switch expectation**

In `test_chip_canned_soundbias_internal_rate_switches`, replace the block that documents the old boot-time-only behavior:

```c
/* Also assert the deliberate mid-call limitation: an event in the
 * current call is applied to the mix bus, but doesn't change
 * internal_rate within the same call.  This is the documented
 * boot-time-only target restriction. */
{
    M4ARegWrite ev[] = {{0, M4A_REG_SOUNDBIAS, 0x200u | (3u << 14)}};
    M4ARegWriteBatch batch = {ev, sizeof(ev) / sizeof(ev[0])};
    hw_audio_destroy(hw);
    hw = hw_audio_create(44100.0f);
    ASSERT_EQ(hw_audio_internal_rate(hw), 131072, "fresh chip starts at 131072");
    hw_audio_render_events(hw, &batch, NULL, scratch, scratch, 1);
    ASSERT_EQ(hw_audio_internal_rate(hw), 131072, "mid-call SOUNDBIAS doesn't switch rate (boot-time-only target)");
}
```

with:

```c
/* A SOUNDBIAS event at offset 0 is a same-call rate change: the event
 * stream has reached that hardware write, so the chip's rate state must
 * be updated before the call returns. */
{
    M4ARegWrite ev[] = {{0, M4A_REG_SOUNDBIAS, 0x200u | (3u << 14)}};
    M4ARegWriteBatch batch = {ev, sizeof(ev) / sizeof(ev[0])};
    hw_audio_destroy(hw);
    hw = hw_audio_create(44100.0f);
    ASSERT_EQ(hw_audio_internal_rate(hw), 131072, "fresh chip starts at 131072");
    hw_audio_render_events(hw, &batch, NULL, scratch, scratch, 1);
    ASSERT_EQ(hw_audio_internal_rate(hw), 262144, "mid-call SOUNDBIAS switches rate before return");
}
```

- [ ] **Step 2: Run the focused test and confirm failure**

Run:

```bash
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests
```

Expected before implementation: `test_chip_canned_soundbias_internal_rate_switches` fails because `hw_audio_internal_rate(hw)` remains `131072` after the same-call event.

---

### Task 2: Move Rate Sync Behind a Private Helper

**Files:**
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_audio.c`

- [ ] **Step 1: Add `hw_audio_sync_rates_from_mix` near `chip_internal_rate`**

Add this private helper after `chip_internal_rate`:

```c
static void hw_audio_sync_rates_from_mix(HwAudio* hw)
{
    int desired_internal_rate = chip_internal_rate(hw->mix.sampling_cycle);
    int desired_quirk_rate = chip_quirk_rate(hw->mix.sampling_cycle);

    if (desired_internal_rate != hw->internal_rate)
    {
        hw->internal_rate = desired_internal_rate;
        hw_psg_set_render_rate(&hw->psg, (float)hw->internal_rate);
        hw_pcm_set_render_rate(&hw->pcm, (float)hw->internal_rate);

        /* Rate changes define a new resampler epoch.  The old ring was
         * filled at the previous input cadence, so keep the transition
         * local by flushing and rebuilding here. */
        hw_resample_init(&hw->resample, (double)hw->internal_rate, (double)hw->host_rate);
        hw->total_inputs_pushed = 0;
        hw->total_outputs_target = 0;
    }

    hw_pcm_set_quirk_rate(&hw->pcm, desired_quirk_rate);
}
```

- [ ] **Step 2: Replace the start-of-call rate block**

Remove the current start-of-call block that computes `desired_internal_rate` and `desired_quirk_rate` inline, and call:

```c
hw_audio_sync_rates_from_mix(hw);
```

Keep a shortened comment explaining that this catches `SOUNDBIAS` changes from prior calls before the first segment of the current call renders.

- [ ] **Step 3: Build and confirm no behavior changed yet**

Run:

```bash
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests
```

Expected: the new test still fails. Existing tests should otherwise compile and run to the same point.

---

### Task 3: Render Per Host Span With Current Rate

**Files:**
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_audio.c`

- [ ] **Step 1: Add a host-span render helper**

Add this helper before `hw_audio_render_events`:

```c
static void render_to_host_offset(HwAudio* hw,
                                  const M4APcmRing* pcm_ring,
                                  float* outL,
                                  float* outR,
                                  int target_host,
                                  int* rendered_host)
{
    int host_delta = target_host - *rendered_host;
    if (host_delta <= 0)
        return;

    const double step = (double)hw->internal_rate / (double)hw->host_rate;
    const int64_t new_outputs_target = hw->total_outputs_target + (int64_t)host_delta;
    const int64_t target_inputs_total = inputs_for_total_outputs(new_outputs_target, step);
    int64_t internal_to_render_64 = target_inputs_total - hw->total_inputs_pushed;
    if (internal_to_render_64 < 0)
        internal_to_render_64 = 0;

    int internal_to_render = (int)internal_to_render_64;
    if (internal_to_render > 0)
    {
        render_segment(hw, pcm_ring, outL, outR, internal_to_render, rendered_host, target_host);
    }

    hw->total_inputs_pushed += internal_to_render_64;
    hw->total_outputs_target = new_outputs_target;

    /* Preserve the event timeline when a freshly reset resampler has
     * startup latency.  Later segments must start writing at target_host,
     * not at an earlier index. */
    while (*rendered_host < target_host)
    {
        if (outL)
            outL[*rendered_host] = 0.0f;
        if (outR)
            outR[*rendered_host] = 0.0f;
        (*rendered_host)++;
    }
}
```

- [ ] **Step 2: Replace the single-rate event loop**

Inside `hw_audio_render_events`, remove the precomputed `step`, `prev_outputs_target`, `new_outputs_target`, `target_inputs_total`, `internal_to_render`, and `rendered_internal` logic.

Use this structure instead:

```c
int rendered_host = 0;

if (events)
{
    for (size_t i = 0; i < events->count; i++)
    {
        const M4ARegWrite* ev = &events->events[i];
        int H = (int)ev->sample_offset;
        if (H > frames)
            H = frames;
        if (H < 0)
            H = 0;

        render_to_host_offset(hw, pcm, outL, outR, H, &rendered_host);

        hw_psg_apply_event(&hw->psg, ev);
        hw_pcm_apply_event(&hw->pcm, ev);
        hw_mix_apply_event(&hw->mix, ev);

        if (ev->reg == M4A_REG_SOUNDBIAS)
            hw_audio_sync_rates_from_mix(hw);
    }
}

render_to_host_offset(hw, pcm, outL, outR, frames, &rendered_host);
```

Leave the final silence-padding loop in place as a defensive no-op. It should not normally run because `render_to_host_offset` pads each segment to its target.

- [ ] **Step 3: Run the focused test**

Run:

```bash
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests
```

Expected: `test_chip_canned_soundbias_internal_rate_switches` passes. If a block-size or resampler test fails, inspect whether the helper is updating `total_inputs_pushed` and `total_outputs_target` once per host span, not once per whole call.

---

### Task 4: Add One-Call vs Split-Call Equivalence Coverage

**Files:**
- Modify: `packages/poryaaaa/test/test_engine.c`

- [ ] **Step 1: Add a regression near the other canned `SOUNDBIAS` tests**

Add a test that renders the same SQ2 note in two equivalent ways:

1. One call of `N` frames with `M4A_REG_SOUNDBIAS` at `SWITCH_AT`.
2. Two calls: first `SWITCH_AT` frames, then a second call with the same `SOUNDBIAS` event at offset 0.

Use this structure:

```c
static void test_chip_canned_soundbias_mid_call_matches_split_call(void)
{
    printf("Testing chip-only: mid-call SOUNDBIAS switch matches split-call switch...\n");

    enum
    {
        N = 2048,
        SWITCH_AT = 768,
    };

    float one_L[N] = {0}, one_R[N] = {0};
    float split_L[N] = {0}, split_R[N] = {0};

    M4ARegWrite start_and_switch[] = {
        {0, M4A_REG_NR52, 0x80},
        {0, M4A_REG_NR50, 0x77},
        {0, M4A_REG_NR51, 0x22},
        {0, M4A_REG_NR21, 0x80},
        {0, M4A_REG_NR22, 0xF0},
        {0, M4A_REG_NR23, 0x00},
        {0, M4A_REG_NR24, 0x80 | 0x02},
        {SWITCH_AT, M4A_REG_SOUNDBIAS, 0x200u | (3u << 14)},
    };
    M4ARegWriteBatch one_batch = {start_and_switch, sizeof(start_and_switch) / sizeof(start_and_switch[0])};

    HwAudio* one = hw_audio_create(44100.0f);
    ASSERT(one != NULL, "one-call HwAudio allocation succeeds");
    hw_audio_render_events(one, &one_batch, NULL, one_L, one_R, N);
    ASSERT_EQ(hw_audio_internal_rate(one), 262144, "one-call render ends at sampling_cycle 3 rate");
    hw_audio_destroy(one);

    M4ARegWrite start_only[] = {
        {0, M4A_REG_NR52, 0x80},
        {0, M4A_REG_NR50, 0x77},
        {0, M4A_REG_NR51, 0x22},
        {0, M4A_REG_NR21, 0x80},
        {0, M4A_REG_NR22, 0xF0},
        {0, M4A_REG_NR23, 0x00},
        {0, M4A_REG_NR24, 0x80 | 0x02},
    };
    M4ARegWriteBatch start_batch = {start_only, sizeof(start_only) / sizeof(start_only[0])};
    M4ARegWrite switch_only[] = {{0, M4A_REG_SOUNDBIAS, 0x200u | (3u << 14)}};
    M4ARegWriteBatch switch_batch = {switch_only, sizeof(switch_only) / sizeof(switch_only[0])};

    HwAudio* split = hw_audio_create(44100.0f);
    ASSERT(split != NULL, "split-call HwAudio allocation succeeds");
    hw_audio_render_events(split, &start_batch, NULL, split_L, split_R, SWITCH_AT);
    hw_audio_render_events(split, &switch_batch, NULL, split_L + SWITCH_AT, split_R + SWITCH_AT, N - SWITCH_AT);
    ASSERT_EQ(hw_audio_internal_rate(split), 262144, "split-call render ends at sampling_cycle 3 rate");
    hw_audio_destroy(split);

    float max_diff = 0.0f;
    for (int i = 0; i < N; i++)
    {
        float dl = fabsf(one_L[i] - split_L[i]);
        float dr = fabsf(one_R[i] - split_R[i]);
        if (dl > max_diff)
            max_diff = dl;
        if (dr > max_diff)
            max_diff = dr;
    }
    ASSERT(max_diff < 1e-4f, "mid-call SOUNDBIAS output matches split-call equivalent");
}
```

Add the test to the canned-chip section in `main()` after `test_chip_canned_soundbias_internal_rate_switches()`.

- [ ] **Step 2: Run the focused suite**

Run:

```bash
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests
```

Expected: all unit tests pass, including the new split-call equivalence test.

---

### Task 5: Update Comments and Docs

**Files:**
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_audio.h`
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_audio.c`
- Modify: `packages/poryaaaa/docs/arch-parity-fix-plan.md`

- [ ] **Step 1: Update `hw_audio.h` comments**

Revise comments that currently say `sampling_cycle` is synced only at start-of-render-call or that mid-call changes wait until the next call. The new comment should say:

```c
/* Sampling_cycle is synced from HwMixBus before each rendered span and
 * immediately after SOUNDBIAS events.  If a SOUNDBIAS event changes the
 * chip rate mid-call, HwAudio starts a new resampler/rate epoch at that
 * event offset. */
```

- [ ] **Step 2: Replace the old boot-time-only comment in `hw_audio.c`**

Replace the long "Boot-time-only target restriction" block with a shorter comment:

```c
/* Catch any SOUNDBIAS sampling_cycle written by a prior call before
 * rendering this call's first span.  Same-call SOUNDBIAS events are
 * handled inside the event loop after HwMixBus consumes the event. */
hw_audio_sync_rates_from_mix(hw);
```

- [ ] **Step 3: Update `docs/arch-parity-fix-plan.md`**

Change the sections that describe mid-call `SOUNDBIAS` handling as out of scope. Record it as closed by the new test:

```markdown
**Mid-call SOUNDBIAS sampling_cycle changes** are now handled at the event
offset inside `hw_audio_render_events()`. A `SOUNDBIAS` event updates
`HwMixBus`, then `HwAudio` syncs `internal_rate`, `HwPcm` quirk rate, and
the resampler epoch before rendering the next host span. Regression:
`test_chip_canned_soundbias_mid_call_matches_split_call`.
```

- [ ] **Step 4: Check for stale wording**

Run:

```bash
rg -n "boot-time-only|mid-call SOUNDBIAS|next render boundary|snapshot-at-start|test_chip_canned_soundbias_internal_rate_switches" packages/poryaaaa/plugin/hw_audio packages/poryaaaa/test packages/poryaaaa/docs/arch-parity-fix-plan.md
```

Expected: no remaining comments say that mid-call `SOUNDBIAS` rate changes are intentionally deferred. Mentions in old historical context are acceptable only if they explicitly say the limitation has been removed.

---

### Task 6: Implement DirectSound FIFO Reset Bits

**Files:**
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_pcm.h`
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_pcm.c`
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_mix.h`
- Modify: `packages/poryaaaa/test/test_engine.c`
- Modify: `packages/poryaaaa/docs/arch-parity-fix-plan.md`

- [ ] **Step 1: Name the `SOUNDCNT_H` reset bits in `hw_pcm.c`**

Use the standard GBA `SOUNDCNT_H` bit layout:

```c
enum
{
    HW_PCM_SOUND_A_FIFO_RESET = 0x0800,
    HW_PCM_SOUND_B_FIFO_RESET = 0x8000,
};
```

Do not move routing, timer-select, or DirectSound volume handling out of `HwMixBus`. This task only makes reset bits clear the modeled DirectSound FIFO/held-output state.

- [ ] **Step 2: Split shared PCM cache sentinels per FIFO**

In `HwPcm`, replace the shared cache sentinels:

```c
int64_t pcm_last_int;
int64_t quirk_last_int;
```

with per-FIFO sentinels:

```c
int64_t pcm_last_int_a;
int64_t pcm_last_int_b;
int64_t quirk_last_int_a;
int64_t quirk_last_int_b;
```

Initialize all four to `-1` in `hw_pcm_init`. This is needed because hardware has separate FIFO A/B reset bits; resetting A should not force B to reload, and resetting B should not disturb A.

- [ ] **Step 3: Update render cache checks independently**

In `hw_pcm_render`, keep `pcm_pos` and `quirk_pos` shared, but update A and B held bytes independently:

```c
if (pcm_published)
{
    size_t idx = (size_t)pcm_int % M4A_PCM_DMA_BUF_SIZE;
    if (pcm_int != pcm->pcm_last_int_a)
    {
        pcm->held_pcm_a = ring->ring_a[idx];
        pcm->pcm_last_int_a = pcm_int;
    }
    if (pcm_int != pcm->pcm_last_int_b)
    {
        pcm->held_pcm_b = ring->ring_b[idx];
        pcm->pcm_last_int_b = pcm_int;
    }
}
```

Do the same for `held_quirk_a` and `held_quirk_b` with `quirk_last_int_a` and `quirk_last_int_b`.

- [ ] **Step 4: Handle reset bits in `hw_pcm_apply_event`**

When `ev->reg == M4A_REG_SOUNDCNT_H`, clear only the selected FIFO side:

```c
if (ev->value & HW_PCM_SOUND_A_FIFO_RESET)
{
    pcm->held_pcm_a = 0;
    pcm->held_quirk_a = 0;
    pcm->pcm_last_int_a = (int64_t)pcm->pcm_pos;
    pcm->quirk_last_int_a = (int64_t)pcm->quirk_pos;
}
if (ev->value & HW_PCM_SOUND_B_FIFO_RESET)
{
    pcm->held_pcm_b = 0;
    pcm->held_quirk_b = 0;
    pcm->pcm_last_int_b = (int64_t)pcm->pcm_pos;
    pcm->quirk_last_int_b = (int64_t)pcm->quirk_pos;
}
```

This intentionally does not rewind `pcm_pos` or `quirk_pos`. A hardware FIFO reset clears the FIFO contents/state; it does not rewind the source ring clock.

- [ ] **Step 5: Add focused reset tests**

Add a direct `HwPcm` unit test that:

1. Initializes `HwPcm` and a small `M4APcmRing` with non-zero A and B data.
2. Publishes enough samples to render non-zero held output on both sides.
3. Applies `M4A_REG_SOUNDCNT_H` with `0x0800`.
4. Renders one more sample and asserts FIFO A is zero while FIFO B is still non-zero.
5. Repeats with `0x8000` and asserts FIFO B is zero while FIFO A is not disturbed.

Use a direct `HwPcm` test for this regression. Do not route this test through `HwAudio`.

- [ ] **Step 6: Update stale DirectSound docs/comments**

Update the `hw_pcm_apply_event` comment that currently says PCM needs no event subscriptions. Update `docs/arch-parity-fix-plan.md` to record FIFO reset bits as implemented.

In `hw_mix.h`, correct only stale `SOUNDCNT_H` bit comments. Do not refactor `HwMixBus`.

- [ ] **Step 7: Run the focused suite**

Run:

```bash
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests
```

Expected: the new FIFO reset regression passes and existing PCM timing tests still pass.

---

### Task 7: Implement PSG Length Counters

**Files:**
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_psg.h`
- Modify: `packages/poryaaaa/plugin/hw_audio/hw_psg.c`
- Modify: `packages/poryaaaa/test/test_engine.c`
- Modify: `packages/poryaaaa/docs/arch-parity-fix-plan.md`

- [ ] **Step 1: Add per-channel length state**

Add these fields to `HwPsgSynth` near the related channel state:

```c
uint16_t sq1_length_counter;
uint16_t sq2_length_counter;
uint16_t wave_length_counter;
uint16_t noise_length_counter;
bool sq1_length_enabled;
bool sq2_length_enabled;
bool wave_length_enabled;
bool noise_length_enabled;
```

Clear them in `hw_psg_clear_channel_state`.

- [ ] **Step 2: Add small local length helpers**

Add helpers near the frame-sequencer helpers:

```c
static uint16_t hw_psg_length64_from_load(uint32_t v)
{
    return (uint16_t)(64u - (v & 0x3Fu));
}

static uint16_t hw_psg_length256_from_load(uint32_t v)
{
    return (uint16_t)(256u - (v & 0xFFu));
}
```

Do not add a generic length abstraction.

- [ ] **Step 3: Decode length-load and length-enable registers**

Update register handling:

- `NR11`: keep duty decode, and load `sq1_length_counter = hw_psg_length64_from_load(v)`.
- `NR21`: keep duty decode, and load `sq2_length_counter = hw_psg_length64_from_load(v)`.
- `NR31`: load `wave_length_counter = hw_psg_length256_from_load(v)`.
- `NR41`: load `noise_length_counter = hw_psg_length64_from_load(v)`.
- `NR14`, `NR24`, `NR34`, `NR44`: set the channel's `*_length_enabled` from bit 6.
- On a trigger write, if that channel's length counter is zero, reload it to the hardware maximum: 64 for square/noise, 256 for wave.

Keep the existing trigger behavior for DAC gating, phase, sweep, wave reset, and noise LFSR reset.

- [ ] **Step 4: Decrement counters from the existing length hook**

Replace `hw_psg_frame_length`'s debug-only implementation with length-counter progression:

```c
static void hw_psg_frame_length(HwPsgSynth* psg)
{
    psg->frame_seq_length_ticks++;
    if (psg->sq1_length_enabled && psg->sq1_length_counter > 0 && --psg->sq1_length_counter == 0)
        psg->sq1_enabled = false;
    if (psg->sq2_length_enabled && psg->sq2_length_counter > 0 && --psg->sq2_length_counter == 0)
        psg->sq2_enabled = false;
    if (psg->wave_length_enabled && psg->wave_length_counter > 0 && --psg->wave_length_counter == 0)
        psg->wave_enabled = false;
    if (psg->noise_length_enabled && psg->noise_length_counter > 0 && --psg->noise_length_counter == 0)
        psg->noise_enabled = false;
}
```

This keeps length ticking at existing frame-sequencer steps 0, 2, 4, and 6.

- [ ] **Step 5: Add focused length-counter tests**

Add direct `HwPsgSynth` tests that prove:

- Square 2 with `NR21` length load `63`, `NR24` length-enable + trigger, and a non-zero envelope becomes silent after one length clock.
- Wave with `NR31` load `255`, `NR34` length-enable + trigger, DAC on, and non-zero wave RAM becomes silent after one length clock.
- Noise with `NR41` length load `63`, `NR44` length-enable + trigger, and a non-zero envelope becomes silent after one length clock.

Keep the tests direct against `HwPsgSynth`; this is hardware-register behavior, not an m4a sequencing behavior.

- [ ] **Step 6: Protect normal m4a behavior**

Add a regression proving the normal m4a path emits `length_en=false` for regular sustained notes. Assert sustained notes keep playing because the event stream keeps bit 6 clear.

- [ ] **Step 7: Update docs**

Remove the deferred-subsystem comments for `NR31` and `NR41`. Update `docs/arch-parity-fix-plan.md` to record PSG length counters as implemented. Do not implement hardware envelope progression in this plan.

- [ ] **Step 8: Run the focused suite**

Run:

```bash
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests
```

Expected: new length-counter regressions pass; existing frame-sequencer, sweep, wave, and noise tests still pass.

---

### Task 8: Check Reference Behavior

**Files:**
- Modify: `packages/poryaaaa/docs/arch-parity-fix-plan.md`

- [ ] **Step 1: Record the local reference sources used**

Record these source checks in `docs/arch-parity-fix-plan.md`:

- `SOUNDBIAS`: local mGBA `src/gba/audio.c`, `GBAAudioWriteSOUNDBIAS`, samples before applying the write, updates `sampleInterval`, and emits `audioRateChanged` when the rate changes.
- DirectSound FIFO reset: local mGBA `src/gba/audio.c`, `GBAAudioWriteSOUNDCNT_HI`, resets FIFO A/B read/write pointers when the reset bits are set.
- DirectSound reset bit constants: GBA headers define `SOUND_A_FIFO_RESET = 0x0800` and `SOUND_B_FIFO_RESET = 0x8000`.
- PSG length counters: local mGBA `src/gb/audio.c`, `GBAudioWriteNR11`, `GBAudioWriteNR21`, `GBAudioWriteNR31`, and `GBAudioWriteNR41` load length counters; `NR14`, `NR24`, `NR34`, and `NR44` trigger/stop handling reloads or decrements according to frame phase.

- [ ] **Step 2: Run existing reference comparisons only**

Search for existing reference comparison material:

```bash
rg -n "SOUNDBIAS|FIFO reset|fifo-a|fifo-b|length counter|NR11|NR21|NR31|NR41" \
  /Users/spencer/dev/poryaaaa/poryaaaa/tools/captures \
  packages/poryaaaa
```

If a matching README/script/test exists, run its documented command and record the exact command/output in `docs/arch-parity-fix-plan.md`.

If no existing reference comparison covers a fix, record the search command and result in `docs/arch-parity-fix-plan.md`. Do not create a new capture framework, test ROM, or savestate in this plan.

---

### Task 9: Final Verification

**Files:**
- No new source files.

- [ ] **Step 1: Run package-local unit tests**

Run:

```bash
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests
```

Expected: build succeeds and `poryaaaa_unit_tests` exits 0.

- [ ] **Step 2: Build the touched runtime targets**

Run:

```bash
cmake --build build --target poryaaaa_render poryaaaa
```

Expected: build succeeds.

- [ ] **Step 3: Verify the M4L external still links the same libraries**

Run:

```bash
npm run build:externals
```

from `packages/poryaaaa-m4l`.

Expected: build succeeds. This is the downstream library-style consumer that links `m4a_driver` and `hw_audio`.

- [ ] **Step 4: Run diff hygiene**

Run:

```bash
git diff --check
```

Expected: no whitespace errors.

---

## Success Criteria

- `M4A_REG_SOUNDBIAS` at event offset 0 changes `hw_audio_internal_rate()` before `hw_audio_render_events()` returns.
- A one-call mid-call `SOUNDBIAS` switch matches the equivalent split-call render within the established canned-chip tolerance.
- `HwPcm` quirk rate updates after every `SOUNDBIAS` event, including sampling-cycle changes that do not change `internal_rate`.
- `M4A_REG_SOUNDCNT_H` FIFO reset bit `0x0800` clears modeled DirectSound FIFO A state without disturbing FIFO B.
- `M4A_REG_SOUNDCNT_H` FIFO reset bit `0x8000` clears modeled DirectSound FIFO B state without disturbing FIFO A.
- PSG square, wave, and noise length counters load from their NR length registers, decrement on the existing length frame-sequencer ticks, and disable their channels at zero only when length-enable bit 6 is set.
- Normal m4a/NP2K sustained notes keep playing because the emitted register stream keeps length-enable bit 6 clear.
- `docs/arch-parity-fix-plan.md` records the local mGBA/source evidence used for each closed chip-behavior fix, plus any missing capture/trace comparison as an open validation gate.
- No new public chip-audio interface is added.
- Stale comments no longer describe same-call `SOUNDBIAS`, DirectSound FIFO reset, or PSG length counters as deliberately unsupported.
- The render-cadence strategy and analog output filter remain explicitly out of scope.
