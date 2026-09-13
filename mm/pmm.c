/* =============================================================================
 * Quark-OS mm/pmm.c
 * Physical Memory Manager: bitmap-based page frame allocator.
 *
 * Layout decisions:
 *   - Bitmap lives immediately after __kernel_end (page-aligned).
 *   - Everything below (and including) the kernel image is reserved.
 *   - Kernel heap region (KERNEL_HEAP_START..KERNEL_HEAP_END) is reserved,
 *     because heap.c uses absolute physical addresses, not pmm_alloc_page().
 *   - The framebuffer, if present, is reserved.
 *   - Page 0 is always reserved.
 * ============================================================================= */

#include <quark/kernel.h>

/* Linker exports */
extern uint8_t __kernel_end[];

/* Framebuffer state from drivers/video.c */
extern uint32_t* framebuffer_pixels;
extern uint32_t  fb_width;
extern uint32_t  fb_height;
extern uint32_t  fb_pitch_get(void);

static uint8_t  *pmm_bitmap;
static uint32_t  pmm_total_pages;
static uint32_t  pmm_free_count;
static uint32_t  pmm_last_hint = 0;     /* start search from here next time */

/* -------------------------------------------------------------------------- */
static inline void pmm_set_bit(uint32_t page) {
    pmm_bitmap[page >> 3] |= (uint8_t)(1u << (page & 7));
}

static inline void pmm_clear_bit(uint32_t page) {
    pmm_bitmap[page >> 3] &= (uint8_t)~(1u << (page & 7));
}

static inline int pmm_test_bit(uint32_t page) {
    return (pmm_bitmap[page >> 3] >> (page & 7)) & 1u;
}

/* Reserve every page overlapping [addr, addr+size). */
static void pmm_reserve_range(uint32_t addr, uint32_t size) {
    if (size == 0) return;

    uint32_t start = addr / PAGE_SIZE;
    uint32_t end   = (addr + size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (end > pmm_total_pages) end = pmm_total_pages;

    for (uint32_t p = start; p < end; p++) {
        if (!pmm_test_bit(p)) {
            pmm_set_bit(p);
            if (pmm_free_count) pmm_free_count--;
        }
    }
}

/* Free every page in [addr, addr+size). */
static void pmm_free_range(uint32_t addr, uint32_t size) {
    if (size == 0) return;

    uint32_t start = addr / PAGE_SIZE;
    uint32_t end   = (addr + size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (end > pmm_total_pages) end = pmm_total_pages;

    for (uint32_t p = start; p < end; p++) {
        if (pmm_test_bit(p)) {
            pmm_clear_bit(p);
            pmm_free_count++;
        }
    }
}

/* -------------------------------------------------------------------------- */
void pmm_init(uint32_t mem_kb) {
    /* 1. Compute total pages and bitmap size. */
    uint32_t total_bytes = mem_kb * 1024u;

    /* Hard cap: don't try to manage more than 4 GB of physical memory, and
     * ignore the classic BIOS hole below 1 MB for bitmap sizing purposes. */
    if (total_bytes < (4u * 1024u * 1024u)) total_bytes = 4u * 1024u * 1024u;
    if (total_bytes > (4u * 1024u * 1024u * 1024u)) total_bytes = 0xFFFFFFFFu;

    pmm_total_pages = total_bytes / PAGE_SIZE;
    pmm_free_count  = 0;

    /* 2. Place the bitmap immediately after the kernel image, page-aligned. */
    uint32_t bitmap_addr = PAGE_ALIGN((uint32_t)(uintptr_t)__kernel_end);
    uint32_t bitmap_bytes = (pmm_total_pages + 7u) / 8u;
    uint32_t bitmap_pages = (bitmap_bytes + PAGE_SIZE - 1u) / PAGE_SIZE;

    pmm_bitmap = (uint8_t *)(uintptr_t)bitmap_addr;

    /* 3. Mark everything used initially. */
    kmemset(pmm_bitmap, 0xFF, bitmap_pages * PAGE_SIZE);

    /* 4. Free all physical memory above the kernel + bitmap. */
    uint32_t first_free_addr = bitmap_addr + bitmap_pages * PAGE_SIZE;
    uint32_t first_free_page = first_free_addr / PAGE_SIZE;

    /* Ceiling: never manage memory above the amount the BIOS reported. */
    uint32_t top_page = pmm_total_pages;
    if (top_page > (total_bytes / PAGE_SIZE)) top_page = total_bytes / PAGE_SIZE;

    for (uint32_t p = first_free_page; p < top_page; p++) {
        pmm_clear_bit(p);
        pmm_free_count++;
    }

    /* 5. Reserve critical regions. */
    /*    - Low memory + kernel image + bitmap (everything below first_free). */
    pmm_reserve_range(0, first_free_addr);

    /*    - Kernel heap (heap.c uses absolute physical addresses here). */
    pmm_reserve_range(KERNEL_HEAP_START, KERNEL_HEAP_END - KERNEL_HEAP_START);

    /*    - Framebuffer, if active. */
    if (framebuffer_pixels) {
        uint32_t fb_size = fb_pitch_get() * fb_height;
        pmm_reserve_range((uint32_t)(uintptr_t)framebuffer_pixels, fb_size);
    }

    pmm_last_hint = first_free_page;

    kprintf("  [PMM] %u pages total, %u free, bitmap @ 0x%x (%u bytes)\n",
            pmm_total_pages, pmm_free_count,
            bitmap_addr, bitmap_bytes);
    kprintf("  [PMM] kernel_end=0x%x, first_free=0x%x, heap reserved 0x%x-0x%x\n",
            (unsigned)(uintptr_t)__kernel_end,
            first_free_addr,
            KERNEL_HEAP_START, KERNEL_HEAP_END);
}

/* -------------------------------------------------------------------------- */
uint32_t pmm_alloc_page(void) {
    if (!pmm_bitmap) return 0;

    /* Two-pass search: from hint to end, then from 0 to hint. */
    for (uint32_t pass = 0; pass < 2; pass++) {
        uint32_t start = (pass == 0) ? pmm_last_hint : 0;
        uint32_t end   = (pass == 0) ? pmm_total_pages : pmm_last_hint;

        for (uint32_t p = start; p < end; p++) {
            if (!pmm_test_bit(p)) {
                pmm_set_bit(p);
                pmm_free_count--;
                pmm_last_hint = p + 1;
                if (pmm_last_hint >= pmm_total_pages) pmm_last_hint = 0;
                return p * PAGE_SIZE;
            }
        }
    }
    return 0;   /* truly out of memory */
}

void pmm_free_page(uint32_t addr) {
    if (!pmm_bitmap) return;
    uint32_t page = addr / PAGE_SIZE;
    if (page < pmm_total_pages && pmm_test_bit(page)) {
        pmm_clear_bit(page);
        pmm_free_count++;
        if (page < pmm_last_hint) pmm_last_hint = page;
    }
}

uint32_t pmm_free_pages(void) {
    return pmm_free_count;
}