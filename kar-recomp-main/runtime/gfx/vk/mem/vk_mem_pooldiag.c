// MKW_MEM_POOL_LABELS=1 (diagnosis, with the labels of nvn_api_core.c pool_label): which of the game's memory behind
// NVN pools sits in RAM twice. Where pools are shadowed (Linux), a pool's GPU storage is a host-visible copy of its
// pages, so the bytes are in the game's pages and in the copy. A CPU_NO_ACCESS pool's pages are read by the runtime
// only to fill that copy (shadow_pool), to upload a texture cut from it (vk_tex_upload.c) and to take the GPU's writes
// at their fence (vk_gpu_written); if fills and uploads are all there is, the game's pages could go once copied.
// Logged every 300 frames (vk_mem_census): per kind of pool, storage, shadow copies and the game's bytes in RAM
// (mincore); then those CPU_NO_ACCESS reads and write-backs so far. Off by default: mincore over every pool.
#include "cfg.h"
#include "host.h"
#include "nvn_api.h"
#include "nvn_pool_index.h"
#include "vk.h"
#include "vk_mem_internal.h"
#include "platform/platform.h"

static int on = -1;
static void noacc_log(void);
static int64_t noacc[4][2];   // shadow fills, texture uploads, fence write-backs, ASTC texture uploads: count, bytes

int vk_pool_diag_on(void) {
    if (on < 0) {
        const char *e = cfg_get("MKW_MEM_POOL_LABELS");
        on = e && *e && *e != '0';
    }
    return on;
}

void vk_pool_diag_note(uint32_t flags, int what, uint64_t bytes) {
    if (!vk_pool_diag_on() || !(flags & NVN_MEMORY_POOL_FLAGS_CPU_NO_ACCESS) || what < 0 || what > 3) return;
    int64_t k = __atomic_add_fetch(&noacc[what][0], 1, __ATOMIC_RELAXED);
    __atomic_add_fetch(&noacc[what][1], (int64_t)bytes, __ATOMIC_RELAXED);
    if (what == 1 && k % 128 == 0) noacc_log();   // a capture's replay has fewer frames than the census's 300
}

void vk_pool_diag_log(uint64_t frame) {
    if (!vk_pool_diag_on()) return;
    typedef struct { uint32_t flags; uint64_t n, bytes, shadowed, nshadow, nlazy; int64_t resident, lazy_resident; } Group;
    Group g[32];
    int ng = 0;
    plat_mutex_lock_shared(&pool_lock);
    for (int i = 0; i < pool_index.n; i++) {
        const PoolRec *r = pool_index.e[i].buf;
        if (r->kind != POOL_IMPORT) continue;
        int j = 0;
        while (j < ng && g[j].flags != r->flags) j++;
        if (j == ng && ng < 32) g[ng++] = (Group){r->flags, 0, 0, 0, 0, 0, 0, 0};
        if (j == ng) continue;
        g[j].n++, g[j].bytes += r->size;
        if (r->smap) g[j].nshadow++, g[j].shadowed += r->import_hi - r->import_lo;
        uint64_t lo = r->lo & ~0xfffull, hi = (r->lo + r->size + 0xfff) & ~0xfffull;
        int64_t in = plat_mem_resident((const void *)(uintptr_t)lo, hi - lo);
        if (in > 0) g[j].resident += in;
        if (r->lazy) g[j].nlazy++, g[j].lazy_resident += in > 0 ? in : 0;   // no GPU storage yet: what a spill could take
    }
    plat_mutex_unlock_shared(&pool_lock);
    for (int j = 0; j < ng; j++)
        rt_log("mem: pools %#x at frame %llu: %llu pools %.1f MiB, %llu shadowed %.1f MiB, game's pages in RAM %.1f MiB; "
               "%llu never resolved by the GPU (lazy), %.1f MiB of them in RAM",
               g[j].flags, (unsigned long long)frame, (unsigned long long)g[j].n, g[j].bytes / 1048576.0,
               (unsigned long long)g[j].nshadow, g[j].shadowed / 1048576.0, g[j].resident / 1048576.0,
               (unsigned long long)g[j].nlazy, g[j].lazy_resident / 1048576.0);
    noacc_log();
}

static void noacc_log(void) {
    rt_log("mem: CPU_NO_ACCESS pool storage so far: shadow fills %lld (%.1f MiB), texture uploads %lld (%.1f MiB), "
           "fence write-backs %lld (%.1f MiB); of the uploads, ASTC (the GPU holds a decode, not these bytes) %lld (%.1f MiB)",
           (long long)noacc[0][0], noacc[0][1] / 1048576.0, (long long)noacc[1][0], noacc[1][1] / 1048576.0,
           (long long)noacc[2][0], noacc[2][1] / 1048576.0, (long long)noacc[3][0], noacc[3][1] / 1048576.0);
}
