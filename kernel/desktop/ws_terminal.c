/* =============================================================================
 * Quark-OS kernel/desktop/ws_terminal.c
 * Fullscreen terminal for workspace 0.
 * ============================================================================= */

#include <quark/de.h>
#include <quark/de_apps.h>
#include <quark/de_workspace.h>

#define WS_TERM_INPUT_MAX 96
#define WS_TERM_LINE_H   12
#define WS_TERM_PAD      8

static char  input[WS_TERM_INPUT_MAX];
static int   input_len = 0;

extern uint32_t timer_get_ticks(void);

static void print(const char* s) { de_console_log(s); }

static void cmd_help(void) {
    print("Commands:");
    print("  help          this list");
    print("  about         system info");
    print("  clear         clear the screen");
    print("  echo <text>   print text");
    print("  apps          list installed apps");
    print("  open <id>     open an app on the desktop workspace");
    print("  time          current time");
    print("  ticks         timer ticks");
    print("  mem           heap usage");
    print("  desktop       switch to the desktop workspace");
}

static void cmd_about(void) {
    print("Quark-OS v3.0");
    print("32-bit protected mode, i386");
    print("VESA linear framebuffer, PS/2 input");
}

static void cmd_clear(void) {
    for (int i = 0; i < DE_CONSOLE_LINES; i++) de_console_log("");
    de_damage_add_all();
}

static void cmd_apps(void) {
    int n = app_registry_count();
    print("Installed apps:");
    for (int i = 0; i < n; i++) {
        const app_t* a = app_registry_at(i);
        if (!a) continue;
        char buf[64];
        int p = 0;
        buf[p++] = ' '; buf[p++] = ' ';
        const char* id = a->id;
        while (*id && p < 30) buf[p++] = *id++;
        buf[p++] = ' '; buf[p++] = '-'; buf[p++] = ' ';
        const char* nm = a->name;
        while (*nm && p < 62) buf[p++] = *nm++;
        buf[p] = '\0';
        print(buf);
    }
}

static void cmd_open(const char* arg) {
    if (!arg || !arg[0]) { print("usage: open <id>"); return; }
    if (!app_registry_find(arg)) { print("no such app"); return; }
    de_app_open(arg);
    print("opened on desktop workspace");
}

static void cmd_time(void) {
    char buf[16];
    de_format_clock(buf, sizeof(buf));
    char out[32];
    int p = 0;
    const char* pre = "time: ";
    while (*pre) out[p++] = *pre++;
    int q = 0;
    while (buf[q] && p < 30) out[p++] = buf[q++];
    out[p] = '\0';
    print(out);
}

static void cmd_ticks(void) {
    uint32_t t = timer_get_ticks();
    char num[16]; int n = 0;
    char tmp[16]; int m = 0;
    if (t == 0) tmp[m++] = '0';
    while (t > 0) { tmp[m++] = '0' + t % 10; t /= 10; }
    while (m > 0) num[n++] = tmp[--m];
    num[n] = '\0';
    char out[32];
    int p = 0;
    const char* pre = "ticks: ";
    while (*pre) out[p++] = *pre++;
    int q = 0;
    while (num[q] && p < 30) out[p++] = num[q++];
    out[p] = '\0';
    print(out);
}

static void cmd_mem(void) {
    uint32_t used = 0, freeb = 0;
    heap_get_stats(&used, &freeb);
    char out[64]; int p = 0;
    const char* pre = "heap used=";
    while (*pre && p < 60) out[p++] = *pre++;
    char num[16]; int n = 0;
    char tmp[16]; int m = 0;
    uint32_t v = used;
    if (v == 0) tmp[m++] = '0';
    while (v > 0) { tmp[m++] = '0' + v % 10; v /= 10; }
    while (m > 0) num[n++] = tmp[--m];
    num[n] = '\0';
    for (int i = 0; num[i] && p < 60; i++) out[p++] = num[i];
    const char* mid = " free=";
    while (*mid && p < 60) out[p++] = *mid++;
    n = 0; m = 0; v = freeb;
    if (v == 0) tmp[m++] = '0';
    while (v > 0) { tmp[m++] = '0' + v % 10; v /= 10; }
    while (m > 0) num[n++] = tmp[--m];
    num[n] = '\0';
    for (int i = 0; num[i] && p < 62; i++) out[p++] = num[i];
    out[p] = '\0';
    print(out);
}

static void run_command(const char* line) {
    while (*line == ' ') line++;
    if (!*line) return;

    char cmd[32];
    int ci = 0;
    while (*line && *line != ' ' && ci < 31) cmd[ci++] = *line++;
    cmd[ci] = '\0';
    while (*line == ' ') line++;
    const char* args = line;

    if (kstrcmp(cmd, "help") == 0)          cmd_help();
    else if (kstrcmp(cmd, "about") == 0)    cmd_about();
    else if (kstrcmp(cmd, "clear") == 0)    cmd_clear();
    else if (kstrcmp(cmd, "apps") == 0)     cmd_apps();
    else if (kstrcmp(cmd, "open") == 0)     cmd_open(args);
    else if (kstrcmp(cmd, "time") == 0)     cmd_time();
    else if (kstrcmp(cmd, "ticks") == 0)    cmd_ticks();
    else if (kstrcmp(cmd, "mem") == 0)      cmd_mem();
    else if (kstrcmp(cmd, "desktop") == 0) {
        de_workspace_switch(DE_WORKSPACE_DESKTOP);
    }
    else if (kstrcmp(cmd, "echo") == 0) {
        if (args && args[0]) print(args);
    }
    else {
        char msg[64]; int p = 0;
        const char* pre = "unknown: ";
        while (*pre && p < 60) msg[p++] = *pre++;
        int q = 0;
        while (cmd[q] && p < 62) msg[p++] = cmd[q++];
        msg[p] = '\0';
        print(msg);
    }
}

void ws_terminal_init(void) {
    input[0] = '\0';
    input_len = 0;
}

void ws_terminal_log_line(const char* line) {
    de_console_log(line);
    de_damage_add_all();
}

void ws_terminal_draw(void) {
    const de_theme_t* t = g_theme;
    int sw = de_fb_w();
    int sh = de_fb_h();

    de_fill(0, 0, sw, sh, 0x000A0A12);

    int usable_h = sh - WS_TERM_LINE_H * 2;
    int rows = usable_h / WS_TERM_LINE_H;
    if (rows < 1) rows = 1;

    int total = 0;
    for (int i = 0; i < DE_CONSOLE_LINES; i++) {
        if (de_console_line(i)) total++;
    }

    int first_line = total - rows;
    if (first_line < 0) first_line = 0;

    int y = WS_TERM_PAD;
    for (int i = first_line; i < total; i++) {
        const char* line = de_console_line(i);
        if (!line) continue;
        if (y + WS_TERM_LINE_H > sh - WS_TERM_LINE_H * 2) break;
        de_text(WS_TERM_PAD, y, line, 0x00D0F0D0);
        y += WS_TERM_LINE_H;
    }

    int py = sh - WS_TERM_LINE_H - WS_TERM_PAD;
    de_text(WS_TERM_PAD, py, "$ ", t->accent);
    de_text(WS_TERM_PAD + 16, py, input, 0x00FFFFFF);

    uint32_t blink = (timer_get_ticks() / 25) & 1;
    if (blink) {
        int cx = WS_TERM_PAD + 16 + input_len * 8;
        de_fill(cx, py, 2, 8, 0x00FFFFFF);
    }

    const char* hint = "Ctrl+Alt+Right: Desktop";
    int hw = de_text_width(hint);
    de_text(sw - hw - WS_TERM_PAD, WS_TERM_PAD, hint, 0x00506070);
}

int ws_terminal_handle_key(const de_key_event_t* k) {
    if (!k) return 0;
    if (k->ctrl && k->alt) return 0;

    if (k->ascii == '\n') {
        char buf[128]; int n = 0;
        buf[n++] = '$'; buf[n++] = ' ';
        for (int i = 0; i < input_len && n < 124; i++) buf[n++] = input[i];
        buf[n] = '\0';
        de_console_log(buf);

        input[input_len] = '\0';
        run_command(input);

        input[0] = '\0';
        input_len = 0;
        de_damage_add_all();
        return 1;
    }
    if (k->ascii == '\b') {
        if (input_len > 0) {
            input_len--;
            input[input_len] = '\0';
            de_damage_add_all();
        }
        return 1;
    }
    if (k->ascii >= 32 && k->ascii < 127 && input_len < WS_TERM_INPUT_MAX - 1) {
        input[input_len++] = k->ascii;
        input[input_len] = '\0';
        de_damage_add_all();
        return 1;
    }
    return 0;
}