#include <quark/de.h>
#include <quark/de_apps.h>

#define MENU_W 240
#define MENU_H 320
#define ITEM_H 30
#define MENU_PAD 6

static uint8_t open_flag = 0;
static int     scroll = 0;

static int menu_x(void) { return 8; }
static int menu_y(void) { return de_panel_y() - MENU_H - 4; }

static int visible_items(void) {
    return (MENU_H - 2 * MENU_PAD - 2 * ITEM_H) / ITEM_H;
}

static int total_items(void) { return app_registry_count() + 2; }

void de_launcher_rect(int* x, int* y, int* w, int* h) {
    *x = menu_x(); *y = menu_y(); *w = MENU_W; *h = MENU_H;
}

void de_launcher_toggle(void) {
    open_flag = !open_flag;
    scroll = 0;
    de_damage_add(menu_x() - 4, menu_y() - 4, MENU_W + 8, MENU_H + 8);
}

void de_launcher_close(void) {
    if (!open_flag) return;
    open_flag = 0;
    de_damage_add(menu_x() - 4, menu_y() - 4, MENU_W + 8, MENU_H + 8);
}

int de_launcher_is_open(void) { return open_flag; }

int de_launcher_hit(int mx, int my) {
    if (!open_flag) return -1;
    int mx0 = menu_x(), my0 = menu_y();
    if (mx < mx0 || mx >= mx0 + MENU_W) return -1;
    if (my < my0 || my >= my0 + MENU_H) return -1;
    int vi = visible_items();

    if (my < my0 + MENU_PAD + ITEM_H) {
        if (scroll > 0) return -2;
        return -1;
    }
    if (my >= my0 + MENU_H - MENU_PAD - ITEM_H) {
        if (scroll + vi < total_items()) return -3;
        return -1;
    }
    int rel = my - my0 - MENU_PAD - ITEM_H;
    int idx = rel / ITEM_H + scroll;
    if (idx < 0 || idx >= total_items()) return -1;
    return idx;
}

static const char* item_label(int idx) {
    int apps = app_registry_count();
    if (idx < apps) {
        const app_t* a = app_registry_at(idx);
        return a ? a->name : "";
    }
    switch (idx - apps) {
        case 0: return "System Settings";
        case 1: return "Shut Down";
    }
    return "";
}

static char item_icon(int idx) {
    int apps = app_registry_count();
    if (idx < apps) {
        const app_t* a = app_registry_at(idx);
        if (a && a->icon_char) return a->icon_char[0];
    }
    switch (idx - apps) {
        case 0: return 'S';
        case 1: return 'X';
    }
    return '?';
}

static void activate_item(int idx) {
    int apps = app_registry_count();
    if (idx < apps) {
        const app_t* a = app_registry_at(idx);
        if (a) de_app_open(a->id);
        return;
    }
    switch (idx - apps) {
        case 0: de_wm_create(DE_ROLE_SETTINGS, "System Settings",
                             200, 80, 360, 400); break;
        case 1: de_notify("Power", "Shutdown not implemented"); break;
    }
}

int de_launcher_click(int mx, int my) {
    if (!open_flag) return 0;
    int hit = de_launcher_hit(mx, my);
    if (hit == -1) {
        open_flag = 0;
        de_damage_add(menu_x() - 4, menu_y() - 4, MENU_W + 8, MENU_H + 8);
        return 1;
    }
    if (hit == -2) { if (scroll > 0) scroll--; de_damage_add(menu_x(), menu_y(), MENU_W, MENU_H); return 1; }
    if (hit == -3) {
        if (scroll + visible_items() < total_items()) scroll++;
        de_damage_add(menu_x(), menu_y(), MENU_W, MENU_H);
        return 1;
    }
    open_flag = 0;
    de_damage_add(menu_x() - 4, menu_y() - 4, MENU_W + 8, MENU_H + 8);
    activate_item(hit);
    return 1;
}

void de_launcher_draw(void) {
    if (!open_flag) return;
    const de_theme_t* t = g_theme;
    int mx = menu_x(), my = menu_y();
    int vi = visible_items();

    de_fill(mx + 3, my + 3, MENU_W, MENU_H, t->widget_shadow);
    de_fill_rounded(mx, my, MENU_W, MENU_H, 6, t->widget_bg);
    de_border(mx, my, MENU_W, MENU_H, t->widget_edge);

    int y_cursor = my + MENU_PAD;
    if (scroll > 0) {
        de_fill(mx + 2, y_cursor, MENU_W - 4, ITEM_H - 2, t->panel_btn);
        de_text_centered(mx + MENU_W / 2, y_cursor + (ITEM_H - 10) / 2,
                         "^", t->widget_txt);
    }
    y_cursor += ITEM_H;

    int total = total_items();
    for (int i = 0; i < vi && scroll + i < total; i++) {
        int idx = scroll + i;
        int iy = y_cursor + i * ITEM_H;
        int hovered = (mouse_x >= mx + 2 && mouse_x < mx + MENU_W - 2 &&
                       mouse_y >= iy && mouse_y < iy + ITEM_H - 2);
        if (hovered) {
            de_fill(mx + 2, iy, MENU_W - 4, ITEM_H - 2, t->panel_sel);
        }
        uint32_t fg = hovered ? t->panel_sel_txt : t->widget_txt;

        de_fill(mx + 10, iy + (ITEM_H - 14) / 2, 14, 14,
                hovered ? 0x00FFFFFF : t->accent);
        de_border(mx + 10, iy + (ITEM_H - 14) / 2, 14, 14, t->widget_edge);

        char ic[2] = { item_icon(idx), 0 };
        de_text_centered(mx + 17, iy + (ITEM_H - 8) / 2, ic,
                         hovered ? t->accent : 0x00FFFFFF);
        de_text(mx + 34, iy + (ITEM_H - 10) / 2, item_label(idx), fg);
    }

    if (scroll + vi < total) {
        int dy = my + MENU_H - MENU_PAD - ITEM_H;
        de_fill(mx + 2, dy, MENU_W - 4, ITEM_H - 2, t->panel_btn);
        de_text_centered(mx + MENU_W / 2, dy + (ITEM_H - 10) / 2,
                         "v", t->widget_txt);
    }
}