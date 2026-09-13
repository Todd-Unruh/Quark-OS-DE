/* =============================================================================
 * Quark-OS drivers/terminal.c
 * Hard-Aligned Video Driver with Active History Scrollback & Snap-to-Prompt
 * ============================================================================= */

#include <quark/kernel.h>
#include <stdarg.h>

#define SCROLLBACK_ROWS 100

static volatile uint16_t *const vga_hardware_buffer = (volatile uint16_t *)VGA_MEMORY;
static uint16_t history_buffer[SCROLLBACK_ROWS][VGA_WIDTH];

static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t current_color = 0x07;
static int current_view_offset = 0;

/* FIXED: Initializing with a literal string forces GCC to allocate a real multi-byte 
   data segment in memory, meaning it compiles perfectly as a pointer (char*) address. */
static char global_print_buffer[32] = {0};

static void flush_view_to_hardware(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        int history_y = current_view_offset + y;
        volatile uint16_t *hardware_row = vga_hardware_buffer + (y * VGA_WIDTH);
        uint16_t *history_row = history_buffer[history_y];
        
        for (int x = 0; x < VGA_WIDTH; x++) {
            hardware_row[x] = history_row[x];
        }
    }
    
    int active_hardware_y = cursor_y - current_view_offset;
    if (active_hardware_y >= 0 && active_hardware_y < VGA_HEIGHT) {
        uint16_t pos = active_hardware_y * VGA_WIDTH + cursor_x;
        outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
        outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    }
}

void terminal_refresh(void) {
    flush_view_to_hardware();
}

void terminal_initialize(void) {
    cursor_x = 0;
    cursor_y = 0;
    current_view_offset = 0;
    current_color = 0x07;
    terminal_clear();
}

void terminal_init(void) {
    terminal_initialize();
}

void terminal_clear(void) {
    uint16_t blank = (uint16_t)' ' | ((uint16_t)0x07 << 8);
    for (int y = 0; y < SCROLLBACK_ROWS; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            history_buffer[y][x] = blank;
        }
    }
    cursor_x = 0;
    cursor_y = 0;
    current_view_offset = 0;
    flush_view_to_hardware();
}

void terminal_setcolor(uint8_t fg, uint8_t bg) {
    current_color = fg | (bg << 4);
}

void terminal_putchar(char c) {
    int max_possible_offset = (cursor_y >= VGA_HEIGHT) ? (cursor_y - VGA_HEIGHT + 1) : 0;
    current_view_offset = max_possible_offset;

    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } 
    else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            uint16_t blank = (uint16_t)' ' | ((uint16_t)current_color << 8);
            history_buffer[cursor_y][cursor_x] = blank;
        }
    } 
    else {
        uint16_t raw_entry = (uint16_t)c | ((uint16_t)current_color << 8);
        history_buffer[cursor_y][cursor_x] = raw_entry;
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }

    if (cursor_y >= SCROLLBACK_ROWS) {
        for (int y = 1; y < SCROLLBACK_ROWS; y++) {
            kmemcpy(history_buffer[y - 1], history_buffer[y], VGA_WIDTH * 2);
        }
        uint16_t blank = (uint16_t)' ' | ((uint16_t)current_color << 8);
        for (int x = 0; x < VGA_WIDTH; x++) history_buffer[SCROLLBACK_ROWS - 1][x] = blank;
        cursor_y = SCROLLBACK_ROWS - 1;
    }
    
    if (cursor_y >= VGA_HEIGHT) {
        current_view_offset = cursor_y - VGA_HEIGHT + 1;
    } else {
        current_view_offset = 0;
    }
    
    flush_view_to_hardware();
}

void terminal_scroll_up(void) {
    if (current_view_offset > 0) {
        current_view_offset--;
        flush_view_to_hardware();
    }
}

void terminal_scroll_down(void) {
    int max_possible_offset = (cursor_y >= VGA_HEIGHT) ? (cursor_y - VGA_HEIGHT + 1) : 0;
    if (current_view_offset < max_possible_offset) {
        current_view_offset++;
        flush_view_to_hardware();
    }
}

void terminal_write(const char *data) {
    while (*data) terminal_putchar(*data++);
}

void terminal_writeln(const char *data) {
    terminal_write(data);
    terminal_putchar('\n');
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kmemset(global_print_buffer, 0, 32);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'c': {
                    char c = (char)va_arg(args, int);
                    terminal_putchar(c);
                    break;
                }
                case 's': {
                    const char *s = va_arg(args, const char *);
                    if (!s) s = "(null)";
                    terminal_write(s);
                    break;
                }
                case 'd': {
                    int n = va_arg(args, int);
                    /* FIXED: Passes the string buffer parameter smoothly as an unadorned array pointer */
                    kitoa(n, global_print_buffer, 10);
                    terminal_write(global_print_buffer);
                    break;
                }
                case 'u': {
                    unsigned int n = va_arg(args, unsigned int);
                    kuitoa(n, global_print_buffer, 10);
                    terminal_write(global_print_buffer);
                    break;
                }
                case 'x':
                case 'p': {
                    unsigned int n = va_arg(args, unsigned int);
                    kuitoa(n, global_print_buffer, 16);
                    terminal_write("0x");
                    terminal_write(global_print_buffer);
                    break;
                }
                case '%': {
                    terminal_putchar('%');
                    break;
                }
                default: {
                    terminal_putchar('%');
                    terminal_putchar(*fmt);
                    break;
                }
            }
        } else {
            terminal_putchar(*fmt);
        }
        fmt++;
    }
    va_end(args);
}

