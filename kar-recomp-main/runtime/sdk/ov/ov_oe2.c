// nn::oe functions MKW imports that totk-pc-recomp does not replace (ov_system.c has the rest).
#include <string.h>

#include "host.h"
#include "version.h"

// The dump has no control.nacp, which the SDK reads the version from: the version of the copy the game code was
// generated from (version.c).

// void nn::oe::GetDisplayVersion(DisplayVersion *out): char[16]
static void ov_display_version(Cpu *c) {
    char *out = arg_p(c, 0);
    if (!out) return;
    const char *v = rt_version()->display;
    memset(out, 0, 16);
    memcpy(out, v, strlen(v) < 15 ? strlen(v) : 15);
}

// int nn::oe::GetCurrentGraphicsScale(): 10, 20 or 30 (eden-nx2 NX2_NOTES.md row 15); 10 until the scale is measured.
static void ov_graphics_scale(Cpu *c) { ret_x(c, 10); }

// Exit-request sections, server maintenance overlay, scale-change notifications: nothing for a host process to do.
static void ov_nothing(Cpu *c) { (void)c; }

static void ov_pseudo_device_id(Cpu *c) {
    uint8_t *out = arg_p(c, 0);
    if (!out) return;
    static const uint8_t uuid[16] = {
        0x4b, 0x41, 0x52, 0x2d, 0x52, 0x45, 0x43, 0x4f,
        0x4d, 0x50, 0x2d, 0x55, 0x55, 0x49, 0x44, 0x00
    };
    memcpy(out, uuid, 16);
    ret_x(c, 0);
}

// Result nn::oe::GetDefaultDisplayResolutionChangeEvent(nn::os::SystemEvent *out)
// The SDK creates the event via its IPC path (a null service pointer on PC).
// CreateSystemEvent is local to the process and works, so call it with
// clear mode 0 (auto-clear) and initially-signaled 0 (never fires: the host
// window size is fixed).
static void ov_display_resolution_event(Cpu *c) {
    static uint64_t create_fn;
    if (!create_fn && !(create_fn = ldr_symbol("_ZN2nn2os17CreateSystemEventEPNS0_15SystemEventTypeENS0_14EventClearModeEb")))
        rt_fatal("oe: nn::os::CreateSystemEvent not found");
    rt_call(c, create_fn, 3, arg_x(c, 0), 0, 0);
    ret_x(c, 0);
}

HOST_OVERRIDE("_ZN2nn2oe17GetPseudoDeviceIdEPNS_4util4UuidE", ov_pseudo_device_id);
// Result nn::oe::GetDefaultDisplayResolution(int* width, int* height)
// The host window is 1920x1080 (see the swapchain line at startup). The SDK's
// real implementation resolves through sm::Initialize (null on PC).
static void ov_default_display_resolution(Cpu *c) {
    int *w = arg_p(c, 0), *h = arg_p(c, 1);
    if (w) *w = 1920;
    if (h) *h = 1080;
    ret_x(c, 0);
}

HOST_OVERRIDE("_ZN2nn2oe38GetDefaultDisplayResolutionChangeEventEPNS_2os11SystemEventE", ov_display_resolution_event);
HOST_OVERRIDE("_ZN2nn2oe27GetDefaultDisplayResolutionEPiS1_", ov_default_display_resolution);
HOST_OVERRIDE("_ZN2nn2oe17GetDisplayVersionEPNS0_14DisplayVersionE", ov_display_version);
HOST_OVERRIDE("_ZN2nn2oe23GetCurrentGraphicsScaleEv", ov_graphics_scale);
HOST_OVERRIDE("_ZN2nn2oe31EnterExitRequestHandlingSectionEv", ov_nothing);
HOST_OVERRIDE("_ZN2nn2oe31LeaveExitRequestHandlingSectionEv", ov_nothing);
HOST_OVERRIDE("_ZN2nn2oe40SendServerMaintenanceOverlayNotificationENS_4time9PosixTimeES2_", ov_nothing);
HOST_OVERRIDE("_ZN2nn2oe42SetGraphicsScaleChangedNotificationEnabledEb", ov_nothing);
