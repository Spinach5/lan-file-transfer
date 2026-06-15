#ifndef SCANNER_H
#define SCANNER_H

#include <stdint.h>

/* Start a LAN scan for devices on the given port.
   Uses UDP broadcast + TCP connect scan.
   Non-blocking: spawns background thread. Results via SDL_UserEvent. */
void scanner_start(uint16_t port);

/* Fill ips[count] with local non-loopback IPv4 addresses.
   Returns number of IPs found (max 8). */
int scanner_get_local_ips(char ips[][64], int max_count);

#endif /* SCANNER_H */
