/* UDP Discovery Responder — makes this instance discoverable on LAN.
   Call discovery_start() at startup, discovery_stop() on exit. */
#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <stdint.h>

void discovery_start(uint16_t port);
void discovery_stop(void);

/* Collect all local non-loopback IPv4 addresses for display.
   Returns number of IPs found (max 8). */
int scanner_get_local_ips(char ips[][64], int max_count);

#endif
