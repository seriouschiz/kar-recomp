// nn::ro diagnostics + stubs. Guest addresses are host addresses here.
#include "host.h"
#include "dispatch.h"

#define MAIN_BASE 0x7000000000ULL

static void dump_slot(uint64_t offset, const char *name) {
    uint64_t obj = *(uint64_t *)(uintptr_t)(MAIN_BASE + offset);
    rt_log("  %s at main+%#llx: first word = %#llx", name, (unsigned long long)offset, (unsigned long long)obj);
    if (!obj) return;
    for (int i = 0; i < 12; i++) {
        uint64_t fn = *(uint64_t *)(uintptr_t)(obj + (uint64_t)i * 8);
        if (!fn) continue;
        char buf[192];
        ldr_describe(fn, buf, sizeof buf);
        rt_log("    vtable[%d] = %#llx  %s", i, (unsigned long long)fn, buf);
    }
}

static void ov_ro_init(Cpu *c) {
    rt_log("=== nn::ro::Initialize ===");
    ret_x(c, 0);
}

static void ov_ro_get_buffer_size(Cpu *c) {
    size_t *out = arg_p(c, 0);
    const void *nro = arg_p(c, 1);
    rt_log("=== nn::ro::GetBufferSize(out=%p nro=%p) ===", (void*)out, nro);
    if (nro) {
        const uint8_t *h = (const uint8_t *)nro;
        rt_log("  nro[0..16] = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
               h[0],h[1],h[2],h[3],h[4],h[5],h[6],h[7],h[8],h[9],h[10],h[11],h[12],h[13],h[14],h[15]);
        rt_log("  nro[0x10..0x14] = %.4s", (const char *)(h + 0x10));
    }
    if (out) *out = 0x80000;
    ret_x(c, 0);
}

static void ov_ro_load_module(Cpu *c) {
    void *out = arg_p(c, 0);
    const void *nro = arg_p(c, 1);
    void *work = arg_p(c, 2);
    size_t work_sz = arg_x(c, 3);
    int flags = (int)arg_x(c, 4);
    rt_log("=== nn::ro::LoadModule(out=%p nro=%p work=%p work_sz=%#llx flags=%d) ===",
           out, nro, work, (unsigned long long)work_sz, flags);
    if (nro) {
        const uint8_t *h = (const uint8_t *)nro;
        rt_log("  nro[0x10..0x14] = %.4s  u32[0x24]=%#x u32[0x28]=%#x u32[0x2c]=%#x u32[0x30]=%#x",
               (const char *)(h + 0x10),
               *(const uint32_t*)(h+0x24), *(const uint32_t*)(h+0x28),
               *(const uint32_t*)(h+0x2c), *(const uint32_t*)(h+0x30));
        dump_slot(0x91f52b8ULL, "slot A");
        dump_slot(0x91f55a8ULL, "slot B");
    }
    // Real nn error: bit 0 = 0 means failure. 0xE0000BAD.
    ret_x(c, 0xE0000000ULL | 0xBADULL);
}

static void ov_ro_lookup(Cpu *c) {
    void *out = arg_p(c, 0);
    void *mod = arg_p(c, 1);
    const char *name = arg_p(c, 2);
    rt_log("=== nn::ro::LookupModuleSymbol(out=%p mod=%p name=%s) ===",
           out, mod, name ? name : "(null)");
    ret_x(c, 0xE0000000ULL | 0xBAD4ULL);
}

static void ov_ro_noop(Cpu *c) { ret_x(c, 0); }

HOST_OVERRIDE("_ZN2nn2ro10InitializeEv",                    ov_ro_init);
HOST_OVERRIDE("_ZN2nn2ro13GetBufferSizeEPmPKv",            ov_ro_get_buffer_size);
HOST_OVERRIDE("_ZN2nn2ro10LoadModuleEPNS0_6ModuleEPKvPvmi", ov_ro_load_module);
HOST_OVERRIDE("_ZN2nn2ro18LookupModuleSymbolEPmPKNS0_6ModuleEPKc", ov_ro_lookup);
HOST_OVERRIDE("_ZN2nn2ro12UnloadModuleEPNS0_6ModuleE",      ov_ro_noop);
HOST_OVERRIDE("_ZN2nn2ro8FinalizeEv",                       ov_ro_noop);
HOST_OVERRIDE("_ZN2nn2ro18RegisterModuleInfoEPNS0_16RegistrationInfoEPKv", ov_ro_noop);
HOST_OVERRIDE("_ZN2nn2ro20UnregisterModuleInfoEPNS0_16RegistrationInfoE",   ov_ro_noop);
HOST_OVERRIDE("_ZN2nn2ro12ProtectRelroEPKvS2_S2_S2_S2_",    ov_ro_noop);
