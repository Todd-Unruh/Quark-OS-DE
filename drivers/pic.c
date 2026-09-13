/* =============================================================================
 * Quark-OS drivers/pic.c
 * Intel 8259A Programmable Interrupt Controller driver
 * Remaps IRQ0-15 from BIOS default (INT 8-15) to INT 32-47 to avoid
 * conflicts with CPU exception vectors.
 * ============================================================================= */

#include <quark/kernel.h>

#define PIC1_CMD    0x20    /* Master PIC command port  */
#define PIC1_DATA   0x21    /* Master PIC data port     */
#define PIC2_CMD    0xA0    /* Slave PIC command port   */
#define PIC2_DATA   0xA1    /* Slave PIC data port      */

#define PIC_EOI     0x20    /* End-Of-Interrupt command */

/* ICW1: initialization words */
#define ICW1_INIT   0x10
#define ICW1_ICW4   0x01

/* ICW4 flags */
#define ICW4_8086   0x01    /* 8086/88 mode (vs MCS-80/85) */

void pic_init(void) {
    /* Save current masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* Start initialization sequence (cascade mode) */
    outb(PIC1_CMD,  ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD,  ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2: set interrupt vector offsets */
    outb(PIC1_DATA, 32);    /* Master: IRQ0-7 -> INT 32-39 */
    io_wait();
    outb(PIC2_DATA, 40);    /* Slave:  IRQ8-15 -> INT 40-47 */
    io_wait();

    /* ICW3: configure cascade */
    outb(PIC1_DATA, 0x04);  /* Master: slave on IRQ2 (bit 2) */
    io_wait();
    outb(PIC2_DATA, 0x02);  /* Slave: cascade identity (IRQ2) */
    io_wait();

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Restore saved masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);    /* slave needs EOI too */
    }
    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) | (1 << irq);
    outb(port, value);
}

void pic_unmask(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}