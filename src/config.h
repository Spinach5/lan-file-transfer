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
   2. /etc/lanft/config.toml (system) -- skip if missing
   3. ~/.config/lanft/config.toml (user) -- if missing, copy from system if available
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
