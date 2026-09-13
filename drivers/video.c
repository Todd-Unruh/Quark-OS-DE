/* =============================================================================
 * Quark-OS drivers/video.c
 * Linear framebuffer discovery and pitch-aware pixel writes.
 * ============================================================================= */

#include <quark/kernel.h>

uint32_t  fb_width  = 0;
uint32_t  fb_height = 0;
uint32_t* framebuffer_pixels = NULL;

static uint32_t fb_pitch = 0;
static uint8_t  fb_bpp   = 0;

uint32_t fb_pitch_get(void) {
    return fb_pitch ? fb_pitch : (fb_width * 4);
}

void framebuffer_init(multiboot_info_t *mb_info) {
    if (mb_info && (mb_info->flags & (1u << 11))) {
        fb_width  = mb_info->framebuffer_width;
        fb_height = mb_info->framebuffer_height;
        fb_pitch  = mb_info->framebuffer_pitch;
        fb_bpp    = mb_info->framebuffer_bpp;
        framebuffer_pixels = (uint32_t*)(uintptr_t)mb_info->framebuffer_addr;

        kprintf("  [VIDEO] %ux%u x %u bpp, pitch=%u, addr=0x%x\n",
                fb_width, fb_height, fb_bpp, fb_pitch,
                (unsigned)(uintptr_t)framebuffer_pixels);
    } else {
        fb_width = fb_height = fb_pitch = fb_bpp = 0;
        framebuffer_pixels = NULL;
        kprintf("  [VIDEO] No linear framebuffer (multiboot flags bit 11 unset)\n");
    }
}

void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!framebuffer_pixels) return;
    if (x >= fb_width || y >= fb_height) return;
    framebuffer_pixels[y * (fb_pitch_get() / 4) + x] = color;
}