#include <quark/de.h>
#include <quark/de_apps.h>

static int find_index_of(window_t* w) {
    for (int i = 0; i < DE_MAX_WINDOWS; i++) {
        if (&g_windows[i] == w) return i;
    }
    return -1;
}

void de_wm_raise(int i) {
    if (i <= 0 || i >= DE_MAX_WINDOWS) return;
    de_wm_damage_window(&g_windows[i]);
    window_t tmp = g_windows[i];
    for (int j = i; j > 0; j--) g_windows[j] = g_windows[j - 1];
    g_windows[0] = tmp;
}

void de_wm_clamp(window_t* w) {
    if (!w) return;
    int max_x = de_fb_w() - w->width;
    int max_y = de_screen_bottom() - w->height;
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    if (w->x < 0) w->x = 0;
    if (w->y < 0) w->y = 0;
    if (w->x > max_x) w->x = max_x;
    if (w->y > max_y) w->y = max_y;
}

int de_wm_find_role(int role) {
    if (role == DE_ROLE_APP || role == DE_ROLE_NONE) return -1;
    for (int i = 0; i < DE_MAX_WINDOWS; i++) {
        if (g_windows[i].is_active && g_windows[i].role == role) return i;
    }
    return -1;
}

int de_wm_create(int role, const char* title, int x, int y, int w, int h) {
    int existing = de_wm_find_role(role);
    if (existing >= 0) {
        g_windows[existing].is_minimized = 0;
        de_wm_raise(existing);
        de_wm_damage_window(&g_windows[existing]);
        return existing;
    }
    for (int i = DE_MAX_WINDOWS - 1; i >= 0; i--) {
        if (g_windows[i].is_active) continue;
        kmemset(&g_windows[i], 0, sizeof(window_t));
        g_windows[i].x = x; g_windows[i].y = y;
        g_windows[i].width = w; g_windows[i].height = h;
        g_windows[i].title = title;
        g_windows[i].is_active = 1;
        g_windows[i].role = role;
        g_windows[i].restore_x = x; g_windows[i].restore_y = y;
        g_windows[i].restore_w = w; g_windows[i].restore_h = h;
        de_wm_raise(i);
        de_wm_damage_window(&g_windows[0]);
        return i;
    }
    return -1;
}

int de_wm_create_app(const app_t* app, int x, int y, int w, int h) {
    if (!app) return -1;
    for (int i = 0; i < DE_MAX_WINDOWS; i++) {
        window_t* win = &g_windows[i];
        if (win->is_active && win->app == app) {
            win->is_minimized = 0;
            de_wm_raise(i);
            de_wm_damage_window(&g_windows[0]);
            return i;
        }
    }
    for (int i = DE_MAX_WINDOWS - 1; i >= 0; i--) {
        if (g_windows[i].is_active) continue;
        kmemset(&g_windows[i], 0, sizeof(window_t));
        g_windows[i].x = x;
        g_windows[i].y = y;
        g_windows[i].width = w;
        g_windows[i].height = h;
        g_windows[i].title = app->name;
        g_windows[i].is_active = 1;
        g_windows[i].role = DE_ROLE_APP;
        g_windows[i].app = app;
        g_windows[i].app_state = app->on_open ? app->on_open() : NULL;
        g_windows[i].restore_x = x; g_windows[i].restore_y = y;
        g_windows[i].restore_w = w; g_windows[i].restore_h = h;
        de_wm_raise(i);
        de_wm_damage_window(&g_windows[0]);
        return i;
    }
    return -1;
}

void de_wm_minimize(int i) {
    if (i < 0 || i >= DE_MAX_WINDOWS) return;
    de_wm_damage_window(&g_windows[i]);
    g_windows[i].is_minimized = 1;
    g_windows[i].is_dragging = 0;
    g_windows[i].is_resizing = 0;
}

void de_wm_restore_and_raise(int i) {
    if (i < 0 || i >= DE_MAX_WINDOWS) return;
    g_windows[i].is_minimized = 0;
    de_wm_raise(i);
    de_wm_damage_window(&g_windows[0]);
}

void de_wm_maximize_toggle(int i) {
    if (i < 0 || i >= DE_MAX_WINDOWS) return;
    window_t* w = &g_windows[i];
    de_wm_damage_window(w);
    if (!w->is_maximized) {
        w->restore_x = w->x; w->restore_y = w->y;
        w->restore_w = w->width; w->restore_h = w->height;
        w->x = 0; w->y = 0;
        w->width  = de_fb_w();
        w->height = de_screen_bottom();
        w->is_maximized = 1;
    } else {
        w->x = w->restore_x; w->y = w->restore_y;
        w->width = w->restore_w; w->height = w->restore_h;
        w->is_maximized = 0;
    }
    de_wm_damage_window(w);
}

void de_wm_close(int i) {
    if (i < 0 || i >= DE_MAX_WINDOWS) return;
    if (g_windows[i].app && g_windows[i].app->on_close) {
        g_windows[i].app->on_close(g_windows[i].app_state);
    }
    de_wm_damage_window(&g_windows[i]);
    kmemset(&g_windows[i], 0, sizeof(window_t));
}

void de_wm_snap_to_edge(window_t* w) {
    if (!w) return;
    if (!g_gui_config.snap_enabled) return;
    if (w->is_maximized) return;

    const int T = 12;
    int sw = de_fb_w();
    int sh = de_screen_bottom();

    if (w->y <= T) {
        int idx = find_index_of(w);
        if (idx >= 0) de_wm_maximize_toggle(idx);
        return;
    }
    if (w->x <= T) { w->x = 0; w->y = 0; w->width = sw / 2; w->height = sh; return; }
    if (w->x + w->width >= sw - T) {
        w->x = sw / 2; w->y = 0; w->width = sw - sw / 2; w->height = sh; return;
    }
    if (w->y + w->height >= sh - T) {
        w->x = 0; w->y = sh / 2; w->width = sw; w->height = sh - sh / 2; return;
    }
}

int de_wm_handle_resize(window_t* w, int mx, int my) {
    if (!w || w->is_maximized) return 0;
    int gx = w->x + w->width - DE_RESIZE_GRIP;
    int gy = w->y + w->height - DE_RESIZE_GRIP;
    if (mx < gx || my < gy) return 0;
    w->is_resizing = 1;
    w->resize_start_x = mx;
    w->resize_start_y = my;
    w->resize_start_w = w->width;
    w->resize_start_h = w->height;
    return 1;
}

int de_wm_handle_titlebar_click(window_t* w, int mx, int my) {
    if (!w) return 0;
    int bx_close = w->x + w->width - DE_BTN_W - 6;
    int bx_max   = bx_close - DE_BTN_W - DE_BTN_GAP;
    int bx_min   = bx_max   - DE_BTN_W - DE_BTN_GAP;
    int by       = w->y + (DE_TITLEBAR_H - DE_BTN_H) / 2;

    if (mx >= bx_close && mx < bx_close + DE_BTN_W &&
        my >= by && my < by + DE_BTN_H) {
        int idx = find_index_of(w);
        if (idx >= 0) de_wm_close(idx);
        return 1;
    }
    if (mx >= bx_max && mx < bx_max + DE_BTN_W &&
        my >= by && my < by + DE_BTN_H) {
        int idx = find_index_of(w);
        if (idx >= 0) de_wm_maximize_toggle(idx);
        return 1;
    }
    if (mx >= bx_min && mx < bx_min + DE_BTN_W &&
        my >= by && my < by + DE_BTN_H) {
        int idx = find_index_of(w);
        if (idx >= 0) de_wm_minimize(idx);
        return 1;
    }
    w->is_dragging   = 1;
    w->drag_offset_x = mx - w->x;
    w->drag_offset_y = my - w->y;
    return 1;
}