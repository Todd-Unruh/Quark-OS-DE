#include <quark/de.h>
#include <quark/de_apps.h>

static int find_index_of(window_t* w) {
    for (int i = 0; i < DE_MAX_WINDOWS; i++) {
        if (&g_windows[i] == w) return i;
    }
    return -1;
}

/* ------------------------------------------------------------------------- */
/* Animation channels                                                         */
/* ------------------------------------------------------------------------- */

/* Damage both the new and previous shadow-inclusive area. Used when a
 * window is about to move, resize, or be raised so we don't leave
 * ghost shadow pixels behind. */
static void damage_old_and_new(const window_t* w,
                               int old_x, int old_y,
                               int old_w, int old_h) {
    de_damage_add(old_x - DE_SHADOW_BLUR, old_y - DE_SHADOW_BLUR,
                  old_w + DE_SHADOW_BLUR * 2,
                  old_h + DE_SHADOW_BLUR * 2);
    de_wm_damage_window(w);
}

void de_wm_tick(float dt) {
    for (int i = 0; i < DE_MAX_WINDOWS; i++) {
        window_t* w = &g_windows[i];
        if (!w->is_active) continue;

        de_anim_tick(&w->anim_open, dt);

        if (w->anim_move_x.active) {
            de_anim_tick(&w->anim_move_x, dt);
            de_anim_tick(&w->anim_move_y, dt);
            de_anim_tick(&w->anim_move_w, dt);
            de_anim_tick(&w->anim_move_h, dt);

            int old_x = w->x, old_y = w->y;
            int old_w = w->width, old_h = w->height;

            w->x      = (int)de_anim_value(&w->anim_move_x);
            w->y      = (int)de_anim_value(&w->anim_move_y);
            w->width  = (int)de_anim_value(&w->anim_move_w);
            w->height = (int)de_anim_value(&w->anim_move_h);

            damage_old_and_new(w, old_x, old_y, old_w, old_h);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Z-order                                                                    */
/* ------------------------------------------------------------------------- */

void de_wm_raise(int i) {
    if (i <= 0 || i >= DE_MAX_WINDOWS) return;
    de_wm_damage_window(&g_windows[i]);
    window_t tmp = g_windows[i];
    for (int j = i; j > 0; j--) g_windows[j] = g_windows[j - 1];
    g_windows[0] = tmp;
    de_wm_damage_window(&g_windows[0]);
}

void de_wm_clamp(window_t* w) {
    if (!w) return;
    /* Keep at least a slice of the title bar reachable, so a window
     * dragged mostly off-screen can still be recovered. */
    int min_visible = 60;
    int max_x = de_fb_w() - min_visible;
    int max_y = de_screen_bottom() - DE_TITLEBAR_H;
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    if (w->x < -(w->width - min_visible)) w->x = -(w->width - min_visible);
    if (w->y < 0) w->y = 0;
    if (w->x > max_x) w->x = max_x;
    if (w->y > max_y) w->y = max_y;
}

/* ------------------------------------------------------------------------- */
/* Lookup / creation                                                          */
/* ------------------------------------------------------------------------- */

int de_wm_find_role(int role) {
    if (role == DE_ROLE_APP || role == DE_ROLE_NONE) return -1;
    for (int i = 0; i < DE_MAX_WINDOWS; i++) {
        if (g_windows[i].is_active && g_windows[i].role == role) return i;
    }
    return -1;
}

static void init_window_animations(window_t* w) {
    de_anim_start(&w->anim_open, 0.0f, 1.0f, 0.18f);
    w->anim_move_x.active = 0;
    w->anim_move_y.active = 0;
    w->anim_move_w.active = 0;
    w->anim_move_h.active = 0;
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
        init_window_animations(&g_windows[i]);
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
        init_window_animations(&g_windows[i]);
        de_wm_raise(i);
        de_wm_damage_window(&g_windows[0]);
        return i;
    }
    return -1;
}

/* ------------------------------------------------------------------------- */
/* Minimize / restore / maximize / close                                      */
/* ------------------------------------------------------------------------- */

void de_wm_minimize(int i) {
    if (i < 0 || i >= DE_MAX_WINDOWS) return;
    de_wm_damage_window(&g_windows[i]);
    g_windows[i].is_minimized = 1;
    g_windows[i].is_dragging = 0;
    g_windows[i].is_resizing = 0;
    g_windows[i].is_hovered  = 0;
}

void de_wm_restore_and_raise(int i) {
    if (i < 0 || i >= DE_MAX_WINDOWS) return;
    g_windows[i].is_minimized = 0;
    /* Replay the open animation so restoring from the dock feels alive. */
    de_anim_start(&g_windows[i].anim_open, 0.0f, 1.0f, 0.18f);
    de_wm_raise(i);
    de_wm_damage_window(&g_windows[0]);
}

void de_wm_maximize_toggle(int i) {
    if (i < 0 || i >= DE_MAX_WINDOWS) return;
    window_t* w = &g_windows[i];
    de_wm_damage_window(w);

    int tx, ty, tw, th;
    if (!w->is_maximized) {
        w->restore_x = w->x; w->restore_y = w->y;
        w->restore_w = w->width; w->restore_h = w->height;
        tx = 0; ty = 0;
        tw = de_fb_w();
        th = de_screen_bottom();
        w->is_maximized = 1;
    } else {
        tx = w->restore_x; ty = w->restore_y;
        tw = w->restore_w; th = w->restore_h;
        w->is_maximized = 0;
    }

    de_anim_start(&w->anim_move_x, (float)w->x,     (float)tx, 0.22f);
    de_anim_start(&w->anim_move_y, (float)w->y,     (float)ty, 0.22f);
    de_anim_start(&w->anim_move_w, (float)w->width, (float)tw, 0.22f);
    de_anim_start(&w->anim_move_h, (float)w->height,(float)th, 0.22f);

    /* The whole screen may be affected; be generous. */
    de_damage_add_all();
}

void de_wm_close(int i) {
    if (i < 0 || i >= DE_MAX_WINDOWS) return;
    if (g_windows[i].app && g_windows[i].app->on_close) {
        g_windows[i].app->on_close(g_windows[i].app_state);
    }
    de_wm_damage_window(&g_windows[i]);
    kmemset(&g_windows[i], 0, sizeof(window_t));
}

/* ------------------------------------------------------------------------- */
/* Snapping                                                                   */
/* ------------------------------------------------------------------------- */

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

/* ------------------------------------------------------------------------- */
/* Resize                                                                     */
/* ------------------------------------------------------------------------- */

int de_wm_handle_resize(window_t* w, int mx, int my) {
    if (!w || w->is_maximized) return 0;
    int gx = w->x + w->width  - DE_RESIZE_GRIP;
    int gy = w->y + w->height - DE_RESIZE_GRIP;
    if (mx < gx || my < gy) return 0;
    w->is_resizing = 1;
    w->resize_start_x = mx;
    w->resize_start_y = my;
    w->resize_start_w = w->width;
    w->resize_start_h = w->height;
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Title-bar hit testing (traffic lights now on the LEFT)                     */
/* ------------------------------------------------------------------------- */

int de_wm_handle_titlebar_click(window_t* w, int mx, int my) {
    if (!w) return 0;

    /* Geometry must match de_windows.c exactly. */
    int r  = DE_BTN_W / 2;
    int by = w->y + DE_TITLEBAR_H / 2;
    int bx = w->x + DE_BTN_LEFT_PAD + r;    /* center of "close" */

    /* Hit regions are circles; use a squared-distance test. */
    int ccx[3];
    ccx[0] = bx;                                        /* close */
    ccx[1] = bx + (DE_BTN_W + DE_BTN_GAP);              /* min   */
    ccx[2] = bx + (DE_BTN_W + DE_BTN_GAP) * 2;          /* max   */

    int rr = r + 2;          /* slight forgiveness */
    int rr2 = rr * rr;
    for (int k = 0; k < 3; k++) {
        int dx = mx - ccx[k];
        int dy = my - by;
        if (dx * dx + dy * dy <= rr2) {
            int idx = find_index_of(w);
            if (idx < 0) return 1;
            if (k == 0)      de_wm_close(idx);
            else if (k == 1) de_wm_minimize(idx);
            else             de_wm_maximize_toggle(idx);
            return 1;
        }
    }

    /* Anywhere else on the title bar starts a drag. */
    w->is_dragging   = 1;
    w->drag_offset_x = mx - w->x;
    w->drag_offset_y = my - w->y;
    return 1;
}