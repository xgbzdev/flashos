; FlashOS master boot record (used when the image is written to a USB stick / HDD)
; Copyright (c) 2026 FlashOS. All rights reserved.
;
; Relocates itself to 0x0600, loads the 2 KB stage loader from disk sectors 1-4
; to 0x7C00 and jumps to it. The partition table is filled in by mkiso.py.
bits 16
org 0x0600

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld
    mov si, 0x7C00
    mov di, 0x0600
    mov cx, 256
    rep movsw
    jmp 0x0000:relocated

relocated:
    sti
    mov [drive], dl
    mov si, dap
    mov ah, 0x42
    int 0x13
    jc fail
    mov dl, [drive]
    jmp 0x0000:0x7C00

fail:
    mov si, msg
.p: lodsb
    test al, al
    jz .h
    mov ah, 0x0E
    mov bx, 0x0004
    int 0x10
    jmp .p
.h: cli
    hlt
    jmp .h

drive: db 0
msg:   db "FlashOS: disk read error", 0
align 4
dap:   db 16, 0
       dw 4                 ; sectors
       dw 0x7C00, 0x0000    ; offset, segment
       dq 1                 ; LBA

times 440-($-$$) db 0
disk_sig: dd 0
          dw 0
partitions: times 64 db 0   ; filled by mkiso.py
dw 0xAA55
