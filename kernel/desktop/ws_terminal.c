/* =============================================================================
 * Quark-OS kernel/desktop/ws_terminal.c
 * Interactive bash-style terminal for the desktop workspace.
 * Features: private scrollback, command history via Up/Down, PageUp/Down
 * for scrollback navigation, live shell integration.
 * ============================================================================= */

#include <quark/de.h>
#include <quark/de_apps.h>
#include <quark/de_workspace.h>
#include <quark/de_font.h>
#include <quark/shell.h>

#define TERM_COLS        120
#define TERM_ROWS        200      /* scrollback capacity */
#define TERM_INPUT_MAX   96
#define TERM_LINE_H      14
#define TERM_PAD_X       8
#define TERM_PAD_Y       6

/* ---------------------------------------------------------------------------
 * Private scrollback ring buffer.
 * Each line is a fixed 120-char text buffer plus a color byte for the
 * first 2 columns (used to color the prompt "~ $"). If a color byte is
 * 0xFF, the line is drawn entirely with the output color.
 * ------------------------------------------------------------------------ */

typedef struct {
    char    text[TERM_COLS];
    uint8_t len;
    uint8_t color_flag;   /* 0 = normal output, 1 = contains prompt coloring */
} term_line_t;

static term_line_t scrollback[TERM_ROWS];
static int   sb_head = 0;         /* next write slot */
static int   sb_count = 0;        /* how many lines have been written */

/* Current working line (being built by putchar) */
static char  cur_line[TERM_COLS];
static int   cur_len = 0;
static uint8_t cur_color_flag = 0;
static int   cur_prompt_visible = 0;

/* View offset: 0 = pinned to bottom, positive = scrolled up that many rows */
static int   view_offset = 0;

/* Input buffer */
static char  input[TERM_INPUT_MAX];
static int   input_len = 0;

/* History recall index. -1 means "not currently recalling". */
static int   hist_recall = -1;

extern uint32_t timer_get_ticks(void);

/* ---------------------------------------------------------------------------
 * Colors from the active theme
 * ------------------------------------------------------------------------ */
static uint32_t col_prompt_tilde(void) { return g_theme->accent; }
static uint32_t col_prompt_dollar(void){ return 0x00FFFFFF; }
static uint32_t col_input(void)        { return 0x00E6ECF2; }
static uint32_t col_output(void)       { return 0x00C8D0DA; }
static uint32_t col_hint(void)         { return 0x00506070; }

/* ---------------------------------------------------------------------------
 * Scrollback management
 * ------------------------------------------------------------------------ */

static void sb_clear(void) {
    sb_head = 0;
    sb_count = 0;
    cur_len = 0;
    cur_color_flag = 0;
    cur_prompt_visible = 0;
    view_offset = 0;
}

/* Commit cur_line as a completed scrollback line and reset it. */
static void sb_commit_line(void) {
    term_line_t* slot = &scrollback[sb_head];
    int n = cur_len;
    if (n > TERM_COLS) n = TERM_COLS;
    for (int i = 0; i < n; i++) slot->text[i] = cur_line[i];
    slot->len = (uint8_t)n;
    slot->color_flag = cur_color_flag;
    sb_head = (sb_head + 1) % TERM_ROWS;
    if (sb_count < TERM_ROWS) sb_count++;
    cur_len = 0;
    cur_color_flag = 0;
}

/* Append a char to the current line, wrapping and committing on overflow. */
static void sb_putc(char c) {
    if (c == '\n') {
        sb_commit_line();
        if (view_offset != 0) {
            /* View stays where the user put it */
        }
        return;
    }
    if (c == '\r') return;
    if (c == '\b') {
        if (cur_len > 0) cur_len--;
        return;
    }
    if (cur_len >= TERM_COLS - 1) {
        sb_commit_line();
    }
    cur_line[cur_len++] = c;
}

/* Retrieve the Nth most-recently-written completed line (0 = newest). */
static const term_line_t* sb_line_from_top(int n) {
    if (n < 0 || n >= sb_count) return NULL;
    int idx = (sb_head - 1 - n + TERM_ROWS * 2) % TERM_ROWS;
    return &scrollback[idx];
}

/* Total lines visible = completed lines + the in-progress line. */
static int sb_visible_lines(void) {
    return sb_count + (cur_len > 0 ? 1 : 0);
}

/* ---------------------------------------------------------------------------
 * Sink: receives all chars from kprintf / shell_out
 * ------------------------------------------------------------------------ */

static void term_sink(char c) {
    sb_putc(c);
    de_damage_add_all();
}

/* ---------------------------------------------------------------------------
 * Input row helpers
 * ------------------------------------------------------------------------ */

/* Number of pixels for one row of text */
static int line_height(void) { return TERM_LINE_H; }

/* Visible area rows */
static int visible_rows(void) {
    int sh = de_fb_h();
    int usable = sh - TERM_PAD_Y * 2 - TERM_LINE_H;
    int r = usable / TERM_LINE_H;
    if (r < 1) r = 1;
    return r;
}

/* The prompt is always drawn at the bottom row, one line height tall. */
static int prompt_row_y(void) {
    return de_fb_h() - TERM_LINE_H - TERM_PAD_Y;
}

/* ---------------------------------------------------------------------------
 * Auto-scroll: keep the view at the bottom unless user has scrolled up.
 * Called when a new line is committed by sb_putc.
 * ------------------------------------------------------------------------ */

/* We don't need anything special here: view_offset stays put unless the
 * user changed it. New lines go into the ring, and the renderer computes
 * which slice of the ring to draw based on view_offset. */

/* ---------------------------------------------------------------------------
 * Drawing
 * ------------------------------------------------------------------------ */

static void draw_prompt(int y) {
    int x = TERM_PAD_X;
    de_text(x, y, "~", col_prompt_tilde());
    x += de_text_width("~");
    x += 4;
    de_text(x, y, "$", col_prompt_dollar());
    x += de_text_width("$");
    x += 4;
    de_text(x, y, input, col_input());
}

static void ws_terminal_draw_internal(void) {
    int sw = de_fb_w();
    int sh = de_fb_h();

    /* Background */
    de_fill(0, 0, sw, sh, 0x000A0A12);

    int rows_visible = visible_rows();
    int total_lines  = sb_visible_lines();

    /* Determine the first line (from the top of scrollback) that we draw.
     * The bottom of the visible area shows the most recent line minus
     * view_offset rows. */
    int bottom_line_index = total_lines - 1 - view_offset;
    if (bottom_line_index < 0) bottom_line_index = 0;
    int top_line_index = bottom_line_index - (rows_visible - 1);
    if (top_line_index < 0) top_line_index = 0;

    int y = TERM_PAD_Y;

    /* Draw completed lines: from top_line_index up to bottom_line_index,
     * but excluding the in-progress line. The in-progress line is drawn
     * after we know where we are. */
    int in_progress = (cur_len > 0);
    int completed_count = sb_count;

    for (int line_idx = top_line_index; line_idx <= bottom_line_index; line_idx++) {
        if (line_idx < 0) continue;
        if (line_idx >= total_lines) break;

        if (in_progress && line_idx == completed_count) {
            /* This is the in-progress line: draw as text (no prompt). */
            char tmp[TERM_COLS + 1];
            int n = cur_len;
            for (int i = 0; i < n; i++) tmp[i] = cur_line[i];
            tmp[n] = '\0';
            if (n > 0) de_text(TERM_PAD_X, y, tmp, col_output());
            y += line_height();
            continue;
        }

        const term_line_t* ln = sb_line_from_top(completed_count - 1 - line_idx);
        if (!ln) { y += line_height(); continue; }

        if (ln->len > 0) {
            char tmp[TERM_COLS + 1];
            int n = ln->len;
            if (n > TERM_COLS) n = TERM_COLS;
            for (int i = 0; i < n; i++) tmp[i] = ln->text[i];
            tmp[n] = '\0';
            de_text(TERM_PAD_X, y, tmp, col_output());
        }
        y += line_height();
        if (y > sh - TERM_LINE_H * 2) break;
    }

    /* Prompt row at the bottom */
    int py = prompt_row_y();
    draw_prompt(py);

    /* Blinking caret aligned with the input text */
    if (((timer_get_ticks() / 25) & 1) != 0) {
        int text_x = TERM_PAD_X
                   + de_text_width("~") + 4
                   + de_text_width("$") + 4
                   + de_text_width(input);
        int top = py + de_font_body.ascent + 2;
        int h   = de_font_body.ascent - 2;
        if (h < 6) h = 6;
        de_fill(text_x, top, 2, h, col_input());
    }

    /* Bottom-right hint */
    const char* hint = "Ctrl+Alt+Right: Desktop";
    int hw = de_text_width(hint);
    de_text(sw - hw - TERM_PAD_X, TERM_PAD_Y, hint, col_hint());
}

void ws_terminal_draw(void) {
    ws_terminal_draw_internal();
}

/* ---------------------------------------------------------------------------
 * Public API used by de_workspace.c
 * ------------------------------------------------------------------------ */

void ws_terminal_init(void) {
    input[0] = '\0';
    input_len = 0;
    hist_recall = -1;
    sb_clear();
    terminal_set_sink(term_sink);
}

void ws_terminal_shutdown(void) {
    terminal_set_sink(NULL);
}

/* Kept for compatibility with old callers. */
void ws_terminal_log_line(const char* line) {
    while (line && *line) {
        sb_putc(*line++);
    }
    sb_putc('\n');
    de_damage_add_all();
}

/* ---------------------------------------------------------------------------
 * Input handling
 * ------------------------------------------------------------------------ */

/* Write the current prompt + input into the scrollback so it becomes a
 * permanent line. Called before running a command. */
static void echo_prompt_line(void) {
    cur_color_flag = 0;
    sb_putc('~');
    sb_putc(' ');
    sb_putc('$');
    sb_putc(' ');
    for (int i = 0; i < input_len; i++) sb_putc(input[i]);
    sb_putc('\n');
    cur_color_flag = 0;
}

static void clear_input(void) {
    input[0] = '\0';
    input_len = 0;
    hist_recall = -1;
}

static void snap_view_to_bottom(void) {
    view_offset = 0;
}

/* Number of entries in the shell's history table that are non-empty. */
static int history_count(void) {
    shell_context_t* c = shell_ctx();
    if (!c) return 0;
    if (c->history_count > SHELL_MAX_HISTORY) return SHELL_MAX_HISTORY;
    return c->history_count;
}

static const char* history_at(int n) {
    /* n = 0 -> most recent, n = 1 -> one before that, etc. */
    shell_context_t* c = shell_ctx();
    if (!c) return NULL;
    int total = history_count();
    if (n < 0 || n >= total) return NULL;
    int base = (c->history_count > SHELL_MAX_HISTORY)
             ? (c->history_count % SHELL_MAX_HISTORY)
             : 0;
    int idx = (c->history_count - 1 - n + SHELL_MAX_HISTORY * 2) % SHELL_MAX_HISTORY;
    (void)base;
    return c->history[idx];
}

static void load_history_into_input(int n) {
    const char* h = history_at(n);
    if (!h) return;
    int i = 0;
    while (h[i] && i < TERM_INPUT_MAX - 1) { input[i] = h[i]; i++; }
    input[i] = '\0';
    input_len = i;
}

int ws_terminal_handle_key(const de_key_event_t* k) {
    if (!k) return 0;
    if (k->ctrl && k->alt) return 0;

    /* Extended keys: navigation */
    if (k->extended) {
        switch (k->keycode) {
            case DE_KEY_UP:
                /* Command history */
                if (hist_recall < history_count() - 1) {
                    hist_recall++;
                }
                if (hist_recall >= 0) load_history_into_input(hist_recall);
                de_damage_add_all();
                return 1;

            case DE_KEY_DOWN:
                if (hist_recall > 0) {
                    hist_recall--;
                    load_history_into_input(hist_recall);
                } else {
                    hist_recall = -1;
                    clear_input();
                }
                de_damage_add_all();
                return 1;

            case DE_KEY_PGUP: {
                int max = sb_visible_lines() - visible_rows();
                if (max < 0) max = 0;
                view_offset += visible_rows();
                if (view_offset > max) view_offset = max;
                de_damage_add_all();
                return 1;
            }

            case DE_KEY_PGDN: {
                view_offset -= visible_rows();
                if (view_offset < 0) view_offset = 0;
                de_damage_add_all();
                return 1;
            }

            case DE_KEY_HOME: {
                int max = sb_visible_lines() - visible_rows();
                if (max < 0) max = 0;
                view_offset = max;
                de_damage_add_all();
                return 1;
            }

            case DE_KEY_END:
                view_offset = 0;
                de_damage_add_all();
                return 1;

            case DE_KEY_DEL:
                /* Fall through to backspace handling below */
                break;

            default:
                return 0;
        }
    }

    /* Ctrl+L: clear */
    if (k->ctrl && (k->ascii == 'l' || k->ascii == 'L')) {
        sb_clear();
        clear_input();
        de_damage_add_all();
        return 1;
    }

    /* Ctrl+C: cancel input */
    if (k->ctrl && (k->ascii == 'c' || k->ascii == 'C')) {
        echo_prompt_line();
        clear_input();
        snap_view_to_bottom();
        de_damage_add_all();
        return 1;
    }

    /* Enter: run command */
    if (k->ascii == '\n') {
        echo_prompt_line();
        input[input_len] = '\0';
        if (input_len > 0) {
            shell_history_add(input);
            shell_execute_line(input);
        }
        clear_input();
        snap_view_to_bottom();
        de_damage_add_all();
        return 1;
    }

    /* Backspace */
    if (k->ascii == '\b' || (k->extended && k->keycode == DE_KEY_DEL)) {
        if (input_len > 0) {
            input_len--;
            input[input_len] = '\0';
            de_damage_add_all();
        }
        return 1;
    }

    /* Printable character: append and snap to bottom */
    if (k->ascii >= 32 && k->ascii < 127 && input_len < TERM_INPUT_MAX - 1) {
        input[input_len++] = k->ascii;
        input[input_len] = '\0';
        hist_recall = -1;
        snap_view_to_bottom();
        de_damage_add_all();
        return 1;
    }

    return 0;
}