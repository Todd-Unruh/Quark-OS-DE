; =============================================================================
; Quark-OS Boot Loader Handoff (boot.asm)
; Multiboot 1 header + 32-bit entry stub.
; GRUB hands off with:
;   EAX = 0x2BADB002 (multiboot magic)
;   EBX = physical address of multiboot_info_t
; =============================================================================

bits 32

global _start
extern kernel_main

; ---------------------------------------------------------------------------
; Multiboot header constants
; ---------------------------------------------------------------------------
MODULEALIGN equ 1<<0
MEMINFO     equ 1<<1
VIDEO_MODE  equ 1<<2
FLAGS       equ MODULEALIGN | MEMINFO | VIDEO_MODE
MAGIC       equ 0x1BADB002
CHECKSUM    equ -(MAGIC + FLAGS)

; ---------------------------------------------------------------------------
; Multiboot header — must be in the first 8 KB of the file, 4-byte aligned.
; ---------------------------------------------------------------------------
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

    ; a.out kludge fields (all zero because bit 16 is not set)
    dd 0    ; header_addr
    dd 0    ; load_addr
    dd 0    ; load_end_addr
    dd 0    ; bss_end_addr
    dd 0    ; entry_addr

    ; Graphics request fields
    dd 0    ; mode_type: 0 = linear framebuffer
    dd 640  ; width
    dd 480  ; height
    dd 32   ; depth

; ---------------------------------------------------------------------------
; Kernel entry point
; ---------------------------------------------------------------------------
section .text
align 4
_start:
    cli                         ; no interrupts until the kernel is ready

    ; --- Set up a temporary kernel stack ---
    mov esp, stack_top
    mov ebp, esp

    ; --- Push multiboot args for kernel_main(magic, mb_info) ---
    ; cdecl: rightmost arg pushed first, so push mb_info then magic.
    push ebx                    ; mb_info pointer
    push eax                    ; multiboot magic

    ; --- Call into C ---
    call kernel_main

    ; kernel_main should never return, but if it does, halt cleanly.
.hang:
    cli
    hlt
    jmp .hang

; ---------------------------------------------------------------------------
; Stack (16 KB, in .bss so it is zero-initialized and not in the image)
; ---------------------------------------------------------------------------
section .bss
align 16
stack_bottom:
    resb 16384
stack_top: