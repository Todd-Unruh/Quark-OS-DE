/* =============================================================================
 * Quark-OS kernel/syscall.c
 * Vector interrupt entry point handling software routing via INT 0x80
 * ============================================================================= */

#include <quark/kernel.h>

/* Dispatched context entry handler function from assembler stub stack frames */
void syscall_handler(registers_t *regs) {
    uint32_t sys_index = regs->eax;

    switch (sys_index) {
        case 1: /* Example print system call routing mapped inside EAX index */
            terminal_write((const char *)regs->ebx);
            break;
        case 2: /* Example process termination call */
            proc_exit((int32_t)regs->ebx);
            break;
        default:
            kprintf("Unknown Syscall invoked: Index %u\n", sys_index);
            break;
    }
}

void syscall_init(void) {
    /* Gates are safely installed within idt.c under vector index entry 0x80 */
}
