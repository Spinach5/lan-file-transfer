#ifndef TRANSFER_H
#define TRANSFER_H

#include <stdint.h>

struct net_context;

/* Send a file over the network.
   nc must already be connected/listening.
   Runs the transfer loop — call from a worker thread. */
void transfer_send(struct net_context *nc, const char *filepath, int protocol);
void transfer_recv(struct net_context *nc, const char *savepath, int protocol);

#endif /* TRANSFER_H */
