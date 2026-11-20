// Spilled pages (MKW_MEM_SPILL): game memory that only the runtime reads, and seldom, leaves RAM for a file of our
// own and comes back when something touches it. The case is the NVN pools without CPU access the GPU never uses
// directly (vk_mem_spill.c picks them): their bytes are the game's textures, which the runtime reads only to upload a
// texture cut from them, and they held ~0.8 GB of a race's peak (ram4b). The GPU cannot hand those bytes back (an
// ASTC texture is a decode of them, an evicted one is gone), so they go to the file.
// A spilled page is inaccessible (trapped == 4) and its bytes are in its slot of the file (slot 0: the page was all
// zero, nothing written); the first access to one, a read or a write, by the game or a runtime thread, fills it
// (plat_mem_poke writes through the protection, so no other thread sees it before it holds them) and the spilled pages
// right after it, then they are read-write again. Every path that thaws fills the pages it covers first, as with the
// read traps (memory.c). Under frz_lock. Linux only: it needs the trap map (mem_trap_init) and a scratch file. Off while
// capturing (MKW_CAPTURE): the capture reads guest pages holding frz_lock (cap_mem.c mem_hold_layout), and a spilled
// page's fault would wait on that same lock.
// The write watch (plat_mem_written) must not see a spill or a fill: dropping a written page loses its written mark,
// so a run with a write not yet collected is not spilled (vk_mem_spill.c collects them just before); the fill's own
// write through the protection marks the pages, so it clears those marks (nothing else could write them meanwhile).
// Without a write watch the texture checks hash the storage at every submission, which would fill it back each time:
// off there.
#include <string.h>

#include "cfg.h"
#include "host.h"
#include "memory_int.h"

#define PAGE 0x1000ull
#define FILL_PAGES 64     // a fault fills up to this many spilled pages from it on: readers go through a texture in order
#define CHUNK_PAGES 512   // pages spilled per frz_lock hold: the game's faults wait no longer than that

static int on = -1;
static PlatFile *file;
static uint32_t *slot;      // frz_lock: per guest page, its slot in the file (0 none); kept when the page is filled, so a
                            // page spilled again writes the same place
static uint32_t nslots;     // frz_lock: slots handed out (slot k is at (k - 1) * PAGE)
static int64_t nspilled;    // frz_lock: pages spilled now
static volatile int64_t counts[4];   // pages spilled, of them all zero, filled by an access, filled by a thaw
static uint8_t buf[FILL_PAGES * PAGE];   // frz_lock

int mem_spill_on(void) {
    if (on >= 0) return on;
    on = 0;
    if (!rt_knob("MEM_SPILL", "1")) return 0;
    const char *cap = cfg_get("MKW_CAPTURE");
    if (cap && *cap && strcmp(cap, "0")) {
        rt_log("mem: MKW_MEM_SPILL is off while capturing (MKW_CAPTURE)");
        return 0;
    }
    char dir[4096];
    char *sep = plat_exe_path(dir, sizeof dir) ? plat_last_sep(dir) : NULL;
    if (sep) *sep = 0;
    uint64_t probe;
    if (plat_mem_written(mem_layout()->heap.base, PAGE, &probe, 1, 0) == UINT64_MAX) {
        rt_log("mem: MKW_MEM_SPILL is off without a write watch (texture checks would read it all back)");
        return 0;
    }
    if (!mem_trap_init() || !sep || !(slot = plat_mem_alloc(RT_ADDRESS_LIMIT / PAGE * sizeof *slot)) ||
        !(file = plat_file_scratch(dir))) {
        rt_log("mem: MKW_MEM_SPILL unavailable here (needs the trap map and a scratch file beside the executable)");
        return 0;
    }
    rt_log("mem: spilling pools only the runtime reads to a file in %s (MKW_MEM_SPILL)", dir);
    return on = 1;
}

// frz_lock held: the spilled page of p and those right after it, up to FILL_PAGES, back in memory and read-write.
void mem_spill_fill_locked(uint64_t p, int why) {
    p &= ~(PAGE - 1);
    uint64_t n = 1;
    while (n < FILL_PAGES && trapped[p / PAGE + n] == 4) n++;
    for (uint64_t i = 0; i < n;) {   // runs of consecutive slots read at once; zero pages were dropped and read zero
        uint32_t s = slot[p / PAGE + i];
        uint64_t k = 1;
        while (s && i + k < n && slot[p / PAGE + i + k] == s + k) k++;
        if (s && (plat_file_pread(file, buf, (uint32_t)(k * PAGE), (uint64_t)(s - 1) * PAGE) != (int64_t)(k * PAGE) ||
                  !plat_mem_poke(p + i * PAGE, buf, k * PAGE)))
            rt_fatal("memory: filling the spilled page %#llx failed", (unsigned long long)(p + i * PAGE));
        i += k;
    }
    uint64_t marks[FILL_PAGES];
    plat_mem_written(p, n * PAGE, marks, FILL_PAGES, 1);   // the poke's own marks
    if (!plat_mem_protect(p, n * PAGE, PLAT_PROT_RW))
        rt_fatal("memory: giving back the spilled pages %#llx+%#llx failed", (unsigned long long)p,
                 (unsigned long long)(n * PAGE));
    memset(trapped + p / PAGE, 0, n);
    nspilled -= (int64_t)n;
    plat_add64(&counts[why == 1 ? 2 : 3], (int64_t)n);
}

static int zero_page(const uint8_t *q) {
    const uint64_t *w = (const uint64_t *)q;
    for (int i = 0; i < (int)(PAGE / 8); i++)
        if (w[i]) return 0;
    return 1;
}

// frz_lock held: the read-write run [a, b) with no trap spilled; 0 when the file refused it (nothing changed).
static int spill_run_locked(uint64_t a, uint64_t b) {
    memset(trapped + a / PAGE, 4, (b - a) / PAGE);   // before the protection: a write fault in between finds it
    uint64_t w;
    if (!plat_mem_protect(a, b - a, PLAT_PROT_READ) || plat_mem_written(a, b - a, &w, 1, 0)) {
        if (!plat_mem_protect(a, b - a, PLAT_PROT_RW)) rt_fatal("memory: undoing a spill at %#llx failed",
                                                               (unsigned long long)a);
        memset(trapped + a / PAGE, 0, (b - a) / PAGE);
        return 0;
    }
    int64_t zeros = 0;
    for (uint64_t q = a; q < b; q += PAGE) {
        const uint8_t *src = (const uint8_t *)(uintptr_t)q;
        uint32_t *s = &slot[q / PAGE];
        if (zero_page(src)) {
            *s = 0, zeros++;   // a slot it had stays unused: the file may hold a stale copy there, never read
            continue;
        }
        if (!*s) *s = ++nslots;
        if (plat_file_pwrite(file, src, PAGE, (uint64_t)(*s - 1) * PAGE) != (int64_t)PAGE) {
            if (!plat_mem_protect(a, b - a, PLAT_PROT_RW)) rt_fatal("memory: undoing a spill at %#llx failed",
                                                                   (unsigned long long)a);
            memset(trapped + a / PAGE, 0, (b - a) / PAGE);
            return 0;
        }
    }
    if (!plat_mem_protect(a, b - a, PLAT_PROT_NONE) || !plat_mem_drop(a, b - a))
        rt_fatal("memory: spilling %#llx+%#llx failed", (unsigned long long)a, (unsigned long long)(b - a));
    nspilled += (int64_t)((b - a) / PAGE);
    plat_add64(&counts[0], (int64_t)((b - a) / PAGE));
    plat_add64(&counts[1], zeros);
    return 1;
}

uint64_t mem_spill(uint64_t lo, uint64_t hi) {
    lo = (lo + PAGE - 1) & ~(PAGE - 1), hi &= ~(PAGE - 1);
    if (lo >= hi || !mem_spill_on() || !watched(lo, hi - lo)) return 0;
    uint64_t done = 0;
    for (uint64_t c = lo; c < hi; c += CHUNK_PAGES * PAGE) {
        uint64_t ce = hi - c > CHUNK_PAGES * PAGE ? c + CHUNK_PAGES * PAGE : hi;
        plat_mutex_lock(&frz_lock);
        mem_thaw_regions_locked(c, ce - c);   // a frozen region is read-only as a whole: its thaw would undo the spill
        for (uint64_t x = c, y; x < ce; x = y) {
            if (plat_mem_state(x, ce, &y) != PLAT_PROT_RW) continue;
            for (uint64_t a = x, b; a < y; a = b) {   // runs of pages with no other trap
                while (a < y && trapped[a / PAGE]) a += PAGE;
                b = a;
                while (b < y && !trapped[b / PAGE]) b += PAGE;
                if (a < b && spill_run_locked(a, b)) done += b - a;
            }
        }
        plat_mutex_unlock(&frz_lock);
    }
    return done;
}

int64_t mem_spill_pages(void) { return __atomic_load_n(&nspilled, __ATOMIC_RELAXED); }

void mem_spill_stats(int64_t out[5]) {
    out[0] = mem_spill_pages();
    for (int i = 0; i < 4; i++) out[i + 1] = __atomic_load_n(&counts[i], __ATOMIC_RELAXED);
}
