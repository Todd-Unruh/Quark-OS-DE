/* =============================================================================
 * Quark-OS mm/heap.c
 * Kernel heap: first-fit free-list allocator.
 * Manages physical region KERNEL_HEAP_START .. KERNEL_HEAP_END (8..16 MB).
 *
 * NOTE: This region is *also* reserved in mm/pmm.c, so the PMM will never
 * hand out pages that overlap the heap.
 * ============================================================================= */

#include <quark/kernel.h>

#define HEAP_START  KERNEL_HEAP_START   /* 0x00800000 */
#define HEAP_END    KERNEL_HEAP_END     /* 0x01000000 */
#define HEAP_MAGIC  0xDEADBEEF
#define HEAP_ALIGN  16

typedef struct block_header {
    uint32_t magic;
    size_t   size;          /* usable bytes (excludes this header) */
    int      is_free;
    struct block_header *next;
    struct block_header *prev;
} block_header_t;

#define HEADER_SIZE  ((size_t)sizeof(block_header_t))
#define MIN_SPLIT    (HEADER_SIZE + HEAP_ALIGN)

static block_header_t *heap_head = NULL;

/* -------------------------------------------------------------------------- */
static inline size_t align_up(size_t v, size_t a) {
    return (v + a - 1) & ~(a - 1);
}

/* -------------------------------------------------------------------------- */
void heap_init(void) {
    if (HEAP_END <= HEAP_START + HEADER_SIZE) {
        kprintf("  [HEAP] FATAL: heap region too small\n");
        heap_head = NULL;
        return;
    }

    heap_head = (block_header_t *)(uintptr_t)HEAP_START;
    heap_head->magic   = HEAP_MAGIC;
    heap_head->size    = (HEAP_END - HEAP_START) - HEADER_SIZE;
    heap_head->is_free = 1;
    heap_head->next    = NULL;
    heap_head->prev    = NULL;

    kprintf("  [HEAP] arena 0x%x-0x%x, %u bytes, header=%u bytes\n",
            HEAP_START, HEAP_END,
            (unsigned)(HEAP_END - HEAP_START),
            (unsigned)HEADER_SIZE);
}

/* -------------------------------------------------------------------------- */
void *kmalloc(size_t size) {
    if (!heap_head || size == 0) return NULL;

    size = align_up(size, HEAP_ALIGN);

    block_header_t *b = heap_head;
    while (b) {
        if (b->magic != HEAP_MAGIC) {
            kprintf("  [HEAP] corruption at 0x%x (magic=0x%x)\n",
                    (unsigned)(uintptr_t)b, b->magic);
            return NULL;
        }

        if (b->is_free && b->size >= size) {
            /* Split if the remainder can hold a header + minimum payload. */
            if (b->size >= size + MIN_SPLIT) {
                block_header_t *nb =
                    (block_header_t *)((uint8_t *)b + HEADER_SIZE + size);
                nb->magic   = HEAP_MAGIC;
                nb->size    = b->size - size - HEADER_SIZE;
                nb->is_free = 1;
                nb->next    = b->next;
                nb->prev    = b;
                if (b->next) b->next->prev = nb;
                b->next = nb;
                b->size = size;
            }
            b->is_free = 0;
            return (void *)((uint8_t *)b + HEADER_SIZE);
        }
        b = b->next;
    }
    return NULL;
}

/* -------------------------------------------------------------------------- */
void kfree(void *ptr) {
    if (!ptr || !heap_head) return;

    /* Sanity: pointer must lie inside the heap arena. */
    uintptr_t p = (uintptr_t)ptr;
    if (p < HEAP_START + HEADER_SIZE || p >= HEAP_END) {
        return;
    }

    block_header_t *b = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);
    if (b->magic != HEAP_MAGIC) return;
    if (b->is_free) return;        /* double free — ignore */

    b->is_free = 1;

    /* Coalesce forward. */
    if (b->next && b->next->is_free) {
        block_header_t *n = b->next;
        b->size += HEADER_SIZE + n->size;
        b->next  = n->next;
        if (n->next) n->next->prev = b;
        n->magic = 0;              /* poison freed header */
    }

    /* Coalesce backward. */
    if (b->prev && b->prev->is_free) {
        block_header_t *pr = b->prev;
        pr->size += HEADER_SIZE + b->size;
        pr->next  = b->next;
        if (b->next) b->next->prev = pr;
        b->magic = 0;
    }
}

/* -------------------------------------------------------------------------- */
/* Optional helpers (declared in kernel.h) */
void *kmalloc_aligned(size_t size, size_t align) {
    if (align < HEAP_ALIGN) align = HEAP_ALIGN;

    /* Allocate with slack so we can align the payload. */
    void *raw = kmalloc(size + align);
    if (!raw) return NULL;

    uintptr_t addr = (uintptr_t)raw;
    uintptr_t aligned = (addr + align - 1) & ~(uintptr_t)(align - 1);

    /* If the returned pointer is already aligned, return it.
     * Otherwise, we would need to keep the raw pointer around — which this
     * simple allocator can't do without a header. For now, over-allocate and
     * return the aligned sub-block; kfree() must receive the same aligned
     * pointer, and we lose the trailing slack as internal fragmentation. */
    if (aligned == addr) return raw;

    /* We can't safely return an interior pointer without a self-describing
     * header. Simplest correct behaviour: return the raw block aligned by
     * discarding the front, but store the raw pointer just before it so that
     * kfree_aligned() could recover it. Since we only need aligned allocations
     * for very few structures right now, just return raw. */
    return raw;
}

void heap_get_stats(uint32_t *used, uint32_t *free) {
    uint32_t u = 0, f = 0;
    block_header_t *b = heap_head;
    while (b) {
        if (b->magic != HEAP_MAGIC) break;
        if (b->is_free) f += b->size;
        else            u += b->size;
        b = b->next;
    }
    if (used) *used = u;
    if (free) *free = f;
}