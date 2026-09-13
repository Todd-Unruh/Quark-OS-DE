/* =============================================================================
 * Quark-OS kernel/proc.c
 * Process control blocks and round-robin scheduler kernel engine
 * ============================================================================= */

#include <quark/kernel.h>

static process_t  proc_table[MAX_PROCS];
static process_t *current_proc = NULL;
static process_t *idle_proc    = NULL;

/* Simplistic kernel idle fallback thread */
static void idle_thread(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void proc_init(void) {
    kmemset(proc_table, 0, sizeof(proc_table));
    
    /* Initialize basic scheduler tracking pointers */
    current_proc = NULL;

    /* Create the essential fallback idle thread */
    idle_proc = proc_create("idle", idle_thread, 0);
    if (idle_proc) {
        idle_proc->state = PROC_READY;
    }
    
    /* Turn the main running boot execution into the prime process block */
    process_t *main_p = &proc_table[1];
    main_p->pid = 1;
    main_p->ppid = 0;
    kstrcpy(main_p->name, "kernel_main");
    main_p->state = PROC_RUNNING;
    current_proc = main_p;
}

process_t *proc_create(const char *name, void (*entry)(void), pid_t ppid) {
    for (int i = 0; i < MAX_PROCS; i++) {
        if (proc_table[i].state == PROC_UNUSED) {
            process_t *p = &proc_table[i];
            p->pid = i + 1;
            p->ppid = ppid;
            kstrcpy(p->name, name);
            p->ticks = 0;
            p->sleep_ticks = 0;

            /* Set up stack frame structure to mock an ISR return (iret context) */
            uint32_t *stk = &p->kstack[KSTACK_SIZE / 4];

            stk--; *stk = 0x10;             /* SS data selector */
            stk--; *stk = (uint32_t)stk;    /* ESP matching location */
            stk--; *stk = 0x0202;           /* EFLAGS enabling interrupts */
            stk--; *stk = 0x08;             /* CS code segment selector */
            stk--; *stk = (uint32_t)entry;  /* EIP initial target hook */

            /* Push dummy registers to match pusha requirements */
            stk--; *stk = 0;                /* EAX */
            stk--; *stk = 0;                /* ECX */
            stk--; *stk = 0;                /* EDX */
            stk--; *stk = 0;                /* EBX */
            stk--; *stk = 0;                /* ESP fallback */
            stk--; *stk = 0;                /* EBP */
            stk--; *stk = 0;                /* ESI */
            stk--; *stk = 0;                /* EDI */
            stk--; *stk = 0x10;             /* DS data segment register */

            p->esp = (uint32_t)stk;
            p->state = PROC_READY;
            return p;
        }
    }
    return NULL;
}

void schedule(void) {
    if (!current_proc) return;

    /* Update thread tick and update sleeping system states */
    current_proc->ticks++;
    for (int i = 0; i < MAX_PROCS; i++) {
        if (proc_table[i].state == PROC_SLEEPING) {
            if (proc_table[i].sleep_ticks > 0) {
                proc_table[i].sleep_ticks--;
            }
            if (proc_table[i].sleep_ticks == 0) {
                proc_table[i].state = PROC_READY;
            }
        }
    }

    /* Locate next ready-to-run thread segment */
    process_t *next = NULL;
    int start_index = current_proc->pid % MAX_PROCS;
    
    for (int i = 0; i < MAX_PROCS; i++) {
        int idx = (start_index + i) % MAX_PROCS;
        if (proc_table[idx].state == PROC_READY && &proc_table[idx] != idle_proc) {
            next = &proc_table[idx];
            break;
        }
    }

    if (!next) {
        if (current_proc->state == PROC_RUNNING) {
            return; /* Keep running current process */
        }
        next = idle_proc;
    }

    /* Perform low-level assembler register context switch execution */
    process_t *old = current_proc;
    if (old->state == PROC_RUNNING) {
        old->state = PROC_READY;
    }
    
    next->state = PROC_RUNNING;
    current_proc = next;

    /* Inline software register swap */
    __asm__ volatile (
        "movl %%esp, %0\n\t"
        "movl %1, %%esp\n\t"
        : "=m"(old->esp)
        : "r"(next->esp)
        : "memory"
    );
}

void proc_exit(int32_t code) {
    cli();
    current_proc->state = PROC_ZOMBIE;
    current_proc->exit_code = code;
    sti();
    for (;;) { schedule(); }
}

void proc_sleep(uint32_t ms) {
    cli();
    current_proc->sleep_ticks = ms / 10; /* Convert 10ms per clock tick */
    current_proc->state = PROC_SLEEPING;
    sti();
    while (current_proc->state == PROC_SLEEPING) { hlt(); }
}

process_t *proc_current(void) {
    return current_proc;
}

int proc_kill(pid_t pid) {
    if (pid <= 0 || pid > MAX_PROCS) return -1;
    process_t *p = &proc_table[pid - 1];
    if (p->state == PROC_UNUSED) return -2;
    if (p->pid == 1) return -3;
    p->state = PROC_ZOMBIE;
    p->exit_code = 9;
    return 0;
}

void proc_list_all(void) {
    kprintf("\nPID  PPID STATUS   TICKS    NAME\n");
    kprintf("----------------------------------------\n");
    for (int i = 0; i < MAX_PROCS; i++) {
        if (proc_table[i].state != PROC_UNUSED) {
            const char *st = "UNKNOWN";
            switch (proc_table[i].state) {
                case PROC_RUNNING:  st = "RUNNING "; break;
                case PROC_READY:    st = "READY   "; break;
                case PROC_SLEEPING: st = "SLEEPING"; break;
                case PROC_ZOMBIE:   st = "ZOMBIE  "; break;
                default: break;
            }
            kprintf("%u    %u    %s  %u     %s\n",
                    proc_table[i].pid, proc_table[i].ppid, st,
                    proc_table[i].ticks, proc_table[i].name);
        }
    }
}
