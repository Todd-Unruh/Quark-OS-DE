/* =============================================================================
 * Quark-OS kernel/main.c — kernel entry point.
 * Called from boot.asm with the multiboot magic and info pointer.
 * Initializes all subsystems in order, then starts the desktop.
 * ============================================================================= */

#include <quark/kernel.h>

/* --------------------------------------------------------------------------
 * Boot banner (text mode only)
 * -------------------------------------------------------------------------- */
static void print_banner(void) {
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_writeln("  ___  _   _   _   ____  _  __       ___  ____");
    terminal_writeln(" / _ \\| | | | / \\ |  _ \\| |/ /      / _ \\/ ___|");
    terminal_writeln("| | | | | | |/ _ \\| |_) | ' /  ___ | | | \\___ \\");
    terminal_writeln("| |_| | |_| / ___ \\  _ <| . \\ |___|| |_| |___) |");
    terminal_writeln(" \\__\\_\\\\___/_/   \\_\\_| \\_\\_|\\_\\      \\___/|____/");
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeln("  32-bit x86 Protected Mode Operating System  v3.0");
    terminal_setcolor(VGA_DARK_GREY, VGA_BLACK);
    terminal_writeln("  ------------------------------------------------");
    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
}

/* --------------------------------------------------------------------------
 * Boot log helper: only writes to VGA text when we don't have a framebuffer.
 * -------------------------------------------------------------------------- */
static void boot_ok(const char *msg) {
    if (framebuffer_pixels != NULL) return;

    terminal_setcolor(VGA_DARK_GREY, VGA_BLACK);
    terminal_write("  [");
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_write(" OK ");
    terminal_setcolor(VGA_DARK_GREY, VGA_BLACK);
    terminal_write("] ");
    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    terminal_writeln(msg);
}

/* --------------------------------------------------------------------------
 * Kernel entry
 * -------------------------------------------------------------------------- */
void kernel_main(uint32_t mb_magic, multiboot_info_t *mb_info) {
    /* 1. Framebuffer discovery first: we need to know if we're in graphics
     *    mode before we try to print anything to VGA text. */
    framebuffer_init(mb_info);
    uint8_t have_fb = (framebuffer_pixels != NULL);

    /* 2. Text console only if we're not in graphics mode. */
    if (!have_fb) {
        terminal_init();
        terminal_clear();
        print_banner();
    }

    /* 3. Validate multiboot magic. */
    if (mb_magic != MULTIBOOT_MAGIC) {
        if (!have_fb) {
            terminal_setcolor(VGA_LIGHT_RED, VGA_BLACK);
            terminal_writeln("PANIC: Invalid multiboot magic!");
        }
        goto halt;
    }

    /* 4. Read usable memory from the BIOS. */
    uint32_t mem_kb = 640;   /* safe default: 640 KB lower + 0 upper */
    if (mb_info && (mb_info->flags & 0x1)) {
        mem_kb = mb_info->mem_lower + mb_info->mem_upper;
    }
    if (!have_fb) {
        kprintf("  Memory reported by BIOS: %u KB (%u MB)\n",
                mem_kb, mem_kb / 1024);
    }

    /* --- Descriptor tables --- */
    gdt_init();
    boot_ok("GDT loaded (null/kernel code/data/user code/data segments)");

    idt_init();
    boot_ok("IDT loaded (256 vectors: exceptions + IRQs + syscall 0x80)");

    /* --- Interrupt controller --- */
    pic_init();
    boot_ok("PIC 8259A initialized (IRQ0-15 remapped to INT 32-47)");

    /* --- Timer --- */
    timer_init(100);
    boot_ok("PIT 8254 timer @ 100 Hz (10 ms tick)");

    /* --- PS/2 devices --- */
    keyboard_init();
    boot_ok("PS/2 keyboard controller initialized");

    mouse_init();
    boot_ok("PS/2 mouse initialized (IRQ 12 hooked)");

    /* --- Memory managers --- */
    pmm_init(mem_kb);
    if (!have_fb) {
        kprintf("  [ OK ]  Physical memory manager: %u free pages (%u MB free)\n",
                pmm_free_pages(),
                (pmm_free_pages() * PAGE_SIZE) / (1024 * 1024));
    } else {
        kprintf("[BOOT] PMM: kernel_end=0x%x free_pages=%u\n",
                (unsigned)(uintptr_t)__kernel_end,
                pmm_free_pages());
    }

    heap_init();
    boot_ok("Kernel heap initialized (8 MB - 16 MB)");

    /* --- Higher-level subsystems --- */
    proc_init();
    boot_ok("Process manager initialized (Round-Robin scheduler)");

    ramfs_init();
    boot_ok("Virtual RamFS file allocation tables mounted");

    syscall_init();
    boot_ok("Syscall table installed (INT 0x80)");

    pci_init();
    boot_ok("PCI bus scanned (e1000 if present)");

    /* --- Interrupts on --- */
    sti();
    boot_ok("Interrupts enabled");

    if (!have_fb) {
        terminal_setcolor(VGA_DARK_GREY, VGA_BLACK);
        terminal_writeln("  ------------------------------------------------");
        terminal_clear();
    }

    /* --- Hand off to the desktop --- */
    if (have_fb) {
        kprintf("[BOOT] Framebuffer: %ux%u pitch=%u\n",
                fb_width, fb_height, fb_pitch_get());
        kprintf("[BOOT] Entering desktop...\n");
    }

    desktop_run(0);

    /* Only reached if the desktop ever returns (it doesn't today). */
    shell_run();

halt:
    cli();
    for (;;) hlt();
}