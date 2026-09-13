/* =============================================================================
 * Quark-OS kernel/desktop/de_focus.c
 * Alt-Tab focus cycling among visible app windows.
 * ============================================================================= */

#include <quark/de.h>

static int alt_tab_index = 0;
static int alt_tab_active = 0;

int de_focus_get(void) {
    for (int i = 0; i < DE_MAX_WINDOWS; i++) {
        window_t* w = &g_windows[i];
        if (w->is_active && !w->is_minimized) return i;
    }
    return -1;
}

void de_focus_set(int idx) {
    if (idx < 0 || idx >= DE_MAX_WINDOWS) return;
    if (!g_windows[idx].is_active) return;
    de_wm_raise(idx);
}

static int count_visible(void) {
    int n = 0;
    for (int i = 0; i < DE_MAX_WINDOWS; i++) {
        if (g_windows[i].is_active && !g_windows[i].is_minimized) n++;
    }
    return n;
}

void de_focus_cycle_next(void) {
    int n = count_visible();
    if (n < 2) return;
    if (!alt_tab_active) {
        alt_tab_active = 1;
        alt_tab_index = 1;   /* start at second */
    } else {
        alt_tab_index = (alt_tab_index + 1) % n;
    }
    /* Find the alt_tab_index-th visible window */
    int seen = 0;
    for (int i = 0; i < DE_MAX_WINDOWS; i++) {
        if (!g_windows[i].is_active || g_windows[i].is_minimized) continue;
        if (seen == alt_tab_index) {
            de_wm_raise(i);
            break;
        }
        seen++;
    }
}

void de_focus_cycle_prev(void) {
    int n = count_visible();
    if (n < 2) return;
    if (!alt_tab_active) {
        alt_tab_active = 1;
        alt_tab_index = n - 1;
    } else {
        alt_tab_index = (alt_tab_index + n - 1) % n;
    }
    int seen = 0;
    for (int i = 0; i < DE_MAX_WINDOWS; i++) {
        if (!g_windows[i].is_active || g_windows[i].is_minimized) continue;
        if (seen == alt_tab_index) {
            de_wm_raise(i);
            break;
        }
        seen++;
    }
}
