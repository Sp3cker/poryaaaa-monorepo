/*
 * poryaaaa_project_bench: times the native half of Porydaw's project scan.
 *
 * Porydaw opens one VoicegroupProject per project (voicegroup_project_open
 * with a NULL config) and loads each song's bank with voicegroup_project_load
 * on an absolute file target. This binary repeats that sequence against a
 * real decomp checkout and reports per-stage and per-bank timings.
 *
 * Asset reads go through a batch adapter shaped like Porydaw's FileIo.swift:
 * whole-file reads on up to --io-threads threads started per batch, relative
 * paths resolved against the project root, and unreadable files reported as
 * soft misses. It reads each file once; Porydaw's Swift adapter additionally
 * copies every blob twice, which this benchmark does not measure.
 */
#include "voicegroup_loader.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

enum
{
    BENCH_PATH_LEN = PATH_MAX,
    BENCH_SECTION_LEN = 256,
    BENCH_MAX_IO_THREADS = 64,
};

typedef struct
{
    char path[BENCH_PATH_LEN];
    char section[BENCH_SECTION_LEN];
    char label[BENCH_PATH_LEN];
} BenchBank;

typedef struct
{
    char projectRoot[BENCH_PATH_LEN];
    int runs;
    int ioThreads;
    int top;
    bool openOnly;
    BenchBank* banks;
    int bankCount;
    int bankCapacity;
} BenchOptions;

/* Adapter-side I/O totals for one run; atomics because readers run in parallel. */
typedef struct
{
    atomic_size_t batches;
    atomic_size_t files;
    atomic_size_t misses;
    atomic_size_t bytes;
} BenchIoStats;

typedef struct
{
    const BenchOptions* options;
    BenchIoStats stats;
} BenchReader;

typedef struct
{
    BenchReader* reader;
    const char* const* paths;
    VoicegroupFileBlob* out;
    size_t count;
    atomic_size_t next;
    atomic_bool hardFailure;
} BenchReadBatch;

/* Monotonic nanoseconds; wall-clock adjustments must not skew stage timings. */
static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* Parse a positive integer option and reject partial strings. */
static bool parse_positive_int(const char* text, int* out)
{
    char* end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value <= 0 || value > INT_MAX)
        return false;
    *out = (int)value;
    return true;
}

/* Keep usage beside the parser so a mistyped profiling run fails early. */
static void print_usage(const char* exe)
{
    fprintf(stderr,
            "usage: %s --project PATH [--runs N] [--io-threads N] [--top N]\n"
            "          [--bank FILE[#SECTION]]... [--open-only]\n"
            "\n"
            "  --project     decomp checkout root (required)\n"
            "  --runs        measured iterations; run 0 is reported as cold (default 10)\n"
            "  --io-threads  asset-read threads per batch, Porydaw uses 4 (default 4)\n"
            "  --top         slowest banks to list (default 10)\n"
            "  --bank        bank target, absolute or project-relative; repeatable.\n"
            "                Default: every sound/voicegroups/*.inc as a whole file.\n"
            "  --open-only   time only voicegroup_project_open (profiling loop)\n",
            exe);
}

/* Append one bank target, growing the array geometrically. */
static bool add_bank(BenchOptions* opt, const char* path, const char* section, const char* label)
{
    if (opt->bankCount == opt->bankCapacity)
    {
        int capacity = opt->bankCapacity ? opt->bankCapacity * 2 : 64;
        BenchBank* grown = realloc(opt->banks, (size_t)capacity * sizeof(BenchBank));
        if (!grown)
            return false;
        opt->banks = grown;
        opt->bankCapacity = capacity;
    }
    BenchBank* bank = &opt->banks[opt->bankCount];
    if (snprintf(bank->path, sizeof(bank->path), "%s", path) >= (int)sizeof(bank->path) ||
        snprintf(bank->section, sizeof(bank->section), "%s", section) >= (int)sizeof(bank->section) ||
        snprintf(bank->label, sizeof(bank->label), "%s", label) >= (int)sizeof(bank->label))
        return false;
    opt->bankCount++;
    return true;
}

/* Resolve one --bank argument (FILE or FILE#SECTION) against the project root. */
static bool add_bank_argument(BenchOptions* opt, const char* argument)
{
    char file[BENCH_PATH_LEN];
    if (snprintf(file, sizeof(file), "%s", argument) >= (int)sizeof(file))
        return false;
    const char* section = "";
    char* hash = strrchr(file, '#');
    if (hash)
    {
        *hash = '\0';
        section = hash + 1;
    }
    char path[BENCH_PATH_LEN];
    int written = file[0] == '/' ? snprintf(path, sizeof(path), "%s", file)
                                 : snprintf(path, sizeof(path), "%s/%s", opt->projectRoot, file);
    if (written >= (int)sizeof(path))
        return false;
    return add_bank(opt, path, section, argument);
}

/* qsort comparator: stable bank order makes runs comparable across builds. */
static int compare_names(const void* a, const void* b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

/* Default targets: each top-level voicegroup file, which is how Porydaw loads split layouts. */
static bool add_default_banks(BenchOptions* opt)
{
    char dirPath[BENCH_PATH_LEN];
    if (snprintf(dirPath, sizeof(dirPath), "%s/sound/voicegroups", opt->projectRoot) >= (int)sizeof(dirPath))
        return false;
    DIR* dir = opendir(dirPath);
    if (!dir)
    {
        fprintf(stderr, "cannot open %s; pass --bank targets for this layout\n", dirPath);
        return false;
    }
    char** names = NULL;
    size_t count = 0, capacity = 0;
    bool ok = true;
    struct dirent* entry;
    while (ok && (entry = readdir(dir)) != NULL)
    {
        size_t length = strlen(entry->d_name);
        if (entry->d_name[0] == '.' || length < 4 || strcmp(entry->d_name + length - 4, ".inc") != 0)
            continue;
        char path[BENCH_PATH_LEN];
        struct stat st;
        if (snprintf(path, sizeof(path), "%s/%s", dirPath, entry->d_name) >= (int)sizeof(path) ||
            stat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;
        if (count == capacity)
        {
            capacity = capacity ? capacity * 2 : 256;
            char** grown = realloc(names, capacity * sizeof(char*));
            if (!grown)
            {
                ok = false;
                break;
            }
            names = grown;
        }
        names[count] = strdup(entry->d_name);
        ok = names[count] != NULL;
        count += ok;
    }
    closedir(dir);
    qsort(names, count, sizeof(char*), compare_names);
    for (size_t i = 0; ok && i < count; i++)
    {
        char path[BENCH_PATH_LEN];
        snprintf(path, sizeof(path), "%s/%s", dirPath, names[i]);
        ok = add_bank(opt, path, "", names[i]);
    }
    for (size_t i = 0; i < count; i++)
        free(names[i]);
    free(names);
    if (ok && opt->bankCount == 0)
    {
        fprintf(stderr, "no .inc files in %s; pass --bank targets\n", dirPath);
        return false;
    }
    return ok;
}

/* Convert CLI strings to options without accepting unknown knobs. */
static bool parse_args(int argc, char** argv, BenchOptions* opt)
{
    *opt = (BenchOptions){.runs = 10, .ioThreads = 4, .top = 10};
    const char* project = NULL;
    for (int i = 1; i < argc; i++)
    {
        const char* arg = argv[i];
        const char* value = i + 1 < argc ? argv[i + 1] : NULL;
        if (strcmp(arg, "--open-only") == 0)
        {
            opt->openOnly = true;
            continue;
        }
        if (!value)
            return false;
        i++;
        if (strcmp(arg, "--project") == 0)
            project = value;
        else if (strcmp(arg, "--runs") == 0 && parse_positive_int(value, &opt->runs))
            continue;
        else if (strcmp(arg, "--io-threads") == 0 && parse_positive_int(value, &opt->ioThreads) &&
                 opt->ioThreads <= BENCH_MAX_IO_THREADS)
            continue;
        else if (strcmp(arg, "--top") == 0 && parse_positive_int(value, &opt->top))
            continue;
        else if (strcmp(arg, "--bank") != 0)
            return false;
    }
    /* Porydaw passes a standardized absolute root; match it before resolving banks. */
    if (!project || !realpath(project, opt->projectRoot))
        return false;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--bank") == 0 && !add_bank_argument(opt, argv[++i]))
            return false;
    }
    return opt->openOnly || opt->bankCount > 0 || add_default_banks(opt);
}

/* Read one whole file; any open/read failure is a soft miss, as in FileIo.swift. */
static bool read_whole_file(const char* path, VoicegroupFileBlob* blob)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return true;
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode))
    {
        close(fd);
        return true;
    }
    size_t size = (size_t)st.st_size;
    uint8_t* data = malloc(size ? size : 1);
    if (!data)
    {
        close(fd);
        return false;
    }
    size_t done = 0;
    while (done < size)
    {
        ssize_t got = read(fd, data + done, size - done);
        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0)
            break;
        done += (size_t)got;
    }
    close(fd);
    if (done != size)
    {
        free(data);
        return true;
    }
    *blob = (VoicegroupFileBlob){.data = data, .size = size, .found = true};
    return true;
}

/* Claim batch entries until none remain; mirrors FileReadBatch.runWorker. */
static void* read_worker(void* arg)
{
    BenchReadBatch* batch = arg;
    const char* root = batch->reader->options->projectRoot;
    for (;;)
    {
        size_t index = atomic_fetch_add(&batch->next, 1);
        if (index >= batch->count)
            return NULL;
        const char* requested = batch->paths[index];
        char path[BENCH_PATH_LEN];
        if (requested[0] == '/')
            snprintf(path, sizeof(path), "%s", requested);
        else if (snprintf(path, sizeof(path), "%s/%s", root, requested) >= (int)sizeof(path))
            continue;
        if (!read_whole_file(path, &batch->out[index]))
        {
            atomic_store(&batch->hardFailure, true);
            continue;
        }
        BenchIoStats* stats = &batch->reader->stats;
        atomic_fetch_add(&stats->files, 1);
        if (batch->out[index].found)
            atomic_fetch_add(&stats->bytes, batch->out[index].size);
        else
            atomic_fetch_add(&stats->misses, 1);
    }
}

/* VoicegroupReadBatchFn: start up to N readers per batch and wait for all of them. */
static bool bench_read_batch(
    void* user, const char* const* paths, size_t count, VoicegroupFileBlob* out, char* error, size_t errorCapacity)
{
    BenchReader* reader = user;
    if (error && errorCapacity > 0)
        error[0] = '\0';
    if (count == 0)
        return true;
    for (size_t i = 0; i < count; i++)
    {
        out[i] = (VoicegroupFileBlob){0};
        if (!paths[i] || paths[i][0] == '\0')
        {
            snprintf(error, errorCapacity, "Voicegroup file-read batch contains an empty path.");
            return false;
        }
    }
    atomic_fetch_add(&reader->stats.batches, 1);
    BenchReadBatch batch = {.reader = reader, .paths = paths, .out = out, .count = count};
    atomic_init(&batch.next, 0);
    atomic_init(&batch.hardFailure, false);

    size_t workers = count < (size_t)reader->options->ioThreads ? count : (size_t)reader->options->ioThreads;
    pthread_t threads[BENCH_MAX_IO_THREADS];
    size_t started = 0;
    if (workers > 1)
    {
        for (; started < workers; started++)
        {
            if (pthread_create(&threads[started], NULL, read_worker, &batch) != 0)
                break;
        }
    }
    if (started == 0)
        read_worker(&batch);
    for (size_t i = 0; i < started; i++)
        pthread_join(threads[i], NULL);

    if (atomic_load(&batch.hardFailure))
    {
        snprintf(error, errorCapacity, "Out of memory reading a voicegroup asset.");
        return false;
    }
    return true;
}

/* VoicegroupReleaseBatchFn: free every populated blob and leave the slots zeroed. */
static void bench_release_batch(void* user, VoicegroupFileBlob* blobs, size_t count)
{
    (void)user;
    for (size_t i = 0; i < count; i++)
    {
        free(blobs[i].data);
        blobs[i] = (VoicegroupFileBlob){0};
    }
}

/* qsort comparator for the order statistics below. */
static int compare_u64(const void* a, const void* b)
{
    uint64_t x = *(const uint64_t*)a, y = *(const uint64_t*)b;
    return (x > y) - (x < y);
}

typedef struct
{
    double median;
    double min;
    double max;
} BenchStats;

/* Median/min/max in milliseconds of `count` samples spaced `stride` apart. */
static BenchStats summarize(const uint64_t* samples, int count, int stride, uint64_t* scratch)
{
    for (int i = 0; i < count; i++)
        scratch[i] = samples[(size_t)i * (size_t)stride];
    qsort(scratch, (size_t)count, sizeof(uint64_t), compare_u64);
    double median =
        count % 2 ? (double)scratch[count / 2] : ((double)scratch[count / 2 - 1] + (double)scratch[count / 2]) / 2.0;
    return (BenchStats){median / 1e6, (double)scratch[0] / 1e6, (double)scratch[count - 1] / 1e6};
}

typedef struct
{
    int bank;
    double median;
} BankRank;

/* Sort banks slowest-first by warm median. */
static int compare_rank(const void* a, const void* b)
{
    double x = ((const BankRank*)a)->median, y = ((const BankRank*)b)->median;
    return (x < y) - (x > y);
}

int main(int argc, char** argv)
{
    BenchOptions opt;
    if (!parse_args(argc, argv, &opt))
    {
        print_usage(argv[0]);
        free(opt.banks);
        return 2;
    }

    BenchReader reader = {.options = &opt};
    const VoicegroupFileIo io = {.user = &reader, .readBatch = bench_read_batch, .releaseBatch = bench_release_batch};
    const int runs = opt.runs, banks = opt.openOnly ? 0 : opt.bankCount;
    const int scratchCount = runs > banks ? runs : banks;

    /* Row-major [run][stage]: open, all banks, then one column per bank. */
    const int columns = 2 + banks;
    uint64_t* samples = calloc((size_t)runs * (size_t)columns, sizeof(uint64_t));
    uint64_t* scratch = calloc((size_t)(scratchCount > 0 ? scratchCount : 1), sizeof(uint64_t));
    int* failures = calloc((size_t)(banks > 0 ? banks : 1), sizeof(int));
    if (!samples || !scratch || !failures)
    {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    size_t batches = 0, files = 0, misses = 0, bytes = 0;
    for (int run = 0; run < runs; run++)
    {
        atomic_init(&reader.stats.batches, 0);
        atomic_init(&reader.stats.files, 0);
        atomic_init(&reader.stats.misses, 0);
        atomic_init(&reader.stats.bytes, 0);
        uint64_t* row = &samples[(size_t)run * (size_t)columns];

        uint64_t start = now_ns();
        VoicegroupProject* project = voicegroup_project_open(opt.projectRoot, NULL, &io);
        row[0] = now_ns() - start;
        if (!project)
        {
            fprintf(stderr, "voicegroup_project_open failed for %s\n", opt.projectRoot);
            return 1;
        }

        for (int b = 0; b < banks; b++)
        {
            const BenchBank* bank = &opt.banks[b];
            const VoicegroupTarget target = {.filePath = bank->path, .sectionLabel = bank->section};
            start = now_ns();
            LoadedVoiceGroup* vg = voicegroup_project_load(project, &target);
            row[2 + b] = now_ns() - start;
            row[1] += row[2 + b];
            if (vg)
                voicegroup_free(vg);
            else
                failures[b]++;
        }
        voicegroup_project_free(project);

        batches = atomic_load(&reader.stats.batches);
        files = atomic_load(&reader.stats.files);
        misses = atomic_load(&reader.stats.misses);
        bytes = atomic_load(&reader.stats.bytes);
    }

    /* Run 0 pays first-touch page-cache and allocator costs; keep it separate. */
    const int warmFirst = runs > 1 ? 1 : 0, warm = runs - warmFirst;
    printf("project   %s\n", opt.projectRoot);
    printf("runs      %d (cold: run 0; warm: runs %d-%d)\n", runs, warmFirst, runs - 1);
    printf("banks     %d   io-threads %d\n", banks, opt.ioThreads);
    printf("asset io  per run: %zu batches, %zu files, %zu misses, %.1f MiB\n\n",
           batches,
           files,
           misses,
           (double)bytes / (1024.0 * 1024.0));

    const char* stageNames[2] = {"open (discovery + symbol maps)", "bank loads (all targets)"};
    printf("%-32s %10s %10s %10s %10s\n", "stage", "cold ms", "warm med", "warm min", "warm max");
    for (int stage = 0; stage < (banks > 0 ? 2 : 1); stage++)
    {
        BenchStats s = summarize(&samples[(size_t)warmFirst * (size_t)columns + (size_t)stage], warm, columns, scratch);
        printf("%-32s %10.3f %10.3f %10.3f %10.3f\n",
               stageNames[stage],
               (double)samples[stage] / 1e6,
               s.median,
               s.min,
               s.max);
    }

    if (banks > 0)
    {
        BankRank* ranks = calloc((size_t)banks, sizeof(BankRank));
        if (!ranks)
            return 1;
        for (int b = 0; b < banks; b++)
        {
            BenchStats s =
                summarize(&samples[(size_t)warmFirst * (size_t)columns + 2 + (size_t)b], warm, columns, scratch);
            ranks[b] = (BankRank){b, s.median};
        }
        qsort(ranks, (size_t)banks, sizeof(BankRank), compare_rank);
        int shown = opt.top < banks ? opt.top : banks;
        printf("\nslowest banks (warm median ms)\n");
        for (int i = 0; i < shown; i++)
            printf("  %10.3f  %s%s\n",
                   ranks[i].median,
                   opt.banks[ranks[i].bank].label,
                   failures[ranks[i].bank] ? "  [load failed]" : "");
        int failed = 0;
        for (int b = 0; b < banks; b++)
            failed += failures[b] > 0;
        if (failed)
            printf("\n%d bank target(s) failed to load; their timings cover the failed attempt.\n", failed);
        free(ranks);
    }

    free(failures);
    free(scratch);
    free(samples);
    free(opt.banks);
    return 0;
}
