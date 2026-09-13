/* =============================================================================
 * Quark-OS drivers/timer.c
 * Intel 8254 Programmable Interval Timer driver
 * Generates IRQ0 at a configurable frequency (default 100 Hz = 10 ms tick)
 * ============================================================================= */

#include <quark/kernel.h>

#define PIT_CHANNEL0    0x40    /* Channel 0 data port     */
#define PIT_CMD         0x43    /* Mode/command register   */
#define PIT_BASE_FREQ   1193180 /* PIT input frequency Hz  */

static volatile uint32_t tick_count = 0;

/* IRQ0 handler: increment tick and trigger scheduler */
static void timer_irq_handler(registers_t *regs) {
    (void)regs;
    tick_count++;
    /* Every tick: give scheduler a chance to switch processes */
    schedule();
}

void timer_init(uint32_t hz) {
    uint32_t divisor = PIT_BASE_FREQ / hz;

    /* Command: channel 0, lobyte/hibyte, square wave mode, binary */
    outb(PIT_CMD, 0x36);

    /* Send divisor low byte then high byte */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    /* Register IRQ0 handler and unmask it */
    irq_register(0, timer_irq_handler);
    pic_unmask(0);
}

uint32_t timer_get_ticks(void) {
    return tick_count;
}

/* Busy-wait sleep using tick counter (100 Hz = 10 ms per tick) */
void timer_sleep(uint32_t ms) {
    uint32_t target = tick_count + (ms / 10) + 1;
    while (tick_count < target) {
        __asm__ volatile ("nop");
    }
}