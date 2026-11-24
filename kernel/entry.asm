; FlashKernel entry point (32-bit protected mode, loaded at 1 MB)
; Copyright (c) 2026 FlashOS. All rights reserved.
bits 32
section .text.entry
global _start
extern kmain
extern _bss_start
extern _bss_end
extern _kernel_end

_start:
    cli
    mov esp, 0x9F000
    fninit
    ; keep a pristine copy of the kernel image for INSTALL (before .data changes)
    mov esi, 0x100000
    mov edi, 0x1E00000
    mov ecx, _kernel_end
    sub ecx, esi
    cld
    rep movsb
    mov edi, _bss_start
    mov ecx, _bss_end
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb
    call kmain
.halt:
    cli
    hlt
    jmp .halt
