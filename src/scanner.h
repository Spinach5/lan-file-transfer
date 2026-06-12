#ifndef SCANNER_H
#define SCANNER_H

#include <stdint.h>

/* Start a LAN scan for devices listening on the given port.
   Non-blocking: spawns a background thread. Results delivered via SDL_UserEvent:
   - USEREVENT_SCAN_FOUND (struct event_scan_found) for each device
   - USEREVENT_SCAN_DONE  (struct event_scan_done) when finished */
void scanner_start(uint16_t port);

#endif /* SCANNER_H */
