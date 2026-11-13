/* =============================================================================
 * Quark-OS Linker Script
 * Kernel loads at 1 MB and exports boundary symbols for the PMM/heap.
 *
 * Layout order:
 *   .text    .rodata    .data    .bss    .bss.stack
 *
 * The stack is placed LAST, after all other data, so it cannot collide with
 * large .bss arrays or with the heap. __kernel_end is exported AFTER the
 * stack, so the PMM and heap allocator never hand out pages that overlap it.
 * ============================================================================= */

ENTRY(_start)
OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)

SECTIONS
{
    . = 0x00100000;

    __kernel_start = .;

    .text : ALIGN(4K)
    {
        KEEP(*(.multiboot))
        *(.text .text.*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4K)
    {
        *(.data .data.*)
    }

    __bss_start = .;

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    . = ALIGN(4K);
    __bss_end = .;

    /* Kernel stack: placed after .bss so it can never overlap heap data.
     * The __kernel_end symbol comes AFTER this section, so PMM/heap know
     * to skip past the stack too. */
    .bss.stack : ALIGN(16)
    {
        *(.bss.stack)
    }

    . = ALIGN(4K);
    __kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note .note.*)
    }
}