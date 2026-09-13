/* =============================================================================
 * Quark-OS kernel/gdt.c
 * Global Descriptor Table: null, kernel code, kernel data, user code, user data
 * ============================================================================= */

#include <quark/kernel.h>

#define GDT_ENTRIES 5

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;

/* Load GDT and reload segment registers (defined in gdt_flush.asm) */
extern void gdt_flush(uint32_t gdt_ptr_addr);

static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran) {
    gdt[i].base_low  = (base & 0xFFFF);
    gdt[i].base_mid  = (base >> 16) & 0xFF;
    gdt[i].base_high = (base >> 24) & 0xFF;
    
    /* Safely separate the true 20-bit segment limit fields */
    gdt[i].limit_low = (limit & 0xFFFF);
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access    = access;
}

void gdt_init(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;

    /* 0: Null segment (required by x86 standard specifications) */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* 1: Kernel code segment - ring 0, execute/read, 4 GB flat map space
     * NOTE: When using 4KB Page Granularity (0xCF), the target structure limit 
     * parameter value MUST map as 0xFFFFF instead of 0xFFFFFFFF! */
    gdt_set_entry(1, 0x00000000, 0xFFFFF,
                  0x9A,   /* present | ring0 | code | exec | readable */
                  0xCF);  /* 4KB gran | 32-bit protected mode operational bit */

    /* 2: Kernel data segment - ring 0, read/write, 4 GB flat map space */
    gdt_set_entry(2, 0x00000000, 0xFFFFF,
                  0x92,   /* present | ring0 | data | writable */
                  0xCF);

    /* 3: User code segment - ring 3, execute/read, 4 GB flat map space */
    gdt_set_entry(3, 0x00000000, 0xFFFFF,
                  0xFA,   /* present | ring3 | code | exec | readable */
                  0xCF);

    /* 4: User data segment - ring 3, read/write, 4 GB flat map space */
    gdt_set_entry(4, 0x00000000, 0xFFFFF,
                  0xF2,   /* present | ring3 | data | writable */
                  0xCF);

    gdt_flush((uint32_t)&gdt_ptr);
}
