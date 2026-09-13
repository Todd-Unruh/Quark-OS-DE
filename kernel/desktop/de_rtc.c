/* =============================================================================
 * Quark-OS kernel/de_rtc.c — CMOS real-time clock reader.
 * Reads the RTC once at boot; the desktop synthesizes wall-clock time from
 * rtc_boot_seconds + ticks elapsed since boot.
 * ============================================================================= */

#include <quark/de.h>

extern uint32_t timer_get_ticks(void);

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static int bcd_to_bin(int b) { return (b & 0x0F) + ((b >> 4) * 10); }

static uint8_t rtc_status_b(void) { return cmos_read(0x0B); }

static int is_updating(void) { return cmos_read(0x0A) & 0x80; }

static uint32_t boot_seconds = 0;
static int      inited = 0;

void de_rtc_init(void) {
    while (is_updating()) { /* spin */ }

    int sec  = cmos_read(0x00);
    int min  = cmos_read(0x02);
    int hour = cmos_read(0x04);

    uint8_t status = rtc_status_b();
    int is_bcd = !(status & 0x04);
    if (is_bcd) {
        sec  = bcd_to_bin(sec);
        min  = bcd_to_bin(min);
        hour = bcd_to_bin(hour & 0x7F) | (hour & 0x80);
    }
    if (!(status & 0x02)) {
        /* 12-hour mode */
        int pm = hour & 0x80;
        hour &= 0x7F;
        if (pm && hour != 12) hour += 12;
        if (!pm && hour == 12) hour = 0;
    }
    boot_seconds = (uint32_t)hour * 3600 + min * 60 + sec;
    inited = 1;

    kprintf("  [RTC] boot time: %02d:%02d:%02d\n", hour, min, sec);
}

void de_now_hms(int* h, int* m, int* s) {
    uint32_t elapsed = 0;
    if (inited) {
        elapsed = timer_get_ticks() / 100;   /* 100 Hz timer */
    }
    uint32_t total = (boot_seconds + elapsed) % 86400u;
    if (h) *h = (int)(total / 3600);
    if (m) *m = (int)((total / 60) % 60);
    if (s) *s = (int)(total % 60);
}

void de_format_clock(char* buf, int buflen) {
    int h, m, s;
    de_now_hms(&h, &m, &s);
    int i = 0;
    if (g_gui_config.clock_24h) {
        buf[i++] = '0' + h / 10;
        buf[i++] = '0' + h % 10;
    } else {
        int pm = (h >= 12);
        h = h % 12; if (h == 0) h = 12;
        buf[i++] = '0' + h / 10;
        buf[i++] = '0' + h % 10;
        (void)pm;
    }
    buf[i++] = ':';
    buf[i++] = '0' + m / 10;
    buf[i++] = '0' + m % 10;
    if (g_gui_config.clock_show_secs) {
        buf[i++] = ':';
        buf[i++] = '0' + s / 10;
        buf[i++] = '0' + s % 10;
    }
    if (i < buflen - 1) buf[i] = '\0';
    else buf[buflen - 1] = '\0';
}