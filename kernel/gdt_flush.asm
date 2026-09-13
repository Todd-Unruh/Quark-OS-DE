; =============================================================================
; Quark-OS kernel/gdt_flush.asm
; Installs the GDT and reloads all segment registers
; =============================================================================

BITS 32
global gdt_flush

gdt_flush:
    mov eax, [esp+4]    ; load gdt_ptr address from stack
    lgdt [eax]          ; load GDT register

    ; Reload data segment registers with kernel data selector (index 2, RPL 0)
    mov ax, 0x10        ; 0x10 = GDT entry 2 (kernel data)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS with kernel code selector (index 1, RPL 0)
    jmp 0x08:.flush     ; 0x08 = GDT entry 1 (kernel code)
.flush:
    ret