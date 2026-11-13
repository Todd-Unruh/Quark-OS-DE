#ifndef QUARK_DE_H
#define QUARK_DE_H

#include <quark/kernel.h>
#include <quark/de_damage.h>


void ws_terminal_shutdown(void);

/* --------------------------------------------------------------------------- */
/* Config                                                                      */
/* --------------------------------------------------------------------------- */
typedef struct {
    int theme;
    int wallpaper;
    int snap_enabled;
    int show_icons;
} gui_config_t;

#define DE_THEME_DARK  0
#define DE_THEME_LIGHT 1

#define DE_MENUBAR_H  24
#define DE_DOCK_H     64
#define DE_PANEL_H    DE_MENUBAR_H   /* legacy alias, keep for de_screen_bottom */

#define DE_WALL_FLOW    0
#define DE_WALL_DARK    1
#define DE_WALL_LIGHT   2
#define DE_WALL_SOLID_A 3
#define DE_WALL_SOLID_B 4

/* Baked-sprite wallpapers (from tools/Sprites/wallpaper_*.png) */
#define DE_WALL_DEFAULT         5
#define DE_WALL_MIDNIGHT        6
#define DE_WALL_PLASMA_SUNSET   7

int de_panel_menu_y(void);
int de_panel_dock_y(void);

extern gui_config_t g_gui_config;

/* --------------------------------------------------------------------------- */
/* Animation                                                                   */
/* --------------------------------------------------------------------------- */
typedef struct {
    float from;
    float to;
    float elapsed;
    float duration;
    uint8_t active;
} de_anim_t;

void  de_anim_start(de_anim_t* a, float from, float to, float dur);
void  de_anim_to   (de_anim_t* a, float to, float dur);
void  de_anim_tick (de_anim_t* a, float dt);
float de_anim_value(const de_anim_t* a);
int   de_anim_done (const de_anim_t* a);

float de_ease_linear      (float t);
float de_ease_out_cubic   (float t);
float de_ease_in_out_cubic(float t);
float de_ease_out_back    (float t);

/* --------------------------------------------------------------------------- */
/* Window                                                                      */
/* --------------------------------------------------------------------------- */
#define DE_MAX_WINDOWS 12

struct app_s;

typedef struct window_s {
    int x, y;
    int width, height;
    const char* title;
    uint8_t is_active;
    uint8_t is_dragging;
    uint8_t is_minimized;
    uint8_t is_maximized;
    uint8_t is_resizing;
    uint8_t is_hovered;
    int drag_offset_x;
    int drag_offset_y;
    int resize_start_x, resize_start_y;
    int resize_start_w, resize_start_h;
    int restore_x, restore_y, restore_w, restore_h;
    int role;
    const struct app_s* app;
    void* app_state;
    int scroll_y;
    int content_h;

    de_anim_t anim_open;
    de_anim_t anim_move_x;
    de_anim_t anim_move_y;
    de_anim_t anim_move_w;
    de_anim_t anim_move_h;
} window_t;

#define DE_ROLE_NONE     0
#define DE_ROLE_TERMINAL 1
#define DE_ROLE_FILES    2
#define DE_ROLE_SETTINGS 3
#define DE_ROLE_NETWORK  4
#define DE_ROLE_EDITOR   5
#define DE_ROLE_CALC     6
#define DE_ROLE_ABOUT    7
#define DE_ROLE_APP      100

extern window_t g_windows[DE_MAX_WINDOWS];

/* --------------------------------------------------------------------------- */
/* Spotlight (search-based launcher)                                           */
/* --------------------------------------------------------------------------- */
struct de_key_event_s;
typedef struct de_key_event_s de_key_event_t;

void de_spotlight_open(void);
void de_spotlight_close(void);
int  de_spotlight_is_open(void);
void de_spotlight_toggle(void);
void de_spotlight_draw(void);
int  de_spotlight_click(int mx, int my);
int  de_spotlight_hit(int mx, int my);
void de_spotlight_rect(int* x, int* y, int* w, int* h);
int  de_spotlight_handle_key(de_key_event_t* k);

/* --------------------------------------------------------------------------- */
/* Window decoration metrics                                                   */
/* --------------------------------------------------------------------------- */
#define DE_TITLEBAR_H     28
#define DE_WIN_RADIUS     10
#define DE_BTN_W          12
#define DE_BTN_H          12
#define DE_BTN_GAP         8
#define DE_BTN_LEFT_PAD   12
#define DE_SHADOW         8
#define DE_SHADOW_BLUR    16
#define DE_RESIZE_GRIP    14

static inline void de_wm_damage_window(const window_t* w) {
    de_damage_add(w->x - DE_SHADOW_BLUR, w->y - DE_SHADOW_BLUR,
                  w->width  + DE_SHADOW_BLUR * 2,
                  w->height + DE_SHADOW_BLUR * 2);
}

/* --------------------------------------------------------------------------- */
/* Theme                                                                       */
/* --------------------------------------------------------------------------- */
typedef struct {
    uint32_t wall_top, wall_bot, wall_grid;
    uint32_t panel, panel_edge, panel_txt;
    uint32_t panel_btn, panel_hover, panel_sel, panel_sel_txt;
    uint32_t win_body, win_title_a, win_title_b, win_title_inactive;
    uint32_t win_border, win_shadow;
    uint32_t txt_dark, txt_light, txt_dim;
    uint32_t btn_close, btn_max, btn_min, btn_glyph;
    uint32_t widget_bg, widget_edge, widget_txt, widget_shadow;
    uint32_t accent;
    uint32_t app_bg, app_alt_bg, app_fg;

    uint8_t panel_alpha;
    uint8_t titlebar_alpha;
    uint8_t win_alpha;
    uint8_t shadow_alpha;
    uint8_t hover_alpha;
} de_theme_t;

extern const de_theme_t* g_theme;

void de_theme_apply(int theme_id);
const de_theme_t* de_theme_get(int theme_id);

/* --------------------------------------------------------------------------- */
/* Widgets / drawing                                                           */
/* --------------------------------------------------------------------------- */
int  de_fb_w(void);
int  de_fb_h(void);
int  de_panel_y(void);
int  de_screen_bottom(void);

uint32_t de_blend(uint32_t a, uint32_t b, uint8_t t);

void de_fill(int x, int y, int w, int h, uint32_t color);
void de_fill_alpha(int x, int y, int w, int h, uint32_t color, uint8_t a);
void de_fill_rounded(int x, int y, int w, int h, int r, uint32_t color);
void de_fill_rounded_alpha(int x, int y, int w, int h, int r,
                           uint32_t color, uint8_t a);
void de_fill_circle(int cx, int cy, int r, uint32_t color);
void de_fill_circle_alpha(int cx, int cy, int r, uint32_t color, uint8_t a);
void de_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bot);
void de_hline(int x, int y, int w, uint32_t color);
void de_vline(int x, int y, int h, uint32_t color);
void de_border(int x, int y, int w, int h, uint32_t color);
void de_border_rounded(int x, int y, int w, int h, int r, uint32_t color);
void de_line(int x0, int y0, int x1, int y1, uint32_t color);

void de_shadow_rect(int x, int y, int w, int h, int radius,
                    int blur, uint32_t color, uint8_t alpha);
void de_blur_region(int x, int y, int w, int h, int radius);

int  de_text_width(const char* s);
void de_text(int x, int y, const char* s, uint32_t color);
void de_text_clipped(int x, int y, const char* s, int max_w, uint32_t color);
void de_text_centered(int cx, int y, const char* s, uint32_t color);

void de_wallpaper_draw(void);
void de_wallpaper_invalidate(void);
void de_wallpaper_init(void);
void de_wallpaper_erase_region(int x, int y, int w, int h);

void de_cursor_draw(void);

int  de_backbuffer_alloc(void);
void de_backbuffer_release(void);
void de_backbuffer_clear(void);
void de_blit_to_screen(void);
void de_blit_region(int x, int y, int w, int h);

uint32_t* de_bb_ptr(void);
int       de_bb_w(void);
int       de_bb_h(void);

extern volatile uint8_t g_frame_dirty;
void de_mark_dirty(void);

/* --------------------------------------------------------------------------- */
/* Windows / WM                                                                */
/* --------------------------------------------------------------------------- */
void de_windows_draw_all(void);
void de_window_draw(window_t* w);
void de_windows_draw_one(window_t* w);

void de_wm_raise(int i);
void de_wm_clamp(window_t* w);
int  de_wm_find_role(int role);
int  de_wm_create(int role, const char* title, int x, int y, int w, int h);
int  de_wm_create_app(const struct app_s* app, int x, int y, int w, int h);
void de_wm_minimize(int i);
void de_wm_restore_and_raise(int i);
void de_wm_maximize_toggle(int i);
void de_wm_close(int i);
int  de_wm_handle_titlebar_click(window_t* w, int mx, int my);
int  de_wm_handle_resize(window_t* w, int mx, int my);
void de_wm_snap_to_edge(window_t* w);
void de_wm_tick(float dt);

/* --------------------------------------------------------------------------- */
/* Panel                                                                       */
/* --------------------------------------------------------------------------- */
#define DE_LAUNCHER_W  40
#define DE_TASK_BTN_W  130
#define DE_TRAY_W      140

void de_panel_draw(void);
int  de_panel_hit_launcher(int mx, int my);
int  de_panel_hit_task(int mx, int my, int* out_index);
int  de_panel_hit_any(int mx, int my);

/* --------------------------------------------------------------------------- */
/* Launcher (kept for compatibility; forwards to Spotlight)                    */
/* --------------------------------------------------------------------------- */
void de_launcher_toggle(void);
void de_launcher_close(void);
int  de_launcher_is_open(void);
void de_launcher_draw(void);
int  de_launcher_click(int mx, int my);
int  de_launcher_hit(int mx, int my);
void de_launcher_rect(int* x, int* y, int* w, int* h);

/* --------------------------------------------------------------------------- */
/* Settings                                                                    */
/* --------------------------------------------------------------------------- */
void de_settings_draw(window_t* w);
int  de_settings_click(window_t* w, int mx, int my);

/* --------------------------------------------------------------------------- */
/* Notifications                                                               */
/* --------------------------------------------------------------------------- */
void de_notify_init(void);
void de_notify(const char* title, const char* body);
void de_notify_draw(void);
void de_notify_tick(void);
int  de_notify_has_any(void);
void de_notify_rect(int* x, int* y, int* w, int* h);

/* --------------------------------------------------------------------------- */
/* RTC (still used by the menu bar clock)                                      */
/* --------------------------------------------------------------------------- */
void de_rtc_init(void);
void de_now_hms(int* h, int* m, int* s);
void de_format_clock(char* buf, int buflen);

/* --------------------------------------------------------------------------- */
/* Console log                                                                 */
/* --------------------------------------------------------------------------- */
#define DE_CONSOLE_LINES     256
#define DE_CONSOLE_LINE_LEN  96
void        de_console_log(const char* line);
const char* de_console_line(int i);

/* --------------------------------------------------------------------------- */
/* Entry point                                                                 */
/* --------------------------------------------------------------------------- */
int desktop_run(int mode);

extern int      mouse_x;
extern int      mouse_y;
extern uint8_t  mouse_left_click;
extern uint8_t  mouse_right_click;

#endif