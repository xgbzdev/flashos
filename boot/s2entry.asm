; FlashOS stage 2 entry (real mode, loaded at 0000:9000)
; Copyright (c) 2026 FlashOS. All rights reserved.
;
; Glue between the 2 KB boot loader and stage2.c (compiled with gcc -m16):
; BIOS disk calls and unreal mode for C code that needs 32-bit addresses.
bits 16
section .text.entry
global _s2start
global s2_bios13
global s2_unreal
extern s2_main

_s2start:
    call dword s2_main
    ret

; u32 s2_bios13(u32 ax, u32 dx, u32 si, u32 bx)
;   returns AX in bits 0-15 and the carry flag in bit 16; BX saved in s2_bx
s2_bios13:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov eax, [ebp + 8]
    mov edx, [ebp + 12]
    mov esi, [ebp + 16]
    mov ebx, [ebp + 20]
    push ds
    push es
    int 0x13
    pop es
    pop ds
    mov ecx, 0
    jnc .ok
    mov ecx, 0x10000
.ok:
    mov [s2_bx], bx
    and eax, 0xFFFF
    or eax, ecx
    pop edi
    pop esi
    pop ebx
    pop ebp
    o32 ret

; re-arm unreal mode (BIOS calls may reload segment registers)
s2_unreal:
    pushfd
    push eax
    push ebx
    cli
    push ds
    push es
    lgdt [s2_gdt_desc]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp short .pm
.pm:
    mov bx, 0x10
    mov ds, bx
    mov es, bx
    and al, 0xFE
    mov cr0, eax
    jmp short .rm
.rm:
    pop es
    pop ds
    pop ebx
    pop eax
    popfd
    o32 ret

section .data
global s2_bx
s2_bx: dw 0
align 8
s2_gdt:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
s2_gdt_end:
s2_gdt_desc:
    dw s2_gdt_end - s2_gdt - 1
    dd s2_gdt
