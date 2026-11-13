/* =============================================================================
 * Quark-OS drivers/mouse.c — PS/2 mouse driver on IRQ 12.
 * ============================================================================= */

#include <quark/kernel.h>

#define MOUSE_STATUS   0x64
#define MOUSE_COMMAND  0x64
#define MOUSE_DATA     0x60

int     mouse_x = 0;
int     mouse_y = 0;
uint8_t mouse_left_click  = 0;
uint8_t mouse_right_click = 0;

static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[3];

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) if ((inb(MOUSE_STATUS) & 0x01) == 1) return;
    } else {
        while (timeout--) if ((inb(MOUSE_STATUS) & 0x02) == 0) return;
    }
}

static void mouse_write(uint8_t v) {
    mouse_wait(1);
    outb(MOUSE_COMMAND, 0xD4);
    mouse_wait(1);
    outb(MOUSE_DATA, v);
}

static uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(MOUSE_DATA);
}

static void mouse_callback(registers_t *regs) {
    (void)regs;

    if (!(inb(MOUSE_STATUS) & 0x20)) return;   /* not from AUX device */

    mouse_packet[mouse_cycle++] = inb(MOUSE_DATA);
    if (mouse_cycle < 3) return;
    mouse_cycle = 0;

    mouse_left_click  = (mouse_packet[0] & 0x01);
    mouse_right_click = (mouse_packet[0] & 0x02);

    int8_t dx = (int8_t)mouse_packet[1];
    int8_t dy = (int8_t)mouse_packet[2];

    mouse_x += dx;
    mouse_y -= dy;

    uint32_t w = (fb_width  > 0) ? fb_width  : 1024;
    uint32_t h = (fb_height > 0) ? fb_height : 768;

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= (int)w) mouse_x = (int)w - 1;
    if (mouse_y >= (int)h) mouse_y = (int)h - 1;
}

void mouse_init(void) {
    uint32_t w = (fb_width  > 0) ? fb_width  : 1024;
    uint32_t h = (fb_height > 0) ? fb_height : 768;
    mouse_x = (int)w / 2;
    mouse_y = (int)h / 2;

    mouse_wait(1); outb(MOUSE_COMMAND, 0xA8);       /* enable AUX port */

    mouse_wait(1); outb(MOUSE_COMMAND, 0x20);       /* read config byte */
    mouse_wait(0);
    uint8_t cfg = inb(MOUSE_DATA) | 0x02;           /* IRQ12 on */
    mouse_wait(1); outb(MOUSE_COMMAND, 0x60);       /* write config byte */
    mouse_wait(1); outb(MOUSE_DATA, cfg);

    mouse_write(0xF6); mouse_read();                /* set defaults */
    mouse_write(0xF4); mouse_read();                /* enable streaming */

    irq_register(12, mouse_callback);
}