/* sine_purity_capture - deterministic sine-purity benchmark harness.
 *
 * Renders one sustained DirectSound voice whose PCM table is a mathematically
 * pure sine through the real M4A driver -> hw_audio mixer chain, captures the
 * output to WAV, then measures how close the captured signal is to that pure
 * tone: least-squares fits DC + harmonics of the detected fundamental and
 * reports distortion + residual energy relative to fundamental energy.
 * Prints METRIC lines for the autoresearch loop; exits non-zero on failure.
 *
 * Usage: sine_purity_capture --output out.wav [options]
 */

#include "hw_audio/hw_audio.h"
#include "m4a/m4a_driver.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    VG_SIZE = 128,
    SINE_TABLE_MAX = 4096,
    /* 18157 / 284 = 63.93 Hz — the GBA DirectSound low-frequency target. */
    SINE_TABLE_DEFAULT = 284,
    HARMONICS = 10,
};


typedef struct
{
    const char* output;
    int seconds;
    int sample_rate;
    int block;
    M4APcmMixerMode mixer;
    int polyphony;
    int volume;
    uint32_t sample_hz;
    int pcm_mix_rate;
    int table_len;
    int antialias;
} Options;

static void write_u16_le(FILE* f, uint16_t val)
{
    uint8_t buf[2] = {val & 0xFF, (val >> 8) & 0xFF};
    fwrite(buf, 1, 2, f);
}

static void write_u32_le(FILE* f, uint32_t val)
{
    uint8_t buf[4] = {val & 0xFF, (val >> 8) & 0xFF, (val >> 16) & 0xFF, (val >> 24) & 0xFF};
    fwrite(buf, 1, 4, f);
}

/* Write the captured stereo float buffers as 16-bit PCM WAV. */
static int write_wav(const char* path, const float* left, const float* right, uint64_t numSamples, int sampleRate)
{
    FILE* f = fopen(path, "wb");
    if (!f)
        return -1;

    uint16_t numChannels = 2;
    uint16_t bitsPerSample = 16;
    uint32_t byteRate = (uint32_t)sampleRate * numChannels * bitsPerSample / 8;
    uint16_t blockAlign = numChannels * bitsPerSample / 8;
    uint32_t dataSize = (uint32_t)(numSamples * numChannels * bitsPerSample / 8);

    fwrite("RIFF", 1, 4, f);
    write_u32_le(f, 36 + dataSize);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    write_u32_le(f, 16);
    write_u16_le(f, 1);
    write_u16_le(f, numChannels);
    write_u32_le(f, (uint32_t)sampleRate);
    write_u32_le(f, byteRate);
    write_u16_le(f, blockAlign);
    write_u16_le(f, bitsPerSample);
    fwrite("data", 1, 4, f);
    write_u32_le(f, dataSize);

    for (uint64_t i = 0; i < numSamples; i++)
    {
        int32_t l = (int32_t)(left[i] * 32767.0f);
        int32_t r = (int32_t)(right[i] * 32767.0f);
        if (l > 32767)
            l = 32767;
        if (l < -32768)
            l = -32768;
        if (r > 32767)
            r = 32767;
        if (r < -32768)
            r = -32768;
        write_u16_le(f, (uint16_t)(int16_t)l);
        write_u16_le(f, (uint16_t)(int16_t)r);
    }

    fclose(f);
    return 0;
}

/* Goertzel magnitude^2 of one frequency for x[0..n). */
static double goertzel_power(const double* x, size_t n, double freq_hz, double rate)
{
    double coeff = 2.0 * cos(2.0 * M_PI * freq_hz / rate);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = 0; i < n; i++)
    {
        double s0 = x[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return s1 * s1 + s2 * s2 - coeff * s1 * s2;
}

/* Locate the dominant tone in [nominal, nominal*1.25] by plain Goertzel
 * grid argmax. Coarse by design: refine_fundamental() tightens it against
 * the least-squares residual afterwards. */
static bool find_fundamental(const double* x, size_t n, double rate, double nominal, double* out_hz)
{
    const int steps = 512;
    double best_f = 0.0, best_p = -1.0;
    for (int i = 0; i <= steps; i++)
    {
        double f = nominal * (1.0 + 0.25 * (double)i / steps);
        double p = goertzel_power(x, n, f, rate);
        if (p > best_p)
        {
            best_p = p;
            best_f = f;
        }
    }
    if (best_p <= 0.0 || best_f <= 0.0)
        return false;
    *out_hz = best_f;
    return true;
}

/* Least-squares fit DC + K harmonics of f0 onto x[0..n).
 * harm_power[k-1] gets harmonic k's fitted energy; returns residual energy
 * (total minus full model projection). Exact even when off-bin. */
static bool fit_harmonics(const double* x, size_t n, double f0, double rate, int K, double* harm_power, double* resid_power, double* dc_out)
{
    int P = 1 + 2 * K;
    double* gram = calloc((size_t)(P * P), sizeof(double));
    double* ata = calloc((size_t)(P * P), sizeof(double));
    double* rhs = calloc((size_t)P, sizeof(double));
    double* coef = calloc((size_t)P, sizeof(double));
    double* row = malloc(sizeof(double) * (size_t)P);
    if (!gram || !ata || !rhs || !coef || !row)
        goto fail;

    for (size_t i = 0; i < n; i++)
    {
        double t = (double)i / rate;
        row[0] = 1.0;
        for (int k = 1; k <= K; k++)
        {
            double w = 2.0 * M_PI * (double)k * f0 * t;
            row[2 * k - 1] = sin(w);
            row[2 * k] = cos(w);
        }
        for (int a = 0; a < P; a++)
        {
            rhs[a] += row[a] * x[i];
            for (int b = 0; b < P; b++)
                gram[a * P + b] += row[a] * row[b];
        }
    }

    memcpy(ata, gram, sizeof(double) * (size_t)(P * P));

    /* Gauss-Jordan solve on a copy so ata keeps the true Gram matrix and
     * rhs must be preserved separately for the projection below. */
    double* rhs_orig = malloc(sizeof(double) * (size_t)P);
    if (!rhs_orig)
        goto fail;
    memcpy(rhs_orig, rhs, sizeof(double) * (size_t)P);

    for (int col = 0; col < P; col++)
    {
        int piv = col;
        for (int r = col + 1; r < P; r++)
            if (fabs(gram[r * P + col]) > fabs(gram[piv * P + col]))
                piv = r;
        if (fabs(gram[piv * P + col]) < 1e-30)
        {
            free(rhs_orig);
            goto fail;
        }
        if (piv != col)
        {
            for (int c = 0; c < P; c++)
            {
                double tmp = gram[col * P + c];
                gram[col * P + c] = gram[piv * P + c];
                gram[piv * P + c] = tmp;
            }
            double tmp = rhs[col];
            rhs[col] = rhs[piv];
            rhs[piv] = tmp;
        }
        double d = gram[col * P + col];
        for (int c = col; c < P; c++)
            gram[col * P + c] /= d;
        rhs[col] /= d;
        for (int r = 0; r < P; r++)
        {
            if (r == col)
                continue;
            double fct = gram[r * P + col];
            if (fct == 0.0)
                continue;
            for (int c = col; c < P; c++)
                gram[r * P + c] -= fct * gram[col * P + c];
            rhs[r] -= fct * rhs[col];
        }
    }
    for (int a = 0; a < P; a++)
        coef[a] = rhs[a];

    /* For LS solutions: residual SSQ = total - coef . (X^T x). */
    double total = 0.0;
    for (size_t i = 0; i < n; i++)
        total += x[i] * x[i];
    double model_energy = 0.0;
    for (int a = 0; a < P; a++)
        model_energy += coef[a] * rhs_orig[a];

    double resid = total - model_energy;
    if (resid < 0.0)
        resid = 0.0;
    for (int k = 1; k <= K; k++)
        harm_power[k - 1] = 0.5 * (coef[2 * k - 1] * coef[2 * k - 1] + coef[2 * k] * coef[2 * k]) * (double)n;
    *resid_power = resid;
    *dc_out = coef[0];

    free(rhs_orig);
    free(gram);
    free(ata);
    free(rhs);
    free(coef);
    free(row);
    return true;

fail:
    free(gram);
    free(ata);
    free(rhs);
    free(coef);
    free(row);
    return false;
}

/* Tighten f0 by minimizing the harmonic-fit residual over a two-stage local
 * grid. The Goertzel argmax is biased by up to ~half a grid step, which over
 * multi-second windows decorrelates the fit; the residual criterion does not
 * suffer from that bias. */
static double refine_fundamental(const double* x, size_t n, double rate, double f0)
{
    double harm[HARMONICS], resid = 0.0, dc = 0.0;
    double best_f = f0, best_r = 1e300;
    static const double ranges[] = {0.15, 0.02};
    static const int counts[] = {31, 21};
    for (int stage = 0; stage < 2; stage++)
    {
        double span = ranges[stage];
        int count = counts[stage];
        best_f = f0;
        best_r = 1e300;
        for (int i = 0; i < count; i++)
        {
            double f = f0 - span + 2.0 * span * (double)i / (double)(count - 1);
            if (!fit_harmonics(x, n, f, rate, HARMONICS, harm, &resid, &dc))
                continue;
            if (resid < best_r)
            {
                best_r = resid;
                best_f = f;
            }
        }
        f0 = best_f;
    }
    return best_f;
}

static void print_usage(const char* exe)
{
    fprintf(stderr,
            "Usage: %s --output <file.wav> [options]\n"
            "\n"
            "Options:\n"
            "  --seconds N          Render length in seconds (default 3)\n"
            "  --sample-rate N      Host render rate (default 32768)\n"
            "  --block N            Host frames per block (default 256)\n"
            "  --pcm-mixer MODE     ipatix|sappy (default ipatix)\n"
            "  --polyphony N        Max PCM channels 1-15 (default 15)\n"
            "  --volume N           Song volume 0-127 (default 127)\n"
            "  --sample-hz F        WaveData playback rate (default 18157)\n",
            exe);
}

static bool parse_args(int argc, char** argv, Options* opt)
{
    opt->output = NULL;
    opt->seconds = 3;
    opt->sample_rate = 32768;
    opt->block = 256;
    opt->mixer = M4A_PCM_MIXER_IPATIX;
    opt->polyphony = 15;
    opt->volume = 127;
    opt->sample_hz = 18157;
    opt->table_len = SINE_TABLE_DEFAULT;
    opt->pcm_mix_rate = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            opt->output = argv[++i];
        else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc)
            opt->seconds = atoi(argv[++i]);
        else if (strcmp(argv[i], "--sample-rate") == 0 && i + 1 < argc)
            opt->sample_rate = atoi(argv[++i]);
        else if (strcmp(argv[i], "--block") == 0 && i + 1 < argc)
            opt->block = atoi(argv[++i]);
        else if (strcmp(argv[i], "--pcm-mixer") == 0 && i + 1 < argc)
        {
            const char* m = argv[++i];
            if (strcmp(m, "ipatix") == 0)
                opt->mixer = M4A_PCM_MIXER_IPATIX;
            else if (strcmp(m, "sappy") == 0)
                opt->mixer = M4A_PCM_MIXER_SAPPY;
            else
                return false;
        }
        else if (strcmp(argv[i], "--polyphony") == 0 && i + 1 < argc)
            opt->polyphony = atoi(argv[++i]);
        else if (strcmp(argv[i], "--volume") == 0 && i + 1 < argc)
            opt->volume = atoi(argv[++i]);
        else if (strcmp(argv[i], "--sample-hz") == 0 && i + 1 < argc)
            opt->sample_hz = (uint32_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--table-len") == 0 && i + 1 < argc)
            opt->table_len = atoi(argv[++i]);
        else if (strcmp(argv[i], "--antialias") == 0)
            opt->antialias = 1;
        else if (strcmp(argv[i], "--pcm-mix-rate") == 0 && i + 1 < argc)
            opt->pcm_mix_rate = atoi(argv[++i]);
        else
            return false;
    }

    return opt->output && opt->seconds > 0 && opt->sample_rate > 0 && opt->block > 0 && opt->polyphony > 0 && opt->polyphony <= M4A_MAX_PCM_CHANNELS && opt->volume >= 0 && opt->volume <= 127 && opt->sample_hz > 1000 && opt->table_len > 0 && opt->table_len <= SINE_TABLE_MAX;
}

/* Build one cycle of a pure unit sine quantised to signed 8-bit PCM. */
static void build_sine_table(int8_t* table, int len)
{
    for (int i = 0; i < len; i++)
    {
        double v = sin(2.0 * M_PI * (double)i / (double)len);
        table[i] = (int8_t)lround(v * 127.0);
    }
}
int main(int argc, char** argv)
{
    Options opt;
    if (!parse_args(argc, argv, &opt))
    {
        print_usage(argv[0]);
        return 2;
    }

    int8_t table[SINE_TABLE_MAX];
    build_sine_table(table, opt.table_len);

    static WaveData wav;
    wav.type = 0;
    wav.status = 0xC000; /* loop enabled */
    /* Engine convention: WaveData.freq is the sample rate in Hz * 1024
     * (see voicegroup_loader.c AIFF path: "freq = COMM rate * 1024"). */
    wav.freq = opt.sample_hz * 1024u;
    wav.loopStart = 0;
    wav.size = (uint32_t)opt.table_len;
    wav.data = table;

    static ToneData voices[VG_SIZE];
    memset(voices, 0, sizeof(voices));
    voices[0].type = VOICE_DIRECTSOUND;
    voices[0].key = 60;
    voices[0].panSweep = 0x40; /* centre */
    voices[0].wav = &wav;
    voices[0].attack = 0xFF;
    voices[0].decay = 0xFF;
    voices[0].sustain = 0xFF;
    voices[0].release = 0xFF;

    int frames = opt.seconds * opt.sample_rate;
    float* left = malloc(sizeof(float) * (size_t)frames);
    float* right = malloc(sizeof(float) * (size_t)frames);
    if (!left || !right)
    {
        fprintf(stderr, "out of memory\n");
        free(left);
        free(right);
        return 1;
    }

    M4ADriver* drv = m4a_driver_create((float)opt.sample_rate);
    HwAudio* hw = hw_audio_create((float)opt.sample_rate);
    if (!drv || !hw)
    {
        fprintf(stderr, "engine init failed\n");
        if (drv)
            m4a_driver_destroy(drv);
        if (hw)
            hw_audio_destroy(hw);
        free(left);
        free(right);
        return 1;
    }
    if (opt.pcm_mix_rate > 0)
        m4a_driver_set_pcm_mix_rate(drv, (float)opt.pcm_mix_rate);
    if (opt.antialias)
        hw_audio_set_resample_antialias(hw, 1);

    m4a_driver_set_pcm_mixer_mode(drv, opt.mixer);
    m4a_set_max_pcm_channels(drv, (uint8_t)opt.polyphony);
    m4a_driver_set_voicegroup(drv, voices);
    m4a_program_change(drv, 0, 0);
    m4a_cc(drv, 0, 7, (uint8_t)opt.volume);
    m4a_cc(drv, 0, 10, 64);
    m4a_note_on(drv, 0, 60, 127);
    uint64_t timer_events = 0, fifo_a_words = 0;
    const M4APcmRing* ring0 = m4a_get_pcm_ring(drv);
    uint64_t wc0 = ring0 ? ring0->write_cursor : 0;

    for (int done = 0; done < frames;)
    {
        int n = frames - done < opt.block ? frames - done : opt.block;
        m4a_advance(drv, n);
        const M4ARegWriteBatch* batch = m4a_get_pending_writes(drv);
        for (size_t e = 0; batch && e < batch->count; e++)
        {
            if (batch->events[e].reg == M4A_REG_TIMER_0)
                timer_events++;
            else if (batch->events[e].reg == M4A_REG_FIFO_A)
                fifo_a_words++;
        }
        hw_audio_render_events(hw, batch, left + done, right + done, n);
        m4a_consume_writes(drv);
        done += n;
    }

    const M4APcmRing* ring1 = m4a_get_pcm_ring(drv);
    double secs = (double)frames / opt.sample_rate;
    fprintf(stderr, "diag timer0/s=%.3f fifo_words/s=%.3f ring_produced/s=%.3f samples_per_vblank=%u\n",
            (double)timer_events / secs, (double)fifo_a_words / secs,
            ring1 ? (double)(ring1->write_cursor - wc0) / secs : -1.0,
            ring1 ? ring1->pcm_samples_per_vblank : 0u);

    /* Dump the last second of raw mixer ring output (int8 at the canonical
     * 13379 Hz mix rate) so distortion can be localized to the driver vs
     * the hw_audio FIFO/DAC chain. */
    if (ring1)
    {
        FILE* rf = fopen("/tmp/sine_ring.raw", "wb");
        if (rf)
        {
            uint64_t total = ring1->write_cursor;
            uint64_t start = total > 13379 ? total - 13379 : 0;
            for (uint64_t i = start; i < total; i++)
            {
                int8_t v = ring1->ring_a[i % ring1->pcm_dma_buf_size];
                fwrite(&v, 1, 1, rf);
            }
            fclose(rf);
        }
    }

    m4a_all_sound_off(drv);
    hw_audio_destroy(hw);
    m4a_driver_destroy(drv);

    if (write_wav(opt.output, left, right, (uint64_t)frames, opt.sample_rate) != 0)
    {
        fprintf(stderr, "cannot write %s\n", opt.output);
        free(left);
        free(right);
        return 1;
    }

    /* ---- Analysis on the left channel steady-state window ---- */
    size_t skip = (size_t)(opt.sample_rate / 4);
    size_t win = (size_t)frames - 2 * skip;
    double* x = malloc(sizeof(double) * win);
    if (!x || win < 1024)
    {
        fprintf(stderr, "window too small\n");
        free(x);
        free(left);
        free(right);
        return 1;
    }
    double mean = 0.0;
    for (size_t i = 0; i < win; i++)
    {
        x[i] = left[skip + i];
        mean += x[i];
    }
    mean /= (double)win;
    double total = 0.0;
    for (size_t i = 0; i < win; i++)
    {
        x[i] -= mean;
        total += x[i] * x[i];
    }

    double nominal = (double)opt.sample_hz / (double)opt.table_len;
    double f0 = 0.0;
    if (!find_fundamental(x, win, (double)opt.sample_rate, nominal, &f0))
    {
        fprintf(stderr, "no dominant tone found\n");
        free(x);
        free(left);
        free(right);
        return 1;
    }
    f0 = refine_fundamental(x, win, (double)opt.sample_rate, f0);

    double harm_power[HARMONICS];
    double resid = 0.0, dc = 0.0;
    if (!fit_harmonics(x, win, f0, (double)opt.sample_rate, HARMONICS, harm_power, &resid, &dc))
    {
        fprintf(stderr, "harmonic fit failed\n");
        free(x);
        free(left);
        free(right);
        return 1;
    }

    double fund = harm_power[0];
    if (fund <= 0.0 || total <= 0.0 || fund / total < 1e-6)
    {
        fprintf(stderr, "signal too weak or not tonal\n");
        free(x);
        free(left);
        free(right);
        return 1;
    }

    double dist = 0.0;
    for (int k = 1; k < HARMONICS; k++)
        dist += harm_power[k];
    dist += resid;

    /* Diagnostic breakdown on stderr: per-harmonic and residual share,
     * relative to the fundamental. Not part of the METRIC contract. */
    for (int k = 1; k < HARMONICS; k++)
        fprintf(stderr, "diag h%d=%.2f dB\n", k + 1, 10.0 * log10(harm_power[k] / fund));
    fprintf(stderr, "diag residual=%.2f dB\n", 10.0 * log10(resid / fund));
    fprintf(stderr, "diag total=%.2f dB\n", 10.0 * log10(dist / fund));

    printf("METRIC thd_db=%.6f\n", 10.0 * log10(dist / fund));
    printf("METRIC purity_pct=%.4f\n", 100.0 * fund / total);
    printf("METRIC fundamental_hz=%.3f\n", f0);
    printf("METRIC nominal_hz=%.3f\n", nominal);

    free(x);
    free(left);
    free(right);
    return 0;
}
