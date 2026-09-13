#ifndef QUARK_DE_APPS_H
#define QUARK_DE_APPS_H

#include <quark/de.h>
#include <quark/kernel.h>

/* Forward decls */
struct window_s;
typedef struct window_s window_t;

typedef struct de_key_event_s {
    int  scancode;       /* raw */
    char ascii;          /* 0 if not printable */
    uint8_t shift;
    uint8_t ctrl;
    uint8_t alt;
    uint8_t extended;    /* arrow keys etc. set this */
    uint16_t keycode;    /* DE_KEY_* for special keys */
} de_key_event_t;

/* Special keycodes (put in de_key_event_t.keycode when extended==1) */
#define DE_KEY_NONE    0
#define DE_KEY_UP      1
#define DE_KEY_DOWN    2
#define DE_KEY_LEFT    3
#define DE_KEY_RIGHT   4
#define DE_KEY_HOME    5
#define DE_KEY_END     6
#define DE_KEY_PGUP    7
#define DE_KEY_PGDN    8
#define DE_KEY_DEL     9
#define DE_KEY_ENTER   10
#define DE_KEY_ESC     11
#define DE_KEY_TAB     12

/* Every app implements this. All callbacks except on_draw are optional. */
typedef struct app_s {
    const char* id;         /* unique, e.g. "editor" */
    const char* name;       /* display, e.g. "Text Editor" */
    const char* icon_char;  /* single char for launcher / panel icon */
    int default_w, default_h;

    void* (*on_open)(void);                          /* allocate state */
    void  (*on_close)(void* st);                     /* free state */
    void  (*on_draw)(void* st, window_t* w);
    void  (*on_click)(void* st, window_t* w, int mx, int my);
    void  (*on_key)(void* st, window_t* w, de_key_event_t* k);
    void  (*on_scroll)(void* st, window_t* w, int dy);
} app_t;

/* Registry */
const app_t* app_registry_find(const char* id);
const app_t* app_registry_at(int index);   /* NULL when out of range */
int          app_registry_count(void);

/* Helper to open an app by id; returns window index or -1. */
int de_app_open(const char* id);

/* Scroll helper API (implemented in de_scroll.c). */
typedef struct {
    int scroll_y;
    int content_h;
    int viewport_h;
} de_scroll_t;

void de_scroll_draw(window_t* w, de_scroll_t* s, int viewport_x, int viewport_y,
                    int viewport_w, int viewport_h);
int  de_scroll_wheel(de_scroll_t* s, int dy);
int  de_scroll_hit_scrollbar(window_t* w, de_scroll_t* s,
                             int vx, int vy, int vw, int vh,
                             int mx, int my);
void de_scroll_drag_scrollbar(de_scroll_t* s,
                              int vy, int vh,
                              int mx, int my);
void de_scroll_clamp(de_scroll_t* s);

/* Input queue (implemented in de_input.c). */
void de_input_init(void);
void de_input_push_scancode(uint8_t sc);
int  de_input_pop(de_key_event_t* out);
int  de_input_pending(void);
/* Called by keyboard.c IRQ handler if you add the hook there. */
void de_input_irq_sink(uint8_t scancode);

/* Focus / alt-tab (de_focus.c). */
void de_focus_cycle_next(void);
void de_focus_cycle_prev(void);
int  de_focus_get(void);          /* topmost app window index, or -1 */
void de_focus_set(int window_index);

#endif /* QUARK_DE_APPS_H */
