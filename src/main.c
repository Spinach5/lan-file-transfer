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

/* ═══════════════════════════════════════════════════════════
   Transfer thread wrappers
   ═══════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════
   Network orchestration — called from main thread
   ═══════════════════════════════════════════════════════════ */

static void start_send(struct app_state *state)
{
    struct net_context *nc = net_create(state->send_protocol);
    if (!nc) {
        struct event_error *err = calloc(1, sizeof(*err));
        snprintf(err->message, sizeof(err->message), "Failed to create network context");
        SDL_Event ev; SDL_memset(&ev, 0, sizeof(ev));
        ev.type = USEREVENT_ERROR; ev.user.data1 = err;
        SDL_PushEvent(&ev);
        return;
    }

    if (state->send_protocol == FT_PROTO_TCP) {
        if (net_listen(nc, state->send_port) != 0) {
            struct event_error *err = calloc(1, sizeof(*err));
            snprintf(err->message, sizeof(err->message), "Failed to listen on port %d", state->send_port);
            SDL_Event ev; SDL_memset(&ev, 0, sizeof(ev));
            ev.type = USEREVENT_ERROR; ev.user.data1 = err;
            SDL_PushEvent(&ev);
            net_destroy(nc);
            return;
        }
    } else {
        if (net_udp_bind(nc, state->send_port) != 0) {
            struct event_error *err = calloc(1, sizeof(*err));
            snprintf(err->message, sizeof(err->message), "Failed to bind UDP port %d", state->send_port);
            SDL_Event ev; SDL_memset(&ev, 0, sizeof(ev));
            ev.type = USEREVENT_ERROR; ev.user.data1 = err;
            SDL_PushEvent(&ev);
            net_destroy(nc);
            return;
        }
        net_udp_set_peer(nc, state->send_target_ip, state->send_port);
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
        SDL_Event ev; SDL_memset(&ev, 0, sizeof(ev));
        ev.type = USEREVENT_ERROR; ev.user.data1 = err;
        SDL_PushEvent(&ev);
        return;
    }

    if (state->recv_protocol == FT_PROTO_TCP) {
        if (net_connect(nc, state->recv_target_ip, state->recv_port) != 0) {
            struct event_error *err = calloc(1, sizeof(*err));
            snprintf(err->message, sizeof(err->message),
                     "Failed to connect to %s:%d. Ensure sender is listening.",
                     state->recv_target_ip, state->recv_port);
            SDL_Event ev; SDL_memset(&ev, 0, sizeof(ev));
            ev.type = USEREVENT_ERROR; ev.user.data1 = err;
            SDL_PushEvent(&ev);
            net_destroy(nc);
            return;
        }
    } else {
        if (net_udp_bind(nc, state->recv_port) != 0) {
            struct event_error *err = calloc(1, sizeof(*err));
            snprintf(err->message, sizeof(err->message), "Failed to bind UDP port %d", state->recv_port);
            SDL_Event ev; SDL_memset(&ev, 0, sizeof(ev));
            ev.type = USEREVENT_ERROR; ev.user.data1 = err;
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

/* ═══════════════════════════════════════════════════════════
   Main
   ═══════════════════════════════════════════════════════════ */

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

    /* Init app state */
    struct app_state state;
    memset(&state, 0, sizeof(state));
    state.current_tab = TAB_SCAN;
    state.selected_device = -1;
    state.active_input = 0;
    state.send_port = FT_DEFAULT_PORT;
    state.recv_port = FT_DEFAULT_PORT;
    state.send_protocol = FT_PROTO_TCP;
    state.recv_protocol = FT_PROTO_TCP;
    strncpy(state.status_text, "Ready — select a tab to begin",
            sizeof(state.status_text) - 1);
    SDL_GetWindowSize(window, &state.window_w, &state.window_h);

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
                    else {
                        state.active_input = 0;
                        SDL_StopTextInput();
                    }
                    break;  /* ESC handled */
                }
                /* Pass other keys to UI handler */
                ui_handle_event(&event, &state);
                break;

            /* ── Custom events from worker threads ────── */

            case USEREVENT_SCAN_FOUND: {
                struct event_scan_found *evt = (struct event_scan_found *)event.user.data1;
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
                struct event_progress *evt = (struct event_progress *)event.user.data1;
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
                             (unsigned long)evt->bytes_done, (unsigned long)evt->bytes_total);
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
                             ? state.send_progress_total : state.recv_progress_total));
                size_t hl = strlen(state.history_log);
                size_t el = strlen(entry);
                if (hl + el < sizeof(state.history_log) - 1) {
                    memmove(state.history_log + el, state.history_log, hl + 1);
                    memcpy(state.history_log, entry, el);
                }
                state.send_running = false;
                state.recv_running = false;
                pending_send = false;
                pending_recv = false;
                strncpy(state.status_text, "Transfer complete!", sizeof(state.status_text) - 1);
                break;
            }

            case USEREVENT_ERROR: {
                struct event_error *evt = (struct event_error *)event.user.data1;
                if (evt) {
                    strncpy(state.modal_message, evt->message, sizeof(state.modal_message) - 1);
                    state.modal_visible = true;
                    state.send_running = false;
                    state.recv_running = false;
                    pending_send = false;
                    pending_recv = false;
                    snprintf(state.status_text, sizeof(state.status_text), "Error: %s", evt->message);
                }
                free(evt);
                break;
            }

            default:
                ui_handle_event(&event, &state);
                break;
            }
        }

        /* ── Detect send/receive start requests ────────── */

        if (state.send_running && !pending_send && state.send_progress_total == 0) {
            pending_send = true;
            start_send(&state);
        }
        if (!state.send_running) pending_send = false;

        if (state.recv_running && !pending_recv && state.recv_progress_total == 0) {
            pending_recv = true;
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
