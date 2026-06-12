#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct net_context;

/* Opaque handle for a TCP connection via lws raw socket */
typedef void (*net_rx_fn)(void *user, const void *data, size_t len);
typedef void (*net_close_fn)(void *user);

/* Create a network context (owns the lws_context and vhost).
   mode: FT_PROTO_TCP or FT_PROTO_UDP */
struct net_context *net_create(int mode);

/* TCP: start listening, return 0 on success */
int net_listen(struct net_context *nc, int port);

/* TCP: accept one client (blocks), returns 0 on success */
int net_accept(struct net_context *nc);

/* TCP: connect to server, return 0 on success */
int net_connect(struct net_context *nc, const char *ip, int port);

/* UDP: bind local port, return 0 on success */
int net_udp_bind(struct net_context *nc, int port);

/* UDP: set destination peer address */
int net_udp_set_peer(struct net_context *nc, const char *ip, int port);

/* Set receive/close callbacks (for TCP) */
void net_set_rx_cb(struct net_context *nc, net_rx_fn rx, void *user);
void net_set_close_cb(struct net_context *nc, net_close_fn cb, void *user);

/* Send data (TCP: reliable stream; UDP: single datagram) */
int net_send(struct net_context *nc, const void *data, size_t len);

/* Receive data from UDP (fills buf, returns bytes received or -1) */
int net_udp_recv(struct net_context *nc, void *buf, size_t len, int timeout_ms);

/* Run one iteration of the event loop.
   Returns: 1 if more work to do, 0 if idle, -1 on error */
int net_service(struct net_context *nc, int timeout_ms);

/* Check if connected */
bool net_is_connected(struct net_context *nc);

/* Check if an error has occurred */
bool net_has_error(struct net_context *nc);

/* Get the internal lws wsi pointer (for direct access if needed) */
void *net_get_wsi(struct net_context *nc);

/* Destroy and free */
void net_destroy(struct net_context *nc);

#endif /* NETWORK_H */
