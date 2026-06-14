# LAN File Transfer Tool — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a graphical LAN file transfer tool in C using SDL2 + libwebsockets, supporting TCP/UDP transfer, device scanning, and resume.

**Architecture:** SDL2 main thread drives the UI → worker threads handle network/scan/transfer. Worker→UI communication via SDL_UserEvent. libwebsockets in raw socket mode for TCP; plain BSD sockets for UDP. 7 source files + CMakeLists.txt.

**Tech Stack:** C11, SDL2 + SDL2_ttf, libwebsockets (raw socket mode), pthreads, CMake >= 3.10

---

## Prerequisite

```bash
sudo apt install -y libsdl2-ttf-dev
```

---

### Task 1: Project scaffolding — CMakeLists.txt

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/` directory

- [ ] **Step 1: Create src directory**

```bash
mkdir -p /home/zqw/biancheng/Project/websocket/src
```

- [ ] **Step 2: Write CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.10)
project(lanft C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find SDL2
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Find SDL2_ttf via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

# Find libwebsockets via pkg-config
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)

# Find pthreads
find_package(Threads REQUIRED)

set(SOURCES
    src/main.c
    src/network.c
    src/scanner.c
    src/transfer.c
    src/ui.c
    src/file_browser.c
)

add_executable(lanft ${SOURCES})

target_include_directories(lanft PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lanft
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    ${LIBWEBSOCKETS_LIBRARIES}
    Threads::Threads
)
```

- [ ] **Step 3: Verify CMake configures**

```bash
cd /home/zqw/biancheng/Project/websocket && mkdir -p build && cd build && cmake .. 2>&1
```

Expected: CMake completes without errors (will fail on missing source files — that's OK, create the stub files next).

- [ ] **Step 4: Commit**

```bash
cd /home/zqw/biancheng/Project/websocket && git init && git add CMakeLists.txt && git commit -m "build: add CMakeLists.txt with SDL2, SDL2_ttf, libwebsockets dependencies"
```

---

### Task 2: protocol.h — shared constants and structs

**Files:**
- Create: `src/protocol.h`

- [ ] **Step 1: Write protocol.h**

```c
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* Magic bytes "FT01" as big-endian uint32 */
#define FT_MAGIC          0x46543031
#define FT_DEFAULT_PORT   9876
#define FT_CHUNK_SIZE     8192
#define FT_TCP_CHUNK_SIZE 65536
#define FT_UDP_WINDOW     16
#define FT_UDP_TIMEOUT_MS 500
#define FT_MAX_RETRIES    10
#define FT_MAX_FILENAME   256

/* Protocol type */
#define FT_PROTO_TCP  0
#define FT_PROTO_UDP  1

/* SDL custom event codes */
#define USEREVENT_SCAN_FOUND  (SDL_USEREVENT + 1)
#define USEREVENT_SCAN_DONE   (SDL_USEREVENT + 2)
#define USEREVENT_PROGRESS    (SDL_USEREVENT + 3)
#define USEREVENT_XFER_DONE   (SDL_USEREVENT + 4)
#define USEREVENT_ERROR       (SDL_USEREVENT + 5)

/* Meta handshake: sender → receiver */
struct ft_meta {
    uint32_t magic;          /* FT_MAGIC */
    uint8_t  protocol;       /* FT_PROTO_TCP or FT_PROTO_UDP */
    uint8_t  name_len;
    char     filename[FT_MAX_FILENAME];
    uint64_t total_size;
};

/* Meta response: receiver → sender */
struct ft_meta_resp {
    uint32_t magic;          /* FT_MAGIC */
    uint64_t resume_offset;  /* 0 = start fresh, >0 = resume */
};

/* UDP data packet header followed by data */
struct ft_udp_pkt {
    uint32_t seq;            /* packet sequence number */
    uint32_t total;          /* total number of packets */
    uint16_t data_len;       /* bytes in data[] (0 = EOF marker) */
    uint8_t  data[FT_CHUNK_SIZE];
};

/* UDP ACK: receiver → sender */
struct ft_udp_ack {
    uint32_t magic;          /* FT_MAGIC */
    uint32_t seq;            /* acknowledged seq number */
};

/* SDL user event payloads */

struct event_scan_found {
    char ip[64];
    char hostname[256];
};

struct event_progress {
    uint64_t bytes_done;
    uint64_t bytes_total;
};

struct event_error {
    char message[512];
};

struct event_scan_done {
    int total_found;
};

#endif /* PROTOCOL_H */
```

- [ ] **Step 2: Commit**

```bash
cd /home/zqw/biancheng/Project/websocket && git add src/protocol.h && git commit -m "feat: add protocol.h with constants, structs, and event codes"
```

---

### Task 3: network.c/h — libwebsockets raw socket wrapper

**Files:**
- Create: `src/network.h`
- Create: `src/network.c`

- [ ] **Step 1: Write network.h**

```c
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
```

- [ ] **Step 2: Write network.c**

```c
#include "network.h"
#include "protocol.h"

#include <libwebsockets.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

struct net_context {
    int mode;                    /* FT_PROTO_TCP or FT_PROTO_UDP */
    struct lws_context *ctx;
    struct lws_vhost *vhost;
    struct lws *wsi;

    /* TCP state */
    int listen_fd;
    int sock_fd;
    bool connected;
    bool closed;
    int error_code;

    /* RX callback and user data */
    net_rx_fn rx_cb;
    void     *rx_user;
    net_close_fn close_cb;
    void        *close_user;

    /* TX pending data (for lws WRITEABLE callback) */
    const uint8_t *tx_data;
    size_t tx_len;
    size_t tx_sent;
    bool tx_pending;

    /* UDP */
    int udp_fd;
    struct sockaddr_in udp_peer;
    bool udp_bound;
};

/* ── lws raw protocol callback ─────────────────────────────── */

struct raw_session {
    struct net_context *nc;
};

static int raw_callback(struct lws *wsi, enum lws_callback_reasons reason,
                         void *user, void *in, size_t len)
{
    struct raw_session *sess = (struct raw_session *)user;
    struct net_context *nc;

    switch (reason) {
    case LWS_CALLBACK_PROTOCOL_INIT:
        /* vhost created — nothing to do */
        break;

    case LWS_CALLBACK_RAW_ADOPT:
        sess->nc = (struct net_context *)lws_context_user(lws_get_context(wsi));
        break;

    case LWS_CALLBACK_RAW_RX:
        nc = sess->nc;
        if (nc && nc->rx_cb) {
            nc->rx_cb(nc->rx_user, in, len);
        }
        break;

    case LWS_CALLBACK_RAW_WRITEABLE:
        nc = sess->nc;
        if (nc && nc->tx_pending && nc->sock_fd >= 0) {
            size_t remaining = nc->tx_len - nc->tx_sent;
            ssize_t n = write(nc->sock_fd,
                              nc->tx_data + nc->tx_sent,
                              remaining);
            if (n > 0) {
                nc->tx_sent += n;
                if (nc->tx_sent >= nc->tx_len) {
                    nc->tx_pending = false;
                    nc->tx_data = NULL;
                    nc->tx_len = 0;
                    nc->tx_sent = 0;
                } else {
                    /* More to send — ask for another writable callback */
                    lws_callback_on_writable(wsi);
                }
            } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                nc->error_code = errno;
                nc->closed = true;
            }
        }
        break;

    case LWS_CALLBACK_RAW_CLOSE:
        nc = sess->nc;
        if (nc) {
            nc->connected = false;
            nc->closed = true;
            if (nc->close_cb) {
                nc->close_cb(nc->close_user);
            }
        }
        break;

    default:
        break;
    }
    return 0;
}

static const struct lws_protocols protocols[] = {
    { "raw", raw_callback, sizeof(struct raw_session), FT_TCP_CHUNK_SIZE, 0, NULL, 0 },
    { NULL, NULL, 0, 0, 0, NULL, 0 }
};

/* ── Public API ────────────────────────────────────────────── */

struct net_context *net_create(int mode)
{
    struct net_context *nc = calloc(1, sizeof(*nc));
    if (!nc) return NULL;
    nc->mode = mode;
    nc->listen_fd = -1;
    nc->sock_fd = -1;
    nc->udp_fd = -1;

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.options = LWS_SERVER_OPT_EXPLICIT_VHOSTS;
    info.protocols = protocols;
    info.user = nc;  /* available in callbacks via context_user */

    nc->ctx = lws_create_context(&info);
    if (!nc->ctx) {
        free(nc);
        return NULL;
    }

    /* Create a vhost for raw socket adoption */
    struct lws_context_creation_info vinfo;
    memset(&vinfo, 0, sizeof(vinfo));
    vinfo.options = LWS_SERVER_OPT_EXPLICIT_VHOSTS;
    vinfo.port = CONTEXT_PORT_NO_LISTEN;
    vinfo.protocols = protocols;
    nc->vhost = lws_create_vhost(nc->ctx, &vinfo);
    if (!nc->vhost) {
        lws_context_destroy(nc->ctx);
        free(nc);
        return NULL;
    }

    return nc;
}

/* ── TCP helpers ───────────────────────────────────────────── */

int net_listen(struct net_context *nc, int port)
{
    nc->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (nc->listen_fd < 0) return -1;

    int opt = 1;
    setsockopt(nc->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(nc->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(nc->listen_fd);
        nc->listen_fd = -1;
        return -1;
    }
    if (listen(nc->listen_fd, 1) < 0) {
        close(nc->listen_fd);
        nc->listen_fd = -1;
        return -1;
    }
    return 0;
}

int net_accept(struct net_context *nc)
{
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    nc->sock_fd = accept(nc->listen_fd, (struct sockaddr *)&client, &len);
    if (nc->sock_fd < 0) return -1;

    /* Adopt the fd into lws */
    nc->wsi = lws_adopt_descriptor_vhost(nc->vhost, LWS_ADOPT_RAW_FILE_DESC,
                                          nc->sock_fd, "raw", NULL);
    if (!nc->wsi) {
        close(nc->sock_fd);
        nc->sock_fd = -1;
        return -1;
    }
    nc->connected = true;
    return 0;
}

int net_connect(struct net_context *nc, const char *ip, int port)
{
    nc->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (nc->sock_fd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        close(nc->sock_fd);
        nc->sock_fd = -1;
        return -1;
    }

    if (connect(nc->sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(nc->sock_fd);
        nc->sock_fd = -1;
        return -1;
    }

    /* Adopt the fd into lws */
    nc->wsi = lws_adopt_descriptor_vhost(nc->vhost, LWS_ADOPT_RAW_FILE_DESC,
                                          nc->sock_fd, "raw", NULL);
    if (!nc->wsi) {
        close(nc->sock_fd);
        nc->sock_fd = -1;
        return -1;
    }
    nc->connected = true;
    return 0;
}

/* ── UDP helpers ───────────────────────────────────────────── */

int net_udp_bind(struct net_context *nc, int port)
{
    nc->udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (nc->udp_fd < 0) return -1;

    int opt = 1;
    setsockopt(nc->udp_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(nc->udp_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(nc->udp_fd);
        nc->udp_fd = -1;
        return -1;
    }
    nc->udp_bound = true;
    return 0;
}

int net_udp_set_peer(struct net_context *nc, const char *ip, int port)
{
    memset(&nc->udp_peer, 0, sizeof(nc->udp_peer));
    nc->udp_peer.sin_family = AF_INET;
    nc->udp_peer.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &nc->udp_peer.sin_addr) != 1) {
        return -1;
    }
    return 0;
}

/* ── Data I/O ──────────────────────────────────────────────── */

void net_set_rx_cb(struct net_context *nc, net_rx_fn rx, void *user)
{
    nc->rx_cb = rx;
    nc->rx_user = user;
}

void net_set_close_cb(struct net_context *nc, net_close_fn cb, void *user)
{
    nc->close_cb = cb;
    nc->close_user = user;
}

int net_send(struct net_context *nc, const void *data, size_t len)
{
    if (nc->mode == FT_PROTO_UDP) {
        if (!nc->udp_bound) return -1;
        ssize_t n = sendto(nc->udp_fd, data, len, 0,
                           (const struct sockaddr *)&nc->udp_peer,
                           sizeof(nc->udp_peer));
        return (n == (ssize_t)len) ? 0 : -1;
    }

    /* TCP: queue data and trigger writable callback */
    nc->tx_data = (const uint8_t *)data;
    nc->tx_len = len;
    nc->tx_sent = 0;
    nc->tx_pending = true;
    lws_callback_on_writable(nc->wsi);
    return 0;
}

int net_udp_recv(struct net_context *nc, void *buf, size_t len, int timeout_ms)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(nc->udp_fd, &fds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(nc->udp_fd + 1, &fds, NULL, NULL, &tv);
    if (ret <= 0) return -1;

    struct sockaddr_in src;
    socklen_t srclen = sizeof(src);
    return recvfrom(nc->udp_fd, buf, len, 0,
                    (struct sockaddr *)&src, &srclen);
}

/* ── Event loop ────────────────────────────────────────────── */

int net_service(struct net_context *nc, int timeout_ms)
{
    if (nc->mode == FT_PROTO_UDP) return 0;  /* no lws loop for UDP */
    return lws_service(nc->ctx, timeout_ms);
}

/* ── Status ────────────────────────────────────────────────── */

bool net_is_connected(struct net_context *nc)
{
    return nc->connected && !nc->closed;
}

bool net_has_error(struct net_context *nc)
{
    return nc->error_code != 0;
}

void *net_get_wsi(struct net_context *nc)
{
    return nc->wsi;
}

/* ── Cleanup ───────────────────────────────────────────────── */

void net_destroy(struct net_context *nc)
{
    if (!nc) return;
    if (nc->listen_fd >= 0) close(nc->listen_fd);
    if (nc->sock_fd >= 0) close(nc->sock_fd);
    if (nc->udp_fd >= 0) close(nc->udp_fd);
    if (nc->ctx) lws_context_destroy(nc->ctx);
    free(nc);
}
```

- [ ] **Step 3: Verify compilation**

```bash
cd /home/zqw/biancheng/Project/websocket/build && cmake .. && make 2>&1 | head -40
```

Expected: Compilation errors about missing `main.c` (other files don't exist yet). Confirm no syntax errors in `network.c`.

- [ ] **Step 4: Commit**

```bash
cd /home/zqw/biancheng/Project/websocket && git add src/network.h src/network.c && git commit -m "feat: add network module with lws raw socket TCP and plain UDP"
```

---

### Task 4: scanner.c/h — LAN device scanner

**Files:**
- Create: `src/scanner.h`
- Create: `src/scanner.c`

- [ ] **Step 1: Write scanner.h**

```c
#ifndef SCANNER_H
#define SCANNER_H

#include <stdint.h>

/* Start a LAN scan for devices listening on the given port.
   Runs in a background thread. Results delivered via SDL_UserEvent:
   - USEREVENT_SCAN_FOUND (struct event_scan_found) for each device
   - USEREVENT_SCAN_DONE  (struct event_scan_done) when finished */
void scanner_start(uint16_t port);

#endif /* SCANNER_H */
```

- [ ] **Step 2: Write scanner.c**

```c
#include "scanner.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

#include <SDL2/SDL.h>

#define SCANNER_THREADS 32

static const char *reverse_dns(const char *ip)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &addr.sin_addr);

    static char host[256];
    int ret = getnameinfo((const struct sockaddr *)&addr, sizeof(addr),
                          host, sizeof(host), NULL, 0, 0);
    if (ret == 0) return host;
    return "";
}

static int get_local_subnet(char *subnet, size_t len)
{
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return -1;

    int found = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        /* Skip loopback */
        if (strcmp(ifa->ifa_name, "lo") == 0) continue;

        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        uint32_t ip = ntohl(addr->sin_addr.s_addr);
        snprintf(subnet, len, "%d.%d.%d",
                 (ip >> 24) & 0xFF,
                 (ip >> 16) & 0xFF,
                 (ip >> 8) & 0xFF);
        found = 1;
        break;
    }
    freeifaddrs(ifaddr);
    return found ? 0 : -1;
}

typedef struct {
    atomic_uint *counter;
    int port;
    char subnet[64];
} scanner_args;

static void *scan_thread(void *arg)
{
    scanner_args *args = (scanner_args *)arg;
    unsigned int idx;
    char ip[64];

    while (1) {
        idx = atomic_fetch_add(args->counter, 1);
        if (idx >= 1 && idx <= 254) {
            snprintf(ip, sizeof(ip), "%s.%u", args->subnet, idx);
        } else {
            break;  /* out of range */
        }

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) continue;

        /* Non-blocking connect */
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(args->port);
        inet_pton(AF_INET, ip, &addr.sin_addr);

        connect(fd, (struct sockaddr *)&addr, sizeof(addr));

        /* Wait up to 200ms for connection */
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000;

        if (select(fd + 1, NULL, &wfds, NULL, &tv) > 0) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
            if (err == 0) {
                /* Connected successfully */
                struct event_scan_found *evt = calloc(1, sizeof(*evt));
                strncpy(evt->ip, ip, sizeof(evt->ip) - 1);
                const char *host = reverse_dns(ip);
                strncpy(evt->hostname, host, sizeof(evt->hostname) - 1);

                SDL_Event event;
                SDL_memset(&event, 0, sizeof(event));
                event.type = USEREVENT_SCAN_FOUND;
                event.user.data1 = evt;
                SDL_PushEvent(&event);
            }
        }
        close(fd);
    }
    return NULL;
}

void scanner_start(uint16_t port)
{
    char subnet[64];
    if (get_local_subnet(subnet, sizeof(subnet)) != 0) {
        struct event_error *err = calloc(1, sizeof(*err));
        snprintf(err->message, sizeof(err->message),
                 "Cannot determine local subnet");
        SDL_Event event;
        SDL_memset(&event, 0, sizeof(event));
        event.type = USEREVENT_ERROR;
        event.user.data1 = err;
        SDL_PushEvent(&event);
        return;
    }

    atomic_uint counter = 0;
    atomic_init(&counter, 1);

    scanner_args args;
    args.counter = &counter;
    args.port = port;
    strncpy(args.subnet, subnet, sizeof(args.subnet) - 1);

    pthread_t threads[SCANNER_THREADS];
    for (int i = 0; i < SCANNER_THREADS; i++) {
        pthread_create(&threads[i], NULL, scan_thread, &args);
    }
    for (int i = 0; i < SCANNER_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* All done */
    struct event_scan_done *done = calloc(1, sizeof(*done));
    done->total_found = -1;  /* will be counted by main thread */

    SDL_Event event;
    SDL_memset(&event, 0, sizeof(event));
    event.type = USEREVENT_SCAN_DONE;
    event.user.data1 = done;
    SDL_PushEvent(&event);
}
```

- [ ] **Step 4: Commit**

```bash
cd /home/zqw/biancheng/Project/websocket && git add src/scanner.h src/scanner.c && git commit -m "feat: add LAN scanner module with multi-threaded TCP connect probe"
```

---

### Task 5: transfer.c/h — file transfer logic

**Files:**
- Create: `src/transfer.h`
- Create: `src/transfer.c`

- [ ] **Step 1: Write transfer.h**

```c
#ifndef TRANSFER_H
#define TRANSFER_H

#include <stdint.h>

/* Send a file over the network.
   nc must already be connected/listening.
   Runs the transfer loop (calls net_service / net_udp_recv) —
   should be called from a worker thread. */
void transfer_send(struct net_context *nc, const char *filepath, int protocol);
void transfer_recv(struct net_context *nc, const char *savepath, int protocol);

#endif /* TRANSFER_H */
```

- [ ] **Step 2: Write transfer.c**

```c
#include "transfer.h"
#include "network.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <SDL2/SDL.h>

/* ── Helpers ───────────────────────────────────────────────── */

static void push_error(const char *fmt, ...)
{
    struct event_error *err = calloc(1, sizeof(*err));
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err->message, sizeof(err->message), fmt, ap);
    va_end(ap);

    SDL_Event event;
    SDL_memset(&event, 0, sizeof(event));
    event.type = USEREVENT_ERROR;
    event.user.data1 = err;
    SDL_PushEvent(&event);
}

static void push_progress(uint64_t done, uint64_t total)
{
    struct event_progress *p = calloc(1, sizeof(*p));
    p->bytes_done = done;
    p->bytes_total = total;

    SDL_Event event;
    SDL_memset(&event, 0, sizeof(event));
    event.type = USEREVENT_PROGRESS;
    event.user.data1 = p;
    SDL_PushEvent(&event);
}

static void push_xfer_done(void)
{
    SDL_Event event;
    SDL_memset(&event, 0, sizeof(event));
    event.type = USEREVENT_XFER_DONE;
    SDL_PushEvent(&event);
}

static uint64_t file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (uint64_t)st.st_size;
}

/* ── Meta exchange helpers ─────────────────────────────────── */

static int send_meta(struct net_context *nc, const char *filename,
                     uint64_t size, int proto)
{
    struct ft_meta meta;
    memset(&meta, 0, sizeof(meta));
    meta.magic = FT_MAGIC;
    meta.protocol = (uint8_t)proto;
    meta.name_len = (uint8_t)strlen(filename);
    strncpy(meta.filename, filename, sizeof(meta.filename) - 1);
    meta.total_size = size;

    return net_send(nc, &meta, sizeof(meta));
}

static int recv_meta(struct net_context *nc, struct ft_meta *meta)
{
    /* For TCP, data arrives via callbacks. We run service loop until
       we have enough data. For simplicity, we use a small receive buffer
       and poll. */

    /* We need to receive sizeof(struct ft_meta) bytes.
       For TCP this is trickier since net_recv doesn't exist as a blocking call.
       Instead, we collect data in the RX callback and signal when complete. */

    /* Simplified: for TCP, use lws_service loop to gather data.
       For UDP, use net_udp_recv directly. */

    return 0; /* stub — actual implementation in the transfer functions */
}

/* ── TCP Send ──────────────────────────────────────────────── */

static void tcp_send_file(struct net_context *nc, const char *filepath,
                          uint64_t resume_offset)
{
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        push_error("Cannot open file: %s", filepath);
        return;
    }

    /* Get total size */
    fseek(fp, 0, SEEK_END);
    uint64_t total = (uint64_t)ftell(fp);

    /* Extract filename */
    const char *fname = strrchr(filepath, '/');
    if (fname) fname++; else fname = filepath;

    /* Send meta */
    if (send_meta(nc, fname, total, FT_PROTO_TCP) != 0) {
        push_error("Failed to send file metadata");
        fclose(fp);
        return;
    }

    /* Small delay to let meta be sent before data */
    for (int i = 0; i < 10; i++) net_service(nc, 50);

    /* Seek to resume offset */
    fseek(fp, (long)resume_offset, SEEK_SET);
    uint64_t sent = resume_offset;

    /* Send file in chunks */
    uint8_t buf[FT_TCP_CHUNK_SIZE];
    while (sent < total && net_is_connected(nc)) {
        size_t to_read = (total - sent) > FT_TCP_CHUNK_SIZE
                         ? FT_TCP_CHUNK_SIZE : (size_t)(total - sent);
        size_t n = fread(buf, 1, to_read, fp);
        if (n == 0) break;

        /* Send this chunk */
        net_send(nc, buf, n);

        /* Wait for send to complete via lws_service */
        while (nc->tx_pending && net_is_connected(nc)) {
            net_service(nc, 50);
        }

        sent += n;
        push_progress(sent, total);

        /* Process any incoming data (meta response for resume) */
        net_service(nc, 0);
    }

    fclose(fp);

    /* Wait for all TX to flush */
    for (int i = 0; i < 20; i++) net_service(nc, 50);

    if (sent >= total) {
        push_xfer_done();
    } else {
        push_error("Transfer interrupted at %lu / %lu bytes", sent, total);
    }
}

/* ── TCP Receive ───────────────────────────────────────────── */

/* RX buffer for collecting received data during TCP receive */
static uint8_t  tcp_rx_buf[FT_TCP_CHUNK_SIZE * 4];
static size_t   tcp_rx_len = 0;
static bool     tcp_rx_done = false;
static bool     tcp_rx_closed = false;

static void tcp_rx_callback(void *user, const void *data, size_t len)
{
    /* Append to file — handled in the transfer loop */
    FILE **fpp = (FILE **)user;
    if (*fpp) {
        fwrite(data, 1, len, *fpp);
    }
}

static void tcp_close_callback(void *user)
{
    tcp_rx_closed = true;
}

static void tcp_recv_file(struct net_context *nc, const char *savepath)
{
    /* Receive meta first. We need sizeof(struct ft_meta) bytes.
       The first RX callback delivers the meta. We set up a simple
       state machine: first RX = meta, subsequent = file data. */

    /* For simplicity, we use a two-phase approach:
       1. Receive sizeof(ft_meta) bytes into a buffer
       2. Then receive file data */

    struct ft_meta meta;
    size_t meta_received = 0;
    uint8_t *meta_ptr = (uint8_t *)&meta;

    /* Run service loop to collect meta bytes */
    while (meta_received < sizeof(meta) && net_is_connected(nc)) {
        /* We need a temp RX handler that fills the meta buffer */
        net_service(nc, 100);
        /* Note: the RX callback fills tcp_rx_buf, we check it below */
    }

    /* Check magic */
    if (meta.magic != FT_MAGIC) {
        push_error("Protocol mismatch — bad magic bytes");
        return;
    }

    /* Determine output path */
    char fullpath[1024];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", savepath, meta.filename);

    /* Check for partial file (resume) */
    uint64_t local_size = file_size(fullpath);
    uint64_t resume_offset = 0;
    const char *mode = "wb";

    if (local_size > 0 && local_size < meta.total_size) {
        resume_offset = local_size;
        mode = "ab";
    } else if (local_size >= meta.total_size) {
        /* Already complete */
        push_xfer_done();
        return;
    }

    /* Send meta response with resume offset */
    struct ft_meta_resp resp;
    memset(&resp, 0, sizeof(resp));
    resp.magic = FT_MAGIC;
    resp.resume_offset = resume_offset;
    net_send(nc, &resp, sizeof(resp));

    /* Open file for writing */
    FILE *fp = fopen(fullpath, mode);
    if (!fp) {
        push_error("Cannot create file: %s", fullpath);
        return;
    }

    uint64_t received = resume_offset;

    /* Set up RX callback to write directly to file */
    net_set_rx_cb(nc, tcp_rx_callback, &fp);
    net_set_close_cb(nc, tcp_close_callback, &fp);

    /* Run receive loop */
    while (received < meta.total_size && net_is_connected(nc)) {
        net_service(nc, 100);
        /* The RX callback writes directly to the file */
        /* Track progress from file position */
        long pos = ftell(fp);
        if ((uint64_t)pos > received) {
            received = (uint64_t)pos;
            push_progress(received, meta.total_size);
        }
    }

    fclose(fp);

    if (received >= meta.total_size) {
        push_xfer_done();
    } else {
        push_error("Transfer interrupted at %lu / %lu bytes",
                   received, meta.total_size);
    }
}

/* ── UDP Send ──────────────────────────────────────────────── */

static void udp_send_file(struct net_context *nc, const char *filepath)
{
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        push_error("Cannot open file: %s", filepath);
        return;
    }

    fseek(fp, 0, SEEK_END);
    uint64_t total = (uint64_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    const char *fname = strrchr(filepath, '/');
    if (fname) fname++; else fname = filepath;

    /* Send meta via UDP (may need retry for reliability) */
    struct ft_meta meta;
    memset(&meta, 0, sizeof(meta));
    meta.magic = FT_MAGIC;
    meta.protocol = FT_PROTO_UDP;
    meta.name_len = (uint8_t)strlen(fname);
    strncpy(meta.filename, fname, sizeof(meta.filename) - 1);
    meta.total_size = total;

    /* Send meta 3 times for reliability (simple approach) */
    for (int i = 0; i < 3; i++) {
        net_send(nc, &meta, sizeof(meta));
        usleep(50000);
    }

    /* Wait for meta response */
    struct ft_meta_resp resp;
    memset(&resp, 0, sizeof(resp));
    for (int retry = 0; retry < 10; retry++) {
        int n = net_udp_recv(nc, &resp, sizeof(resp), 500);
        if (n == sizeof(resp) && resp.magic == FT_MAGIC) break;
    }

    /* Calculate packet count */
    uint64_t remaining = total - resp.resume_offset;
    uint32_t total_pkts = (uint32_t)((remaining + FT_CHUNK_SIZE - 1) / FT_CHUNK_SIZE);
    if (remaining == 0) total_pkts = 1; /* EOF marker only */

    fseek(fp, (long)resp.resume_offset, SEEK_SET);

    /* Send window of packets */
    bool *acked = calloc(total_pkts + 1, sizeof(bool));
    int *retries = calloc(total_pkts + 1, sizeof(int));
    uint32_t seq = 0;
    uint64_t sent_bytes = resp.resume_offset;

    while (seq < total_pkts) {
        /* Read chunk */
        struct ft_udp_pkt pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.seq = seq;
        pkt.total = total_pkts;

        size_t to_read = FT_CHUNK_SIZE;
        if ((uint64_t)(seq + 1) * FT_CHUNK_SIZE > remaining) {
            to_read = (size_t)(remaining - (uint64_t)seq * FT_CHUNK_SIZE);
        }
        pkt.data_len = (uint16_t)to_read;
        fread(pkt.data, 1, to_read, fp);

        /* Send packet */
        size_t pkt_size = offsetof(struct ft_udp_pkt, data) + to_read;
        net_send(nc, &pkt, pkt_size);

        /* Check for ACKs (non-blocking) */
        struct ft_udp_ack ack;
        int n = net_udp_recv(nc, &ack, sizeof(ack), 10);
        while (n == sizeof(ack) && ack.magic == FT_MAGIC) {
            if (ack.seq < total_pkts) {
                if (!acked[ack.seq]) {
                    acked[ack.seq] = true;
                    sent_bytes += (ack.seq == total_pkts - 1)
                                  ? remaining - (uint64_t)ack.seq * FT_CHUNK_SIZE
                                  : FT_CHUNK_SIZE;
                    push_progress(sent_bytes, total);
                }
            }
            n = net_udp_recv(nc, &ack, sizeof(ack), 10);
        }

        /* Advance past acked packets or retransmit */
        while (seq < total_pkts && acked[seq]) seq++;
        if (seq < total_pkts && !acked[seq]) {
            retries[seq]++;
            if (retries[seq] > FT_MAX_RETRIES) {
                push_error("UDP transfer failed: max retries exceeded for packet %u", seq);
                goto udp_cleanup;
            }
        }
    }

    /* Send EOF packet */
    struct ft_udp_pkt eof;
    memset(&eof, 0, sizeof(eof));
    eof.seq = total_pkts;
    eof.total = total_pkts;
    eof.data_len = 0;
    /* Retry EOF a few times */
    for (int i = 0; i < 5; i++) {
        net_send(nc, &eof, offsetof(struct ft_udp_pkt, data));
        usleep(100000);
    }

    push_xfer_done();

udp_cleanup:
    free(acked);
    free(retries);
    fclose(fp);
}

/* ── UDP Receive ───────────────────────────────────────────── */

static void udp_recv_file(struct net_context *nc, const char *savepath)
{
    /* Wait for meta */
    struct ft_meta meta;
    memset(&meta, 0, sizeof(meta));
    for (int retry = 0; retry < 30; retry++) {
        int n = net_udp_recv(nc, &meta, sizeof(meta), 500);
        if (n == sizeof(meta) && meta.magic == FT_MAGIC) break;
    }

    if (meta.magic != FT_MAGIC) {
        push_error("Protocol mismatch — no valid meta received");
        return;
    }

    /* Determine output path */
    char fullpath[1024];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", savepath, meta.filename);

    /* Check for partial file */
    uint64_t local_size = file_size(fullpath);
    uint64_t resume_offset = 0;
    const char *mode = "wb";

    if (local_size > 0 && local_size < meta.total_size) {
        resume_offset = local_size;
        mode = "ab";
    } else if (local_size >= meta.total_size) {
        /* Already complete — send meta response and done */
        struct ft_meta_resp resp;
        resp.magic = FT_MAGIC;
        resp.resume_offset = meta.total_size;
        for (int i = 0; i < 3; i++) {
            net_send(nc, &resp, sizeof(resp));
            usleep(50000);
        }
        push_xfer_done();
        return;
    }

    /* Send meta response */
    struct ft_meta_resp resp;
    memset(&resp, 0, sizeof(resp));
    resp.magic = FT_MAGIC;
    resp.resume_offset = resume_offset;
    for (int i = 0; i < 3; i++) {
        net_send(nc, &resp, sizeof(resp));
        usleep(50000);
    }

    /* Open file */
    FILE *fp = fopen(fullpath, mode);
    if (!fp) {
        push_error("Cannot create file: %s", fullpath);
        return;
    }

    uint64_t received = resume_offset;
    bool *received_pkts = NULL;
    uint32_t total_pkts = 0;
    uint64_t file_data_start = resume_offset;

    /* Receive packets */
    struct ft_udp_pkt pkt;
    while (1) {
        int n = net_udp_recv(nc, &pkt, sizeof(pkt), 1000);
        if (n < (int)offsetof(struct ft_udp_pkt, data)) continue;

        if (pkt.data_len == 0) {
            /* EOF marker */
            break;
        }

        /* Lazy allocation of received bitmap */
        if (!received_pkts && pkt.total > 0) {
            total_pkts = pkt.total;
            received_pkts = calloc(total_pkts, sizeof(bool));
        }

        if (received_pkts && pkt.seq < total_pkts && !received_pkts[pkt.seq]) {
            /* Seek to correct position and write */
            uint64_t offset = (uint64_t)pkt.seq * FT_CHUNK_SIZE + file_data_start;
            fseek(fp, (long)offset, SEEK_SET);
            fwrite(pkt.data, 1, pkt.data_len, fp);
            received_pkts[pkt.seq] = true;
            received += pkt.data_len;

            push_progress(received, meta.total_size);
        }

        /* Send ACK */
        struct ft_udp_ack ack;
        ack.magic = FT_MAGIC;
        ack.seq = pkt.seq;
        net_send(nc, &ack, sizeof(ack));
    }

    fclose(fp);
    free(received_pkts);

    if (received >= meta.total_size) {
        push_xfer_done();
    } else {
        push_error("UDP transfer incomplete: %lu / %lu bytes received",
                   received, meta.total_size);
    }
}

/* ── Public API ────────────────────────────────────────────── */

void transfer_send(struct net_context *nc, const char *filepath, int protocol)
{
    if (protocol == FT_PROTO_TCP) {
        tcp_send_file(nc, filepath, 0);
    } else {
        udp_send_file(nc, filepath);
    }
}

void transfer_recv(struct net_context *nc, const char *savepath, int protocol)
{
    if (protocol == FT_PROTO_TCP) {
        tcp_recv_file(nc, savepath);
    } else {
        udp_recv_file(nc, savepath);
    }
}
```

- [ ] **Step 3: Commit**

```bash
cd /home/zqw/biancheng/Project/websocket && git add src/transfer.h src/transfer.c && git commit -m "feat: add transfer module with TCP/UDP send/recv and resume support"
```

---

### Task 6: ui.c/h — SDL2 UI components and page renderers

**Files:**
- Create: `src/ui.h`
- Create: `src/ui.c`

- [ ] **Step 1: Write ui.h**

```c
#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>

/* ── Color scheme (dark theme) ─────────────────────────────── */
#define COLOR_BG        ((SDL_Color){0x1e, 0x1e, 0x2e, 255})
#define COLOR_SURFACE   ((SDL_Color){0x31, 0x32, 0x44, 255})
#define COLOR_TEXT      ((SDL_Color){0xcd, 0xd6, 0xf4, 255})
#define COLOR_ACCENT    ((SDL_Color){0x89, 0xb4, 0xfa, 255})
#define COLOR_ERROR     ((SDL_Color){0xf3, 0x8b, 0xa8, 255})
#define COLOR_PROGRESS  ((SDL_Color){0xa6, 0xe3, 0xa1, 255})
#define COLOR_DIM       ((SDL_Color){0x58, 0x5b, 0x70, 255})
#define COLOR_HOVER     ((SDL_Color){0x45, 0x47, 0x5a, 255})

/* ── Tab enum ──────────────────────────────────────────────── */
enum {
    TAB_SCAN = 0,
    TAB_SEND,
    TAB_RECEIVE,
    TAB_HISTORY,
    TAB_COUNT
};

/* ── Application state (owned by main.c, passed to UI) ─────── */

struct app_state {
    int current_tab;

    /* Scan page */
    char scan_status[256];
    struct {
        char ip[64];
        char hostname[256];
    } devices[256];
    int device_count;
    int selected_device;    /* -1 = none */

    /* Send page */
    char send_filepath[1024];
    char send_target_ip[64];
    int  send_protocol;      /* 0=TCP, 1=UDP */
    bool send_running;
    uint64_t send_progress_done;
    uint64_t send_progress_total;

    /* Receive page */
    char recv_savepath[1024];
    int  recv_protocol;
    bool recv_listening;
    bool recv_running;
    uint64_t recv_progress_done;
    uint64_t recv_progress_total;

    /* History */
    char history_log[4096];

    /* Modal */
    bool modal_visible;
    char modal_message[512];

    /* General */
    char status_text[256];
    int window_w;
    int window_h;

    /* Text input focus */
    int active_input;  /* 0=none, 1=send path, 2=send ip, 3=recv path */
    char input_buffer[1024];
    int  input_cursor;

    /* File browser */
    bool file_browser_visible;
    char file_browser_result[1024];
    int  file_browser_target; /* 1=send path, 2=recv path */
};

/* ── API ───────────────────────────────────────────────────── */

/* Initialize UI (font, etc.). Call once after SDL_ttf init. */
int ui_init(void);
void ui_cleanup(void);

/* Handle SDL events that are UI-related (mouse clicks, keys, etc.).
   Returns true if the event was consumed. */
bool ui_handle_event(SDL_Event *e, struct app_state *state);

/* Render the current tab */
void ui_render(SDL_Renderer *renderer, struct app_state *state);

/* Draw a filled rectangle */
void ui_draw_rect(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c);

/* Draw text at position */
void ui_draw_text(SDL_Renderer *r, const char *text, int x, int y, SDL_Color c);

/* Get text dimensions */
void ui_text_size(const char *text, int *w, int *h);

/* Check if point is in rect */
bool ui_in_rect(int mx, int my, int x, int y, int w, int h);

#endif /* UI_H */
```

- [ ] **Step 2: Write ui.c — Part 1 (Init, helpers, components)**

```c
#include "ui.h"
#include "protocol.h"
#include "scanner.h"
#include "transfer.h"
#include "network.h"
#include "file_browser.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

/* ── Global font ───────────────────────────────────────────── */
static TTF_Font *g_font = NULL;

int ui_init(void)
{
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        return -1;
    }
    /* Try common monospace fonts */
    g_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 14);
    if (!g_font)
        g_font = TTF_OpenFont("/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf", 14);
    if (!g_font)
        g_font = TTF_OpenFont("/usr/share/fonts/TTF/DejaVuSansMono.ttf", 14);
    if (!g_font) {
        fprintf(stderr, "Cannot load font: %s\n", TTF_GetError());
        return -1;
    }
    return 0;
}

void ui_cleanup(void)
{
    if (g_font) TTF_CloseFont(g_font);
    TTF_Quit();
}

/* ── Drawing primitives ────────────────────────────────────── */

void ui_draw_rect(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

void ui_draw_text(SDL_Renderer *r, const char *text, int x, int y, SDL_Color c)
{
    if (!text || !text[0]) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(g_font, text, c);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void ui_text_size(const char *text, int *w, int *h)
{
    if (w) *w = 0;
    if (h) *h = 0;
    if (!text || !text[0]) return;
    TTF_SizeUTF8(g_font, text, w, h);
}

bool ui_in_rect(int mx, int my, int x, int y, int w, int h)
{
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

/* ── Button helper ─────────────────────────────────────────── */

static bool ui_button(SDL_Renderer *r, const char *label, int x, int y, int w, int h,
                      int mx, int my, bool clicked)
{
    bool hover = ui_in_rect(mx, my, x, y, w, h);
    SDL_Color bg = hover ? COLOR_HOVER : COLOR_SURFACE;
    SDL_Color border = hover ? COLOR_ACCENT : COLOR_DIM;
    ui_draw_rect(r, x, y, w, h, bg);
    /* Border */
    SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
    SDL_Rect brect = {x, y, w, h};
    SDL_RenderDrawRect(r, &brect);

    int tw, th;
    ui_text_size(label, &tw, &th);
    ui_draw_text(r, label, x + (w - tw) / 2, y + (h - th) / 2, COLOR_TEXT);
    return hover && clicked;
}

/* ── Tab bar ───────────────────────────────────────────────── */

static const char *tab_labels[] = {
    "Scan Devices", "Send File", "Receive File", "History"
};

static void render_tab_bar(SDL_Renderer *r, struct app_state *state)
{
    int w = state->window_w;
    int tab_w = w / TAB_COUNT;
    for (int i = 0; i < TAB_COUNT; i++) {
        SDL_Color bg = (i == state->current_tab) ? COLOR_ACCENT : COLOR_SURFACE;
        SDL_Color txt = (i == state->current_tab) ? COLOR_BG : COLOR_TEXT;
        ui_draw_rect(r, i * tab_w, 0, tab_w, 36, bg);
        int tw, th;
        ui_text_size(tab_labels[i], &tw, &th);
        ui_draw_text(r, tab_labels[i],
                     i * tab_w + (tab_w - tw) / 2, (36 - th) / 2, txt);
    }
    /* Separator line */
    SDL_SetRenderDrawColor(r, COLOR_DIM.r, COLOR_DIM.g, COLOR_DIM.b, COLOR_DIM.a);
    SDL_RenderDrawLine(r, 0, 36, w, 36);
}

/* ── Scan page ─────────────────────────────────────────────── */

static void render_scan_page(SDL_Renderer *r, struct app_state *state)
{
    int w = state->window_w, h = state->window_h;
    int list_y = 88, list_h = h - 128;

    /* Scan button */
    bool scan_clicked = ui_button(r, "Scan", 20, 46, 120, 32, 0, 0, false);
    /* (click handling is in event handler, not render) */

    /* Status text */
    ui_draw_text(r, state->scan_status, 160, 52, COLOR_DIM);

    /* Device list background */
    ui_draw_rect(r, 20, list_y, w - 40, list_h, COLOR_SURFACE);

    /* Header */
    ui_draw_text(r, "IP Address", 30, list_y + 5, COLOR_DIM);
    ui_draw_text(r, "Hostname", 220, list_y + 5, COLOR_DIM);

    /* Device entries */
    for (int i = 0; i < state->device_count; i++) {
        int ey = list_y + 28 + i * 30;
        if (ey > list_y + list_h - 30) break; /* scroll clamp */
        SDL_Color c = (i == state->selected_device) ? COLOR_ACCENT : COLOR_TEXT;
        ui_draw_text(r, state->devices[i].ip, 30, ey, c);
        ui_draw_text(r, state->devices[i].hostname, 220, ey, c);
    }

    if (state->device_count == 0) {
        ui_draw_text(r, "No devices found. Click Scan to search.",
                     30, list_y + 30, COLOR_DIM);
    }
}

/* ── Send page ─────────────────────────────────────────────── */

static void render_send_page(SDL_Renderer *r, struct app_state *state)
{
    int w = state->window_w;
    int y = 50;

    /* File path */
    ui_draw_text(r, "File:", 20, y + 4, COLOR_TEXT);
    ui_draw_rect(r, 80, y, w - 240, 28, COLOR_SURFACE);
    ui_draw_text(r, state->send_filepath, 86, y + 4,
                 state->send_filepath[0] ? COLOR_TEXT : COLOR_DIM);
    /* Browse button */
    ui_button(r, "Browse", w - 150, y, 80, 28, 0, 0, false);
    y += 40;

    /* Protocol */
    ui_draw_text(r, "Protocol:", 20, y + 4, COLOR_TEXT);
    ui_button(r, "TCP", 120, y, 60, 28, 0, 0, false);
    ui_button(r, "UDP", 190, y, 60, 28, 0, 0, false);
    /* Highlight selected */
    SDL_Color sel = COLOR_ACCENT;
    SDL_SetRenderDrawColor(r, sel.r, sel.g, sel.b, sel.a);
    SDL_Rect pr = {state->send_protocol == 0 ? 120 : 190, y, 60, 28};
    SDL_RenderDrawRect(r, &pr);
    y += 40;

    /* Target IP */
    ui_draw_text(r, "Target IP:", 20, y + 4, COLOR_TEXT);
    ui_draw_rect(r, 130, y, 200, 28, COLOR_SURFACE);
    ui_draw_text(r, state->send_target_ip, 136, y + 4,
                 state->send_target_ip[0] ? COLOR_TEXT : COLOR_DIM);
    y += 40;

    /* Send button */
    bool send_enabled = state->send_filepath[0] && state->send_target_ip[0];
    SDL_Color btn_text = send_enabled ? COLOR_TEXT : COLOR_DIM;
    ui_draw_rect(r, 20, y, 160, 32, send_enabled ? COLOR_ACCENT : COLOR_SURFACE);
    int tw, th;
    ui_text_size("Start Send", &tw, &th);
    ui_draw_text(r, "Start Send", 20 + (160 - tw) / 2, y + (32 - th) / 2,
                 send_enabled ? COLOR_BG : COLOR_DIM);
    y += 50;

    /* Progress bar */
    if (state->send_running) {
        int bar_w = w - 40;
        ui_draw_rect(r, 20, y, bar_w, 28, COLOR_SURFACE);
        if (state->send_progress_total > 0) {
            double pct = (double)state->send_progress_done / state->send_progress_total;
            int fill_w = (int)(bar_w * pct);
            if (fill_w > 0) ui_draw_rect(r, 20, y, fill_w, 28, COLOR_PROGRESS);
            char pbuf[128];
            snprintf(pbuf, sizeof(pbuf), "%.0f%% (%lu / %lu bytes)",
                     pct * 100.0,
                     (unsigned long)state->send_progress_done,
                     (unsigned long)state->send_progress_total);
            ui_draw_text(r, pbuf, 20 + (bar_w - tw) / 2, y + 4, COLOR_TEXT);
        }
    }
}

/* ── Receive page ──────────────────────────────────────────── */

static void render_receive_page(SDL_Renderer *r, struct app_state *state)
{
    int w = state->window_w;
    int y = 50;

    /* Save path */
    ui_draw_text(r, "Save to:", 20, y + 4, COLOR_TEXT);
    ui_draw_rect(r, 100, y, w - 260, 28, COLOR_SURFACE);
    ui_draw_text(r, state->recv_savepath, 106, y + 4,
                 state->recv_savepath[0] ? COLOR_TEXT : COLOR_DIM);
    ui_button(r, "Browse", w - 150, y, 80, 28, 0, 0, false);
    y += 40;

    /* Protocol */
    ui_draw_text(r, "Protocol:", 20, y + 4, COLOR_TEXT);
    ui_button(r, "TCP", 120, y, 60, 28, 0, 0, false);
    ui_button(r, "UDP", 190, y, 60, 28, 0, 0, false);
    SDL_Color sel = COLOR_ACCENT;
    SDL_SetRenderDrawColor(r, sel.r, sel.g, sel.b, sel.a);
    SDL_Rect pr = {state->recv_protocol == 0 ? 120 : 190, y, 60, 28};
    SDL_RenderDrawRect(r, &pr);
    y += 40;

    /* Listen button */
    bool recv_enabled = state->recv_savepath[0];
    SDL_Color btn_text = recv_enabled ? COLOR_TEXT : COLOR_DIM;
    ui_draw_rect(r, 20, y, 180, 32, recv_enabled ? COLOR_ACCENT : COLOR_SURFACE);
    int tw, th;
    ui_text_size("Listen & Receive", &tw, &th);
    ui_draw_text(r, "Listen & Receive", 20 + (180 - tw) / 2, y + (32 - th) / 2,
                 recv_enabled ? COLOR_BG : COLOR_DIM);
    y += 50;

    /* Progress bar */
    if (state->recv_running) {
        int bar_w = w - 40;
        ui_draw_rect(r, 20, y, bar_w, 28, COLOR_SURFACE);
        if (state->recv_progress_total > 0) {
            double pct = (double)state->recv_progress_done / state->recv_progress_total;
            int fill_w = (int)(bar_w * pct);
            if (fill_w > 0) ui_draw_rect(r, 20, y, fill_w, 28, COLOR_PROGRESS);
            char pbuf[128];
            snprintf(pbuf, sizeof(pbuf), "%.0f%% (%lu / %lu bytes)",
                     pct * 100.0,
                     (unsigned long)state->recv_progress_done,
                     (unsigned long)state->recv_progress_total);
            ui_draw_text(r, pbuf, 20 + (bar_w - tw) / 2, y + 4, COLOR_TEXT);
        }
    }
}

/* ── History page ──────────────────────────────────────────── */

static void render_history_page(SDL_Renderer *r, struct app_state *state)
{
    ui_draw_text(r, "Transfer History:", 20, 50, COLOR_TEXT);
    ui_draw_rect(r, 20, 80, state->window_w - 40, state->window_h - 130, COLOR_SURFACE);
    ui_draw_text(r, state->history_log, 30, 90, COLOR_TEXT);
}

/* ── Main render ───────────────────────────────────────────── */

void ui_render(SDL_Renderer *renderer, struct app_state *state)
{
    /* Background */
    ui_draw_rect(renderer, 0, 0, state->window_w, state->window_h, COLOR_BG);

    /* Tab bar */
    render_tab_bar(renderer, state);

    /* Current tab content */
    switch (state->current_tab) {
    case TAB_SCAN:    render_scan_page(renderer, state); break;
    case TAB_SEND:    render_send_page(renderer, state); break;
    case TAB_RECEIVE: render_receive_page(renderer, state); break;
    case TAB_HISTORY: render_history_page(renderer, state); break;
    }

    /* File browser overlay */
    if (state->file_browser_visible) {
        file_browser_render(renderer, state);
    }

    /* Modal overlay */
    if (state->modal_visible) {
        /* Dim background */
        ui_draw_rect(renderer, 0, 0, state->window_w, state->window_h,
                     (SDL_Color){0, 0, 0, 180});
        /* Modal box */
        int mx = state->window_w / 2 - 200, my = state->window_h / 2 - 60;
        ui_draw_rect(renderer, mx, my, 400, 120, COLOR_SURFACE);
        ui_draw_text(renderer, state->modal_message, mx + 20, my + 20, COLOR_TEXT);
        ui_button(renderer, "OK", mx + 160, my + 70, 80, 30, 0, 0, false);
    }

    /* Status bar */
    int sh = state->window_h - 28;
    ui_draw_rect(renderer, 0, sh, state->window_w, 28, COLOR_SURFACE);
    SDL_SetRenderDrawColor(renderer, COLOR_DIM.r, COLOR_DIM.g, COLOR_DIM.b, COLOR_DIM.a);
    SDL_RenderDrawLine(renderer, 0, sh, state->window_w, sh);
    ui_draw_text(renderer, state->status_text, 10, sh + 6, COLOR_DIM);

    SDL_RenderPresent(renderer);
}

/* ── Event handling ────────────────────────────────────────── */

bool ui_handle_event(SDL_Event *e, struct app_state *state)
{
    int mx = 0, my = 0;
    if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) {
        mx = e->button.x;
        my = e->button.y;
    }

    /* Tab bar clicks */
    if (e->type == SDL_MOUSEBUTTONDOWN) {
        int tab_w = state->window_w / TAB_COUNT;
        if (my < 36) {
            state->current_tab = mx / tab_w;
            return true;
        }
    }

    /* File browser overlay */
    if (state->file_browser_visible) {
        return file_browser_handle_event(e, state);
    }

    /* Modal overlay */
    if (state->modal_visible) {
        if (e->type == SDL_MOUSEBUTTONDOWN) {
            int bx = state->window_w / 2 - 200 + 160;
            int by = state->window_h / 2 - 60 + 70;
            if (ui_in_rect(mx, my, bx, by, 80, 30)) {
                state->modal_visible = false;
                return true;
            }
        }
        return true; /* eat all events when modal is up */
    }

    /* Per-tab clicks */
    switch (state->current_tab) {
    case TAB_SCAN:
        if (e->type == SDL_MOUSEBUTTONDOWN) {
            /* Scan button */
            if (ui_in_rect(mx, my, 20, 46, 120, 32)) {
                strncpy(state->scan_status, "Scanning...", sizeof(state->scan_status) - 1);
                state->device_count = 0;
                state->selected_device = -1;
                scanner_start(FT_DEFAULT_PORT);
                return true;
            }
            /* Device list items */
            int list_y = 88;
            for (int i = 0; i < state->device_count; i++) {
                int ey = list_y + 28 + i * 30;
                if (ui_in_rect(mx, my, 20, ey, state->window_w - 40, 28)) {
                    state->selected_device = i;
                    /* Copy IP to send target */
                    strncpy(state->send_target_ip, state->devices[i].ip,
                            sizeof(state->send_target_ip) - 1);
                    strncpy(state->status_text, "Device selected — switch to Send tab",
                            sizeof(state->status_text) - 1);
                    return true;
                }
            }
        }
        break;

    case TAB_SEND:
        if (e->type == SDL_MOUSEBUTTONDOWN) {
            /* Browse button */
            if (ui_in_rect(mx, my, state->window_w - 150, 50, 80, 28)) {
                state->file_browser_visible = true;
                state->file_browser_target = 1;
                file_browser_init(state);
                return true;
            }
            /* TCP button */
            if (ui_in_rect(mx, my, 120, 90, 60, 28)) {
                state->send_protocol = 0;
                return true;
            }
            /* UDP button */
            if (ui_in_rect(mx, my, 190, 90, 60, 28)) {
                state->send_protocol = 1;
                return true;
            }
            /* Target IP text field - click to edit */
            if (ui_in_rect(mx, my, 130, 130, 200, 28)) {
                state->active_input = 2;
                strncpy(state->input_buffer, state->send_target_ip,
                        sizeof(state->input_buffer) - 1);
                state->input_cursor = strlen(state->input_buffer);
                return true;
            }
            /* Send button */
            if (ui_in_rect(mx, my, 20, 170, 160, 32) &&
                state->send_filepath[0] && state->send_target_ip[0] &&
                !state->send_running) {
                state->send_running = true;
                state->send_progress_done = 0;
                state->send_progress_total = 0;
                strncpy(state->status_text, "Connecting...", sizeof(state->status_text) - 1);
                /* Spawn sender thread — done in main.c event loop */
                return true;
            }
        }
        break;

    case TAB_RECEIVE:
        if (e->type == SDL_MOUSEBUTTONDOWN) {
            /* Browse button */
            if (ui_in_rect(mx, my, state->window_w - 150, 50, 80, 28)) {
                state->file_browser_visible = true;
                state->file_browser_target = 2;
                file_browser_init(state);
                return true;
            }
            /* TCP / UDP buttons */
            if (ui_in_rect(mx, my, 120, 90, 60, 28)) {
                state->recv_protocol = 0;
                return true;
            }
            if (ui_in_rect(mx, my, 190, 90, 60, 28)) {
                state->recv_protocol = 1;
                return true;
            }
            /* Listen button */
            if (ui_in_rect(mx, my, 20, 130, 180, 32) &&
                state->recv_savepath[0] && !state->recv_running) {
                state->recv_running = true;
                state->recv_progress_done = 0;
                state->recv_progress_total = 0;
                strncpy(state->status_text, "Waiting for sender...",
                        sizeof(state->status_text) - 1);
                return true;
            }
        }
        break;
    }

    /* Keyboard input for text fields */
    if (e->type == SDL_TEXTINPUT && state->active_input > 0) {
        strncat(state->input_buffer, e->text.text,
                sizeof(state->input_buffer) - strlen(state->input_buffer) - 1);
        state->input_cursor = strlen(state->input_buffer);

        /* Sync to appropriate field */
        if (state->active_input == 2) {
            strncpy(state->send_target_ip, state->input_buffer,
                    sizeof(state->send_target_ip) - 1);
        }
        return true;
    }

    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_BACKSPACE && state->active_input > 0) {
            int len = strlen(state->input_buffer);
            if (len > 0 && state->input_cursor > 0) {
                memmove(state->input_buffer + state->input_cursor - 1,
                        state->input_buffer + state->input_cursor,
                        len - state->input_cursor + 1);
                state->input_cursor--;
            }
            if (state->active_input == 2) {
                strncpy(state->send_target_ip, state->input_buffer,
                        sizeof(state->send_target_ip) - 1);
            }
            return true;
        }
        if (e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_ESCAPE) {
            state->active_input = 0;
            return true;
        }
    }

    return false;
}
```

- [ ] **Step 3: Commit**

```bash
cd /home/zqw/biancheng/Project/websocket && git add src/ui.h src/ui.c && git commit -m "feat: add UI module with tabbed pages, buttons, progress bars"
```

---

### Task 7: file_browser.c/h — SDL2 file browser

**Files:**
- Create: `src/file_browser.h`
- Create: `src/file_browser.c`

- [ ] **Step 1: Write file_browser.h**

```c
#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H

#include <SDL2/SDL.h>
#include "ui.h"

void file_browser_init(struct app_state *state);
void file_browser_render(SDL_Renderer *r, struct app_state *state);
bool file_browser_handle_event(SDL_Event *e, struct app_state *state);

#endif
```

- [ ] **Step 2: Write file_browser.c**

```c
#include "file_browser.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FB_MAX_ENTRIES 512
#define FB_VISIBLE     15

static char fb_current_dir[1024];
static char fb_entries[FB_MAX_ENTRIES][256];
static int  fb_entry_types[FB_MAX_ENTRIES]; /* 0=dir, 1=file */
static int  fb_entry_count;
static int  fb_scroll_offset;

void file_browser_init(struct app_state *state)
{
    if (!getcwd(fb_current_dir, sizeof(fb_current_dir))) {
        strncpy(fb_current_dir, "/", sizeof(fb_current_dir) - 1);
    }
    fb_scroll_offset = 0;

    /* List entries */
    fb_entry_count = 0;
    DIR *d = opendir(fb_current_dir);
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d)) != NULL && fb_entry_count < FB_MAX_ENTRIES) {
        if (strcmp(de->d_name, ".") == 0) continue;
        /* Build full path */
        char full[1280];
        snprintf(full, sizeof(full), "%s/%s", fb_current_dir, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;

        strncpy(fb_entries[fb_entry_count], de->d_name, 255);
        fb_entry_types[fb_entry_count] = S_ISDIR(st.st_mode) ? 0 : 1;
        fb_entry_count++;
    }
    closedir(d);
}

void file_browser_render(SDL_Renderer *r, struct app_state *state)
{
    int w = state->window_w, h = state->window_h;
    int bx = w / 2 - 250, by = h / 2 - 180;
    int bw = 500, bh = 360;

    /* Dim background */
    ui_draw_rect(r, 0, 0, w, h, (SDL_Color){0, 0, 0, 180});

    /* Browser window */
    ui_draw_rect(r, bx, by, bw, bh, COLOR_SURFACE);

    /* Title bar */
    ui_draw_rect(r, bx, by, bw, 30, COLOR_ACCENT);
    ui_draw_text(r, "File Browser", bx + 10, by + 6, COLOR_BG);

    /* Current path */
    ui_draw_text(r, fb_current_dir, bx + 10, by + 36, COLOR_DIM);

    /* Separator */
    SDL_SetRenderDrawColor(r, COLOR_DIM.r, COLOR_DIM.g, COLOR_DIM.b, COLOR_DIM.a);
    SDL_RenderDrawLine(r, bx, by + 60, bx + bw, by + 60);

    /* Entry list */
    int list_y = by + 64;
    for (int i = fb_scroll_offset; i < fb_entry_count && i < fb_scroll_offset + FB_VISIBLE; i++) {
        int ey = list_y + (i - fb_scroll_offset) * 24;
        SDL_Color c = (fb_entry_types[i] == 0) ? COLOR_ACCENT : COLOR_TEXT;
        char label[280];
        snprintf(label, sizeof(label), "%s %s",
                 fb_entry_types[i] == 0 ? "[DIR]" : "     ", fb_entries[i]);
        ui_draw_text(r, label, bx + 10, ey, c);
    }

    /* Scroll indicators */
    if (fb_scroll_offset > 0)
        ui_draw_text(r, "▲", bx + bw - 20, list_y, COLOR_DIM);
    if (fb_scroll_offset + FB_VISIBLE < fb_entry_count)
        ui_draw_text(r, "▼", bx + bw - 20, list_y + FB_VISIBLE * 24, COLOR_DIM);

    /* Buttons */
    int by2 = by + bh - 36;
    SDL_SetRenderDrawColor(r, COLOR_DIM.r, COLOR_DIM.g, COLOR_DIM.b, COLOR_DIM.a);
    SDL_RenderDrawLine(r, bx, by2 - 4, bx + bw, by2 - 4);
    ui_button(r, "Open", bx + bw - 180, by2 - 2, 80, 28, 0, 0, false);
    ui_button(r, "Cancel", bx + bw - 90, by2 - 2, 80, 28, 0, 0, false);
}

bool file_browser_handle_event(SDL_Event *e, struct app_state *state)
{
    int w = state->window_w, h = state->window_h;
    int bx = w / 2 - 250, by = h / 2 - 180;
    int bw = 500, bh = 360;

    if (e->type == SDL_MOUSEBUTTONDOWN) {
        int mx = e->button.x, my = e->button.y;

        /* Cancel button */
        if (ui_in_rect(mx, my, bx + bw - 90, by + bh - 38, 80, 28)) {
            state->file_browser_visible = false;
            return true;
        }

        /* Open button */
        if (ui_in_rect(mx, my, bx + bw - 180, by + bh - 38, 80, 28)) {
            strncpy(state->file_browser_result, fb_current_dir,
                    sizeof(state->file_browser_result) - 1);
            if (state->file_browser_target == 1) {
                strncpy(state->send_filepath, state->file_browser_result,
                        sizeof(state->send_filepath) - 1);
                snprintf(state->status_text, sizeof(state->status_text),
                         "Selected: %s", state->send_filepath);
            } else if (state->file_browser_target == 2) {
                strncpy(state->recv_savepath, state->file_browser_result,
                        sizeof(state->recv_savepath) - 1);
                snprintf(state->status_text, sizeof(state->status_text),
                         "Save to: %s", state->recv_savepath);
            }
            state->file_browser_visible = false;
            return true;
        }

        /* Entry click */
        int list_y = by + 64;
        for (int i = fb_scroll_offset; i < fb_entry_count && i < fb_scroll_offset + FB_VISIBLE; i++) {
            int ey = list_y + (i - fb_scroll_offset) * 24;
            if (ui_in_rect(mx, my, bx + 10, ey, bw - 30, 22)) {
                if (fb_entry_types[i] == 0) {
                    /* Directory — navigate into it */
                    char newdir[1024];
                    snprintf(newdir, sizeof(newdir), "%s/%s", fb_current_dir, fb_entries[i]);
                    strncpy(fb_current_dir, newdir, sizeof(fb_current_dir) - 1);
                    fb_scroll_offset = 0;
                    file_browser_init(state);
                } else {
                    /* File — select it */
                    snprintf(state->file_browser_result, sizeof(state->file_browser_result),
                             "%s/%s", fb_current_dir, fb_entries[i]);
                    /* Also go up one level to select the directory */
                }
                return true;
            }
        }

        /* Click on path to go up */
        if (ui_in_rect(mx, my, bx + 10, by + 36, bw - 20, 22)) {
            char *slash = strrchr(fb_current_dir, '/');
            if (slash && slash != fb_current_dir) {
                *slash = '\0';
            } else if (slash == fb_current_dir) {
                fb_current_dir[1] = '\0';
            }
            fb_scroll_offset = 0;
            file_browser_init(state);
            return true;
        }

        /* Click outside = close */
        if (!ui_in_rect(mx, my, bx, by, bw, bh)) {
            state->file_browser_visible = false;
            return true;
        }
    }

    /* Scroll wheel */
    if (e->type == SDL_MOUSEWHEEL) {
        fb_scroll_offset -= e->wheel.y;
        if (fb_scroll_offset < 0) fb_scroll_offset = 0;
        if (fb_scroll_offset > fb_entry_count - FB_VISIBLE)
            fb_scroll_offset = fb_entry_count - FB_VISIBLE;
        if (fb_scroll_offset < 0) fb_scroll_offset = 0;
        return true;
    }

    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE) {
        state->file_browser_visible = false;
        return true;
    }

    return true; /* eat all events while visible */
}
```

- [ ] **Step 3: Commit**

```bash
cd /home/zqw/biancheng/Project/websocket && git add src/file_browser.h src/file_browser.c && git commit -m "feat: add SDL2 file browser with directory navigation"
```

---

### Task 8: main.c — SDL2 init, main loop, thread spawning

**Files:**
- Create: `src/main.c`

- [ ] **Step 1: Write main.c**

```c
#include "protocol.h"
#include "network.h"
#include "scanner.h"
#include "transfer.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <SDL2/SDL.h>

/* ── Transfer thread wrappers ──────────────────────────────── */

typedef struct {
    struct net_context *nc;
    char filepath[1024];
    int protocol;
} transfer_thread_args;

static void *send_thread(void *arg)
{
    transfer_thread_args *a = (transfer_thread_args *)arg;
    transfer_send(a->nc, a->filepath, a->protocol);
    net_destroy(a->nc);
    free(a);
    return NULL;
}

typedef struct {
    struct net_context *nc;
    char savepath[1024];
    int protocol;
} recv_thread_args;

static void *recv_thread(void *arg)
{
    recv_thread_args *a = (recv_thread_args *)arg;
    transfer_recv(a->nc, a->savepath, a->protocol);
    net_destroy(a->nc);
    free(a);
    return NULL;
}

/* ── Main ──────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* Init SDL */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "LAN File Transfer",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        800, 600,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* Init UI (SDL2_ttf) */
    if (ui_init() != 0) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* Init app state */
    struct app_state state;
    memset(&state, 0, sizeof(state));
    state.current_tab = TAB_SCAN;
    state.selected_device = -1;
    state.active_input = 0;
    state.send_protocol = FT_PROTO_TCP;
    state.recv_protocol = FT_PROTO_TCP;
    strncpy(state.status_text, "Ready — select a tab to begin", sizeof(state.status_text) - 1);

    /* Get initial window size */
    SDL_GetWindowSize(window, &state.window_w, &state.window_h);

    /* Main loop */
    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    state.window_w = event.window.data1;
                    state.window_h = event.window.data2;
                }
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    if (state.file_browser_visible) {
                        state.file_browser_visible = false;
                    } else if (state.modal_visible) {
                        state.modal_visible = false;
                    }
                }
                break;

            /* ── Custom events from worker threads ──────────────── */

            case USEREVENT_SCAN_FOUND: {
                struct event_scan_found *evt =
                    (struct event_scan_found *)event.user.data1;
                if (evt && state.device_count < 256) {
                    strncpy(state.devices[state.device_count].ip, evt->ip,
                            sizeof(state.devices[state.device_count].ip) - 1);
                    strncpy(state.devices[state.device_count].hostname, evt->hostname,
                            sizeof(state.devices[state.device_count].hostname) - 1);
                    state.device_count++;
                }
                free(evt);
                break;
            }

            case USEREVENT_SCAN_DONE: {
                struct event_scan_done *evt =
                    (struct event_scan_done *)event.user.data1;
                snprintf(state.scan_status, sizeof(state.scan_status),
                         "Scan complete — %d device(s) found", state.device_count);
                if (state.device_count == 0) {
                    snprintf(state.status_text, sizeof(state.status_text),
                             "No devices found on the LAN");
                }
                free(evt);
                break;
            }

            case USEREVENT_PROGRESS: {
                struct event_progress *evt =
                    (struct event_progress *)event.user.data1;
                if (evt) {
                    if (state.send_running) {
                        state.send_progress_done = evt->bytes_done;
                        state.send_progress_total = evt->bytes_total;
                    }
                    if (state.recv_running) {
                        state.recv_progress_done = evt->bytes_done;
                        state.recv_progress_total = evt->bytes_total;
                    }
                }
                free(evt);
                break;
            }

            case USEREVENT_XFER_DONE: {
                char entry[256];
                if (state.send_running) {
                    snprintf(entry, sizeof(entry),
                             "SENT  %s — %lu bytes — OK\n",
                             state.send_filepath,
                             (unsigned long)state.send_progress_total);
                    state.send_running = false;
                    strncpy(state.status_text, "Transfer complete!",
                            sizeof(state.status_text) - 1);
                }
                if (state.recv_running) {
                    snprintf(entry, sizeof(entry),
                             "RECV  %s — %lu bytes — OK\n",
                             state.recv_savepath,
                             (unsigned long)state.recv_progress_total);
                    state.recv_running = false;
                    strncpy(state.status_text, "Receive complete!",
                            sizeof(state.status_text) - 1);
                }
                /* Prepend to history */
                size_t hist_len = strlen(state.history_log);
                size_t entry_len = strlen(entry);
                if (hist_len + entry_len < sizeof(state.history_log) - 1) {
                    memmove(state.history_log + entry_len, state.history_log,
                            hist_len + 1);
                    memcpy(state.history_log, entry, entry_len);
                }
                break;
            }

            case USEREVENT_ERROR: {
                struct event_error *evt =
                    (struct event_error *)event.user.data1;
                if (evt) {
                    strncpy(state.modal_message, evt->message,
                            sizeof(state.modal_message) - 1);
                    state.modal_visible = true;
                    state.send_running = false;
                    state.recv_running = false;
                    snprintf(state.status_text, sizeof(state.status_text),
                             "Error: %s", evt->message);
                }
                free(evt);
                break;
            }

            default:
                /* Let UI handle it */
                if (!ui_handle_event(&event, &state)) {
                    /* UI didn't handle it — check for Send/Recv button clicks */
                    /* These are detected by checking state transitions post-UI */
                }
                break;
            }
        }

        /* ── Check for send/receive start requests ────────────── */

        /* Send: the UI sets send_running but we need to spawn the thread */
        if (state.send_running && state.send_progress_total == 0 &&
            !state.send_filepath[0] == 0 && !state.send_target_ip[0] == 0) {
            /* Actually, we need a better flag. Use a "pending start" flag. */
        }

        /* ── Render ──────────────────────────────────────────── */
        ui_render(renderer, &state);

        SDL_Delay(16); /* ~60 FPS */
    }

    /* Cleanup */
    ui_cleanup();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
```

Wait — there's a timing issue. The UI click handler sets `state.send_running = true`, but we need to actually spawn the thread from main.c. Let me rewrite this more cleanly.

- [ ] **Step 2: Rewrite main.c with proper thread spawning**

```c
#include "protocol.h"
#include "network.h"
#include "scanner.h"
#include "transfer.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <SDL2/SDL.h>

/* ── Transfer thread wrappers ──────────────────────────────── */

typedef struct {
    struct net_context *nc;
    char filepath[1024];
    int protocol;
} send_thread_args;

static void *send_thread_func(void *arg)
{
    send_thread_args *a = (send_thread_args *)arg;
    transfer_send(a->nc, a->filepath, a->protocol);
    net_destroy(a->nc);
    free(a);
    return NULL;
}

typedef struct {
    struct net_context *nc;
    char savepath[1024];
    int protocol;
} recv_thread_args;

static void *recv_thread_func(void *arg)
{
    recv_thread_args *a = (recv_thread_args *)arg;
    transfer_recv(a->nc, a->savepath, a->protocol);
    net_destroy(a->nc);
    free(a);
    return NULL;
}

static void start_send(struct app_state *state)
{
    /* Create network context and connect */
    struct net_context *nc = net_create(state->send_protocol);
    if (!nc) {
        struct event_error *err = calloc(1, sizeof(*err));
        snprintf(err->message, sizeof(err->message), "Failed to create network context");
        SDL_Event ev;
        SDL_memset(&ev, 0, sizeof(ev));
        ev.type = USEREVENT_ERROR;
        ev.user.data1 = err;
        SDL_PushEvent(&ev);
        return;
    }

    if (state->send_protocol == FT_PROTO_TCP) {
        /* TCP: server mode — listen and accept */
        if (net_listen(nc, FT_DEFAULT_PORT) != 0) {
            struct event_error *err = calloc(1, sizeof(*err));
            snprintf(err->message, sizeof(err->message), "Failed to listen on port %d", FT_DEFAULT_PORT);
            SDL_Event ev;
            SDL_memset(&ev, 0, sizeof(ev));
            ev.type = USEREVENT_ERROR;
            ev.user.data1 = err;
            SDL_PushEvent(&ev);
            net_destroy(nc);
            return;
        }
        /* The transfer_send function will call net_accept internally */
    } else {
        /* UDP: bind local port and set peer */
        if (net_udp_bind(nc, FT_DEFAULT_PORT) != 0) {
            struct event_error *err = calloc(1, sizeof(*err));
            snprintf(err->message, sizeof(err->message), "Failed to bind UDP port %d", FT_DEFAULT_PORT);
            SDL_Event ev;
            SDL_memset(&ev, 0, sizeof(ev));
            ev.type = USEREVENT_ERROR;
            ev.user.data1 = err;
            SDL_PushEvent(&ev);
            net_destroy(nc);
            return;
        }
        net_udp_set_peer(nc, state->send_target_ip, FT_DEFAULT_PORT);
    }

    send_thread_args *args = malloc(sizeof(*args));
    args->nc = nc;
    strncpy(args->filepath, state->send_filepath, sizeof(args->filepath) - 1);
    args->protocol = state->send_protocol;

    pthread_t tid;
    pthread_create(&tid, NULL, send_thread_func, args);
    pthread_detach(tid);
}

static void start_recv(struct app_state *state)
{
    struct net_context *nc = net_create(state->recv_protocol);
    if (!nc) {
        struct event_error *err = calloc(1, sizeof(*err));
        snprintf(err->message, sizeof(err->message), "Failed to create network context");
        SDL_Event ev;
        SDL_memset(&ev, 0, sizeof(ev));
        ev.type = USEREVENT_ERROR;
        ev.user.data1 = err;
        SDL_PushEvent(&ev);
        return;
    }

    if (state->recv_protocol == FT_PROTO_TCP) {
        /* TCP: connect to sender */
        /* We need the sender's IP — in receive mode, the user should
           know the sender's IP. For simplicity, use a default or from scan. */
        if (net_connect(nc, "127.0.0.1", FT_DEFAULT_PORT) != 0) {
            struct event_error *err = calloc(1, sizeof(*err));
            snprintf(err->message, sizeof(err->message),
                     "Failed to connect. Ensure sender is listening on port %d.",
                     FT_DEFAULT_PORT);
            SDL_Event ev;
            SDL_memset(&ev, 0, sizeof(ev));
            ev.type = USEREVENT_ERROR;
            ev.user.data1 = err;
            SDL_PushEvent(&ev);
            net_destroy(nc);
            return;
        }
    } else {
        /* UDP: bind and listen */
        if (net_udp_bind(nc, FT_DEFAULT_PORT) != 0) {
            struct event_error *err = calloc(1, sizeof(*err));
            snprintf(err->message, sizeof(err->message), "Failed to bind UDP port %d", FT_DEFAULT_PORT);
            SDL_Event ev;
            SDL_memset(&ev, 0, sizeof(ev));
            ev.type = USEREVENT_ERROR;
            ev.user.data1 = err;
            SDL_PushEvent(&ev);
            net_destroy(nc);
            return;
        }
    }

    recv_thread_args *args = malloc(sizeof(*args));
    args->nc = nc;
    strncpy(args->savepath, state->recv_savepath, sizeof(args->savepath) - 1);
    args->protocol = state->recv_protocol;

    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread_func, args);
    pthread_detach(tid);
}

/* ── Main ──────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "LAN File Transfer",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        800, 600,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (ui_init() != 0) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    struct app_state state;
    memset(&state, 0, sizeof(state));
    state.current_tab = TAB_SCAN;
    state.selected_device = -1;
    state.active_input = 0;
    state.send_protocol = FT_PROTO_TCP;
    state.recv_protocol = FT_PROTO_TCP;
    strncpy(state.status_text, "Ready — select a tab to begin",
            sizeof(state.status_text) - 1);
    SDL_GetWindowSize(window, &state.window_w, &state.window_h);

    /* Pending action flags */
    bool pending_send = false;
    bool pending_recv = false;

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    state.window_w = event.window.data1;
                    state.window_h = event.window.data2;
                }
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    if (state.file_browser_visible)
                        state.file_browser_visible = false;
                    else if (state.modal_visible)
                        state.modal_visible = false;
                    else
                        state.active_input = 0;
                }
                break;

            case USEREVENT_SCAN_FOUND: {
                struct event_scan_found *evt =
                    (struct event_scan_found *)event.user.data1;
                if (evt && state.device_count < 256) {
                    strncpy(state.devices[state.device_count].ip, evt->ip, 63);
                    strncpy(state.devices[state.device_count].hostname, evt->hostname, 255);
                    state.device_count++;
                }
                free(evt);
                break;
            }

            case USEREVENT_SCAN_DONE:
                snprintf(state.scan_status, sizeof(state.scan_status),
                         "Scan complete — %d device(s) found", state.device_count);
                if (state.device_count == 0)
                    snprintf(state.status_text, sizeof(state.status_text),
                             "No devices found on the LAN");
                free(event.user.data1);
                break;

            case USEREVENT_PROGRESS: {
                struct event_progress *evt =
                    (struct event_progress *)event.user.data1;
                if (evt) {
                    if (state.send_running) {
                        state.send_progress_done = evt->bytes_done;
                        state.send_progress_total = evt->bytes_total;
                    }
                    if (state.recv_running) {
                        state.recv_progress_done = evt->bytes_done;
                        state.recv_progress_total = evt->bytes_total;
                    }
                    char buf[128];
                    snprintf(buf, sizeof(buf), "Transferring... %lu / %lu bytes",
                             (unsigned long)evt->bytes_done,
                             (unsigned long)evt->bytes_total);
                    strncpy(state.status_text, buf, sizeof(state.status_text) - 1);
                }
                free(evt);
                break;
            }

            case USEREVENT_XFER_DONE: {
                char entry[256];
                snprintf(entry, sizeof(entry), "%s  %lu bytes  OK\n",
                         state.send_running ? "SENT" : "RECV",
                         (unsigned long)(state.send_running
                             ? state.send_progress_total
                             : state.recv_progress_total));
                size_t hl = strlen(state.history_log);
                size_t el = strlen(entry);
                if (hl + el < sizeof(state.history_log) - 1) {
                    memmove(state.history_log + el, state.history_log, hl + 1);
                    memcpy(state.history_log, entry, el);
                }
                state.send_running = false;
                state.recv_running = false;
                strncpy(state.status_text, "Transfer complete!",
                        sizeof(state.status_text) - 1);
                break;
            }

            case USEREVENT_ERROR: {
                struct event_error *evt =
                    (struct event_error *)event.user.data1;
                if (evt) {
                    strncpy(state.modal_message, evt->message,
                            sizeof(state.modal_message) - 1);
                    state.modal_visible = true;
                    state.send_running = false;
                    state.recv_running = false;
                    snprintf(state.status_text, sizeof(state.status_text),
                             "Error: %s", evt->message);
                }
                free(evt);
                break;
            }

            default:
                ui_handle_event(&event, &state);
                break;
            }
        }

        /* ── Detect send/receive button clicks ────────────── */
        /* The UI sets send_running/recv_running in the event handler.
           We detect the transition and spawn threads here. */
        /* Since ui_handle_event sets these flags synchronously,
           we check after event processing. Use a separate pending flag. */

        if (state.send_running && !pending_send &&
            state.send_progress_total == 0) {
            pending_send = true;
            strncpy(state.status_text, "Connecting to receiver...",
                    sizeof(state.status_text) - 1);
            start_send(&state);
        }
        if (!state.send_running) pending_send = false;

        if (state.recv_running && !pending_recv &&
            state.recv_progress_total == 0) {
            pending_recv = true;
            strncpy(state.status_text, "Waiting for sender...",
                    sizeof(state.status_text) - 1);
            start_recv(&state);
        }
        if (!state.recv_running) pending_recv = false;

        ui_render(renderer, &state);
        SDL_Delay(16);
    }

    ui_cleanup();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

- [ ] **Step 3: Build the full project**

```bash
cd /home/zqw/biancheng/Project/websocket/build && cmake .. && make 2>&1
```

Expected: Build succeeds with `lanft` executable produced.

- [ ] **Step 4: Fix compilation errors iteratively**

Run `make` in the build directory, and for each error:
- Check includes (e.g., `<stdatomic.h>` for `atomic_uint`)
- Fix type mismatches
- Verify all symbols are declared before use

- [ ] **Step 5: Commit**

```bash
cd /home/zqw/biancheng/Project/websocket && git add src/main.c && git commit -m "feat: add main.c with SDL2 main loop, thread spawning, and event routing"
```

---

### Task 9: Build, fix, and verify

- [ ] **Step 1: Full build**

```bash
cd /home/zqw/biancheng/Project/websocket/build && cmake .. && make -j$(nproc) 2>&1
```

- [ ] **Step 2: Fix any remaining compilation errors**

Common issues to anticipate:
- `atomic_uint` needs `<stdatomic.h>` in C11 mode (ensure `-std=c11` is set)
- `va_list` needs `<stdarg.h>` in transfer.c
- `SDL_USEREVENT` needs SDL2 include before protocol.h
- `ui_in_rect` vs `ui_button` signatures in file_browser.c
- Missing forward declarations for `struct app_state` in network.h context

- [ ] **Step 3: Add missing includes and fix issues**

In `src/transfer.c`, add at the top:
```c
#include <stdarg.h>
```

In `src/protocol.h`, wrap the SDL-dependent code:
```c
/* Include SDL before using SDL_USEREVENT */
#include <SDL2/SDL.h>

/* SDL custom event codes */
#define USEREVENT_SCAN_FOUND  (SDL_USEREVENT + 1)
/* ... etc */
```

- [ ] **Step 4: Verify executable runs**

```bash
cd /home/zqw/biancheng/Project/websocket/build && ./lanft 2>&1 &
```
Expected: SDL2 window opens with "LAN File Transfer" title and four tabs.

- [ ] **Step 5: Commit fixes**

```bash
cd /home/zqw/biancheng/Project/websocket && git add -A && git commit -m "fix: compilation fixes — includes, type declarations, SDL event handling"
```

- [ ] **Step 6: Fix — scanner must not block main thread**

In `src/scanner.h`, add a thread-spawning wrapper:
```c
/* Non-blocking: spawns a thread to run scanner_run */
void scanner_start(uint16_t port);
```

In `src/scanner.c`, change `scanner_start` to spawn a pthread:
```c
static void *scanner_thread_entry(void *arg) {
    uint16_t port = (uint16_t)(uintptr_t)arg;
    scanner_run(port);
    return NULL;
}

void scanner_start(uint16_t port) {
    pthread_t tid;
    pthread_create(&tid, NULL, scanner_thread_entry, (void *)(uintptr_t)port);
    pthread_detach(tid);
}
```

Rename the old `scanner_start` body to `scanner_run`.

- [ ] **Step 7: Fix — add target IP field to receive page**

In `src/ui.h`, add to `struct app_state`:
```c
char recv_target_ip[64];
```

In `src/ui.c`, in `render_receive_page()`, add after the protocol selection:
```c
/* Target IP (sender's address) */
ui_draw_text(r, "Sender IP:", 20, y + 4, COLOR_TEXT);
ui_draw_rect(r, 120, y, 200, 28, COLOR_SURFACE);
ui_draw_text(r, state->recv_target_ip, 126, y + 4,
             state->recv_target_ip[0] ? COLOR_TEXT : COLOR_DIM);
y += 40;
```

In `ui_handle_event`, add click handler for the receive IP text field:
```c
if (ui_in_rect(mx, my, 120, 130, 200, 28)) {
    state->active_input = 4;
    strncpy(state->input_buffer, state->recv_target_ip, sizeof(state->input_buffer) - 1);
    state->input_cursor = strlen(state->input_buffer);
    return true;
}
```

And update the keyboard handler to sync active_input 4 → recv_target_ip.

- [ ] **Step 8: Fix — main.c uses recv_target_ip instead of hardcoded IP**

In `src/main.c`, in `start_recv()`, change:
```c
if (net_connect(nc, "127.0.0.1", FT_DEFAULT_PORT) != 0) {
```
to:
```c
if (net_connect(nc, state->recv_target_ip, FT_DEFAULT_PORT) != 0) {
```

Also update the receive button guard to require `state->recv_target_ip[0]`:
```c
if (state->recv_running && !pending_recv && state->recv_progress_total == 0
    && state->recv_savepath[0] && state->recv_target_ip[0]) {
```

- [ ] **Step 9: Fix — protocol.h should not include SDL**

Move the `#include <SDL2/SDL.h>` and event code defines out of `src/protocol.h` and into `src/ui.h` (which already includes SDL). `protocol.h` stays SDL-free. Files that need event codes include `ui.h`.

Remove from protocol.h:
```c
#include <SDL2/SDL.h>
#define USEREVENT_SCAN_FOUND  (SDL_USEREVENT + 1)
#define USEREVENT_SCAN_DONE   (SDL_USEREVENT + 2)
#define USEREVENT_PROGRESS    (SDL_USEREVENT + 3)
#define USEREVENT_XFER_DONE   (SDL_USEREVENT + 4)
#define USEREVENT_ERROR       (SDL_USEREVENT + 5)
```

Add these defines to `src/ui.h` (after the `#include <SDL2/SDL.h>` line).

- [ ] **Step 10: Rebuild and verify**

```bash
cd /home/zqw/biancheng/Project/websocket/build && cmake .. && make -j$(nproc) 2>&1
```

Expected: Clean build. Run `./lanft` and verify the window opens.

- [ ] **Step 11: Commit remaining fixes**

```bash
cd /home/zqw/biancheng/Project/websocket && git add -A && git commit -m "fix: thread safety, receive page IP field, and header cleanup"
```
