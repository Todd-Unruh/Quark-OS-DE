/* =============================================================================
 * Quark-OS kernel/shell_commands.c
 * Bash-compatible built-in command table and handlers
 * ============================================================================= */

#include <quark/shell.h>
#include <quark/forge.h>

extern void shell_out(const char *str);
extern void shell_outc(char c);
extern void shell_outln(const char *str);
extern void run_editor(const char *filename);
extern void run_calculator(void);

static shell_context_t *ctx(void) {
    return shell_ctx();
}

static ram_file_t *file_arg(const char *name) {
    return ramfs_get(name, ctx()->cwd);
}

static int count_lines(const char *data, uint32_t size) {
    int lines = 0;
    for (uint32_t i = 0; i < size; i++) {
        if (data[i] == '\n') lines++;
    }
    if (size > 0 && data[size - 1] != '\n') lines++;
    return lines;
}

static int count_words(const char *data, uint32_t size) {
    int words = 0;
    int in_word = 0;
    for (uint32_t i = 0; i < size; i++) {
        char c = data[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }
    return words;
}

/* --- General / help --- */

static int cmd_help(int argc, char **argv) {
    (void)argc;
    if (argv[1]) {
        shell_show_help(argv[1]);
        return 0;
    }
    shell_show_help(NULL);
    return 0;
}

static int cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    terminal_clear();
    return 0;
}

static int cmd_desktop(int argc, char **argv) {
    (void)argc; (void)argv;
    desktop_run(DESKTOP_VIEW_HOME);
    return 0;
}

static int cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) shell_out(" ");
        shell_out(argv[i]);
    }
    shell_out("\n");
    return 0;
}

static int cmd_printf_cmd(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        shell_out(argv[i]);
        if (i + 1 < argc) shell_out(" ");
    }
    shell_out("\n");
    return 0;
}

static int cmd_pwd(int argc, char **argv) {
    (void)argc; (void)argv;
    if (kstrcmp(ctx()->cwd, "root") == 0) {
        shell_outln("/");
    } else {
        char path[64];
        kstrcpy(path, "/");
        kstrcat(path, ctx()->cwd);
        shell_outln(path);
    }
    return 0;
}

static int cmd_version(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_outln("Quark-OS 3.0 - Forge_High scripting + bash-style shell");
    return 0;
}

static int cmd_true(int argc, char **argv) {
    (void)argc; (void)argv;
    return 0;
}

static int cmd_false(int argc, char **argv) {
    (void)argc; (void)argv;
    return 1;
}

/* --- Filesystem --- */

static int cmd_ls(int argc, char **argv) {
    const char *target = ctx()->cwd;
    if (argc > 1) target = argv[1];
    ramfs_list(target);
    return 0;
}

static int cmd_cd(int argc, char **argv) {
    if (argc < 2) {
        kstrcpy(ctx()->cwd, "root");
        shell_env_set("PWD", "/");
        return 0;
    }
    if (kstrcmp(argv[1], "/") == 0 || kstrcmp(argv[1], "~") == 0) {
        kstrcpy(ctx()->cwd, "root");
        shell_env_set("PWD", "/");
        return 0;
    }
    if (kstrcmp(argv[1], "..") == 0) {
        kstrcpy(ctx()->cwd, "root");
        shell_env_set("PWD", "/");
        return 0;
    }
    ram_file_t *f = ramfs_get(argv[1], ctx()->cwd);
    if (f && f->is_dir) {
        kstrcpy(ctx()->cwd, argv[1]);
        char path[64];
        if (kstrcmp(argv[1], "root") == 0) kstrcpy(path, "/");
        else { kstrcpy(path, "/"); kstrcat(path, argv[1]); }
        shell_env_set("PWD", path);
        return 0;
    }
    kprintf("cd: %s: No such directory\n", argv[1]);
    return 1;
}

static int cmd_mkdir(int argc, char **argv) {
    if (argc < 2) {
        kprintf("mkdir: missing operand\nUsage: mkdir <name>\n");
        return 1;
    }
    if (ramfs_create(argv[1], ctx()->cwd, 1) == 0) return 0;
    kprintf("mkdir: cannot create directory '%s'\n", argv[1]);
    return 1;
}

static int cmd_touch(int argc, char **argv) {
    if (argc < 2) {
        kprintf("touch: missing file operand\n");
        return 1;
    }
    if (ramfs_get(argv[1], ctx()->cwd)) return 0;
    if (ramfs_create(argv[1], ctx()->cwd, 0) == 0) return 0;
    kprintf("touch: cannot create '%s'\n", argv[1]);
    return 1;
}

static int cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        kprintf("cat: missing file operand\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        ram_file_t *file = file_arg(argv[i]);
        if (!file || file->is_dir) {
            kprintf("cat: %s: No such file\n", argv[i]);
            return 1;
        }
        for (uint32_t j = 0; j < file->size; j++) {
            shell_outc((char)file->data[j]);
        }
        if (i + 1 < argc) shell_out("\n");
    }
    shell_out("\n");
    return 0;
}

static int cmd_rm(int argc, char **argv) {
    if (argc < 2) {
        kprintf("rm: missing operand\n");
        return 1;
    }
    int status = 0;
    for (int i = 1; i < argc; i++) {
        int res = ramfs_delete(argv[i], ctx()->cwd, 0);
        if (res == -2) {
            kprintf("rm: cannot remove '%s': Is a directory\n", argv[i]);
            status = 1;
        } else if (res != 0) {
            kprintf("rm: cannot remove '%s': No such file\n", argv[i]);
            status = 1;
        }
    }
    return status;
}

static int cmd_rmdir(int argc, char **argv) {
    if (argc < 2) {
        kprintf("rmdir: missing operand\n");
        return 1;
    }
    int res = ramfs_delete(argv[1], ctx()->cwd, 1);
    if (res == 0) return 0;
    if (res == -2) kprintf("rmdir: '%s': Not a directory\n", argv[1]);
    else if (res == -3) kprintf("rmdir: '%s': Directory not empty\n", argv[1]);
    else kprintf("rmdir: '%s': No such file or directory\n", argv[1]);
    return 1;
}

static int cmd_cp(int argc, char **argv) {
    if (argc < 3) {
        kprintf("cp: missing file operand\nUsage: cp <source> <dest>\n");
        return 1;
    }
    int res = ramfs_copy(argv[1], argv[2], ctx()->cwd);
    if (res == 0) return 0;
    kprintf("cp: cannot copy '%s' to '%s'\n", argv[1], argv[2]);
    return 1;
}

static int cmd_mv(int argc, char **argv) {
    if (argc < 3) {
        kprintf("mv: missing operand\nUsage: mv <source> <dest>\n");
        return 1;
    }
    ram_file_t *src = file_arg(argv[1]);
    if (!src) {
        kprintf("mv: cannot stat '%s': No such file\n", argv[1]);
        return 1;
    }
    if (src->is_dir) {
        kprintf("mv: directory rename not supported\n");
        return 1;
    }
    if (ramfs_copy(argv[1], argv[2], ctx()->cwd) != 0) return 1;
    ramfs_delete(argv[1], ctx()->cwd, 0);
    return 0;
}

static int cmd_ln(int argc, char **argv) {
    if (argc < 3) {
        kprintf("ln: missing operand\nUsage: ln <source> <linkname>\n");
        return 1;
    }
    return cmd_cp(argc, argv);
}

static int cmd_edit(int argc, char **argv) {
    if (argc < 2) {
        kprintf("edit: missing file operand\n");
        return 1;
    }
    if (!file_arg(argv[1])) ramfs_create(argv[1], ctx()->cwd, 0);
    run_editor(argv[1]);
    return 0;
}

static int cmd_find(int argc, char **argv) {
    const char *dir = ctx()->cwd;
    const char *pattern = "*";
    if (argc >= 2) pattern = argv[1];
    if (argc >= 3) dir = argv[2];
    ramfs_find(dir, pattern);
    return 0;
}

static int cmd_tree(int argc, char **argv) {
    const char *dir = (argc >= 2) ? argv[1] : ctx()->cwd;
    ramfs_tree(dir);
    return 0;
}

static int cmd_stat(int argc, char **argv) {
    if (argc < 2) {
        kprintf("stat: missing operand\n");
        return 1;
    }
    ram_file_t *f = file_arg(argv[1]);
    if (!f) {
        kprintf("stat: cannot stat '%s': No such file\n", argv[1]);
        return 1;
    }
    kprintf("  File: %s/%s\n", ctx()->cwd, f->name);
    kprintf("  Type: %s\n", f->is_dir ? "directory" : "regular file");
    kprintf("  Size: %u bytes\n", f->size);
    return 0;
}

static int cmd_file(int argc, char **argv) {
    if (argc < 2) {
        kprintf("file: missing operand\n");
        return 1;
    }
    ram_file_t *f = file_arg(argv[1]);
    if (!f) {
        kprintf("file: %s: cannot open\n", argv[1]);
        return 1;
    }
    kprintf("%s: %s\n", argv[1], f->is_dir ? "directory" : "ASCII text");
    return 0;
}

static int cmd_wc(int argc, char **argv) {
    if (argc < 2) {
        kprintf("wc: missing operand\n");
        return 1;
    }
    ram_file_t *f = file_arg(argv[1]);
    if (!f || f->is_dir) return 1;
    int lines = count_lines((const char *)f->data, f->size);
    int words = count_words((const char *)f->data, f->size);
    kprintf("%d %d %u %s\n", lines, words, f->size, argv[1]);
    return 0;
}

static int cmd_head(int argc, char **argv) {
    if (argc < 2) return 1;
    int n = 10;
    if (argc >= 3) n = argv[2][0] - '0';
    ram_file_t *f = file_arg(argv[1]);
    if (!f || f->is_dir) return 1;
    int lines = 0;
    for (uint32_t i = 0; i < f->size && lines < n; i++) {
        shell_outc((char)f->data[i]);
        if (f->data[i] == '\n') lines++;
    }
    shell_out("\n");
    return 0;
}

static int cmd_tail(int argc, char **argv) {
    if (argc < 2) return 1;
    ram_file_t *f = file_arg(argv[1]);
    if (!f || f->is_dir) return 1;
    int want = 10;
    if (argc >= 3) want = argv[2][0] - '0';
    int line_count = count_lines((const char *)f->data, f->size);
    int skip = line_count - want;
    if (skip < 0) skip = 0;
    int seen = 0;
    for (uint32_t i = 0; i < f->size; i++) {
        if (f->data[i] == '\n') {
            seen++;
            if (seen > skip) shell_outc('\n');
            continue;
        }
        if (seen >= skip) shell_outc((char)f->data[i]);
    }
    shell_out("\n");
    return 0;
}

static int cmd_grep(int argc, char **argv) {
    if (argc < 3) {
        kprintf("grep: missing pattern or file\nUsage: grep <pattern> <file>\n");
        return 1;
    }
    ram_file_t *f = file_arg(argv[2]);
    if (!f || f->is_dir) return 1;
    char line[256];
    int li = 0;
    for (uint32_t i = 0; i <= f->size; i++) {
        char c = (i < f->size) ? (char)f->data[i] : '\n';
        if (c == '\n' || li >= (int)sizeof(line) - 1) {
            line[li] = '\0';
            if (kstrstr(line, argv[1])) {
                shell_out(line);
                shell_out("\n");
            }
            li = 0;
        } else {
            line[li++] = c;
        }
    }
    return 0;
}

static int cmd_sort(int argc, char **argv) {
    if (argc < 2) return 1;
    ram_file_t *f = file_arg(argv[1]);
    if (!f || f->is_dir) return 1;
    char lines[32][128];
    int count = 0;
    char line[128];
    int li = 0;
    for (uint32_t i = 0; i <= f->size && count < 32; i++) {
        char c = (i < f->size) ? (char)f->data[i] : '\n';
        if (c == '\n' || li >= 127) {
            line[li] = '\0';
            kstrcpy(lines[count++], line);
            li = 0;
        } else {
            line[li++] = c;
        }
    }
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (kstrcmp(lines[i], lines[j]) > 0) {
                char tmp[128];
                kstrcpy(tmp, lines[i]);
                kstrcpy(lines[i], lines[j]);
                kstrcpy(lines[j], tmp);
            }
        }
    }
    for (int i = 0; i < count; i++) {
        shell_out(lines[i]);
        shell_out("\n");
    }
    return 0;
}

static int cmd_uniq(int argc, char **argv) {
    if (argc < 2) return 1;
    ram_file_t *f = file_arg(argv[1]);
    if (!f || f->is_dir) return 1;
    char prev[128] = "";
    char line[128];
    int li = 0;
    for (uint32_t i = 0; i <= f->size; i++) {
        char c = (i < f->size) ? (char)f->data[i] : '\n';
        if (c == '\n' || li >= 127) {
            line[li] = '\0';
            if (kstrcmp(line, prev) != 0) {
                shell_out(line);
                shell_out("\n");
                kstrcpy(prev, line);
            }
            li = 0;
        } else {
            line[li++] = c;
        }
    }
    return 0;
}

static int cmd_df(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("Filesystem     Used  Avail  Use%%  Mounted on\n");
    kprintf("ramfs          %3d   %3d   %3d%%  /\n",
            ramfs_used_count(), MAX_FILES - ramfs_used_count(),
            (ramfs_used_count() * 100) / MAX_FILES);
    return 0;
}

static int cmd_du(int argc, char **argv) {
    const char *dir = (argc >= 2) ? argv[1] : ctx()->cwd;
    kprintf("%u\t%s\n", ramfs_dir_size(dir), dir);
    return 0;
}

/* --- Text utilities --- */

static int cmd_basename(int argc, char **argv) {
    if (argc < 2) return 1;
    const char *s = argv[1];
    const char *last = s;
    while (*s) {
        if (*s == '/') last = s + 1;
        s++;
    }
    shell_outln(last);
    return 0;
}

static int cmd_dirname(int argc, char **argv) {
    if (argc < 2) return 1;
    char buf[128];
    kstrcpy(buf, argv[1]);
    char *last = kstrrchr(buf, '/');
    if (!last) shell_outln(".");
    else {
        if (last == buf) shell_outln("/");
        else {
            *last = '\0';
            shell_outln(buf);
        }
    }
    return 0;
}

static int cmd_tr(int argc, char **argv) {
    if (argc < 3) {
        kprintf("tr: missing operands\nUsage: tr <set1> <set2>\n");
        return 1;
    }
    char map[256];
    for (int i = 0; i < 256; i++) map[i] = (char)i;
    int len1 = (int)kstrlen(argv[1]);
    int len2 = (int)kstrlen(argv[2]);
    for (int i = 0; i < len1; i++) {
        map[(unsigned char)argv[1][i]] = argv[2][i % len2];
    }
    char c;
    while ((c = keyboard_getchar()) != '\n') {
        shell_outc(map[(unsigned char)c]);
    }
    shell_out("\n");
    return 0;
}

static int cmd_cut(int argc, char **argv) {
    if (argc < 3) {
        kprintf("cut: missing operands\nUsage: cut -d<delim> -f<field> <file>\n");
        return 1;
    }
    char delim = ':';
    int field = 1;
    const char *file = argv[argc - 1];
    for (int i = 1; i < argc - 1; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'd') delim = argv[i][2];
        if (argv[i][0] == '-' && argv[i][1] == 'f') field = argv[i][2] - '0';
    }
    ram_file_t *f = file_arg(file);
    if (!f || f->is_dir) return 1;
    char line[256];
    int li = 0;
    for (uint32_t i = 0; i <= f->size; i++) {
        char c = (i < f->size) ? (char)f->data[i] : '\n';
        if (c == '\n' || li >= 255) {
            line[li] = '\0';
            int current = 1;
            char out[128];
            int oi = 0;
            for (int j = 0; line[j]; j++) {
                if (line[j] == delim) {
                    if (current == field) break;
                    current++;
                    oi = 0;
                } else if (current == field) {
                    out[oi++] = line[j];
                }
            }
            out[oi] = '\0';
            shell_out(out);
            shell_out("\n");
            li = 0;
        } else {
            line[li++] = c;
        }
    }
    return 0;
}

/* --- System info --- */

static int cmd_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t ticks = timer_get_ticks();
    kprintf("up %u ticks (%u seconds)\n", ticks, ticks / 100);
    return 0;
}

static int cmd_uname(int argc, char **argv) {
    if (argc > 1 && kstrcmp(argv[1], "-a") == 0) {
        shell_outln("Quark i686 Quark-OS 3.0 #1 SMP Forge");
        return 0;
    }
    shell_outln("Quark-OS");
    return 0;
}

static int cmd_hostname(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *host = shell_env_get("HOSTNAME");
    shell_outln(host ? host : "quark");
    return 0;
}

static int cmd_whoami(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *user = shell_env_get("USER");
    shell_outln(user ? user : "root");
    return 0;
}

static int cmd_id(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_outln("uid=0(root) gid=0(root) groups=0(root)");
    return 0;
}

static int cmd_mem(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("Physical memory free: %u MB (%u pages)\n",
            (pmm_free_pages() * PAGE_SIZE) / (1024 * 1024), pmm_free_pages());
    return 0;
}

static int cmd_free(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t free_mb = (pmm_free_pages() * PAGE_SIZE) / (1024 * 1024);
    kprintf("              total        used        free\n");
    kprintf("Mem:          %4u MB      %4u MB      %4u MB\n",
            free_mb + 16, 16, free_mb);
    return 0;
}

static int cmd_ps(int argc, char **argv) {
    (void)argc; (void)argv;
    proc_list_all();
    return 0;
}

static int cmd_top(int argc, char **argv) {
    return cmd_ps(argc, argv);
}

static int cmd_kill(int argc, char **argv) {
    if (argc < 2) {
        kprintf("kill: missing operand\nUsage: kill <pid>\n");
        return 1;
    }
    pid_t pid = 0;
    for (int i = 0; argv[1][i]; i++) {
        if (argv[1][i] < '0' || argv[1][i] > '9') return 1;
        pid = pid * 10 + (argv[1][i] - '0');
    }
    if (proc_kill(pid) == 0) return 0;
    kprintf("kill: (%u) - No such process\n", pid);
    return 1;
}

static int cmd_sleep(int argc, char **argv) {
    if (argc < 2) return 1;
    int sec = argv[1][0] - '0';
    if (argc >= 3) sec = sec * 10 + (argv[1][1] - '0');
    timer_sleep((uint32_t)sec * 1000);
    return 0;
}

static int cmd_calc(int argc, char **argv) {
    (void)argc; (void)argv;
    run_calculator();
    return 0;
}

static int cmd_seq(int argc, char **argv) {
    int start = 1, end = 1;
    if (argc >= 2) start = argv[1][0] - '0';
    if (argc >= 3) end = argv[2][0] - '0';
    for (int i = start; i <= end; i++) {
        char buf[16];
        kitoa(i, buf, 10);
        shell_out(buf);
        shell_out("\n");
    }
    return 0;
}

static int cmd_expr(int argc, char **argv) {
    if (argc < 4) {
        kprintf("expr: syntax error\nUsage: expr <n> <op> <n>\n");
        return 1;
    }
    int a = argv[1][0] - '0';
    int b = argv[3][0] - '0';
    char op = argv[2][0];
    int res = 0;
    switch (op) {
        case '+': res = a + b; break;
        case '-': res = a - b; break;
        case '*': res = a * b; break;
        case '/': res = (b == 0) ? 0 : a / b; break;
        default: return 1;
    }
    char buf[16];
    kitoa(res, buf, 10);
    shell_outln(buf);
    return 0;
}

static int cmd_test(int argc, char **argv) {
    if (argc < 4) return 1;
    if (kstrcmp(argv[2], "=") == 0) {
        return (kstrcmp(argv[1], argv[3]) == 0) ? 0 : 1;
    }
    return 1;
}

/* --- Shell meta --- */

static int cmd_env(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_context_t *c = ctx();
    for (int i = 0; i < c->env_count; i++) {
        kprintf("%s=%s\n", c->env_keys[i], c->env_vals[i]);
    }
    return 0;
}

static int cmd_export_cmd(int argc, char **argv) {
    if (argc < 2) {
        kprintf("export: missing assignment\nUsage: export KEY=VALUE\n");
        return 1;
    }
    char key[16];
    char val[64];
    int i = 0;
    while (argv[1][i] && argv[1][i] != '=' && i < 15) {
        key[i] = argv[1][i];
        i++;
    }
    key[i] = '\0';
    if (argv[1][i] == '=') i++;
    kstrcpy(val, &argv[1][i]);
    shell_env_set(key, val);
    return 0;
}

static int cmd_unset(int argc, char **argv) {
    if (argc < 2) return 1;
    shell_context_t *c = ctx();
    for (int i = 0; i < c->env_count; i++) {
        if (kstrcmp(c->env_keys[i], argv[1]) == 0) {
            for (int j = i; j < c->env_count - 1; j++) {
                kstrcpy(c->env_keys[j], c->env_keys[j + 1]);
                kstrcpy(c->env_vals[j], c->env_vals[j + 1]);
            }
            c->env_count--;
            return 0;
        }
    }
    return 0;
}

static int cmd_alias(int argc, char **argv) {
    shell_context_t *c = ctx();
    if (argc < 2) {
        for (int i = 0; i < c->alias_count; i++) {
            kprintf("alias %s='%s'\n", c->alias_keys[i], c->alias_vals[i]);
        }
        return 0;
    }
    char key[16];
    char val[64];
    int i = 0;
    while (argv[1][i] && argv[1][i] != '=' && i < 15) {
        key[i] = argv[1][i];
        i++;
    }
    key[i] = '\0';
    if (argv[1][i] == '=') i++;
    kstrcpy(val, &argv[1][i]);
    if (c->alias_count < SHELL_MAX_ALIASES) {
        kstrcpy(c->alias_keys[c->alias_count], key);
        kstrcpy(c->alias_vals[c->alias_count], val);
        c->alias_count++;
    }
    return 0;
}

static int cmd_unalias(int argc, char **argv) {
    if (argc < 2) return 1;
    shell_context_t *c = ctx();
    for (int i = 0; i < c->alias_count; i++) {
        if (kstrcmp(c->alias_keys[i], argv[1]) == 0) {
            for (int j = i; j < c->alias_count - 1; j++) {
                kstrcpy(c->alias_keys[j], c->alias_keys[j + 1]);
                kstrcpy(c->alias_vals[j], c->alias_vals[j + 1]);
            }
            c->alias_count--;
            return 0;
        }
    }
    return 1;
}

static int cmd_type(int argc, char **argv) {
    if (argc < 2) return 1;
    if (shell_lookup(argv[1])) {
        kprintf("%s is a shell builtin\n", argv[1]);
        return 0;
    }
    kprintf("%s: not found\n", argv[1]);
    return 1;
}

static int cmd_which(int argc, char **argv) {
    return cmd_type(argc, argv);
}

static int cmd_history(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_context_t *c = ctx();
    int start = 0;
    if (c->history_count > SHELL_MAX_HISTORY) start = c->history_count - SHELL_MAX_HISTORY;
    for (int i = start; i < c->history_count; i++) {
        int idx = i % SHELL_MAX_HISTORY;
        kprintf("%4d  %s\n", i + 1, c->history[idx]);
    }
    return 0;
}

/* --- Network --- */

static int cmd_net(int argc, char **argv) {
    (void)argc; (void)argv;
    net_list_status();
    return 0;
}

static int cmd_ping(int argc, char **argv) {
    (void)argc; (void)argv;
    char dummy_packet[64];
    kmemset(dummy_packet, 0xAB, sizeof(dummy_packet));
    kprintf("PING: sending 64-byte test packet via e1000...\n");
    e1000_send_packet(dummy_packet, 64);
    kprintf("64 bytes transmitted.\n");
    return 0;
}

/* --- Power --- */

static int cmd_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("System rebooting...\n");
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
    for (;;) hlt();
    return 0;
}

static int cmd_halt(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("System halted.\n");
    cli();
    for (;;) hlt();
    return 0;
}

static int cmd_poweroff(int argc, char **argv) {
    return cmd_halt(argc, argv);
}

static int cmd_dmesg(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_outln("[    0.000] Quark-OS boot sequence complete");
    shell_outln("[    0.001] GDT/IDT/PIC initialized");
    shell_outln("[    0.002] RamFS mounted at /");
    shell_outln("[    0.003] Shell ready");
    return 0;
}

static int cmd_date(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t ticks = timer_get_ticks();
    kprintf("Sun Jul  5 %02u:%02u:%02u UTC 2026\n",
            (ticks / 360000) % 24,
            (ticks / 6000) % 60,
            (ticks / 100) % 60);
    return 0;
}

static int cmd_nl(int argc, char **argv) {
    if (argc < 2) return 1;
    ram_file_t *f = file_arg(argv[1]);
    if (!f || f->is_dir) return 1;
    int line = 1;
    char num[8];
    for (uint32_t i = 0; i <= f->size; i++) {
        if (i == 0 || f->data[i - 1] == '\n') {
            kitoa(line++, num, 10);
            shell_out(num);
            shell_out("\t");
        }
        if (i < f->size) shell_outc((char)f->data[i]);
    }
    return 0;
}

static int cmd_rev(int argc, char **argv) {
    if (argc < 2) {
        kprintf("rev: missing operand\n");
        return 1;
    }
    int len = (int)kstrlen(argv[1]);
    for (int i = len - 1; i >= 0; i--) {
        shell_outc(argv[1][i]);
    }
    shell_out("\n");
    return 0;
}

static int cmd_yes(int argc, char **argv) {
    const char *word = (argc >= 2) ? argv[1] : "y";
    for (int i = 0; i < 20; i++) {
        shell_out(word);
        shell_out("\n");
    }
    return 0;
}

static int cmd_logout(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_outln("logout");
    return cmd_halt(argc, argv);
}

static int cmd_source(int argc, char **argv) {
    if (argc < 2) {
        kprintf("source: missing file operand\n");
        return 1;
    }
    ram_file_t *f = file_arg(argv[1]);
    if (!f || f->is_dir) return 1;
    char line[256];
    int li = 0;
    for (uint32_t i = 0; i <= f->size; i++) {
        char c = (i < f->size) ? (char)f->data[i] : '\n';
        if (c == '\n' || li >= 255) {
            line[li] = '\0';
            if (kstrlen(line) > 0) shell_execute_line(line);
            li = 0;
        } else {
            line[li++] = c;
        }
    }
    return 0;
}

/* --- Forge_High scripting --- */

static int cmd_forge(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: forge file.fg\n");
        kprintf("Run a Forge_High program from the current directory.\n");
        return 1;
    }
    ram_file_t *f = file_arg(argv[1]);
    if (!f) {
        kprintf("forge: %s: No such file\n", argv[1]);
        return 1;
    }
    if (f->is_dir) {
        kprintf("forge: %s: Is a directory\n", argv[1]);
        return 1;
    }
    return forge_run_file(f);
}

static int cmd_install(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_outln("Quark-OS-3 Installation");
    shell_outln("=======================");
    shell_outln("");
    shell_outln("On your host computer (Linux):");
    shell_outln("  1. cd Quark-OS-3");
    shell_outln("  2. make clean && make");
    shell_outln("  3. sudo ./install.sh /dev/sdX   (replace sdX with USB drive)");
    shell_outln("");
    shell_outln("Or run the live ISO in QEMU:");
    shell_outln("  make run");
    shell_outln("");
    shell_outln("After boot, use: forge hello.fg");
    shell_outln("See HOW_TO_USE.txt for full documentation.");
    if (ramfs_create("installed.flag", ctx()->cwd, 0) == 0) {
        ramfs_write("installed.flag", ctx()->cwd,
            "Quark-OS-3 install requested from shell.\n", 42);
    }
    return 0;
}

static const shell_command_t command_table[] = {
    {"help",     cmd_help,      "help [cmd]         - Show available commands"},
    {"clear",    cmd_clear,     "clear              - Clear the screen"},
    {"desktop",  cmd_desktop,   "desktop            - Open the Quark desktop"},
    {"echo",     cmd_echo,      "echo [text...]     - Print arguments"},
    {"printf",   cmd_printf_cmd,"printf [fmt...]    - Formatted print (basic)"},
    {"pwd",      cmd_pwd,       "pwd                - Print working directory"},
    {"version",  cmd_version,   "version            - Show OS version"},
    {"true",     cmd_true,      "true               - Exit status 0"},
    {"false",    cmd_false,     "false              - Exit status 1"},
    {"ls",       cmd_ls,        "ls [dir]           - List directory contents"},
    {"cd",       cmd_cd,        "cd [dir]           - Change directory"},
    {"mkdir",    cmd_mkdir,     "mkdir <name>       - Create directory"},
    {"touch",    cmd_touch,     "touch <file>       - Create empty file"},
    {"cat",      cmd_cat,       "cat <file...>      - Display file contents"},
    {"rm",       cmd_rm,        "rm <file...>       - Remove file"},
    {"rmdir",    cmd_rmdir,     "rmdir <dir>        - Remove empty directory"},
    {"cp",       cmd_cp,        "cp <src> <dest>    - Copy file"},
    {"mv",       cmd_mv,        "mv <src> <dest>    - Move/rename file"},
    {"ln",       cmd_ln,        "ln <src> <dest>    - Link (copy) file"},
    {"edit",     cmd_edit,      "edit <file>        - Text editor (^C saves)"},
    {"find",     cmd_find,      "find [pat] [dir]   - Find files by pattern"},
    {"tree",     cmd_tree,      "tree [dir]         - Directory tree view"},
    {"stat",     cmd_stat,      "stat <file>        - File metadata"},
    {"file",     cmd_file,      "file <path>        - Determine file type"},
    {"wc",       cmd_wc,        "wc <file>          - Line/word/byte counts"},
    {"head",     cmd_head,      "head <file> [n]    - First lines of file"},
    {"tail",     cmd_tail,      "tail <file> [n]    - Last lines of file"},
    {"grep",     cmd_grep,      "grep <pat> <file>  - Search file for pattern"},
    {"sort",     cmd_sort,      "sort <file>        - Sort file lines"},
    {"uniq",     cmd_uniq,      "uniq <file>        - Filter duplicate lines"},
    {"df",       cmd_df,        "df                 - Filesystem disk usage"},
    {"du",       cmd_du,        "du [dir]           - Directory size usage"},
    {"basename", cmd_basename,  "basename <path>    - Strip directory from path"},
    {"dirname",  cmd_dirname,   "dirname <path>     - Strip filename from path"},
    {"tr",       cmd_tr,        "tr <set1> <set2>   - Translate characters"},
    {"cut",      cmd_cut,       "cut -d: -f1 <file> - Cut fields from lines"},
    {"uptime",   cmd_uptime,    "uptime             - System uptime"},
    {"uname",    cmd_uname,     "uname [-a]         - System name"},
    {"hostname", cmd_hostname,  "hostname           - Print host name"},
    {"whoami",   cmd_whoami,    "whoami             - Print effective user"},
    {"id",       cmd_id,        "id                 - Print user/group ids"},
    {"mem",      cmd_mem,       "mem                - Memory statistics"},
    {"free",     cmd_free,      "free               - Memory usage summary"},
    {"ps",       cmd_ps,        "ps                 - List processes"},
    {"top",      cmd_top,       "top                - Process monitor"},
    {"kill",     cmd_kill,      "kill <pid>         - Terminate process"},
    {"sleep",    cmd_sleep,     "sleep <sec>        - Pause for seconds"},
    {"calc",     cmd_calc,      "calc               - Interactive calculator"},
    {"seq",      cmd_seq,       "seq [start end]    - Print number sequence"},
    {"expr",     cmd_expr,      "expr <n> <op> <n>  - Evaluate expression"},
    {"test",     cmd_test,      "test a = b         - Compare strings"},
    {"env",      cmd_env,       "env                - Print environment"},
    {"export",   cmd_export_cmd,"export KEY=VAL     - Set environment variable"},
    {"unset",    cmd_unset,     "unset <key>        - Remove env variable"},
    {"alias",    cmd_alias,     "alias [name=cmd]   - List or set aliases"},
    {"unalias",  cmd_unalias,   "unalias <name>     - Remove alias"},
    {"type",     cmd_type,      "type <cmd>         - Describe command type"},
    {"which",    cmd_which,     "which <cmd>        - Locate builtin command"},
    {"history",  cmd_history,   "history            - Command history"},
    {"net",      cmd_net,       "net                - Network interface status"},
    {"ping",     cmd_ping,      "ping               - Send test packet"},
    {"reboot",   cmd_reboot,    "reboot             - Reboot system"},
    {"halt",     cmd_halt,      "halt               - Halt CPU"},
    {"poweroff", cmd_poweroff,  "poweroff           - Power off (halt)"},
    {"dmesg",    cmd_dmesg,     "dmesg              - Print kernel ring buffer"},
    {"date",     cmd_date,      "date               - Print system date/time"},
    {"nl",       cmd_nl,        "nl <file>          - Number file lines"},
    {"rev",      cmd_rev,       "rev <string>       - Reverse text"},
    {"yes",      cmd_yes,       "yes [word]         - Repeated output"},
    {"logout",   cmd_logout,    "logout             - End session (halt)"},
    {"source",   cmd_source,    "source <file>      - Execute shell script file"},
    {"forge",    cmd_forge,     "forge <file.fg>    - Run Forge_High program"},
    {"install",  cmd_install,   "install            - Show install steps for Quark-OS-3"},
    {NULL, NULL, NULL}
};

const shell_command_t *shell_lookup(const char *name) {
    for (int i = 0; command_table[i].name; i++) {
        if (kstrcmp(command_table[i].name, name) == 0) {
            return &command_table[i];
        }
    }
    return NULL;
}

void shell_show_help(const char *topic) {
    if (topic) {
        const shell_command_t *cmd = shell_lookup(topic);
        if (cmd) {
            kprintf("%s\n", cmd->help);
            return;
        }
        kprintf("help: no help topics match '%s'\n", topic);
        return;
    }

    kprintf("Quark-OS bash-style shell - %d built-in commands\n\n", 73);
    for (int i = 0; command_table[i].name; i++) {
        kprintf("  %s\n", command_table[i].help);
    }
    kprintf("\nTips: use ';' to chain commands, '>' to redirect output to a file.\n");
    kprintf("      export KEY=VAL sets environment variables.\n");
}

int shell_execute_parsed(const shell_parsed_t *cmd) {
    if (cmd->argc == 0) return 0;

    const char *name = cmd->argv[0];
    shell_context_t *c = ctx();

    for (int i = 0; i < c->alias_count; i++) {
        if (kstrcmp(c->alias_keys[i], name) == 0) {
            return shell_execute_line(c->alias_vals[i]);
        }
    }

    const shell_command_t *entry = shell_lookup(name);
    if (!entry) {
        kprintf("%s: command not found\n", name);
        return 127;
    }

    int redirect = shell_redirect_begin(cmd);
    int status = entry->handler(cmd->argc, (char **)cmd->argv);
    shell_redirect_end(redirect, cmd);
    return status;
}
