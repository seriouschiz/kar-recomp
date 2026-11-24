// nn::hid touch screen for KAR. Touch is an optional console feature (README "Decisiones"), off: almost no PC has a
// touch screen. Initialization does nothing and every read is one sample with no touches, so the game behaves as if
// nobody touched the screen. totk-pc-recomp has no touch overrides (TotK never reads the touch screen).
#include <string.h>

#include "host.h"

// TouchScreenState<N>: sampling number (8), touch count (4), reserved (4), N TouchState records of 0x28 bytes.
#define TOUCH_STATE_HEADER_SIZE 0x10
#define TOUCH_STATE_ENTRY_SIZE 0x28
#define TOUCH_STATE_16_SIZE (0x10 + 16 * 0x28)

static uint64_t samples;

// void nn::hid::InitializeTouchScreen()
static void ov_initialize_touch(Cpu *c) { (void)c; }

static void touch_state_fill(uint8_t *out, size_t state_size) {
    memset(out, 0, state_size);
    uint64_t n = __atomic_add_fetch(&samples, 1, __ATOMIC_RELAXED);
    memcpy(out, &n, sizeof n);
}

// int nn::hid::GetTouchScreenStates<N>(TouchScreenState<N> *out, int count): the newest sample first.
static void ov_touch_states(Cpu *c, size_t state_size) {
    uint8_t *out = arg_p(c, 0);
    int count = (int)arg_x(c, 1);
    if (!out || count < 1) {
        ret_x(c, 0);
        return;
    }
    touch_state_fill(out, state_size);
    ret_x(c, 1);
}
static void ov_touch_states_16(Cpu *c) { ov_touch_states(c, TOUCH_STATE_16_SIZE); }
// void nn::hid::GetTouchScreenState<16>(TouchScreenState<16> *out)
static void ov_touch_state_16(Cpu *c) {
    uint8_t *out = arg_p(c, 0);
    if (!out) rt_fatal("nn::hid::GetTouchScreenState<16>: null output");
    touch_state_fill(out, TOUCH_STATE_16_SIZE);
}

HOST_OVERRIDE("_ZN2nn3hid21InitializeTouchScreenEv", ov_initialize_touch);
HOST_OVERRIDE("_ZN2nn3hid19GetTouchScreenStateILm16EEEvPNS0_16TouchScreenStateIXT_EEE", ov_touch_state_16);
HOST_OVERRIDE("_ZN2nn3hid20GetTouchScreenStatesILm16EEEiPNS0_16TouchScreenStateIXT_EEEi", ov_touch_states_16);
// KAR imports InitializeTouchScreen(int, int) (Eii), not the no-arg Ev. Same no-op
// behavior; this prevents the SDK's SharedMemoryAccessor/AppletResourceHolder path
// from reaching sm::Initialize (null service pointer on PC).
HOST_OVERRIDE("_ZN2nn3hid21InitializeTouchScreenEii", ov_initialize_touch);
