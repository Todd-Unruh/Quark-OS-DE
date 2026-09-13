#ifndef QUARK_SHELL_H
#define QUARK_SHELL_H

#include <quark/kernel.h>

#define SHELL_MAX_LINE      256
#define SHELL_MAX_ARGS      8
#define SHELL_MAX_ARG_LEN   128
#define SHELL_MAX_HISTORY   16
#define SHELL_MAX_ENV       16
#define SHELL_MAX_ALIASES   16
#define SHELL_MAX_CMDS      128

typedef int (*shell_handler_t)(int argc, char **argv);

typedef struct {
    const char       *name;
    shell_handler_t   handler;
    const char       *help;
} shell_command_t;

typedef struct {
    char cwd[32];
    char redirect_out[MAX_FILE_NAME];
    int  redirect_append;
    char env_keys[SHELL_MAX_ENV][16];
    char env_vals[SHELL_MAX_ENV][64];
    int  env_count;
    char alias_keys[SHELL_MAX_ALIASES][16];
    char alias_vals[SHELL_MAX_ALIASES][64];
    int  alias_count;
    char history[SHELL_MAX_HISTORY][SHELL_MAX_LINE];
    int  history_count;
    int  history_next;
} shell_context_t;

typedef struct {
    int   argc;
    char *argv[SHELL_MAX_ARGS];
    char  arg_storage[SHELL_MAX_LINE];
    char  redirect_out[MAX_FILE_NAME];
    int   redirect_append;
} shell_parsed_t;

shell_context_t *shell_ctx(void);
void shell_init_context(void);
int  shell_parse_line(const char *line, shell_parsed_t *out);
int  shell_execute_parsed(const shell_parsed_t *cmd);
int  shell_execute_line(const char *line);
void shell_show_help(const char *topic);
const shell_command_t *shell_lookup(const char *name);
int  shell_redirect_begin(const shell_parsed_t *cmd);
void shell_redirect_end(int had_redirect, const shell_parsed_t *cmd);
const char *shell_env_get(const char *key);
int  shell_env_set(const char *key, const char *val);
void shell_history_add(const char *line);

#endif /* QUARK_SHELL_H */
