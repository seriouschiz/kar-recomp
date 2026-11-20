// The network interfaces LAN play can use (ov_lanaddr.c), for the menu's Network row (#707).
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char ip[16];     // dotted IPv4 address
    char name[48];   // the adapter's name ("eth0", "ZeroTier One [abc]")
} OvLanIf;

// The IPv4 addresses of the interfaces that are up and on a local network (never loopback), in the system's order;
// returns how many were written (at most max).
int ov_lan_list(OvLanIf *out, int max);

#ifdef __cplusplus
}
#endif
