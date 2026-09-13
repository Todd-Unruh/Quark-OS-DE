#include <quark/de_apps.h>

extern const app_t app_editor;
extern const app_t app_calculator;
extern const app_t app_files;
extern const app_t app_about;

static const app_t* g_apps[] = {
    &app_editor,
    &app_calculator,
    &app_files,
    &app_about,
};

#define APP_COUNT ((int)(sizeof(g_apps)/sizeof(g_apps[0])))

const app_t* app_registry_at(int i) {
    if (i < 0 || i >= APP_COUNT) return NULL;
    return g_apps[i];
}

int app_registry_count(void) { return APP_COUNT; }

const app_t* app_registry_find(const char* id) {
    for (int i = 0; i < APP_COUNT; i++) {
        if (kstrcmp(g_apps[i]->id, id) == 0) return g_apps[i];
    }
    return NULL;
}

int de_app_open(const char* id) {
    const app_t* a = app_registry_find(id);
    if (!a) return -1;
    static int spawn_offset = 0;
    int x = 100 + (spawn_offset % 5) * 30;
    int y = 70 + (spawn_offset % 5) * 30;
    spawn_offset++;
    return de_wm_create_app(a, x, y, a->default_w, a->default_h);
}