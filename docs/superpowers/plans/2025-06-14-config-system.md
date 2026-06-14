# Config System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add TOML-based layered config system (CLI args > user config > system config > built-in defaults) using tomlc17.

**Architecture:** New `config.c/h` module handles TOML parsing and the priority chain. `cli.c` calls `config_load()` then applies CLI overrides. `main.c` calls `config_load()` to init `app_state` defaults. `tomlc17` is a single .c/.h embedded in `src/tomlc17/`.

**Tech Stack:** C11, tomlc17 (MIT, single-file), CMake

---

## File Structure

| File | Role |
|------|------|
| `src/config.h` | **New** — `lanft_config` struct + API |
| `src/config.c` | **New** — load chain, save, print, path helpers |
| `src/tomlc17/tomlc17.c` | Already present — TOML parser |
| `src/tomlc17/tomlc17.h` | Already present — TOML parser header |
| `src/cli.c` | Modify — replace `cli_config` with `lanft_config`, add new CLI args |
| `src/main.c` | Modify — call `config_load()` on GUI startup |
| `CMakeLists.txt` | Modify — add tomlc17.c + config.c to base SOURCES |

---

### Task 1: Create `src/config.h` — config struct and API

**Files:**
- Create: `src/config.h`

- [ ] **Step 1: Write config.h**

```c
#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/* Protocol constants (shared with protocol.h) */
#ifndef FT_PROTO_TCP
#define FT_PROTO_TCP  0
#define FT_PROTO_UDP  1
#endif

struct lanft_config {
    /* Core */
    int  port;                  /* default: 9876 */
    int  protocol;              /* FT_PROTO_TCP or FT_PROTO_UDP, default: TCP */
    char address[64];           /* default: "0.0.0.0" */
    char save_dir[1024];        /* default: "~/Downloads" */
    int  mode;                  /* 0=send, 1=receive, -1=unspecified */

    /* Advanced */
    int  buffer_size;           /* default: 65536 */
    int  timeout_seconds;       /* default: 30 */
    int  max_connections;       /* default: 5 */
    bool auto_accept;           /* default: false */
    char overwrite_policy[16];  /* "rename" | "overwrite" | "skip", default: "rename" */
    bool show_progress;         /* default: true */

    /* Discovery */
    bool discovery_enabled;     /* default: true */
    int  discovery_interval;    /* seconds, default: 5 */
    int  discovery_ttl;         /* default: 1 */

    /* Logging */
    char log_level[16];         /* "debug"|"info"|"warn"|"error", default: "info" */
    char log_file[1024];        /* empty = stderr */

    /* Bandwidth */
    int  send_bandwidth_limit;  /* bytes/sec, 0 = unlimited */
};

/* Set all fields to built-in defaults */
void config_set_defaults(struct lanft_config *cfg);

/* Load config following the priority chain:
   1. Built-in defaults
   2. /etc/lanft/config.toml (system) — skip if missing
   3. ~/.config/lanft/config.toml (user) — if missing, copy from system if available
   Returns 0 on success. Non-zero means a parsed config had syntax errors. */
int config_load(struct lanft_config *cfg);

/* Load a single TOML file and apply its values on top of cfg.
   Used for --config=PATH to load a custom config file.
   Returns 0 on success, -1 if file not found or parse error. */
int config_load_file(struct lanft_config *cfg, const char *path);

/* Save current config to ~/.config/lanft/config.toml.
   Creates parent directories. Returns 0 on success. */
int config_save(const struct lanft_config *cfg);

/* Print effective config as TOML to stdout. */
void config_print(const struct lanft_config *cfg);

/* Get paths. Return static buffers. */
const char *config_user_path(void);
const char *config_system_path(void);

/* Expand ~ in a path to $HOME. Returns a static buffer. */
const char *config_expand_path(const char *path);

#endif /* CONFIG_H */
```

- [ ] **Step 2: Commit**

```bash
git add src/config.h
git commit -m "feat: add config.h with lanft_config struct and API"
```

---

### Task 2: Create `src/config.c` — config loading, saving, printing

**Files:**
- Create: `src/config.c`

- [ ] **Step 1: Write config.c — includes and path helpers**

```c
#include "config.h"
#include "compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#ifdef _WIN32
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

#include "tomlc17/tomlc17.h"

/* ── Path helpers ───────────────────────────────────────────── */

const char *config_system_path(void)
{
    return "/etc/lanft/config.toml";
}

#ifdef _WIN32
const char *config_user_path(void)
{
    static char path[512];
    const char *appdata = getenv("APPDATA");
    if (appdata) {
        snprintf(path, sizeof(path), "%s/lanft/config.toml", appdata);
    } else {
        snprintf(path, sizeof(path), "lanft_config.toml");
    }
    return path;
}
#else
const char *config_user_path(void)
{
    static char path[512];
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(path, sizeof(path), "%s/lanft/config.toml", xdg);
    } else {
        const char *home = getenv("HOME");
        if (!home) home = "/tmp";
        snprintf(path, sizeof(path), "%s/.config/lanft/config.toml", home);
    }
    return path;
}
#endif

const char *config_expand_path(const char *path)
{
    static char buf[1024];
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\\' || path[1] == '\0')) {
        const char *home = getenv("HOME");
#ifdef _WIN32
        if (!home) home = getenv("USERPROFILE");
#endif
        if (!home) home = ".";
        snprintf(buf, sizeof(buf), "%s%s", home, path + 1);
        return buf;
    }
    return path;
}
```

- [ ] **Step 2: Write config_set_defaults()**

```c
void config_set_defaults(struct lanft_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->port               = 9876;
    cfg->protocol           = FT_PROTO_TCP;
    strncpy(cfg->address, "0.0.0.0", sizeof(cfg->address) - 1);
    strncpy(cfg->save_dir, "~/Downloads", sizeof(cfg->save_dir) - 1);
    cfg->mode               = -1;
    cfg->buffer_size        = 65536;
    cfg->timeout_seconds    = 30;
    cfg->max_connections    = 5;
    cfg->auto_accept        = false;
    strncpy(cfg->overwrite_policy, "rename", sizeof(cfg->overwrite_policy) - 1);
    cfg->show_progress      = true;
    cfg->discovery_enabled  = true;
    cfg->discovery_interval = 5;
    cfg->discovery_ttl      = 1;
    strncpy(cfg->log_level, "info", sizeof(cfg->log_level) - 1);
    cfg->log_file[0]        = '\0';
    cfg->send_bandwidth_limit = 0;
}
```

- [ ] **Step 3: Write the TOML-to-struct helper**

```c
/* Apply values from a parsed TOML table onto cfg. Overwrites only keys present. */
static void apply_toml_table(struct lanft_config *cfg, toml_datum_t table)
{
    toml_datum_t v;

    v = toml_get(table, "port");
    if (v.type == TOML_INT64) cfg->port = (int)v.u.int64;

    v = toml_get(table, "protocol");
    if (v.type == TOML_STRING) {
        if (strcasecmp(v.u.s, "UDP") == 0)
            cfg->protocol = FT_PROTO_UDP;
        else
            cfg->protocol = FT_PROTO_TCP;
    }

    v = toml_get(table, "address");
    if (v.type == TOML_STRING) {
        strncpy(cfg->address, v.u.s, sizeof(cfg->address) - 1);
    }

    v = toml_get(table, "save_dir");
    if (v.type == TOML_STRING) {
        strncpy(cfg->save_dir, v.u.s, sizeof(cfg->save_dir) - 1);
    }

    v = toml_get(table, "mode");
    if (v.type == TOML_STRING) {
        if (strcasecmp(v.u.s, "S") == 0 || strcasecmp(v.u.s, "send") == 0)
            cfg->mode = 0;
        else if (strcasecmp(v.u.s, "R") == 0 || strcasecmp(v.u.s, "receive") == 0)
            cfg->mode = 1;
    }

    v = toml_get(table, "buffer_size");
    if (v.type == TOML_INT64) cfg->buffer_size = (int)v.u.int64;

    v = toml_get(table, "timeout_seconds");
    if (v.type == TOML_INT64) cfg->timeout_seconds = (int)v.u.int64;

    v = toml_get(table, "max_connections");
    if (v.type == TOML_INT64) cfg->max_connections = (int)v.u.int64;

    v = toml_get(table, "auto_accept");
    if (v.type == TOML_BOOLEAN) cfg->auto_accept = v.u.boolean;

    v = toml_get(table, "overwrite_policy");
    if (v.type == TOML_STRING) {
        strncpy(cfg->overwrite_policy, v.u.s, sizeof(cfg->overwrite_policy) - 1);
    }

    v = toml_get(table, "show_progress");
    if (v.type == TOML_BOOLEAN) cfg->show_progress = v.u.boolean;

    v = toml_get(table, "discovery_enabled");
    if (v.type == TOML_BOOLEAN) cfg->discovery_enabled = v.u.boolean;

    v = toml_get(table, "discovery_interval");
    if (v.type == TOML_INT64) cfg->discovery_interval = (int)v.u.int64;

    v = toml_get(table, "discovery_ttl");
    if (v.type == TOML_INT64) cfg->discovery_ttl = (int)v.u.int64;

    v = toml_get(table, "log_level");
    if (v.type == TOML_STRING) {
        strncpy(cfg->log_level, v.u.s, sizeof(cfg->log_level) - 1);
    }

    v = toml_get(table, "log_file");
    if (v.type == TOML_STRING) {
        strncpy(cfg->log_file, v.u.s, sizeof(cfg->log_file) - 1);
    }

    v = toml_get(table, "send_bandwidth_limit");
    if (v.type == TOML_INT64) cfg->send_bandwidth_limit = (int)v.u.int64;
}
```

- [ ] **Step 4: Write config_load() — the priority chain**

```c
int config_load(struct lanft_config *cfg)
{
    config_set_defaults(cfg);

    /* ── Layer 1: System config (/etc/lanft/config.toml) ──── */
    toml_result_t sys_res = toml_parse_file_ex(config_system_path());
    if (sys_res.ok) {
        apply_toml_table(cfg, sys_res.toptab);
        toml_free(sys_res);
        fprintf(stderr, "[config] loaded system config: %s\n", config_system_path());
    }
    /* Missing or parse error → silently skip */

    /* ── Layer 2: User config (~/.config/lanft/config.toml) ── */
    const char *user_path = config_user_path();
    struct stat st;
    int user_exists = (stat(user_path, &st) == 0);

    if (!user_exists) {
        /* Try to copy system config as initial user config */
        FILE *fsrc = fopen(config_system_path(), "r");
        if (fsrc) {
            /* Create parent directory */
            char dirbuf[512];
            strncpy(dirbuf, user_path, sizeof(dirbuf) - 1);
            char *slash = strrchr(dirbuf, '/');
#ifdef _WIN32
            char *bslash = strrchr(dirbuf, '\\');
            if (bslash > slash) slash = bslash;
#endif
            if (slash) {
                *slash = '\0';
                /* mkdir -p equivalent */
                char tmp[512];
                for (char *p = dirbuf; *p; p++) {
                    if ((*p == '/' || *p == '\\') && p > dirbuf) {
                        char saved = *p; *p = '\0';
#ifdef _WIN32
                        mkdir(tmp);
#else
                        mkdir(tmp, 0755);
#endif
                        *p = saved;
                    }
                }
#ifdef _WIN32
                mkdir(dirbuf);
#else
                mkdir(dirbuf, 0755);
#endif
            }

            FILE *fdst = fopen(user_path, "w");
            if (fdst) {
                char buf[4096];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0)
                    fwrite(buf, 1, n, fdst);
                fclose(fdst);
                fprintf(stderr, "[config] created initial user config: %s\n", user_path);
            }
            fclose(fsrc);
        } else {
            fprintf(stderr, "[config] no config files found, using built-in defaults\n");
        }
    }

    /* Now parse user config (whether just created or pre-existing) */
    toml_result_t user_res = toml_parse_file_ex(user_path);
    if (user_res.ok) {
        apply_toml_table(cfg, user_res.toptab);
        toml_free(user_res);
    }
    /* Parse error → already have built-in + system, just skip */

    return 0;
}
```

- [ ] **Step 5: Write config_load_file() — load a single TOML onto cfg**

```c
int config_load_file(struct lanft_config *cfg, const char *path)
{
    toml_result_t res = toml_parse_file_ex(path);
    if (!res.ok) {
        fprintf(stderr, "Warning: failed to parse config: %s\n", path);
        return -1;
    }
    apply_toml_table(cfg, res.toptab);
    toml_free(res);
    fprintf(stderr, "[config] loaded: %s\n", path);
    return 0;
}
```

- [ ] **Step 6: Write config_save()**

```c
int config_save(const struct lanft_config *cfg)
{
    const char *path = config_user_path();

    /* Create parent directory */
    char dirbuf[512];
    strncpy(dirbuf, path, sizeof(dirbuf) - 1);
    char *slash = strrchr(dirbuf, '/');
#ifdef _WIN32
    char *bslash = strrchr(dirbuf, '\\');
    if (bslash > slash) slash = bslash;
#endif
    if (slash) {
        *slash = '\0';
        for (char *p = dirbuf; *p; p++) {
            if ((*p == '/' || *p == '\\') && p > dirbuf) {
                char saved = *p; *p = '\0';
#ifdef _WIN32
                mkdir(dirbuf);
#else
                mkdir(dirbuf, 0755);
#endif
                *p = saved;
            }
        }
#ifdef _WIN32
        mkdir(dirbuf);
#else
        mkdir(dirbuf, 0755);
#endif
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "Error: cannot write config to %s: %s\n", path, strerror(errno));
        return -1;
    }

    fprintf(fp, "# lanft configuration\n");
    fprintf(fp, "# Generated automatically — edit and re-run\n\n");

    fprintf(fp, "port = %d\n", cfg->port);
    fprintf(fp, "protocol = \"%s\"\n", cfg->protocol == FT_PROTO_UDP ? "UDP" : "TCP");
    fprintf(fp, "address = \"%s\"\n", cfg->address);
    fprintf(fp, "save_dir = \"%s\"\n", cfg->save_dir);
    fprintf(fp, "mode = \"%s\"\n",
            cfg->mode == 0 ? "S" : (cfg->mode == 1 ? "R" : ""));
    fprintf(fp, "\n");
    fprintf(fp, "# Advanced\n");
    fprintf(fp, "buffer_size = %d\n", cfg->buffer_size);
    fprintf(fp, "timeout_seconds = %d\n", cfg->timeout_seconds);
    fprintf(fp, "max_connections = %d\n", cfg->max_connections);
    fprintf(fp, "auto_accept = %s\n", cfg->auto_accept ? "true" : "false");
    fprintf(fp, "overwrite_policy = \"%s\"\n", cfg->overwrite_policy);
    fprintf(fp, "show_progress = %s\n", cfg->show_progress ? "true" : "false");
    fprintf(fp, "\n");
    fprintf(fp, "# Discovery\n");
    fprintf(fp, "discovery_enabled = %s\n", cfg->discovery_enabled ? "true" : "false");
    fprintf(fp, "discovery_interval = %d\n", cfg->discovery_interval);
    fprintf(fp, "discovery_ttl = %d\n", cfg->discovery_ttl);
    fprintf(fp, "\n");
    fprintf(fp, "# Logging\n");
    fprintf(fp, "log_level = \"%s\"\n", cfg->log_level);
    if (cfg->log_file[0])
        fprintf(fp, "log_file = \"%s\"\n", cfg->log_file);
    else
        fprintf(fp, "# log_file = \"\"\n");
    fprintf(fp, "send_bandwidth_limit = %d\n", cfg->send_bandwidth_limit);

    fclose(fp);
    fprintf(stderr, "[config] saved to %s\n", path);
    return 0;
}
```

- [ ] **Step 7: Write config_print()**

```c
void config_print(const struct lanft_config *cfg)
{
    printf("# Effective configuration\n\n");
    printf("port = %d\n", cfg->port);
    printf("protocol = \"%s\"\n", cfg->protocol == FT_PROTO_UDP ? "UDP" : "TCP");
    printf("address = \"%s\"\n", cfg->address);
    printf("save_dir = \"%s\"\n", cfg->save_dir);
    printf("mode = \"%s\"\n",
           cfg->mode == 0 ? "S" : (cfg->mode == 1 ? "R" : "(unspecified)"));
    printf("\n# Advanced\n");
    printf("buffer_size = %d\n", cfg->buffer_size);
    printf("timeout_seconds = %d\n", cfg->timeout_seconds);
    printf("max_connections = %d\n", cfg->max_connections);
    printf("auto_accept = %s\n", cfg->auto_accept ? "true" : "false");
    printf("overwrite_policy = \"%s\"\n", cfg->overwrite_policy);
    printf("show_progress = %s\n", cfg->show_progress ? "true" : "false");
    printf("\n# Discovery\n");
    printf("discovery_enabled = %s\n", cfg->discovery_enabled ? "true" : "false");
    printf("discovery_interval = %d\n", cfg->discovery_interval);
    printf("discovery_ttl = %d\n", cfg->discovery_ttl);
    printf("\n# Logging\n");
    printf("log_level = \"%s\"\n", cfg->log_level);
    printf("log_file = \"%s\"\n", cfg->log_file[0] ? cfg->log_file : "(stderr)");
    printf("send_bandwidth_limit = %d\n", cfg->send_bandwidth_limit);
}
```

- [ ] **Step 8: Commit**

```bash
git add src/config.c
git commit -m "feat: add config.c — layered TOML config loading/saving"
```

---

### Task 3: Update CMakeLists.txt — add new source files

**Files:**
- Modify: `CMakeLists.txt:27-31`

- [ ] **Step 1: Add tomlc17.c and config.c to base SOURCES**

Edit `CMakeLists.txt`, change the base SOURCES block from:

```cmake
set(SOURCES
    src/network.c
    src/transfer.c
    src/cli.c
)
```

To:

```cmake
set(SOURCES
    src/network.c
    src/transfer.c
    src/cli.c
    src/config.c
    src/tomlc17/tomlc17.c
)
```

- [ ] **Step 2: Build and verify compilation**

```bash
cd build && cmake .. -DBUILD_GUI=OFF && make -j$(nproc)
```

Expected: Compiles without errors.

- [ ] **Step 3: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: add config.c and tomlc17.c to build"
```

---

### Task 4: Modify `src/cli.c` — integrate config, add new CLI args

**Files:**
- Modify: `src/cli.c` (entire file)

- [ ] **Step 1: Remove `struct cli_config`, add `#include "config.h"`**

In `cli.c`:
- Add `#include "config.h"` after `#include "compat.h"`
- Remove the `struct cli_config { ... };` block (lines 16-23)
- Add a static variable: `static char g_cli_path[1024];` (to hold the PATH positional argument, which is not part of lanft_config)

- [ ] **Step 2: Add new CLI long options**

Add these entries to `long_opts[]` in `cli_main()`:

```c
{"save-config",   no_argument,       0, 2000},
{"show-config",   no_argument,       0, 2001},
{"save-dir",      required_argument, 0, 2002},
{"buffer-size",   required_argument, 0, 2003},
{"timeout",       required_argument, 0, 2004},
{"overwrite",     required_argument, 0, 2005},
{"no-progress",   no_argument,       0, 2006},
{"no-discovery",  no_argument,       0, 2007},
{"log-level",     required_argument, 0, 2008},
{"bandwidth-limit", required_argument, 0, 2009},
{"auto-accept",   no_argument,       0, 2010},
{"no-auto-accept",no_argument,       0, 2011},
{"config",        required_argument, 0, 2012},
```

- [ ] **Step 3: Rewrite the getopt_long switch to populate lanft_config**

Replace the entire `cli_main` function body. Here's the full replacement:

The function now:
1. Sets up `struct lanft_config cfg; config_load(&cfg);`
2. Parses CLI args — each recognized flag overrides the corresponding cfg field
3. Passes positional arg into `g_cli_path`
4. Handles `--save-config` and `--show-config` (early exit)
5. Reads `cfg.mode` for send/receive decision

Full replacement for the cli_main function body from the start through the getopt switch:

```c
int cli_main(int argc, char **argv)
{
    struct lanft_config cfg;
    config_load(&cfg);
    g_cli_path[0] = '\0';

    static struct option long_opts[] = {
        {"help",           no_argument,       0, 'h'},
        {"version",        no_argument,       0, 'v'},
        {"gui",            no_argument,       0, 1004},
        {"protocol",       required_argument, 0, 1000},
        {"mode",           required_argument, 0, 1001},
        {"port",           required_argument, 0, 'p'},
        {"address",        required_argument, 0, 1002},
        {"history",        no_argument,       0, 1003},
        {"save-config",    no_argument,       0, 2000},
        {"show-config",    no_argument,       0, 2001},
        {"save-dir",       required_argument, 0, 2002},
        {"buffer-size",    required_argument, 0, 2003},
        {"timeout",        required_argument, 0, 2004},
        {"overwrite",      required_argument, 0, 2005},
        {"no-progress",    no_argument,       0, 2006},
        {"no-discovery",   no_argument,       0, 2007},
        {"log-level",      required_argument, 0, 2008},
        {"bandwidth-limit",required_argument, 0, 2009},
        {"auto-accept",    no_argument,       0, 2010},
        {"no-auto-accept", no_argument,       0, 2011},
        {"config",         required_argument, 0, 2012},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hvSRp:", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'h': print_help(argv[0]); return 0;
        case 'v': printf("%s\n", CLI_VERSION); return 0;
        case 'S': cfg.mode = 0; break;
        case 'R': cfg.mode = 1; break;
        case 'p': cfg.port = atoi(optarg); break;
        case 1000:
            cfg.protocol = (strcasecmp(optarg, "UDP") == 0) ? FT_PROTO_UDP : FT_PROTO_TCP;
            break;
        case 1001:
            cfg.mode = (strcasecmp(optarg, "R") == 0) ? 1 : 0;
            break;
        case 1002:
            strncpy(cfg.address, optarg, sizeof(cfg.address) - 1);
            break;
        case 1003:
            /* --history: handled after the switch */
            break;
        case 1004:
            fprintf(stderr, "GUI mode is not available in this build.\n");
            fprintf(stderr, "Rebuild with: cmake .. -DBUILD_GUI=ON\n");
            return 1;
        case 2000: /* --save-config */
            config_save(&cfg);
            return 0;
        case 2001: /* --show-config */
            config_print(&cfg);
            return 0;
        case 2002: /* --save-dir */
            strncpy(cfg.save_dir, optarg, sizeof(cfg.save_dir) - 1);
            break;
        case 2003: /* --buffer-size */
            cfg.buffer_size = atoi(optarg);
            break;
        case 2004: /* --timeout */
            cfg.timeout_seconds = atoi(optarg);
            break;
        case 2005: /* --overwrite */
            strncpy(cfg.overwrite_policy, optarg, sizeof(cfg.overwrite_policy) - 1);
            break;
        case 2006: /* --no-progress */
            cfg.show_progress = false;
            break;
        case 2007: /* --no-discovery */
            cfg.discovery_enabled = false;
            break;
        case 2008: /* --log-level */
            strncpy(cfg.log_level, optarg, sizeof(cfg.log_level) - 1);
            break;
        case 2009: /* --bandwidth-limit */
            cfg.send_bandwidth_limit = atoi(optarg);
            break;
        case 2010: /* --auto-accept */
            cfg.auto_accept = true;
            break;
        case 2011: /* --no-auto-accept */
            cfg.auto_accept = false;
            break;
        case 2012: /* --config */
            config_load_file(&cfg, optarg);
            break;
        default:
            print_help(argv[0]);
            return 1;
        }
    }

    /* ── History (checks the old flag pattern) ──────────────── */
    /* Detect --history: we use a different approach. Check if argv contains --history */
    {
        bool show_history = false;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--history") == 0) {
                show_history = true;
                break;
            }
        }
        if (show_history) {
            char path[512];
            snprintf(path, sizeof(path), "./lanft_history.dat");
            FILE *fp = fopen(path, "r");
            if (!fp) { printf("No history found.\n"); return 0; }
            char line[1024];
            printf("%-20s %8s %8s %8s %4s %5s %5s %3s %s\n",
                   "Name","Start","End","Dur(ms)","Kind","Port","St","%","Speed");
            while (fgets(line, sizeof(line), fp)) {
                char name[256]="",st[32]="",et[32]="";
                unsigned long dur=0,spd=0; int k=0,port=0,stt=0,prog=0;
                sscanf(line,"%255[^|]|%31[^|]|%31[^|]|%lu|%d|%d|%d|%d|%lu",
                       name, st, et, &dur, &k, &port, &stt, &prog, &spd);
                printf("%-20s %8s %8s %8lu %4s %5d %5s %3d%% %lu\n",
                       name, st, et, dur,
                       k==0?"SEND":"RECV", port,
                       stt==0?"OK":(stt==1?"BAD":"STOP"), prog, spd);
            }
            fclose(fp);
            return 0;
        }
    }

    /* ── Validation ──────────────────────────────────────── */
    if (cfg.mode < 0) {
        fprintf(stderr, "Error: --mode is required (S=send, R=receive)\n\n");
        print_help(argv[0]);
        return 1;
    }
    if (optind >= argc) {
        fprintf(stderr, "Error: missing PATH argument\n\n");
        print_help(argv[0]);
        return 1;
    }
    strncpy(g_cli_path, argv[optind], sizeof(g_cli_path) - 1);

    /* Expand ~ in save_dir for receive mode */
    const char *expanded_save = config_expand_path(cfg.save_dir);

    struct stat path_st;
    if (cfg.mode == 0) {
        if (stat(g_cli_path, &path_st) != 0) {
            fprintf(stderr, "Error: file/directory not found: %s\n", g_cli_path);
            return 1;
        }
    } else {
        if (stat(expanded_save, &path_st) != 0 || !S_ISDIR(path_st.st_mode)) {
            fprintf(stderr, "Error: save directory not found: %s\n", expanded_save);
            return 1;
        }
    }

    /* ── Set CLI callbacks ────────────────────────────────── */
    transfer_set_callbacks(cli_progress, cli_error, cli_done);

    /* ── Execute ──────────────────────────────────────────── */

    cli_ret = 1;

    if (cfg.mode == 0) {
        /* Send */
        struct net_context *nc = net_create(cfg.protocol);
        if (!nc) {
            fprintf(stderr, "Error: failed to create network context\n");
            return 1;
        }

        fprintf(stderr, "Connecting to %s:%d...\n", cfg.address, cfg.port);
        cli_start_ms = now_ms();
        int tries = 0;
        while (tries < 60) {
            if (cfg.protocol == FT_PROTO_TCP) {
                if (net_connect(nc, cfg.address, cfg.port) == 0) break;
            } else {
                if (net_udp_bind(nc, cfg.port) == 0) {
                    net_udp_set_peer(nc, cfg.address, cfg.port);
                    break;
                }
            }
            tries++;
            fprintf(stderr, "\r  Retrying... (%d/60)", tries);
            fflush(stderr);
            sleep(1);
        }
        if (tries >= 60) {
            fprintf(stderr, "\nError: failed to connect to %s:%d\n",
                    cfg.address, cfg.port);
            net_destroy(nc);
            return 1;
        }
        fprintf(stderr, "\nConnected! Sending %s...\n", g_cli_path);
        transfer_send(nc, g_cli_path, cfg.protocol);
        net_destroy(nc);
        return cli_ret;
    }

    /* Receive — persistent listener */
    fprintf(stderr, "Persistent listener on %s:%d (Ctrl+C to stop)\n\n",
            cfg.address, cfg.port);
    int transfer_count = 0;
    while (1) {
        struct net_context *nc = net_create(cfg.protocol);
        if (!nc) {
            fprintf(stderr, "Error: failed to create network context\n");
            return 1;
        }

        if (cfg.protocol == FT_PROTO_TCP) {
            if (net_listen_ip(nc, cfg.address, cfg.port) != 0) {
                fprintf(stderr, "Error: failed to listen on %s:%d\n",
                        cfg.address, cfg.port);
                net_destroy(nc);
                return 1;
            }
        } else {
            if (net_udp_bind(nc, cfg.port) != 0) {
                fprintf(stderr, "Error: failed to bind UDP port %d\n", cfg.port);
                net_destroy(nc);
                return 1;
            }
        }

        transfer_count++;
        cli_start_ms = now_ms();
        cli_ret = 1;
        fprintf(stderr, "[%d] Waiting for sender...\n", transfer_count);
        transfer_recv(nc, expanded_save, cfg.protocol);
        net_destroy(nc);

        if (cli_ret != 0) {
            fprintf(stderr, "\n[%d] Transfer failed, continuing to listen...\n\n",
                    transfer_count);
        } else {
            fprintf(stderr, "\n[%d] Done, listening for next...\n\n",
                    transfer_count);
        }
    }
}
```

Note: We also need to add `static char g_cli_path[1024];` at file scope (near `static int cli_ret = 1;`).

- [ ] **Step 4: Update print_help() to list new CLI args**

Add these lines to the help output (before the "Examples:" line):

```c
    printf("  --save-dir=DIR        Save directory (default: ~/Downloads)\n");
    printf("  --buffer-size=N       Transfer buffer size (default: 65536)\n");
    printf("  --timeout=N           Timeout in seconds (default: 30)\n");
    printf("  --overwrite=POLICY    rename | overwrite | skip (default: rename)\n");
    printf("  --no-progress         Disable progress bar\n");
    printf("  --no-discovery        Disable LAN discovery\n");
    printf("  --log-level=LEVEL     debug | info | warn | error (default: info)\n");
    printf("  --bandwidth-limit=N   Send bandwidth limit in bytes/sec (0=unlimited)\n");
    printf("  --auto-accept         Auto-accept incoming files\n");
    printf("  --no-auto-accept      Prompt before accepting (default)\n");
    printf("  --show-config         Print effective config and exit\n");
    printf("  --save-config         Save current settings to user config\n");
    printf("  --config=PATH         Use custom config file\n");
```

- [ ] **Step 5: Build and test CLI mode**

```bash
cd build && cmake .. -DBUILD_GUI=OFF && make -j$(nproc)
./lanft --show-config
```

Expected: prints built-in defaults as TOML.

```bash
./lanft --help
```

Expected: help includes new options.

- [ ] **Step 6: Commit**

```bash
git add src/cli.c
git commit -m "feat: integrate config system into CLI, add new CLI args"
```

---

### Task 5: Modify `src/main.c` — load config on GUI startup

**Files:**
- Modify: `src/main.c:230-286` (the `--gui` detection and app_state init)

- [ ] **Step 1: Add `#include "config.h"`**

Add `#include "config.h"` after the other includes in main.c.

- [ ] **Step 2: Replace the app_state init to use lanft_config**

In the GUI path, after `history_load(&state)`, replace the hardcoded defaults:

Current:
```c
    state.scan_port = FT_DEFAULT_PORT;
    state.send_port = FT_DEFAULT_PORT;
    state.recv_port = FT_DEFAULT_PORT;
    strncpy(state.recv_target_ip, "0.0.0.0", sizeof(state.recv_target_ip) - 1);
    state.send_protocol = FT_PROTO_TCP;
    state.recv_protocol = FT_PROTO_TCP;
```

Replace with:
```c
    struct lanft_config cfg;
    config_load(&cfg);
    state.scan_port = cfg.port;
    state.send_port = cfg.port;
    state.recv_port = cfg.port;
    strncpy(state.recv_target_ip, cfg.address, sizeof(state.recv_target_ip) - 1);
    state.send_protocol = cfg.protocol;
    state.recv_protocol = cfg.protocol;
    if (cfg.mode == 0) {
        /* Config says send — pre-select send tab */
        state.current_tab = TAB_SEND;
    } else if (cfg.mode == 1) {
        /* Config says receive — pre-select receive tab */
        state.current_tab = TAB_RECEIVE;
    }
    /* Apply send_filepath/send_target_ip/recv_savepath if set in config */
    if (cfg.save_dir[0]) {
        const char *expanded = config_expand_path(cfg.save_dir);
        strncpy(state.recv_savepath, expanded, sizeof(state.recv_savepath) - 1);
    }
```

- [ ] **Step 3: Build GUI and verify**

```bash
cd build && cmake .. -DBUILD_GUI=ON && make -j$(nproc)
./lanft --gui &
```

Expected: GUI launches, config is loaded (check stderr for `[config] loaded...` messages).

- [ ] **Step 4: Commit**

```bash
git add src/main.c
git commit -m "feat: load config on GUI startup to init app_state"
```

---

### Task 6: Integration test — end-to-end config chain

- [ ] **Step 1: Test built-in defaults**

```bash
./lanft --show-config
```

Expected: shows all defaults (port=9876, protocol=TCP, etc.)

- [ ] **Step 2: Test system config layer**

```bash
sudo mkdir -p /etc/lanft
echo 'port = 5555' | sudo tee /etc/lanft/config.toml
./lanft --show-config
```

Expected: port = 5555 (from system config), rest are defaults.

- [ ] **Step 3: Test user config override**

```bash
./lanft --save-config  # Creates ~/.config/lanft/config.toml with current effective config
# Edit the user config
sed -i 's/port = 5555/port = 7777/' ~/.config/lanft/config.toml
./lanft --show-config
```

Expected: port = 7777 (user overrides system).

- [ ] **Step 4: Test CLI arg highest priority**

```bash
./lanft --port=9999 --mode=S --address=1.2.3.4 /tmp/test 2>&1 | head -3
```

Expected: connects to 1.2.3.4:9999 (CLI args beat everything).

- [ ] **Step 5: Cleanup**

```bash
sudo rm /etc/lanft/config.toml
rm ~/.config/lanft/config.toml
```

---

### Task 7: Self-review and final build

- [ ] **Step 1: Full rebuild both modes**

```bash
cd build && cmake .. -DBUILD_GUI=ON && make -j$(nproc)
cmake .. -DBUILD_GUI=OFF && make -j$(nproc)
```

Both must compile cleanly.

- [ ] **Step 2: Verify no regressions**

```bash
./lanft --help
./lanft --version
./lanft --history
```

All existing commands still work.

- [ ] **Step 3: Final commit**

```bash
git add -A
git commit -m "feat: complete config system implementation"
```
