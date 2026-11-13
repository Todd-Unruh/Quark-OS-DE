#include <quark/de.h>
#include <quark/de_apps.h>
#include <quark/de_workspace.h>

extern uint32_t timer_get_ticks(void);

gui_config_t g_gui_config = {
    .theme        = DE_THEME_DARK,
    .wallpaper    = DE_WALL_DEFAULT,
    .snap_enabled = 1,
    .show_icons   = 0,
};

/* ---- Console ring buffer (still used by the workspace terminal) ------- */
static char console_lines[DE_CONSOLE_LINES][DE_CONSOLE_LINE_LEN];
static int  console_count = 0;
static int  console_head  = 0;

void de_console_log(const char* line) {
    int slot = console_head;
    int i = 0;
    while (line[i] && i < DE_CONSOLE_LINE_LEN - 1) {
        console_lines[slot][i] = line[i];
        i++;
    }
    console_lines[slot][i] = '\0';
    console_head = (console_head + 1) % DE_CONSOLE_LINES;
    if (console_count < DE_CONSOLE_LINES) console_count++;
}

const char* de_console_line(int i) {
    if (i < 0 || i >= console_count) return NULL;
    int start = (console_head - console_count + DE_CONSOLE_LINES)
                % DE_CONSOLE_LINES;
    int slot = (start + i) % DE_CONSOLE_LINES;
    return console_lines[slot];
}

window_t g_windows[DE_MAX_WINDOWS];
static uint8_t prev_mouse_click = 0;

/* ---- Desktop icons ---------------------------------------------------- */
#define ICON_SIZE 48
#define ICON_CELL_W 96
#define ICON_CELL_H 96
#define ICON_COLS 2
#define ICON_TOP 24
#define ICON_LEFT 24

typedef struct {
    const char* label;
    const char* app_id;
} desktop_icon_t;

static desktop_icon_t icons[] = {
    { "Editor",   "editor" },
    { "Files",    "files" },
    { "Calc",     "calc" },
    { "About",    "about" },
};
#define ICON_COUNT ((int)(sizeof(icons)/sizeof(icons[0])))

static int selected_icon = -1;
static uint32_t last_icon_click_ticks = 0;
static int      last_icon_click_index = -1;

static void icon_rect(int i, int* x, int* y) {
    *x = ICON_LEFT + (i % ICON_COLS) * ICON_CELL_W;
    *y = ICON_TOP  + (i / ICON_COLS) * ICON_CELL_H;
}

static void icon_draw_one(int i) {
    const de_theme_t* t = g_theme;
    int x, y;
    icon_rect(i, &x, &y);
    int sel = (selected_icon == i);
    if (sel) {
        de_fill_rounded_alpha(x - 4, y - 4, ICON_CELL_W - 8, ICON_CELL_H - 8,
                              6, t->accent, 70);
    }
    de_fill_rounded(x, y, ICON_SIZE, ICON_SIZE, 8, t->accent);
    de_border_rounded(x, y, ICON_SIZE, ICON_SIZE, 8, t->win_border);
    char letter[2] = { icons[i].label[0], 0 };
    de_text_centered(x + ICON_SIZE / 2 - 4, y + ICON_SIZE / 2 - 4,
                     letter, 0x00FFFFFF);
    de_text_centered(x + ICON_SIZE / 2, y + ICON_SIZE + 6,
                     icons[i].label, t->txt_light);
}

static void icons_draw_all(void) {
    if (!g_gui_config.show_icons) return;
    for (int i = 0; i < ICON_COUNT; i++) icon_draw_one(i);
}

static int icon_hit(int mx, int my, int* out_index) {
    if (!g_gui_config.show_icons) return 0;
    for (int i = 0; i < ICON_COUNT; i++) {
        int x, y;
        icon_rect(i, &x, &y);
        if (mx >= x - 4 && mx < x + ICON_CELL_W - 8 &&
            my >= y - 4 && my < y + ICON_CELL_H - 8) {
            if (out_index) *out_index = i;
            return 1;
        }
    }
    return 0;
}

static void icon_activate(int i) { de_app_open(icons[i].app_id); }

/* ---- Compositor (called per damage rect) ------------------------------ */
static void desktop_redraw_at_clip(de_rect_t* r) {
    de_clip_set(r->x, r->y, r->w, r->h);
    de_wallpaper_erase_region(r->x, r->y, r->w, r->h);
    icons_draw_all();
    de_windows_draw_all();
    de_panel_draw();
    de_spotlight_draw();
    de_notify_draw();
    de_cursor_draw();
    de_clip_reset();
}

/* ---- Key routing ------------------------------------------------------ */
static void route_key(de_key_event_t* k) {
    if (de_spotlight_is_open()) {
        if (de_spotlight_handle_key(k)) {
            de_mark_dirty();
        }
        return;
    }

    if (k->ctrl && !k->extended && k->ascii == ' ') {
        de_spotlight_open();
        return;
    }

    if (de_workspace_handle_key(k)) {
        de_mark_dirty();
        return;
    }
    if (g_workspace == DE_WORKSPACE_TERMINAL) return;

    if (k->alt && !k->extended && k->ascii == '\t') {
        de_focus_cycle_next();
        de_damage_add_all();
        return;
    }
    int fi = de_focus_get();
    if (fi < 0) return;
    if (fi != 0) de_wm_raise(fi);
    window_t* w = &g_windows[0];
    if (w->app && w->app->on_key) {
        w->app->on_key(w->app_state, w, k);
        de_wm_damage_window(w);
    }
}

/* ---- Hover tracking --------------------------------------------------- */
static void update_window_hover(void) {
    for (int i = 0; i < DE_MAX_WINDOWS; i++) {
        window_t* w = &g_windows[i];
        if (!w->is_active || w->is_minimized) {
            if (w->is_hovered) {
                w->is_hovered = 0;
                de_wm_damage_window(w);
            }
            continue;
        }
        uint8_t inside = (mouse_x >= w->x && mouse_x < w->x + w->width &&
                          mouse_y >= w->y && mouse_y < w->y + w->height);
        if (inside) {
            for (int j = 0; j < i; j++) {
                window_t* o = &g_windows[j];
                if (!o->is_active || o->is_minimized) continue;
                if (mouse_x >= o->x && mouse_x < o->x + o->width &&
                    mouse_y >= o->y && mouse_y < o->y + o->height) {
                    inside = 0;
                    break;
                }
            }
        }
        if (w->is_hovered != inside) {
            w->is_hovered = inside;
            de_wm_damage_window(w);
        }
    }
}

/* ---- Desktop event handling ------------------------------------------- */
static void process_desktop_events(void) {
    uint8_t clicked = (mouse_left_click && !prev_mouse_click);

    if (!mouse_left_click) {
        for (int i = 0; i < DE_MAX_WINDOWS; i++) {
            if (g_windows[i].is_dragging) {
                g_windows[i].is_dragging = 0;
                de_wm_snap_to_edge(&g_windows[i]);
                de_wm_damage_window(&g_windows[i]);
            }
            g_windows[i].is_resizing = 0;
        }
    }
    prev_mouse_click = mouse_left_click;

    for (int i = 0; i < DE_MAX_WINDOWS; i++) {
        window_t* w = &g_windows[i];
        if (!w->is_active) continue;
        if (w->is_dragging) {
            int old_x = w->x, old_y = w->y;
            w->x = mouse_x - w->drag_offset_x;
            w->y = mouse_y - w->drag_offset_y;
            de_wm_clamp(w);
            de_damage_add(old_x - DE_SHADOW_BLUR, old_y - DE_SHADOW_BLUR,
                          w->width  + DE_SHADOW_BLUR * 2,
                          w->height + DE_SHADOW_BLUR * 2);
            de_damage_add(w->x - DE_SHADOW_BLUR, w->y - DE_SHADOW_BLUR,
                          w->width  + DE_SHADOW_BLUR * 2,
                          w->height + DE_SHADOW_BLUR * 2);
            return;
        }
        if (w->is_resizing) {
            int old_w = w->width, old_h = w->height;
            int nw = w->resize_start_w + (mouse_x - w->resize_start_x);
            int nh = w->resize_start_h + (mouse_y - w->resize_start_y);
            if (nw < 200) nw = 200;
            if (nh < 140) nh = 140;
            if (w->x + nw > de_fb_w()) nw = de_fb_w() - w->x;
            if (w->y + nh > de_screen_bottom()) nh = de_screen_bottom() - w->y;
            w->width = nw;
            w->height = nh;
            int uw = (old_w > nw ? old_w : nw);
            int uh = (old_h > nh ? old_h : nh);
            de_damage_add(w->x - DE_SHADOW_BLUR, w->y - DE_SHADOW_BLUR,
                          uw + DE_SHADOW_BLUR * 2,
                          uh + DE_SHADOW_BLUR * 2);
            return;
        }
    }

    if (!clicked) return;

    if (de_spotlight_is_open()) {
        if (de_spotlight_click(mouse_x, mouse_y)) return;
    }

    if (de_panel_hit_any(mouse_x, mouse_y)) {
        if (de_panel_hit_launcher(mouse_x, mouse_y)) {
            de_spotlight_toggle();
            return;
        }
        int dock_idx;
        if (de_panel_hit_task(mouse_x, mouse_y, &dock_idx)) {
            int n_apps = app_registry_dock_count();
            if (dock_idx == n_apps + 1) {
                de_wm_create(DE_ROLE_SETTINGS, "System Settings", 220, 60, 380, 380);
            } else {
                const app_t* a = app_registry_dock_at(dock_idx - 1);
                if (a) de_app_open(a->id);
            }
            return;
        }
        return;
    }

    for (int i = 0; i < DE_MAX_WINDOWS; i++) {
        window_t* w = &g_windows[i];
        if (!w->is_active || w->is_minimized) continue;
        int inside = mouse_x >= w->x && mouse_x < w->x + w->width &&
                     mouse_y >= w->y && mouse_y < w->y + w->height;
        if (!inside) continue;

        if (i != 0) de_wm_damage_window(w);

        de_wm_raise(i);
        window_t* target = &g_windows[0];

        if (de_wm_handle_resize(target, mouse_x, mouse_y)) return;

        if (mouse_y < target->y + DE_TITLEBAR_H) {
            de_wm_handle_titlebar_click(target, mouse_x, mouse_y);
            return;
        }

        if (target->role == DE_ROLE_SETTINGS) {
            if (de_settings_click(target, mouse_x, mouse_y)) {
                de_wm_damage_window(target);
            }
            return;
        }

        if (target->app && target->app->on_click) {
            target->app->on_click(target->app_state, target, mouse_x, mouse_y);
            de_wm_damage_window(target);
        }
        return;
    }

    int icon_idx;
    if (icon_hit(mouse_x, mouse_y, &icon_idx)) {
        uint32_t now = timer_get_ticks();
        if (last_icon_click_index == icon_idx &&
            now - last_icon_click_ticks < 50) {
            icon_activate(icon_idx);
            last_icon_click_index = -1;
        } else {
            selected_icon = icon_idx;
            last_icon_click_index = icon_idx;
            last_icon_click_ticks = now;
        }
        for (int i = 0; i < ICON_COUNT; i++) {
            int ix, iy;
            icon_rect(i, &ix, &iy);
            de_damage_add(ix - 4, iy - 4, ICON_CELL_W, ICON_CELL_H);
        }
        return;
    }
    if (selected_icon != -1) {
        selected_icon = -1;
        for (int i = 0; i < ICON_COUNT; i++) {
            int ix, iy;
            icon_rect(i, &ix, &iy);
            de_damage_add(ix - 4, iy - 4, ICON_CELL_W, ICON_CELL_H);
        }
    }
}

/* ---- Entry ------------------------------------------------------------ */
int desktop_run(int mode) {
    (void)mode;

    if (fb_width == 0 || fb_height == 0 || framebuffer_pixels == NULL) {
        kprintf("[DE] framebuffer not ready\n");
        return -1;
    }
    if (de_backbuffer_alloc() != 0) return -1;

    kmemset(g_windows, 0, sizeof(g_windows));
    de_backbuffer_clear();

    de_theme_apply(g_gui_config.theme);
    de_rtc_init();
    de_notify_init();
    de_wallpaper_init();
    de_input_init();
    de_workspace_init();

    de_console_log("Quark-OS v3.0 initialized");
    de_console_log("Ctrl+Alt+Left/Right switches workspaces");
    de_console_log("Ctrl+Space opens Spotlight");

    de_notify("Quark OS", "Welcome to the desktop");

    kprintf("[DESKTOP] Session started on desktop workspace.\n");

    de_damage_add_all();

    uint32_t last_second = 0xFFFFFFFF;
    uint32_t last_cursor_blink = 0;
    uint32_t last_tick = timer_get_ticks();
    int last_mx = -1, last_my = -1;

    /* Track workspace transitions so we can install/remove the sink. */
    int last_workspace = g_workspace;

    while (1) {
        net_poll();
        uint32_t now_tick = timer_get_ticks();
        uint32_t tick_delta = now_tick - last_tick;
        last_tick = now_tick;
        float dt = (float)tick_delta * 0.01f;
        if (dt > 0.1f) dt = 0.1f;

        de_key_event_t kev;
        while (de_input_pop(&kev)) route_key(&kev);

        /* Workspace transition: install/remove the terminal sink. */
        if (g_workspace != last_workspace) {
            if (g_workspace == DE_WORKSPACE_TERMINAL) {
                ws_terminal_init();
            } else if (last_workspace == DE_WORKSPACE_TERMINAL) {
                ws_terminal_shutdown();
            }
            last_workspace = g_workspace;
        }

        /* --- Terminal workspace ------------------------------------ */
        if (g_workspace == DE_WORKSPACE_TERMINAL) {
            if (mouse_x != last_mx || mouse_y != last_my) {
                last_mx = mouse_x;
                last_my = mouse_y;
            }

            uint32_t tick = timer_get_ticks();
            if (tick - last_cursor_blink >= 25) {
                last_cursor_blink = tick;
                de_damage_add_all();
            }

            de_rect_t r;
            while (de_damage_next(&r)) {
                de_clip_set(r.x, r.y, r.w, r.h);
                ws_terminal_draw();
                de_clip_reset();
                de_blit_region(r.x, r.y, r.w, r.h);
            }
            de_damage_clear();

            timer_idle_wait();
            continue;
        }

        /* --- Desktop workspace ------------------------------------- */
        process_desktop_events();
        update_window_hover();
        de_wm_tick(dt);

        for (int i = 0; i < DE_MAX_WINDOWS; i++) {
            window_t* w = &g_windows[i];
            if (!w->is_active || !w->app || !w->app->on_tick) continue;
            if (w->app->on_tick(w->app_state, w, now_tick) &&
                !w->is_minimized) {
                de_wm_damage_window(w);
            }
        }

        /* Advance the window-open animation. If a window is still
         * opening, damage it so the animation is visible. */
        for (int i = 0; i < DE_MAX_WINDOWS; i++) {
            window_t* w = &g_windows[i];
            if (!w->is_active) continue;
            if (!de_anim_done(&w->anim_open)) {
                de_wm_damage_window(w);
            }
        }

        /* Mouse movement: damage the cursor's old and new cells,
         * and the dock/menu bar strips when the cursor is near them. */
        if (mouse_x != last_mx || mouse_y != last_my) {
            if (last_mx >= 0) de_damage_add(last_mx, last_my, 18, 18);
            de_damage_add(mouse_x, mouse_y, 18, 18);

            int dock_zone_top = de_fb_h() - DE_DOCK_H - 24;
            if (mouse_y >= dock_zone_top || last_my >= dock_zone_top) {
                de_damage_add(0, de_fb_h() - DE_DOCK_H,
                              de_fb_w(), DE_DOCK_H);
            }

            int bar_zone_bottom = DE_MENUBAR_H + 4;
            if (mouse_y <= bar_zone_bottom || last_my <= bar_zone_bottom) {
                de_damage_add(0, 0, de_fb_w(), DE_MENUBAR_H);
            }

            if (de_spotlight_is_open()) {
                int lx, ly, lw, lh;
                de_spotlight_rect(&lx, &ly, &lw, &lh);
                de_damage_add(lx - 24, ly - 24, lw + 48, lh + 48);
            }

            last_mx = mouse_x;
            last_my = mouse_y;
        }

        /* Second tick: menu-bar clock. */
        int h, m, s;
        de_now_hms(&h, &m, &s);
        uint32_t sec = (uint32_t)(h * 3600 + m * 60 + s);
        if (sec != last_second) {
            de_damage_add(0, 0, de_fb_w(), DE_MENUBAR_H);
            last_second = sec;
        }

        /* Blink tick: redraw focused window's caret area and any open
         * spotlight. */
        uint32_t tick = timer_get_ticks();
        if (tick - last_cursor_blink >= 25) {
            last_cursor_blink = tick;
            int fi = de_focus_get();
            if (fi >= 0 && g_windows[fi].app) {
                de_wm_damage_window(&g_windows[fi]);
            }
            if (de_spotlight_is_open()) {
                int lx, ly, lw, lh;
                de_spotlight_rect(&lx, &ly, &lw, &lh);
                de_damage_add(lx - 24, ly - 24, lw + 48, lh + 48);
            }
        }

        de_notify_tick();

        de_rect_t r;
        while (de_damage_next(&r)) {
            desktop_redraw_at_clip(&r);
            de_blit_region(r.x, r.y, r.w, r.h);
        }
        de_damage_clear();

        timer_idle_wait();
    }

    de_backbuffer_release();
    return 0;
}

void desktop_handle_keyboard(uint8_t scancode) { (void)scancode; }