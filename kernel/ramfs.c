/* =============================================================================
 * Quark-OS kernel/ramfs.c
 * Virtual RamFS file manager.
 * Root directory is "/" everywhere: ramfs_init, shell, editor, Files.
 * ============================================================================= */

#include <quark/kernel.h>

static ram_file_t file_table[MAX_FILES];

static const char *ramfs_normalize_dir(const char *dir) {
    if (!dir) return "/";
    if (kstrcmp(dir, "root") == 0) return "/";
    return dir;
}

void ramfs_init(void) {
    kmemset(file_table, 0, sizeof(file_table));

    /* System folders and default files, all at "/" */
    ramfs_create("docs",        "/", 1);
    ramfs_create("apps",        "/", 1);
    ramfs_create("welcome.txt", "/", 0);
    ramfs_write("welcome.txt", "/",
        "Welcome to Quark-OS 3! Run: forge hello.fg\n"
        "Type 'install' for setup help.\n", 74);

    static const char widgets_script[] =
        "window \"Forge Widgets Demo\" 460 410\n"
        "set count = 0\n"
        "set enabled = 1\n"
        "set changes = 0\n"
        "\n"
        "func draw():\n"
        "    fill 0 0 420 360 \"#171b22\"\n"
        "    panel 12 12 396 336 \"#252a32\"\n"
        "    label 28 24 \"Forge widget gallery\" \"#ffffff\"\n"
        "    text 28 48 \"Shapes, text, buttons, and checkboxes\" \"#d8e0ea\"\n"
        "    rect 28 82 150 34 \"#77aaff\"\n"
        "    text 38 94 \"Outlined rectangle\" \"#ffffff\"\n"
        "    panel 198 82 184 34 \"#343c49\"\n"
        "    label 210 94 \"Rounded panel\" \"#ffffff\"\n"
        "    line 28 140 380 140 \"#77aaff\"\n"
        "    circle 48 174 15 \"#ffbf69\"\n"
        "    text 76 166 \"Filled circle and line\" \"#d8e0ea\"\n"
        "    button 28 208 148 34 \"Click me\" clicked\n"
        "    text 194 218 \"Clicks: @count\" \"#ffffff\"\n"
        "    checkbox 28 264 \"Enable feature\" enabled toggled\n"
        "    label 28 304 \"Checkbox changes: @changes\" \"#d8e0ea\"\n"
        "\n"
        "func clicked():\n"
        "    set count = count + 1\n"
        "\n"
        "func toggled():\n"
        "    set changes = changes + 1\n";

    if (ramfs_create("widgets.fg", "/", 0) == 0)
        ramfs_write("widgets.fg", "/", widgets_script,
                    sizeof(widgets_script) - 1);

    ramfs_create("hello.fg", "/", 0);
    ramfs_write("hello.fg", "/",
        "print \"Hello from Forge on Quark-OS-3!\"\n"
        "set x = 10\n"
        "set y = 32\n"
        "print \"Forge is ready.\"\n"
        "func greet():\n"
        "    print \"Welcome to Quark-OS!\"\n"
        "call greet()\n", 156);
}

int ramfs_create(const char *name, const char *current_dir, uint8_t is_directory) {
    const char *dir = ramfs_normalize_dir(current_dir);
    if (ramfs_get(name, dir) != NULL) return -1;

    for (int i = 0; i < MAX_FILES; i++) {
        if (!file_table[i].used) {
            kmemset(&file_table[i], 0, sizeof(ram_file_t));
            kstrcpy(file_table[i].name, name);
            kstrcpy(file_table[i].parent_dir, dir);
            file_table[i].used = 1;
            file_table[i].is_dir = is_directory;
            file_table[i].size = 0;
            return 0;
        }
    }
    return -2;
}

int ramfs_write(const char *name, const char *current_dir, const char *content, uint32_t size) {
    ram_file_t *file = ramfs_get(name, ramfs_normalize_dir(current_dir));
    if (!file || file->is_dir) return -1;

    if (size > MAX_FILE_SIZE) size = MAX_FILE_SIZE;
    kmemcpy(file->data, content, size);
    file->size = size;
    return 0;
}

ram_file_t* ramfs_get(const char *name, const char *current_dir) {
    const char *dir = ramfs_normalize_dir(current_dir);
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used &&
            kstrcmp(file_table[i].name, name) == 0 &&
            kstrcmp(file_table[i].parent_dir, dir) == 0) {
            return &file_table[i];
        }
    }
    return NULL;
}

ram_file_t* ramfs_get_child(const char *current_dir, int index) {
    const char *dir = ramfs_normalize_dir(current_dir);
    int found = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used &&
            kstrcmp(file_table[i].parent_dir, dir) == 0) {
            if (found == index) return &file_table[i];
            found++;
        }
    }
    return NULL;
}

void ramfs_list(const char *current_dir) {
    const char *dir = ramfs_normalize_dir(current_dir);
    kprintf("\nNAME                            TYPE          SIZE (BYTES)\n");
    kprintf("----------------------------------------------------------\n");
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used && kstrcmp(file_table[i].parent_dir, dir) == 0) {
            kprintf("%s", file_table[i].name);
            int padding = 32 - kstrlen(file_table[i].name);
            while (padding-- > 0) terminal_putchar(' ');

            if (file_table[i].is_dir) {
                kprintf("<DIR>         0\n");
            } else {
                kprintf("FILE          %u\n", file_table[i].size);
            }
        }
    }
}

int ramfs_delete(const char *name, const char *current_dir, uint8_t expect_dir) {
    ram_file_t *file = ramfs_get(name, ramfs_normalize_dir(current_dir));
    if (!file) return -1;

    if (file->is_dir != expect_dir) return -2;

    if (file->is_dir) {
        for (int i = 0; i < MAX_FILES; i++) {
            if (file_table[i].used && kstrcmp(file_table[i].parent_dir, name) == 0) {
                return -3;
            }
        }
    }

    kmemset(file, 0, sizeof(ram_file_t));
    return 0;
}

int ramfs_match(const char *pattern, const char *name) {
    if (!pattern || !name) return 0;
    if (kstrcmp(pattern, "*") == 0) return 1;
    if (pattern[0] == '*' && pattern[1] == '\0') return 1;

    const char *star = kstrchr(pattern, '*');
    if (!star) return kstrcmp(pattern, name) == 0;

    size_t prefix_len = (size_t)(star - pattern);
    if (kstrncmp(pattern, name, prefix_len) != 0) return 0;
    return kstrcmp(name + prefix_len, star + 1) == 0;
}

int ramfs_count_in_dir(const char *current_dir) {
    const char *dir = ramfs_normalize_dir(current_dir);
    int count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used && kstrcmp(file_table[i].parent_dir, dir) == 0) {
            count++;
        }
    }
    return count;
}

uint32_t ramfs_dir_size(const char *current_dir) {
    const char *dir = ramfs_normalize_dir(current_dir);
    uint32_t total = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used && kstrcmp(file_table[i].parent_dir, dir) == 0) {
            if (!file_table[i].is_dir) {
                total += file_table[i].size;
            }
        }
    }
    return total;
}

int ramfs_used_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used) count++;
    }
    return count;
}

uint32_t ramfs_total_bytes(void) {
    uint32_t total = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_table[i].used && !file_table[i].is_dir)
            total += file_table[i].size;
    }
    return total;
}

int ramfs_copy(const char *name, const char *dest_name, const char *current_dir) {
    const char *dir = ramfs_normalize_dir(current_dir);
    ram_file_t *src = ramfs_get(name, dir);
    if (!src) return -1;
    if (src->is_dir) return -2;
    if (ramfs_get(dest_name, dir)) return -3;

    if (ramfs_create(dest_name, dir, 0) != 0) return -4;
    return ramfs_write(dest_name, dir, (const char *)src->data, src->size);
}

int ramfs_rename(const char *old_name, const char *new_name, const char *current_dir) {
    const char *dir = ramfs_normalize_dir(current_dir);
    ram_file_t *file = ramfs_get(old_name, dir);
    if (!file) return -1;
    if (ramfs_get(new_name, dir)) return -2;
    kstrcpy(file->name, new_name);
    return 0;
}

void ramfs_find(const char *dir, const char *pattern) {
    const char *d = ramfs_normalize_dir(dir);
    for (int i = 0; i < MAX_FILES; i++) {
        if (!file_table[i].used) continue;
        if (kstrcmp(file_table[i].parent_dir, d) != 0) continue;
        if (ramfs_match(pattern, file_table[i].name)) {
            kprintf("%s/%s\n", d, file_table[i].name);
        }
        if (file_table[i].is_dir) {
            ramfs_find(file_table[i].name, pattern);
        }
    }
}

static void ramfs_tree_recurse(const char *dir, int depth) {
    const char *d = ramfs_normalize_dir(dir);
    for (int i = 0; i < MAX_FILES; i++) {
        if (!file_table[i].used) continue;
        if (kstrcmp(file_table[i].parent_dir, d) != 0) continue;

        for (int d = 0; d < depth; d++) kprintf("  ");
        kprintf("%s%s\n", file_table[i].name, file_table[i].is_dir ? "/" : "");
        if (file_table[i].is_dir) {
            ramfs_tree_recurse(file_table[i].name, depth + 1);
        }
    }
}

void ramfs_tree(const char *dir) {
    const char *d = ramfs_normalize_dir(dir);
    kprintf("%s/\n", d);
    ramfs_tree_recurse(d, 1);
}