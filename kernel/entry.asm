; FlashKernel entry point (32-bit protected mode, loaded at 1 MB)
; Copyright (c) 2026 FlashOS. All rights reserved.
bits 32
section .text.entry
global _start
extern kmain
extern _bss_start
extern _bss_end

_start:
    cli
    mov esp, 0x9F000
    fninit
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
