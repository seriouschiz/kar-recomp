// --probe-spill (MKW's own; MKW_MEM_SPILL, on by default on Linux): spilled pages (core/memory/memory_spill.c) on 8 MiB of the
// runtime's own guest memory, pages with a pattern and all-zero ones. Each step checks every byte: spilled, the RAM is
// given back; read back on this thread (faults, the fill ahead); a write into a spilled page keeps the page's other
// bytes; a thaw (mem_thaw) fills what it covers; four threads read their quarters at once; spilled twice, the same
// bytes come back.
#include <stdio.h>

#include "host.h"
#include "platform/platform.h"

#define PAGE 4096ull
#define SIZE (8ull << 20)

static int fails;

static uint8_t want(uint64_t off) {
    uint64_t pg = off / PAGE;
    return pg % 5 == 0 ? 0 : (uint8_t)(pg * 131 + off * 7 + 1);
}

static uint64_t check(uint64_t a, uint64_t lo, uint64_t hi) {   // bad bytes of [lo, hi)
    uint64_t bad = 0;
    for (uint64_t o = lo; o < hi; o++) bad += ((volatile uint8_t *)(uintptr_t)a)[o] != want(o);
    return bad;
}

static void expect(int ok, const char *what) {
    printf("%s spill: %s\n", ok ? "ok  " : "FAIL", what);
    fails += !ok;
}

typedef struct { uint64_t a, lo, hi, bad; } Part;
static void reader(void *p) {
    Part *x = p;
    x->bad = check(x->a, x->lo, x->hi);
}

static uint64_t spill(uint64_t a) {   // the write watch's marks taken first, as the sweep does (vk_mem_spill.c)
    static uint64_t pages[SIZE / PAGE];
    while (mem_written_pages(a, SIZE, pages, SIZE / PAGE) == SIZE / PAGE) {}
    return mem_spill(a, a + SIZE);
}

static int64_t spilled_now(void) {
    int64_t s[5];
    mem_spill_stats(s);
    return s[0];
}

int probe_spill(void) {
    if (!mem_spill_on()) {
        printf("probe-spill: spilling is off (MKW_MEM_SPILL=0, capturing, no write watch, or not Linux)\n");
        return 1;
    }
    uint64_t a = mem_runtime_alloc(SIZE, "spill probe");
    if (!a) return printf("probe-spill: no guest memory\n"), 1;
    for (uint64_t o = 0; o < SIZE; o++) ((uint8_t *)(uintptr_t)a)[o] = want(o);

    expect(spill(a) == SIZE && spilled_now() == (int64_t)(SIZE / PAGE), "the whole range spilled");
    int64_t in = plat_mem_resident((const void *)(uintptr_t)a, SIZE);
    expect(in == 0, "none of it in RAM after the spill");
    if (in) printf("     %lld bytes in RAM\n", (long long)in);
    expect(check(a, 0, SIZE) == 0 && spilled_now() == 0, "read back on this thread, every byte");
    static uint64_t marks[SIZE / PAGE];
    uint64_t nm = mem_written_pages(a, SIZE, marks, SIZE / PAGE);
    expect(nm == 0, "the fills left no write mark (no texture uploaded again for them)");
    if (nm) printf("     %llu pages marked\n", (unsigned long long)nm);

    spill(a);
    uint8_t *w = (uint8_t *)(uintptr_t)(a + 3 * PAGE + 100);
    *w = (uint8_t)(want(3 * PAGE + 100) ^ 0xff);
    int ok = *w == (uint8_t)(want(3 * PAGE + 100) ^ 0xff);
    *w = want(3 * PAGE + 100);
    expect(ok && check(a, 3 * PAGE, 4 * PAGE) == 0, "a write into a spilled page keeps its other bytes");
    mem_thaw(a, SIZE);
    expect(spilled_now() == 0 && check(a, 0, SIZE) == 0, "a thaw fills every page it covers");

    expect(spill(a) == SIZE, "spilled again after a write and a thaw");
    Part parts[4];
    PlatThread *t[4];
    for (int i = 0; i < 4; i++) {
        parts[i] = (Part){a, SIZE / 4 * i, SIZE / 4 * (i + 1), 0};
        t[i] = plat_thread_new(reader, &parts[i], "spill probe");
    }
    uint64_t bad = 0;
    for (int i = 0; i < 4; i++) plat_thread_join(t[i]), bad += parts[i].bad;
    expect(bad == 0 && spilled_now() == 0, "four threads read their quarters at once");

    int64_t s[5];
    mem_spill_stats(s);
    printf("     totals: spilled %lld pages (all-zero %lld), filled by an access %lld, by a thaw %lld\n",
           (long long)s[1], (long long)s[2], (long long)s[3], (long long)s[4]);
    printf("probe-spill: %s\n", fails ? "FAILED" : "passed");
    return fails ? 1 : 0;
}
