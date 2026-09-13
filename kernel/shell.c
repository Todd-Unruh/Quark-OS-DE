/* =============================================================================
 * Quark-OS kernel/shell.c
 * Interactive bash-style shell REPL with history, env, aliases, redirection
 * ============================================================================= */

#include <quark/shell.h>

#define CMD_BUFFER_SIZE SHELL_MAX_LINE

static shell_context_t g_shell_ctx;

static char redirect_buf[MAX_FILE_SIZE];
static uint32_t redirect_len = 0;
static int redirect_active = 0;
static char redirect_target[MAX_FILE_NAME];
static int redirect_append_flag = 0;

shell_context_t *shell_ctx(void) {
    return &g_shell_ctx;
}

void shell_init_context(void) {
    kmemset(&g_shell_ctx, 0, sizeof(g_shell_ctx));
    kstrcpy(g_shell_ctx.cwd, "root");
    shell_env_set("USER", "root");
    shell_env_set("HOME", "/");
    shell_env_set("SHELL", "/bin/bash");
    shell_env_set("HOSTNAME", "quark");
    shell_env_set("PATH", "/bin:/usr/bin");
    shell_env_set("PWD", "/");
    kstrcpy(g_shell_ctx.alias_keys[0], "ll");
    kstrcpy(g_shell_ctx.alias_vals[0], "ls");
    kstrcpy(g_shell_ctx.alias_keys[1], "cls");
    kstrcpy(g_shell_ctx.alias_vals[1], "clear");
    g_shell_ctx.alias_count = 2;
}

const char *shell_env_get(const char *key) {
    for (int i = 0; i < g_shell_ctx.env_count; i++) {
        if (kstrcmp(g_shell_ctx.env_keys[i], key) == 0) {
            return g_shell_ctx.env_vals[i];
        }
    }
    return NULL;
}

int shell_env_set(const char *key, const char *val) {
    for (int i = 0; i < g_shell_ctx.env_count; i++) {
        if (kstrcmp(g_shell_ctx.env_keys[i], key) == 0) {
            kstrcpy(g_shell_ctx.env_vals[i], val);
            return 0;
        }
    }
    if (g_shell_ctx.env_count >= SHELL_MAX_ENV) return -1;
    kstrcpy(g_shell_ctx.env_keys[g_shell_ctx.env_count], key);
    kstrcpy(g_shell_ctx.env_vals[g_shell_ctx.env_count], val);
    g_shell_ctx.env_count++;
    return 0;
}

void shell_history_add(const char *line) {
    if (!line || kstrlen(line) == 0) return;
    int idx = g_shell_ctx.history_next % SHELL_MAX_HISTORY;
    kstrcpy(g_shell_ctx.history[idx], line);
    g_shell_ctx.history_next++;
    g_shell_ctx.history_count++;
}

void shell_outc(char c) {
    if (redirect_active) {
        if (redirect_len < MAX_FILE_SIZE - 1) {
            redirect_buf[redirect_len++] = c;
        }
    } else {
        terminal_putchar(c);
    }
}

void shell_out(const char *str) {
    while (str && *str) shell_outc(*str++);
}

void shell_outln(const char *str) {
    shell_out(str);
    shell_out("\n");
}

int shell_redirect_begin(const shell_parsed_t *cmd) {
    if (!cmd || cmd->redirect_out[0] == '\0') return 0;
    redirect_active = 1;
    redirect_len = 0;
    redirect_append_flag = cmd->redirect_append;
    kstrcpy(redirect_target, cmd->redirect_out);
    if (redirect_append_flag) {
        ram_file_t *f = ramfs_get(redirect_target, g_shell_ctx.cwd);
        if (f && !f->is_dir && f->size > 0) {
            uint32_t n = f->size;
            if (n > MAX_FILE_SIZE - 1) n = MAX_FILE_SIZE - 1;
            kmemcpy(redirect_buf, f->data, n);
            redirect_len = n;
        }
    }
    return 1;
}

void shell_redirect_end(int had_redirect, const shell_parsed_t *cmd) {
    (void)cmd;
    if (!had_redirect) return;
    if (!ramfs_get(redirect_target, g_shell_ctx.cwd)) {
        ramfs_create(redirect_target, g_shell_ctx.cwd, 0);
    }
    ramfs_write(redirect_target, g_shell_ctx.cwd, redirect_buf, redirect_len);
    redirect_active = 0;
    redirect_len = 0;
}

int shell_execute_line(const char *line) {
    const char *p = line;
    int last_status = 0;

    while (*p) {
        shell_parsed_t parsed;
        char segment[SHELL_MAX_LINE];
        int seg_idx = 0;

        while (*p && *p != ';') {
            if (seg_idx < SHELL_MAX_LINE - 1) {
                segment[seg_idx++] = *p;
            }
            p++;
        }
        segment[seg_idx] = '\0';
        if (*p == ';') p++;

        if (shell_parse_line(segment, &parsed) > 0) {
            last_status = shell_execute_parsed(&parsed);
        }
    }
    return last_status;
}

/* Text Line Editor Engine with Ctrl+C Save Routing */
void run_editor(const char *filename) {
    char editor_buf[MAX_FILE_SIZE];
    int editor_idx = 0;
    kmemset(editor_buf, 0, sizeof(editor_buf));

    terminal_clear();
    kprintf("=== Quark Interactive Text Editor ===\n");
    kprintf("Editing file: %s/%s\n", g_shell_ctx.cwd, filename);
    kprintf("Instructions: Type text. Press Ctrl+C to SAVE & EXIT.\n");
    kprintf("--------------------------------------------------------\n");

    ram_file_t *existing_file = ramfs_get(filename, g_shell_ctx.cwd);
    if (existing_file && existing_file->size > 0) {
        uint32_t bytes_to_load = existing_file->size;
        if (bytes_to_load > MAX_FILE_SIZE - 2) {
            bytes_to_load = MAX_FILE_SIZE - 2;
        }
        kmemcpy(editor_buf, existing_file->data, bytes_to_load);
        editor_idx = (int)bytes_to_load;
        for (int i = 0; i < editor_idx; i++) {
            terminal_putchar(editor_buf[i]);
        }
    }

    for (;;) {
        char c = keyboard_getchar();
        if (c == 3) break;
        if (c == '\n') {
            if (editor_idx < MAX_FILE_SIZE - 2) {
                editor_buf[editor_idx++] = '\n';
                terminal_putchar('\n');
            }
        } else if (c == '\b') {
            if (editor_idx > 0) {
                editor_idx--;
                editor_buf[editor_idx] = '\0';
                terminal_putchar('\b');
            }
        } else if (editor_idx < MAX_FILE_SIZE - 2) {
            editor_buf[editor_idx++] = c;
            terminal_putchar(c);
        }
    }

    ramfs_write(filename, g_shell_ctx.cwd, editor_buf, (uint32_t)editor_idx);
    terminal_clear();
    kprintf("File '%s' saved.\n\n", filename);
}

static char global_calc_buffer[64];

void run_calculator(void) {
    kprintf("\n--- Quark Calculator ---\n");
    kprintf("Single-digit ops (+,-,*,/). Type 'exit' to quit.\n");

    for (;;) {
        kprintf("calc> ");
        int idx = 0;
        kmemset(global_calc_buffer, 0, sizeof(global_calc_buffer));

        for (;;) {
            char c = keyboard_getchar();
            if (c == '\n') {
                terminal_putchar('\n');
                break;
            } else if (c == '\b' && idx > 0) {
                idx--;
                terminal_putchar('\b');
            } else if (c != '\b' && idx < 63) {
                global_calc_buffer[idx++] = c;
                terminal_putchar(c);
            }
        }
        global_calc_buffer[idx] = '\0';

        if (kstrcmp(global_calc_buffer, "exit") == 0) break;
        if (idx >= 3) {
            int val1 = global_calc_buffer[0] - '0';
            char op = global_calc_buffer[1];
            int val2 = global_calc_buffer[2] - '0';
            int res = 0;

            switch (op) {
                case '+': res = val1 + val2; kprintf("= %d\n", res); break;
                case '-': res = val1 - val2; kprintf("= %d\n", res); break;
                case '*': res = val1 * val2; kprintf("= %d\n", res); break;
                case '/':
                    if (val2 == 0) kprintf("Error: divide by zero\n");
                    else kprintf("= %d\n", val1 / val2);
                    break;
                default: kprintf("Unknown operator\n"); break;
            }
        }
    }
    kprintf("Calculator closed.\n\n");
}

static void print_prompt(void) {
    const char *user = shell_env_get("USER");
    const char *host = shell_env_get("HOSTNAME");

    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("%s", user ? user : "root");
    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("@");
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("%s", host ? host : "quark");
    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf(":");
    terminal_setcolor(VGA_LIGHT_BLUE, VGA_BLACK);

    if (kstrcmp(g_shell_ctx.cwd, "root") == 0) {
        kprintf("~");
    } else {
        kprintf("~/%s", g_shell_ctx.cwd);
    }

    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("$ ");
}

void shell_run(void) {
    char input_buf[CMD_BUFFER_SIZE];
    int buf_idx = 0;

    shell_init_context();
    kprintf("Quark-OS bash-style shell ready. Type 'help' for commands.\n\n");
    print_prompt();

    for (;;) {
        char c = keyboard_getchar();

        if (c == KEY_WORKSPACE_RIGHT) {
            desktop_run(DESKTOP_VIEW_FILES);
            terminal_refresh();
            print_prompt();
        } else if (c == KEY_WORKSPACE_LEFT) {
            desktop_run(DESKTOP_VIEW_SETTINGS);
            terminal_refresh();
            print_prompt();
        } else if (c == '\n') {
            terminal_putchar('\n');
            input_buf[buf_idx] = '\0';
            if (buf_idx > 0) {
                shell_history_add(input_buf);
                shell_execute_line(input_buf);
            }
            buf_idx = 0;
            kmemset(input_buf, 0, sizeof(input_buf));
            print_prompt();
        } else if (c == '\b' && buf_idx > 0) {
            buf_idx--;
            input_buf[buf_idx] = '\0';
            terminal_putchar('\b');
        } else if (c != '\b' && c != '\n' && buf_idx < CMD_BUFFER_SIZE - 1) {
            if (c >= 32 && c <= 126) {
                input_buf[buf_idx++] = c;
                terminal_putchar(c);
            }
        }
    }
}
