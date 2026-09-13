/* =============================================================================
 * Quark-OS drivers/keyboard.c
 * PS/2 keyboard driver. Feeds scancodes to the DE input queue AND translates
 * to ASCII for the shell.
 * ============================================================================= */

#include <quark/kernel.h>

#define KEYBOARD_DATA_PORT 0x60
#define KEY_BUFFER_SIZE    256

/* Feed raw scancodes to the desktop environment's IRQ-safe input queue. */
extern void de_input_irq_sink(uint8_t scancode);

static char key_buffer[KEY_BUFFER_SIZE];
static int  buf_head = 0;
static int  buf_tail = 0;

static uint8_t shift_pressed     = 0;
static uint8_t ctrl_pressed      = 0;
static uint8_t caps_lock         = 0;
static uint8_t extended_scancode = 0;

/* Unshifted US QWERTY scan-code-to-ASCII table (Set 1) */
static const unsigned char kbd_us_normal[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0,
   ' ',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0, '-',   0,   0,   0, '+',   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0
};

/* Shifted US QWERTY scan-code-to-ASCII table (Set 1) */
static const unsigned char kbd_us_shifted[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0,
  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0, '*',   0,
   ' ',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0, '-',   0,   0,   0, '+',   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0
};

static void kbd_irq_handler(registers_t *regs) {
    (void)regs;
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    /* ---- FEED THE DE INPUT QUEUE FIRST -----------------------------------
     * The DE decodes scancodes on its own: modifiers, extended keys, break
     * codes. It ignores anything it doesn't understand. So we can safely
     * feed every byte we read here.
     * -------------------------------------------------------------------- */
    de_input_irq_sink(scancode);

    /* 1. Extended scancode prefix */
    if (scancode == 0xE0) {
        extended_scancode = 1;
        return;
    }

    /* 2. Extended key actions */
    if (extended_scancode) {
        extended_scancode = 0;

        if (scancode == 0x48) { terminal_scroll_up();   return; }
        if (scancode == 0x50) { terminal_scroll_down(); return; }

        if (ctrl_pressed && scancode == 0x4B) {
            int next_tail = (buf_tail + 1) % KEY_BUFFER_SIZE;
            if (next_tail != buf_head) {
                key_buffer[buf_tail] = KEY_WORKSPACE_LEFT;
                buf_tail = next_tail;
            }
            return;
        }
        if (ctrl_pressed && scancode == 0x4D) {
            int next_tail = (buf_tail + 1) % KEY_BUFFER_SIZE;
            if (next_tail != buf_head) {
                key_buffer[buf_tail] = KEY_WORKSPACE_RIGHT;
                buf_tail = next_tail;
            }
            return;
        }

        if (scancode == 0xC8 || scancode == 0xD0) return;
    }

    /* 3. Modifier make codes */
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return; }
    if (scancode == 0x1D)                      { ctrl_pressed  = 1; return; }
    if (scancode == 0x3A) { caps_lock = !caps_lock; return; }

    /* 4. Modifier break codes */
    if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = 0; return; }
    if (scancode == 0x9D)                      { ctrl_pressed  = 0; return; }

    /* 5. Ignore other break codes */
    if (scancode & 0x80) return;

    /* 6. Translate to ASCII for the shell */
    char ascii = shift_pressed ? kbd_us_shifted[scancode]
                               : kbd_us_normal[scancode];
    if (!ascii) return;

    if (ctrl_pressed && (ascii == 'c' || ascii == 'C')) {
        ascii = 3;
    } else if (ascii >= 'a' && ascii <= 'z') {
        if (caps_lock && !shift_pressed) ascii -= 32;
    } else if (ascii >= 'A' && ascii <= 'Z') {
        if (caps_lock && shift_pressed) ascii += 32;
    }

    int next_tail = (buf_tail + 1) % KEY_BUFFER_SIZE;
    if (next_tail != buf_head) {
        key_buffer[buf_tail] = ascii;
        buf_tail = next_tail;
    }
}

void keyboard_init(void) {
    buf_head = 0;
    buf_tail = 0;
    shift_pressed = 0;
    ctrl_pressed = 0;
    caps_lock = 0;
    extended_scancode = 0;
    irq_register(1, kbd_irq_handler);
}

char keyboard_getchar(void) {
    while (buf_head == buf_tail) {
        __asm__ __volatile__ ("hlt");
    }
    char c = key_buffer[buf_head];
    buf_head = (buf_head + 1) % KEY_BUFFER_SIZE;
    return c;
}