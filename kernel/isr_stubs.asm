; =============================================================================
; Quark-OS kernel/isr_stubs.asm
; ISR stubs for CPU exceptions (0-31), IRQ handlers (32-47), syscall (0x80)
; Each stub pushes interrupt number + error code, then calls common handler
; =============================================================================

BITS 32
extern isr_common_handler
extern irq_common_handler
extern syscall_handler

global idt_flush

; Macro for exception WITHOUT an error code (CPU doesn't push one)
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push dword 0        ; dummy error code
    push dword %1       ; interrupt number
    jmp isr_common_stub
%endmacro

; Macro for exception WITH an error code (CPU already pushed it)
%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    push dword %1       ; interrupt number (error code already on stack)
    jmp isr_common_stub
%endmacro

; CPU Exception stubs (INT 0-31)
ISR_NOERR 0     ; Division by zero
ISR_NOERR 1     ; Debug
ISR_NOERR 2     ; NMI
ISR_NOERR 3     ; Breakpoint
ISR_NOERR 4     ; Overflow
ISR_NOERR 5     ; Bound range exceeded
ISR_NOERR 6     ; Invalid opcode
ISR_NOERR 7     ; Device not available
ISR_ERR   8     ; Double fault
ISR_NOERR 9     ; Coprocessor segment overrun
ISR_ERR   10    ; Invalid TSS
ISR_ERR   11    ; Segment not present
ISR_ERR   12    ; Stack fault
ISR_ERR   13    ; General protection fault
ISR_ERR   14    ; Page fault
ISR_NOERR 15
ISR_NOERR 16    ; x87 FPU error
ISR_ERR   17    ; Alignment check
ISR_NOERR 18    ; Machine check
ISR_NOERR 19    ; SIMD FP exception
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; IRQ stubs (INT 32-47, hardware interrupts)
%macro IRQ_STUB 2
global irq%1
irq%1:
    cli
    push dword 0
    push dword %2
    jmp irq_common_stub
%endmacro

IRQ_STUB 0, 32   ; Timer (PIT)
IRQ_STUB 1, 33   ; Keyboard
IRQ_STUB 2, 34   ; Cascade (never fires)
IRQ_STUB 3, 35   ; COM2
IRQ_STUB 4, 36   ; COM1
IRQ_STUB 5, 37   ; LPT2
IRQ_STUB 6, 38   ; Floppy
IRQ_STUB 7, 39   ; LPT1 / spurious
IRQ_STUB 8, 40   ; RTC
IRQ_STUB 9, 41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44  ; PS/2 mouse
IRQ_STUB 13, 45  ; FPU
IRQ_STUB 14, 46  ; ATA primary
IRQ_STUB 15, 47  ; ATA secondary

; Syscall stub (INT 0x80)
global syscall_stub
syscall_stub:
    push dword 0
    push dword 0x80
    pusha
    push ds
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    push esp
    call syscall_handler
    add esp, 4
    pop ds
    popa
    add esp, 8
    sti
    iret

; Common stub: save registers, call C handler, restore
isr_common_stub:
    pusha
    push ds
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    push esp
    call isr_common_handler
    add esp, 4
    pop ds
    popa
    add esp, 8
    iret

irq_common_stub:
    pusha
    push ds
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    push esp
    call irq_common_handler
    add esp, 4
    pop ds
    popa
    add esp, 8
    iret

; Install IDT
idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret