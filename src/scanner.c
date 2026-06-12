#define _POSIX_C_SOURCE 200112L

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
} scan_thread_args;

static void *scan_ip_thread(void *arg)
{
    scan_thread_args *args = (scan_thread_args *)arg;
    unsigned int idx;
    char ip[128];

    while (1) {
        idx = atomic_fetch_add(args->counter, 1);
        if (idx >= 1 && idx <= 254) {
            snprintf(ip, sizeof(ip), "%s.%u", args->subnet, idx);
        } else {
            break;
        }

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) continue;

        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(args->port);
        inet_pton(AF_INET, ip, &addr.sin_addr);

        connect(fd, (struct sockaddr *)&addr, sizeof(addr));

        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000;

        if (select(fd + 1, NULL, &wfds, NULL, &tv) > 0) {
            int err = 0;
            socklen_t errlen = sizeof(err);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
            if (err == 0) {
                struct event_scan_found *evt = calloc(1, sizeof(*evt));
                strncpy(evt->ip, ip, sizeof(evt->ip) - 1);
                const char *host = reverse_dns(ip);
                strncpy(evt->hostname, host, sizeof(evt->hostname) - 1);

                SDL_Event event;
                SDL_memset(&event, 0, sizeof(event));
                event.type = SDL_USEREVENT + 1;  /* USEREVENT_SCAN_FOUND */
                event.user.data1 = evt;
                SDL_PushEvent(&event);
            }
        }
        close(fd);
    }
    return NULL;
}

/* The real scan logic — runs in a worker thread */
static void scanner_run(uint16_t port)
{
    char subnet[64];
    if (get_local_subnet(subnet, sizeof(subnet)) != 0) {
        struct event_error *err = calloc(1, sizeof(*err));
        snprintf(err->message, sizeof(err->message),
                 "Cannot determine local subnet");
        SDL_Event event;
        SDL_memset(&event, 0, sizeof(event));
        event.type = SDL_USEREVENT + 5;  /* USEREVENT_ERROR */
        event.user.data1 = err;
        SDL_PushEvent(&event);
        return;
    }

    atomic_uint counter;
    atomic_init(&counter, 1);

    scan_thread_args args;
    args.counter = &counter;
    args.port = port;
    strncpy(args.subnet, subnet, sizeof(args.subnet) - 1);

    pthread_t threads[SCANNER_THREADS];
    for (int i = 0; i < SCANNER_THREADS; i++) {
        pthread_create(&threads[i], NULL, scan_ip_thread, &args);
    }
    for (int i = 0; i < SCANNER_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* All done */
    struct event_scan_done *done = calloc(1, sizeof(*done));
    done->total_found = -1;

    SDL_Event event;
    SDL_memset(&event, 0, sizeof(event));
    event.type = SDL_USEREVENT + 2;  /* USEREVENT_SCAN_DONE */
    event.user.data1 = done;
    SDL_PushEvent(&event);
}

/* Entry point for the scanner thread */
static void *scanner_thread_entry(void *arg)
{
    uint16_t port = (uint16_t)(uintptr_t)arg;
    scanner_run(port);
    return NULL;
}

/* Non-blocking: spawns scanner in its own thread */
void scanner_start(uint16_t port)
{
    pthread_t tid;
    pthread_create(&tid, NULL, scanner_thread_entry, (void *)(uintptr_t)port);
    pthread_detach(tid);
}
