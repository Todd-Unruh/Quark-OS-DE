#ifndef QUARK_KERNEL_H
#define QUARK_KERNEL_H

/* ============================================================================
 * Quark-OS Kernel Header
 * Core types, constants, and kernel API declarations
 * ============================================================================ */

/* --------------------------------------------------------------------------
 * Basic fixed-width types (no stdint.h in freestanding env)
 * -------------------------------------------------------------------------- */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;
typedef uint32_t           size_t;
typedef int32_t            ssize_t;
typedef int32_t            pid_t;
typedef uint32_t           uintptr_t;

#define NULL    ((void*)0)
#define TRUE    1
#define FALSE   0

/* --------------------------------------------------------------------------
 * Kernel memory layout (virtual addresses)
 * -------------------------------------------------------------------------- */
#define KERNEL_VIRT_BASE    0x00100000
#define KERNEL_HEAP_START   0x00800000
#define KERNEL_HEAP_END     0x01000000
#define USER_SPACE_START    0x08000000
#define PAGE_SIZE           4096
#define PAGE_ALIGN(x)       (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/* --------------------------------------------------------------------------
 * VGA Text Mode constants
 * -------------------------------------------------------------------------- */
#define VGA_WIDTH           80
#define VGA_HEIGHT          25
#define VGA_MEMORY          0x000B8000
#define KEY_WORKSPACE_LEFT  0x11
#define KEY_WORKSPACE_RIGHT 0x12
#define DESKTOP_VIEW_HOME   0
#define DESKTOP_VIEW_FILES  1
#define DESKTOP_VIEW_SETTINGS 2

#define VGA_BLACK           0
#define VGA_BLUE            1
#define VGA_GREEN           2
#define VGA_CYAN            3
#define VGA_RED             4
#define VGA_MAGENTA         5
#define VGA_BROWN           6
#define VGA_LIGHT_GREY      7
#define VGA_DARK_GREY       8
#define VGA_LIGHT_BLUE      9
#define VGA_LIGHT_GREEN     10
#define VGA_LIGHT_CYAN      11
#define VGA_LIGHT_RED       12
#define VGA_LIGHT_MAGENTA   13
#define VGA_LIGHT_BROWN     14
#define VGA_WHITE           15

/* --------------------------------------------------------------------------
 * Multiboot structures
 * -------------------------------------------------------------------------- */
#define MULTIBOOT_MAGIC     0x2BADB002

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint16_t framebuffer_reserved;
} __attribute__((packed)) multiboot_info_t;

/* --------------------------------------------------------------------------
 * GDT / IDT structures
 * -------------------------------------------------------------------------- */
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, useless_esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, esp, ss;
} __attribute__((packed)) registers_t;

/* --------------------------------------------------------------------------
 * Process / scheduler structures
 * -------------------------------------------------------------------------- */
#define MAX_PROCS       64
#define PROC_NAME_LEN   32
#define KSTACK_SIZE     8192

typedef enum {
    PROC_UNUSED  = 0,
    PROC_RUNNING = 1,
    PROC_READY   = 2,
    PROC_SLEEPING= 3,
    PROC_ZOMBIE  = 4
} proc_state_t;

typedef struct process {
    pid_t       pid;
    pid_t       ppid;
    char        name[PROC_NAME_LEN];
    proc_state_t state;
    uint32_t    esp;
    uint32_t    eip;
    uint32_t    kstack[KSTACK_SIZE / 4];
    uint32_t    ticks;
    uint32_t    sleep_ticks;
    int32_t     exit_code;
    struct process *next;
} process_t;

/* --------------------------------------------------------------------------
 * Memory manager structures
 * -------------------------------------------------------------------------- */
typedef struct {
    uint32_t    total_pages;
    uint32_t    free_pages;
    uint32_t    used_pages;
    uint8_t    *bitmap;
} pmm_state_t;

/* --------------------------------------------------------------------------
 * Core function prototypes
 * -------------------------------------------------------------------------- */

/* terminal.c */
void terminal_init(void);
void terminal_clear(void);
void terminal_refresh(void);
void terminal_setcolor(uint8_t fg, uint8_t bg);
void terminal_putchar(char c);
void terminal_write(const char *str);
void terminal_writeln(const char *str);
void kprintf(const char *fmt, ...);
void terminal_scroll_up(void);
void terminal_scroll_down(void);

/* desktop.c */
int  desktop_run(int initial_view);
void framebuffer_init(multiboot_info_t *mb_info);

/* gdt.c */
void gdt_init(void);

/* idt.c */
void idt_init(void);
void irq_register(uint8_t irq, void (*handler)(registers_t *));

/* pic.c */
void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);

/* timer.c */
void     timer_init(uint32_t hz);
uint32_t timer_get_ticks(void);
void     timer_sleep(uint32_t ms);

/* keyboard.c */
void keyboard_init(void);
char keyboard_getchar(void);
int  keyboard_has_input(void);

/* pmm.c */
void     pmm_init(uint32_t mem_kb);
uint32_t pmm_alloc_page(void);
void     pmm_free_page(uint32_t addr);
uint32_t pmm_free_pages(void);

/* heap.c */
void  heap_init(void);
void *kmalloc(size_t size);
void  kfree(void *ptr);
void *kmalloc_aligned(size_t size, size_t align);
void  heap_get_stats(uint32_t *used, uint32_t *free);

/* proc.c */
void      proc_init(void);
process_t *proc_create(const char *name, void (*entry)(void), pid_t ppid);
void      proc_exit(int32_t code);
void      proc_sleep(uint32_t ms);
void      schedule(void);
process_t *proc_current(void);
void      proc_list_all(void);
int       proc_kill(pid_t pid);

/* syscall.c */
void syscall_init(void);

/* shell.c */
void shell_run(void);

/* lib/string.c */
size_t  kstrlen(const char *s);
char   *kstrcpy(char *dst, const char *src);
char   *kstrcat(char *dst, const char *src);
int     kstrcmp(const char *a, const char *b);
int     kstrncmp(const char *a, const char *b, size_t n);
void   *kmemset(void *dst, int c, size_t n);
void   *kmemcpy(void *dst, const void *src, size_t n);
char   *kitoa(int32_t n, char *buf, int base);
char   *kuitoa(uint32_t n, char *buf, int base);
char   *kstrchr(const char *s, int c);
char   *kstrrchr(const char *s, int c);
char   *kstrstr(const char *haystack, const char *needle);
char   *kstrncpy(char *dst, const char *src, size_t n);
char   *kstrncat(char *dst, const char *src, size_t n);

/* I/O port helpers */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile ("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline void io_wait(void) { outb(0x80, 0); }
static inline void sti(void)  { __asm__ volatile ("sti"); }
static inline void cli(void)  { __asm__ volatile ("cli"); }
static inline void hlt(void)  { __asm__ volatile ("hlt"); }

/* --------------------------------------------------------------------------
 * RamFS
 * -------------------------------------------------------------------------- */
#define MAX_FILES       32
#define MAX_FILE_NAME   32
#define MAX_FILE_SIZE   2048

typedef struct {
    char     name[MAX_FILE_NAME];
    uint8_t  data[MAX_FILE_SIZE];
    uint32_t size;
    uint8_t  used;
    uint8_t  is_dir;
    char     parent_dir[MAX_FILE_NAME];
} ram_file_t;

void ramfs_init(void);
int  ramfs_create(const char *name, const char *current_dir, uint8_t is_directory);
int  ramfs_write(const char *name, const char *current_dir, const char *content, uint32_t size);
ram_file_t* ramfs_get(const char *name, const char *current_dir);
ram_file_t* ramfs_get_child(const char *current_dir, int index);
void ramfs_list(const char *current_dir);
int  ramfs_delete(const char *name, const char *current_dir, uint8_t expect_dir);
int  ramfs_match(const char *pattern, const char *name);
int  ramfs_count_in_dir(const char *current_dir);
uint32_t ramfs_dir_size(const char *current_dir);
int  ramfs_used_count(void);
int  ramfs_copy(const char *name, const char *dest_name, const char *current_dir);
int  ramfs_rename(const char *old_name, const char *new_name, const char *current_dir);
void ramfs_find(const char *dir, const char *pattern);
void ramfs_tree(const char *dir);

/* --------------------------------------------------------------------------
 * Networking / PCI
 * -------------------------------------------------------------------------- */
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

#define E1000_RX_DESC_COUNT 32
#define E1000_TX_DESC_COUNT 8
#define E1000_PACKET_SIZE   2048

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t  csum;
    uint8_t  status;
    uint16_t errors;
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t  csum;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

void pci_init(void);
void e1000_init(uint32_t bar0, uint32_t mem_base);
void e1000_send_packet(const void *data, uint16_t length);
void net_list_status(void);

/* --------------------------------------------------------------------------
 * Linker-exported kernel boundary symbols (see linker.ld)
 * -------------------------------------------------------------------------- */
extern uint8_t __kernel_start[];
extern uint8_t __kernel_end[];
extern uint8_t __bss_start[];
extern uint8_t __bss_end[];

/* --------------------------------------------------------------------------
 * Framebuffer state (defined in drivers/video.c)
 * -------------------------------------------------------------------------- */
extern uint32_t  fb_width;
extern uint32_t  fb_height;
extern uint32_t* framebuffer_pixels;
uint32_t fb_pitch_get(void);
void     put_pixel(uint32_t x, uint32_t y, uint32_t color);

/* --------------------------------------------------------------------------
 * Mouse driver (defined in drivers/mouse.c)
 * -------------------------------------------------------------------------- */
void mouse_init(void);
extern int     mouse_x;
extern int     mouse_y;
extern uint8_t mouse_left_click;
extern uint8_t mouse_right_click;

/* --------------------------------------------------------------------------
 * NOTE: Desktop Environment configuration and window types live in
 * include/quark/de.h — that's the single source of truth for g_gui_config,
 * gui_config_t, window_t, and g_windows. Anything that needs them should
 * #include <quark/de.h>.
 * -------------------------------------------------------------------------- */

#endif /* QUARK_KERNEL_H */