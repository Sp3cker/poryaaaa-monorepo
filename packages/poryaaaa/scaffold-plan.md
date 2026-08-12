# Scaffold execution — pre-flight question

Before writing files, one signature mismatch to resolve.

## The mismatch

Plan §7b shows:

```c
void hw_audio_render(HwAudio*,
                     const M4ARegisterFile *regs,
                     const M4APcmRing *pcm,
                     float *out_lr, int frames);
```

— interleaved stereo `out_lr`.

But every existing call site uses **separate L/R buffers**:

```c
void m4a_engine_process(M4AEngine*, float *outL, float *outR, int nframes);
```

Confirmed call sites:
- `plugin/m4a_plugin.c:667`
- `cmd/poryaaaa_render.c:732`
- `test/test_wav_export.c:118, 129, 143, 153, 193, 203, 210`
- `test/test_engine.c:380`

## Two options

1. **Match existing convention** — separate L/R:
   ```c
   void hw_audio_render(HwAudio*,
                        const M4ARegisterFile *regs,
                        const M4APcmRing *pcm,
                        float *outL, float *outR, int frames);
   ```
   - Drops in at every call site verbatim, no interleave glue.
   - Smaller diff; the `#ifdef M4A_ENGINE_V2` fork at each site stays a single line swap.

2. **Per-plan interleaved** (`float *out_lr`):
   - Requires deinterleave glue at every call site (a stack `float tmp[frames*2]` and a copy loop).
   - No win that I can see — DAWs hand CLAP separate-buffer ports anyway in this code path.

## Recommendation

Option 1 (separate L/R). The plan didn't pick interleaved for any stated reason; matching the existing convention is the smaller, safer change. It also keeps the v1/v2 fork mechanically trivial.

Internally `hw_audio` can still process interleaved if the resampler prefers it — that's a private detail behind the function.

## Awaiting confirmation

OK to proceed with separate L/R, or do you want interleaved?
