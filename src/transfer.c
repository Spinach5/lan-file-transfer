#include "transfer.h"
#include "network.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <SDL2/SDL.h>

/* ── Helpers ───────────────────────────────────────────────── */

static void push_event(int code, void *data)
{
    SDL_Event event;
    SDL_memset(&event, 0, sizeof(event));
    event.type = code;
    event.user.data1 = data;
    SDL_PushEvent(&event);
}

static void push_error(const char *fmt, ...)
{
    struct event_error *err = calloc(1, sizeof(*err));
    if (!err) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err->message, sizeof(err->message), fmt, ap);
    va_end(ap);
    push_event(SDL_USEREVENT + 5, err);
}

static void push_progress(uint64_t done, uint64_t total)
{
    struct event_progress *p = calloc(1, sizeof(*p));
    if (!p) return;
    p->bytes_done = done;
    p->bytes_total = total;
    push_event(SDL_USEREVENT + 3, p);
}

static void push_xfer_done(void)
{
    push_event(SDL_USEREVENT + 4, NULL);
}

static uint64_t file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (uint64_t)st.st_size;
}

/* ── Meta exchange ─────────────────────────────────────────── */

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

/* ── TCP Send ──────────────────────────────────────────────── */

/* ── TCP send with direct socket I/O ──────────────────────── */

static int sock_read_full(int fd, void *buf, size_t len, int timeout_ms)
{
    size_t received = 0;
    uint8_t *p = (uint8_t *)buf;
    while (received < len) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        int ret = select(fd + 1, &fds, NULL, NULL, &tv);
        if (ret <= 0) return -1;
        ssize_t n = read(fd, p + received, len - received);
        if (n <= 0) return -1;
        received += n;
    }
    return 0;
}

static int sock_write_full(int fd, const void *buf, size_t len)
{
    size_t sent = 0;
    const uint8_t *p = (const uint8_t *)buf;
    while (sent < len) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        /* Wait up to 5s for writability */
        struct timeval tv = {5, 0};
        int ret = select(fd + 1, NULL, &fds, NULL, &tv);
        if (ret <= 0) return -1;
        ssize_t n = write(fd, p + sent, len - sent);
        if (n <= 0) return -1;
        sent += n;
    }
    return 0;
}

static void tcp_send_file(struct net_context *nc, const char *filepath,
                          uint64_t resume_offset)
{
    int fd = net_get_fd(nc);
    fprintf(stderr, "[SEND] tcp_send_file: fd=%d, file=%s\n", fd, filepath);
    if (fd < 0) { fprintf(stderr, "[SEND] BAD FD!\n"); push_error("No socket"); return; }

    FILE *fp = fopen(filepath, "rb");
    if (!fp) { fprintf(stderr, "[SEND] CANNOT OPEN FILE!\n"); push_error("Cannot open file: %s", filepath); return; }
    fprintf(stderr, "[SEND] file opened OK\n");

    fseek(fp, 0, SEEK_END);
    uint64_t total = (uint64_t)ftell(fp);
    rewind(fp);
    fprintf(stderr, "[SEND] file size=%lu bytes\n", (unsigned long)total);

    const char *fname = strrchr(filepath, '/');
    if (fname) fname++; else fname = filepath;

    /* Send meta */
    struct ft_meta meta;
    memset(&meta, 0, sizeof(meta));
    meta.magic = FT_MAGIC;
    meta.protocol = FT_PROTO_TCP;
    meta.name_len = (uint8_t)strlen(fname);
    strncpy(meta.filename, fname, sizeof(meta.filename) - 1);
    meta.total_size = total;

    fprintf(stderr, "[SEND] sending meta (name=%s, size=%lu)...\n", fname, (unsigned long)total);
    if (sock_write_full(fd, &meta, sizeof(meta)) != 0) {
        fprintf(stderr, "[SEND] FAILED to send meta!\n");
        push_error("Failed to send file metadata");
        fclose(fp);
        return;
    }
    fprintf(stderr, "[SEND] meta sent OK\n");

    /* Read meta response */
    fprintf(stderr, "[SEND] waiting for meta response...\n");
    struct ft_meta_resp resp;
    if (sock_read_full(fd, &resp, sizeof(resp), 10000) == 0 &&
        resp.magic == FT_MAGIC) {
        resume_offset = resp.resume_offset;
        fprintf(stderr, "[SEND] got meta response, resume_offset=%lu\n", (unsigned long)resume_offset);
    } else {
        fprintf(stderr, "[SEND] no valid meta response, starting from 0\n");
    }

    /* If receiver already has the complete file, skip */
    if (resume_offset >= total) {
        fprintf(stderr, "[SEND] file already complete on receiver, done\n");
        push_xfer_done();
        fclose(fp);
        return;
    }

    fseek(fp, (long)resume_offset, SEEK_SET);
    uint64_t sent = resume_offset;

    uint8_t buf[FT_TCP_CHUNK_SIZE];
    while (sent < total) {
        size_t to_read = (total - sent) > FT_TCP_CHUNK_SIZE
                         ? FT_TCP_CHUNK_SIZE : (size_t)(total - sent);
        size_t n = fread(buf, 1, to_read, fp);
        if (n == 0) break;

        if (sock_write_full(fd, buf, n) != 0) {
            fprintf(stderr, "[SEND] FAILED at %lu/%lu\n", (unsigned long)sent, (unsigned long)total);
            push_error("Send failed at %lu / %lu bytes",
                       (unsigned long)sent, (unsigned long)total);
            fclose(fp);
            return;
        }

        sent += n;
        fprintf(stderr, "[SEND] progress %lu/%lu\n", (unsigned long)sent, (unsigned long)total);
        push_progress(sent, total);
    }

    fclose(fp);

    fprintf(stderr, "[SEND] transfer done, sent=%lu/%lu\n", (unsigned long)sent, (unsigned long)total);
    if (sent >= total) push_xfer_done();
}

/* ── TCP receive with direct socket I/O ───────────────────── */

static void tcp_recv_file(struct net_context *nc, const char *savepath)
{
    int fd = net_get_fd(nc);
    fprintf(stderr, "[RECV] tcp_recv_file: fd=%d, save=%s\n", fd, savepath);
    if (fd < 0) { fprintf(stderr, "[RECV] BAD FD!\n"); push_error("No socket"); return; }

    /* Read meta */
    fprintf(stderr, "[RECV] waiting for meta...\n");
    struct ft_meta meta;
    if (sock_read_full(fd, &meta, sizeof(meta), 30000) != 0) {
        fprintf(stderr, "[RECV] FAILED to receive meta!\n");
        push_error("Failed to receive file metadata (timeout)");
        return;
    }
    fprintf(stderr, "[RECV] got meta: name=%s, size=%lu\n", meta.filename, (unsigned long)meta.total_size);

    if (meta.magic != FT_MAGIC) {
        push_error("Protocol mismatch — bad magic bytes");
        return;
    }

    /* Determine output path */
    char fullpath[1024];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", savepath, meta.filename);

    uint64_t local_size = file_size(fullpath);
    uint64_t resume_offset = 0;
    const char *mode = "wb";

    if (local_size > 0 && local_size < meta.total_size) {
        /* Partial file exists — resume from where we left off */
        resume_offset = local_size;
        mode = "ab";
    }
    /* else: local_size >= meta.total_size or file doesn't exist
       → start fresh (overwrite) with resume_offset=0 */

    /* Send meta response */
    struct ft_meta_resp resp;
    memset(&resp, 0, sizeof(resp));
    resp.magic = FT_MAGIC;
    resp.resume_offset = resume_offset;
    if (sock_write_full(fd, &resp, sizeof(resp)) != 0) {
        push_error("Failed to send meta response");
        return;
    }

    /* Open file for writing */
    FILE *fp = fopen(fullpath, mode);
    if (!fp) {
        push_error("Cannot create file: %s", fullpath);
        return;
    }

    fprintf(stderr, "[RECV] ready for data, total=%lu, resume=%lu\n", (unsigned long)meta.total_size, (unsigned long)resume_offset);
    uint64_t received = resume_offset;

    /* Receive file data */
    uint8_t buf[FT_TCP_CHUNK_SIZE];
    while (received < meta.total_size) {
        size_t to_read = (meta.total_size - received) > FT_TCP_CHUNK_SIZE
                         ? FT_TCP_CHUNK_SIZE : (size_t)(meta.total_size - received);

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = {30, 0};  /* 30s timeout */
        if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0) {
            push_error("Receive timeout at %lu / %lu bytes",
                       (unsigned long)received, (unsigned long)meta.total_size);
            fclose(fp);
            return;
        }

        ssize_t n = read(fd, buf, to_read);
        if (n <= 0) {
            push_error("Receive failed at %lu / %lu bytes",
                       (unsigned long)received, (unsigned long)meta.total_size);
            fclose(fp);
            return;
        }

        fwrite(buf, 1, n, fp);
        received += n;
        fprintf(stderr, "[RECV] progress %lu/%lu (got %zd bytes)\n", (unsigned long)received, (unsigned long)meta.total_size, n);
        push_progress(received, meta.total_size);
    }

    fclose(fp);
    fprintf(stderr, "[RECV] transfer done, received=%lu/%lu\n", (unsigned long)received, (unsigned long)meta.total_size);
    push_xfer_done();
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

    /* Send meta 3 times for reliability */
    struct ft_meta meta;
    memset(&meta, 0, sizeof(meta));
    meta.magic = FT_MAGIC;
    meta.protocol = FT_PROTO_UDP;
    meta.name_len = (uint8_t)strlen(fname);
    strncpy(meta.filename, fname, sizeof(meta.filename) - 1);
    meta.total_size = total;

    for (int i = 0; i < 3; i++) {
        net_send(nc, &meta, sizeof(meta));
        usleep(50000);
    }

    /* Wait for meta response */
    struct ft_meta_resp resp;
    memset(&resp, 0, sizeof(resp));
    bool got_resp = false;
    for (int retry = 0; retry < 20; retry++) {
        int n = net_udp_recv(nc, &resp, sizeof(resp), 250);
        if (n == sizeof(resp) && resp.magic == FT_MAGIC) {
            got_resp = true;
            break;
        }
    }
    if (!got_resp) resp.resume_offset = 0;

    /* Calculate packet count */
    uint64_t remaining = total - resp.resume_offset;
    uint32_t total_pkts = (uint32_t)((remaining + FT_CHUNK_SIZE - 1) / FT_CHUNK_SIZE);
    if (remaining == 0) total_pkts = 1;

    fseek(fp, (long)resp.resume_offset, SEEK_SET);

    bool *acked = calloc(total_pkts + 1, sizeof(bool));
    int *retries_arr = calloc(total_pkts + 1, sizeof(int));
    if (!acked || !retries_arr) {
        push_error("Out of memory");
        free(acked); free(retries_arr); fclose(fp);
        return;
    }

    uint32_t seq = 0;
    uint64_t sent_bytes = resp.resume_offset;

    while (seq < total_pkts) {
        struct ft_udp_pkt pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.seq = seq;
        pkt.total = total_pkts;

        size_t to_read = FT_CHUNK_SIZE;
        uint64_t offset = (uint64_t)seq * FT_CHUNK_SIZE;
        if (offset + FT_CHUNK_SIZE > remaining)
            to_read = (size_t)(remaining - offset);
        pkt.data_len = (uint16_t)to_read;
        fread(pkt.data, 1, to_read, fp);

        size_t pkt_size = offsetof(struct ft_udp_pkt, data) + to_read;
        net_send(nc, &pkt, pkt_size);

        /* Check for ACKs (non-blocking) */
        struct ft_udp_ack ack;
        int n = net_udp_recv(nc, &ack, sizeof(ack), 10);
        while (n == sizeof(ack) && ack.magic == FT_MAGIC) {
            if (ack.seq < total_pkts && !acked[ack.seq]) {
                acked[ack.seq] = true;
                uint64_t seg_size = (ack.seq == total_pkts - 1)
                    ? (remaining - (uint64_t)ack.seq * FT_CHUNK_SIZE)
                    : FT_CHUNK_SIZE;
                sent_bytes += seg_size;
                push_progress(sent_bytes, total);
            }
            n = net_udp_recv(nc, &ack, sizeof(ack), 5);
        }

        while (seq < total_pkts && acked[seq]) seq++;
        if (seq < total_pkts && !acked[seq]) {
            retries_arr[seq]++;
            if (retries_arr[seq] > FT_MAX_RETRIES) {
                push_error("UDP transfer failed: max retries for packet %u", seq);
                goto udp_cleanup;
            }
        }
    }

    /* Send EOF markers */
    struct ft_udp_pkt eof;
    memset(&eof, 0, sizeof(eof));
    eof.seq = total_pkts;
    eof.total = total_pkts;
    eof.data_len = 0;
    for (int i = 0; i < 5; i++) {
        net_send(nc, &eof, offsetof(struct ft_udp_pkt, data));
        usleep(100000);
    }

    push_xfer_done();

udp_cleanup:
    free(acked);
    free(retries_arr);
    fclose(fp);
}

/* ── UDP Receive ───────────────────────────────────────────── */

static void udp_recv_file(struct net_context *nc, const char *savepath)
{
    struct ft_meta meta;
    memset(&meta, 0, sizeof(meta));
    bool got_meta = false;

    for (int retry = 0; retry < 30; retry++) {
        int n = net_udp_recv(nc, &meta, sizeof(meta), 500);
        if (n == sizeof(meta) && meta.magic == FT_MAGIC) {
            got_meta = true;
            break;
        }
    }

    if (!got_meta) {
        push_error("Protocol mismatch — no valid meta received");
        return;
    }

    char fullpath[1024];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", savepath, meta.filename);

    uint64_t local_size = file_size(fullpath);
    uint64_t resume_offset = 0;
    const char *mode = "wb";

    if (local_size > 0 && local_size < meta.total_size) {
        resume_offset = local_size;
        mode = "ab";
    } else if (local_size >= meta.total_size) {
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

    struct ft_meta_resp resp;
    memset(&resp, 0, sizeof(resp));
    resp.magic = FT_MAGIC;
    resp.resume_offset = resume_offset;
    for (int i = 0; i < 3; i++) {
        net_send(nc, &resp, sizeof(resp));
        usleep(50000);
    }

    FILE *fp = fopen(fullpath, mode);
    if (!fp) {
        push_error("Cannot create file: %s", fullpath);
        return;
    }

    uint64_t received = resume_offset;
    bool *received_pkts = NULL;
    uint32_t total_pkts = 0;
    uint64_t file_data_start = resume_offset;

    struct ft_udp_pkt pkt;
    while (1) {
        int n = net_udp_recv(nc, &pkt, sizeof(pkt), 1000);
        if (n < (int)offsetof(struct ft_udp_pkt, data)) continue;

        if (pkt.data_len == 0) break;  /* EOF */

        if (!received_pkts && pkt.total > 0) {
            total_pkts = pkt.total;
            received_pkts = calloc(total_pkts, sizeof(bool));
            if (!received_pkts) {
                push_error("Out of memory");
                fclose(fp);
                return;
            }
        }

        if (received_pkts && pkt.seq < total_pkts && !received_pkts[pkt.seq]) {
            uint64_t offset = (uint64_t)pkt.seq * FT_CHUNK_SIZE + file_data_start;
            fseek(fp, (long)offset, SEEK_SET);
            fwrite(pkt.data, 1, pkt.data_len, fp);
            received_pkts[pkt.seq] = true;
            received += pkt.data_len;
            push_progress(received, meta.total_size);
        }

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
                   (unsigned long)received, (unsigned long)meta.total_size);
    }
}

/* ── Public API ────────────────────────────────────────────── */

void transfer_send(struct net_context *nc, const char *filepath, int protocol)
{
    fprintf(stderr, "[SEND] transfer_send start, proto=%d, file=%s\n", protocol, filepath);
    if (protocol == FT_PROTO_TCP) {
        fprintf(stderr, "[SEND] calling net_accept...\n");
        if (net_accept(nc) != 0) {
            fprintf(stderr, "[SEND] net_accept FAILED\n");
            push_error("Failed to accept client connection");
            return;
        }
        fprintf(stderr, "[SEND] net_accept OK, fd=%d, calling tcp_send_file...\n", net_get_fd(nc));
        tcp_send_file(nc, filepath, 0);
        fprintf(stderr, "[SEND] tcp_send_file returned\n");
    } else {
        udp_send_file(nc, filepath);
    }
}

void transfer_recv(struct net_context *nc, const char *savepath, int protocol)
{
    fprintf(stderr, "[RECV] transfer_recv start, proto=%d, save=%s\n", protocol, savepath);
    if (protocol == FT_PROTO_TCP) {
        fprintf(stderr, "[RECV] fd=%d, calling tcp_recv_file...\n", net_get_fd(nc));
        tcp_recv_file(nc, savepath);
        fprintf(stderr, "[RECV] tcp_recv_file returned\n");
    } else {
        udp_recv_file(nc, savepath);
    }
}
