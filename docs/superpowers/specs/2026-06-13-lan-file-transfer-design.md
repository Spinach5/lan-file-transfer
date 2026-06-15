# LAN File Transfer Tool — Design Spec

**Date:** 2026-06-13
**Language:** C
**Dependencies:** libwebsockets (raw socket mode), SDL2 + SDL2_ttf, CMake >= 3.10

---

## 1. Overview

A graphical LAN file transfer tool where two instances of the same app can send/receive files over TCP or UDP. The UI is an SDL2 window with tabbed pages for device scanning, sending, receiving, and history.

---

## 2. Architecture & Module Breakdown

```
src/
├── main.c              — SDL2 init, main event loop, tab routing, top-level orchestration
├── ui.h / ui.c         — Reusable UI components (button, text input, list, progress bar) + 4 page renderers
├── network.h / network.c — libwebsockets raw socket wrapper for TCP and UDP I/O
├── scanner.h / scanner.c — LAN TCP Connect scanner in a worker thread
├── transfer.h / transfer.c — File metadata, chunk I/O, resume offset logic, progress tracking
├── protocol.h          — Shared constants: default port, chunk sizes, magic bytes, packet header structs
└── file_browser.h / file_browser.c — In-window SDL2-rendered file/folder browser
```

### Responsibilities

| Module | Owns | Depends on |
|--------|------|------------|
| `main.c` | SDL2 window, event loop, tab state, spawning workers, routing events | all modules |
| `ui.c` | Rendering: buttons, text fields, scrollable list, progress bar, modal dialogs, page layouts | SDL2, SDL2_ttf |
| `file_browser.c` | Directory tree view, file list, selection callback | ui.c (draw primitives) |
| `network.c` | Raw TCP/UDP socket create/accept/connect/send/recv via libwebsockets lws_adopt_* API | libwebsockets, protocol.h |
| `scanner.c` | Build IP list from local interface, spawn per-IP TCP connect probes, collect results | network.h, pthreads |
| `transfer.c` | Open/read/write files, compute resume offset, invoke progress callbacks, chunk splitting for UDP | stdio, protocol.h |
| `file_browser.c` | In-window file/folder tree for selecting paths | ui.c (draw primitives) |

---

## 3. Thread Model & Data Flow

```
MAIN THREAD (SDL2 event loop)
  │
  ├─ SDL_PollEvent() → handle mouse/keyboard/window events
  ├─ Detect button clicks: spawn worker threads
  ├─ Pick up SDL_UserEvent from queue (SCAN_FOUND, PROGRESS, XFER_DONE, ERROR)
  ├─ Update UI state (progress %, device list, status text)
  └─ Render current tab

WORKER THREADS (one per operation)
  │
  ├─ Scanner:   TCP-connect each IP → SDL_PushEvent(SCAN_FOUND) per host → SDL_PushEvent(SCAN_DONE)
  ├─ Sender:    read chunk → send → SDL_PushEvent(PROGRESS) → loop → SDL_PushEvent(XFER_DONE)
  └─ Receiver:  recv chunk → write → SDL_PushEvent(PROGRESS) → loop → SDL_PushEvent(XFER_DONE)
```

**Thread safety**: Worker threads communicate with the main thread exclusively via `SDL_PushEvent()` with custom `SDL_UserEvent` codes. A heap-allocated struct carries event payload; ownership transfers to the main thread, which frees it after processing. No shared mutable state, no mutexes.

**Custom SDL event codes:**
```
USEREVENT_SCAN_FOUND   — one device discovered (IP, hostname)
USEREVENT_SCAN_DONE    — scan complete (total found)
USEREVENT_PROGRESS     — bytes sent/received update
USEREVENT_XFER_DONE    — transfer finished successfully
USEREVENT_ERROR        — error with message string
```

---

## 4. Network Module (`network.c`)

Uses libwebsockets in **raw socket mode** (`LWS_ADOPT_RAW_FILE_DESC`):

- **Server (sender)**: Creates a listening socket via lws, accepts a single connection, adopts the accepted fd as a raw lws wsi.
- **Client (receiver)**: Creates a socket, connects to target IP:port, adopts the connected fd as a raw lws wsi.
- **I/O**: `lws_callback_vhost_on_writable()` callback handles TX; `lws_callback_vhost_on_readable()` handles RX.
- The lws event loop runs inside the worker thread (not the main thread), using `lws_service()` in a loop until the transfer completes or errors.

### API
```c
// Server
int net_start_server(int port, struct lws_context **ctx, struct lws **wsi);
int net_accept_client(struct lws_context *ctx);  // blocks until one connection

// Client
int net_connect(const char *ip, int port, struct lws_context **ctx, struct lws **wsi);

// Data (TCP mode — raw bytes)
int net_send(struct lws *wsi, const void *data, size_t len);
int net_recv(struct lws *wsi, void *buf, size_t len);

// Data (UDP mode — uses lws raw socket but sendto/recvfrom)
int net_udp_send(struct lws *wsi, const void *data, size_t len);
int net_udp_recv(struct lws *wsi, void *buf, size_t len, struct sockaddr_in *src);

// Cleanup
void net_close(struct lws_context *ctx);
```

---

## 5. Scanner Module (`scanner.c`)

**Algorithm:**
1. Read local interface address via `getifaddrs()` → determine LAN subnet (e.g., 192.168.1.0/24).
2. Generate list of 254 IPs (192.168.1.1 through 192.168.1.254).
3. Spawn up to 32 threads, each consuming IPs from a shared atomic counter.
4. Each thread: `socket()` → `connect()` (non-blocking) → `select()` with 200ms timeout → if connected, push `SCAN_FOUND`.
5. After all IPs processed, push `SCAN_DONE`.

**API:**
```c
void scanner_start(uint16_t port);  // launches worker thread, pushes results via SDL events
```

---

## 6. Transfer Module (`transfer.c`)

### File Metainfo Handshake

Before file data, both sides exchange (sent by sender, replied by receiver):

```
Sender → Receiver:  | magic:4B("FT01") | proto:1B | name_len:1B | filename[name_len] | total_size:8B |
Receiver → Sender:  | magic:4B("FT01") | resume_offset:8B |
```

### TCP Transfer

- Sender reads file in 64 KiB chunks from resume_offset, calls `net_send()`.
- Receiver calls `net_recv()`, appends to file.
- Progress: `(bytes_transferred / total_size) * 100`.

### UDP Transfer

- Sender splits file into 8 KiB chunks, each with header: `[seq:4B][total:4B][data_len:2B][data[...]]`.
- Sliding window of 16 unacked packets — sender keeps sending until window fills, then waits for ACKs.
- Receiver sends ACK per received seq: `[ACK:4B][seq:4B]`.
- Retransmit timer: 500ms per unacked packet, max 10 retries per packet — then error.
- Last packet has `data_len = 0` (EOF marker).

### Resume Logic

- **Sender**: Opens file, sends metainfo; if receiver replies with `resume_offset > 0`, seeks to that offset and resumes sending.
- **Receiver**: On receiving metainfo, checks local save path for existing file. If found and size < total_size, sends `resume_offset = local_size`. If size >= total_size, sends `total_size` (skip). If not found, sends `0`.
- **Edge case**: If local file size > declared total_size, truncate and restart.

### API
```c
void transfer_send(const char *filepath, struct lws *wsi, int protocol);  // runs in worker thread
void transfer_recv(const char *savepath, struct lws *wsi, int protocol);  // runs in worker thread

typedef void (*progress_cb)(uint64_t sent, uint64_t total);
// Implemented as: SDL_PushEvent(USEREVENT_PROGRESS, sent, total)
```

---

## 7. UI Module (`ui.c`) & File Browser (`file_browser.c`)

### Reusable Components

| Component | Description |
|-----------|-------------|
| `ui_button` | Rect with label, hover highlight, click detection |
| `ui_text_input` | Single-line text field with cursor, supports typing and backspace |
| `ui_list` | Scrollable list of text items, clickable selection |
| `ui_progress_bar` | Horizontal filled rectangle + percent/total text overlay |
| `ui_tab_bar` | Row of tab buttons, highlights active tab |
| `ui_modal` | Centered overlay with message and OK button |
| `ui_label` | Single line of TTF-rendered text |

### Page Renderers

- **`ui_render_scan_page()`**: Scan button → list of found devices (IP + hostname), click to select.
- **`ui_render_send_page()`**: File path (text + [Browse] button), protocol radio (TCP/UDP), target IP (text input, auto-filled from scan selection), [Send] button, progress bar (hidden until transfer starts).
- **`ui_render_receive_page()`**: Save path (text + [Browse] button), protocol radio (TCP/UDP), [Listen & Receive] button, progress bar.
- **`ui_render_history_page()`**: Scrollable text log of past transfers.

### File Browser

Modal overlay:
- Left panel: directory tree (collapsible folders)
- Right panel: files in selected directory
- [Open] / [Cancel] buttons
- Navigation via keyboard arrows + Enter

### Font & Colors

- Font: SDL2_ttf with system monospace (e.g., `DejaVu Sans Mono`) at 14pt
- Colors: Dark theme — background `#1e1e2e`, surface `#313244`, text `#cdd6f4`, accent `#89b4fa`, error `#f38ba8`, progress fill `#a6e3a1`

---

## 8. Protocol Constants (`protocol.h`)

```c
#define FT_MAGIC          0x31305446   // "FT01" big-endian
#define FT_DEFAULT_PORT   9876
#define FT_CHUNK_SIZE     8192         // 8 KiB for UDP
#define FT_TCP_CHUNK_SIZE 65536        // 64 KiB for TCP reads
#define FT_UDP_WINDOW     16
#define FT_UDP_TIMEOUT_MS 500
#define FT_MAX_RETRIES    10
#define FT_MAX_FILENAME   256
```

---

## 9. Error Handling

| Scenario | Handling |
|----------|----------|
| Network disconnect mid-transfer | Worker detects send/recv error, pushes `USEREVENT_ERROR`. Modal shown. Partial file preserved for resume. |
| File not found / permission denied | Validated before spawn; error modal shown immediately. |
| Scan returns no devices | `SCAN_DONE` with count=0. UI shows "No devices found" message. |
| UDP packet loss (max retries exceeded) | Treated as connection failure → `USEREVENT_ERROR`. |
| File size mismatch on resume | If local > declared, truncate and restart. If local < declared but different file, resume offset mismatch detected by user (manual restart). |
| Disk full on write | `fwrite()` fails → `USEREVENT_ERROR`. Partial file kept. |
| Window resize | Recompute all component positions relative to new dimensions in render functions. |
| Bad magic bytes | Receiver discards connection, pushes error: "Protocol mismatch — ensure both sides run compatible versions." |

---

## 10. Build Configuration

CMakeLists.txt will:

1. Require CMake >= 3.10
2. Find SDL2, SDL2_ttf via `FindSDL2.cmake` and `pkg-config`
3. Find libwebsockets via `pkg-config`
4. Link pthreads
5. Output: single executable `lanft`

Note: `SDL2_ttf` development headers (`libsdl2-ttf-dev`) must be installed before building.

---

## 11. What's Not Included (YAGNI)

- Encryption / TLS
- Multicast / broadcast auto-discovery (only TCP connect scan)
- Transfer queue / batch files
- Bandwidth throttling
- File integrity verification via checksum (TCP reliability + UDP ACK is sufficient)
- CLI mode
- Save/load transfer history to disk (in-memory only)
