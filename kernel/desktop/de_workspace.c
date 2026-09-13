
/* =============================================================================
 * Quark-OS kernel/desktop/de_workspace.c
 * Two workspaces: a fullscreen terminal and the desktop environment.
 * Switch with Ctrl+Alt+Arrow.
 * ============================================================================= */

#include <quark/de.h>
#include <quark/de_apps.h>
#include <quark/de_workspace.h>

volatile int g_workspace = DE_WORKSPACE_DESKTOP;

void de_workspace_init(void) {
    g_workspace = DE_WORKSPACE_DESKTOP;
    ws_terminal_init();
}

void de_workspace_switch(int ws) {
    if (ws < 0 || ws >= DE_WORKSPACE_COUNT) return;
    if (ws == g_workspace) return;
    g_workspace = ws;

    /* Whole screen is invalid now. */
    de_damage_add_all();
    de_mark_dirty();
}

void de_workspace_next(void) {
    int next = (g_workspace + 1) % DE_WORKSPACE_COUNT;
    de_workspace_switch(next);
}

void de_workspace_prev(void) {
    int prev = (g_workspace + DE_WORKSPACE_COUNT - 1) % DE_WORKSPACE_COUNT;
    de_workspace_switch(prev);
}

int de_workspace_handle_key(const de_key_event_t* k) {
    if (!k) return 0;

    /* Ctrl+Alt+Arrow switches workspaces. */
    if (k->ctrl && k->alt && k->extended) {
        if (k->keycode == DE_KEY_LEFT)  { de_workspace_prev(); return 1; }
        if (k->keycode == DE_KEY_RIGHT) { de_workspace_next(); return 1; }
    }

    /* Everything else goes to the fullscreen terminal when it's active. */
    if (g_workspace == DE_WORKSPACE_TERMINAL) {
        return ws_terminal_handle_key(k);
    }

    return 0;
}