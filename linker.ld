/* =============================================================================
 * Quark-OS Linker Script
 * Kernel loads at 1 MB and exports boundary symbols for the PMM/heap.
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
    __kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note .note.*)
    }
}