# Config System Design

**Date:** 2025-06-14
**Status:** Approved

## Overview

Add a TOML-based configuration system with layered priority: CLI args > user config > system config > built-in defaults. Uses tomlc17 as the TOML parser.

## Config Priority Chain

```
CLI args  >  ~/.config/lanft/config.toml  >  /etc/lanft/config.toml  >  built-in defaults
  (highest)                                                                      (lowest)
```

Each layer is loaded with `toml_merge()` — later layers override earlier ones. Missing files are silently skipped.

## Load Sequence

1. Set built-in defaults (hardcoded in C)
2. `toml_parse_file_ex("/etc/lanft/config.toml")` — skip if missing
3. Check `~/.config/lanft/config.toml`:
   - **Exists**: parse and merge over current config
   - **Missing**: copy `/etc/lanft/config.toml` → `~/.config/lanft/config.toml` if system config exists; otherwise print: "No config found. Using built-in defaults."
4. Apply CLI argument overrides (getopt_long results overwrite struct fields)
5. Return final `lanft_config` struct

## Config Structure (config.h)

```c
struct lanft_config {
    /* Core */
    int  port;                  // default: 9876
    int  protocol;              // FT_PROTO_TCP (0) or FT_PROTO_UDP (1), default: TCP
    char address[64];           // default: "0.0.0.0"
    char save_dir[1024];        // default: "~/Downloads"
    int  mode;                  // 0=send, 1=receive, -1=CLI-required

    /* Advanced */
    int  buffer_size;           // default: 65536
    int  timeout_seconds;       // default: 30 (0 = wait forever)
    int  max_connections;       // default: 5
    bool auto_accept;           // default: false (receiver prompts)
    char overwrite_policy[16];  // "rename" | "overwrite" | "skip", default: "rename"
    bool show_progress;         // default: true

    /* Discovery */
    bool discovery_enabled;     // default: true
    int  discovery_interval;    // seconds, default: 5
    int  discovery_ttl;         // default: 1

    /* Logging */
    char log_level[16];         // "debug"|"info"|"warn"|"error", default: "info"
    char log_file[1024];        // empty = stderr

    /* Bandwidth */
    int  send_bandwidth_limit;  // bytes/sec, 0 = unlimited
};
```

## Example Config File

```toml
port = 9876
protocol = "TCP"
address = "0.0.0.0"
save_dir = "~/Downloads"
mode = "R"

buffer_size = 65536
timeout_seconds = 30
max_connections = 5
auto_accept = false
overwrite_policy = "rename"
show_progress = true

discovery_enabled = true
discovery_interval = 5
discovery_ttl = 1

log_level = "info"
# log_file = "/var/log/lanft.log"
send_bandwidth_limit = 0
```

## New CLI Arguments

| Argument | Type | Description |
|----------|------|-------------|
| `--save-config` | flag | Write current effective config to `~/.config/lanft/config.toml` |
| `--config=PATH` | string | Use custom config file instead of default user config |
| `--save-dir=PATH` | string | Override save/recv directory |
| `--buffer-size=N` | int | Override transfer buffer size |
| `--timeout=N` | int | Override timeout seconds |
| `--overwrite=POLICY` | string | Override overwrite policy (rename/overwrite/skip) |
| `--no-progress` | flag | Disable progress bar |
| `--no-discovery` | flag | Disable LAN discovery |
| `--log-level=LEVEL` | string | Override log level |
| `--bandwidth-limit=N` | int | Override bandwidth limit (bytes/sec) |
| `--show-config` | flag | Print current effective config and exit |
| `--auto-accept` / `--no-auto-accept` | flag | Enable/disable auto-accept mode |

## Files Changed

| File | Change |
|------|--------|
| `src/tomlc17/tomlc17.c` | Already present — add to CMakeLists.txt |
| `src/tomlc17/tomlc17.h` | Already present |
| `src/config.h` | **New** — `lanft_config` struct, `config_load()`, `config_save()`, `config_print()` |
| `src/config.c` | **New** — load (priority chain), save (write toml), path helpers |
| `src/cli.c` | Modify — use `lanft_config`, parse CLI overrides after `config_load()`, add `--save-config` |
| `src/main.c` | Modify — call `config_load()` on startup to init `app_state` fields |
| `CMakeLists.txt` | Modify — add `src/tomlc17/tomlc17.c` and `src/config.c` to SOURCES |

## API (config.h)

```c
/* Load config following the priority chain. argc/argv are for CLI overrides.
   Returns 0 on success, -1 on fatal error (bad config syntax, etc.) */
int config_load(struct lanft_config *cfg, int argc, char **argv);

/* Save current config to ~/.config/lanft/config.toml.
   Creates parent directories if needed. Returns 0 on success. */
int config_save(const struct lanft_config *cfg);

/* Print current effective config to stdout (for --show-config / debugging). */
void config_print(const struct lanft_config *cfg);

/* Get the user config file path (~/.config/lanft/config.toml).
   Returns a static buffer. */
const char *config_user_path(void);

/* Get the system config file path (/etc/lanft/config.toml).
   Returns a static buffer. */
const char *config_system_path(void);
```

## Dependencies

- **tomlc17**: MIT license, single .c + .h, ~2000 LOC. Uses `toml_parse_file_ex()` for file reading and `toml_merge()` for layered config overlay.
- No additional system libraries required.
