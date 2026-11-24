; FlashOS stage loader - 2048 bytes, loaded at 0000:7C00
; Copyright (c) 2026 FlashOS. All rights reserved.
;
; Booted either by the BIOS directly (El Torito "no emulation", CD sectors of
; 2048 bytes) or by mbr.asm (USB/HDD, 512-byte sectors). mkiso.py patches the
; parameter block below for each copy.
;
;  1. enable A20, grab the BIOS 8x16 font to 0x5000
;  2. load FLASHKRN (kernel) to 1 MB and FLASHFS (file archive) to 32 MB
;     through unreal mode
;  3. pick a VESA linear framebuffer mode (1024x768 preferred) -> info at 0x6000
;  4. switch to 32-bit protected mode and jump to the kernel at 0x100000
bits 16
org 0x7C00

KERNEL_DEST equ 0x00100000
FS_DEST     equ 0x02000000
BUF_SEG     equ 0x1000          ; bounce buffer at 0x10000 (32 KB)
VBE_CTRL    equ 0x8800        ; after the loader (0x7C00-0x83FF)
VBE_MODE    equ 0x8A00
STAGE2      equ 0x9000        ; USB/disk SWF scanner (real mode C)
S2_COPY     equ 0x01D00000    ; pristine copies for the installer
SYS_COPY    equ 0x01F00000
BOOTINFO    equ 0x6000
FONT        equ 0x5000

start:
    jmp 0x0000:main             ; 5 bytes, normalises CS
    times 8-($-$$) db 0
; ---- parameter block (offset 8), patched by mkiso.py ----
p_sector_size: dw 2048
p_kernel_lba:  dd 0
p_kernel_cnt:  dd 0
p_fs_lba:      dd 0
p_fs_cnt:      dd 0
p_fs_bytes:    dd 0
p_s2_lba:      dd 0
p_s2_cnt:      dd 0
p_flags:       dd 0              ; bit 0: installed on a hard disk (set by INSTALL)

main:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    cld
    mov [boot_drive], dl

    ; --- A20 ---
    in al, 0x92
    test al, 2
    jnz .a20ok
    or al, 2
    and al, 0xFE
    out 0x92, al
.a20ok:
    mov ax, 0x2401
    int 0x15

    ; --- BIOS 8x16 font -> 0x5000 ---
    push ds
    mov ax, 0x1130
    mov bh, 6
    int 0x10                    ; ES:BP -> font
    push es
    pop ds
    mov si, bp
    xor ax, ax
    mov es, ax
    mov di, FONT
    mov cx, 2048
    rep movsw
    pop ds

    mov eax, [p_flags]
    mov [BOOTINFO + 60], eax

    ; --- memory size (E801) -> BOOTINFO+16 = top of RAM ---
    mov dword [BOOTINFO + 16], 0
    mov ax, 0xE801
    xor bx, bx
    xor cx, cx
    xor dx, dx
    int 0x15
    jc .memdone
    test ax, ax
    jnz .memok
    mov ax, cx
    mov bx, dx
.memok:
    test bx, bx
    jz .memlow
    movzx eax, bx
    shl eax, 16
    add eax, 0x01000000
    jmp .memstore
.memlow:
    movzx eax, ax
    shl eax, 10
    add eax, 0x00100000
.memstore:
    mov [BOOTINFO + 16], eax
.memdone:

    ; --- sectors per 32 KB chunk ---
    xor dx, dx
    mov ax, 0x8000
    div word [p_sector_size]
    mov [chunk_secs], ax

    ; --- load kernel and file archive ---
    mov eax, [p_kernel_lba]
    mov ecx, [p_kernel_cnt]
    mov edi, KERNEL_DEST
    call load_region
    mov eax, [p_fs_lba]
    mov ecx, [p_fs_cnt]
    mov edi, FS_DEST
    call load_region
    mov eax, [p_fs_bytes]
    mov [BOOTINFO + 12], eax
    ; first 32 KB of the boot medium (MBR + loader) for INSTALL
    xor eax, eax
    movzx ecx, word [chunk_secs]
    mov edi, SYS_COPY
    call load_region
    ; stage 2: scan USB sticks / extra disks for .swf files
    mov dword [BOOTINFO + 20], 0
    mov dword [BOOTINFO + 24], 0
    mov ecx, [p_s2_cnt]
    test ecx, ecx
    jz .no_s2
    mov eax, [p_s2_lba]
    mov edi, S2_COPY
    call load_region
    mov eax, [p_s2_lba]
    mov ecx, [p_s2_cnt]
    mov edi, STAGE2
    call load_region
    call go_unreal
    and esp, 0xFFFF
    mov dl, [boot_drive]
    call STAGE2
    xor ax, ax
    mov ds, ax
    mov es, ax
.no_s2:

    ; --- graphics mode ---
    call set_video

    ; --- protected mode ---
    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm_entry

; ------------------------------------------------------------------
; load_region: EAX = first sector, ECX = sector count, EDI = destination
load_region:
    mov [cur_lba], eax
    mov [remaining], ecx
    mov [dest], edi
.loop:
    mov ecx, [remaining]
    test ecx, ecx
    jz .done
    movzx ebx, word [chunk_secs]
    cmp ecx, ebx
    jae .chunk
    mov ebx, ecx
.chunk:
    mov [this_cnt], bx
    mov [dap_count], bx
    mov eax, [cur_lba]
    mov [dap_lba], eax
    mov dword [dap_lba+4], 0
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error

    call go_unreal
    movzx eax, word [this_cnt]
    movzx edx, word [p_sector_size]
    mul edx
    mov [this_bytes], eax
    mov ecx, eax
    shr ecx, 2
    mov esi, BUF_SEG * 16
    mov edi, [dest]
    xor ax, ax
    mov ds, ax
    mov es, ax
    a32 rep movsd

    mov eax, [this_bytes]
    add [dest], eax
    movzx eax, word [this_cnt]
    add [cur_lba], eax
    sub [remaining], eax
    jmp .loop
.done:
    ret

; unreal mode: DS/ES keep a 4 GB limit while back in real mode
go_unreal:
    cli
    push ds
    push es
    lgdt [gdt_desc]
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
    sti
    ret

; ------------------------------------------------------------------
; set_video: find a linear framebuffer mode from the preference list
set_video:
    mov di, VBE_CTRL
    mov dword [di], 'VBE2'
    mov ax, 0x4F00
    int 0x10
    cmp ax, 0x004F
    jne video_error
    mov si, prefs
.pref:
    cmp word [si], 0
    je video_error
    mov ax, [VBE_CTRL + 16]     ; mode list segment
    mov fs, ax
    mov bx, [VBE_CTRL + 14]     ; mode list offset
.mode:
    mov cx, [fs:bx]
    cmp cx, 0xFFFF
    je .next_pref
    push si
    push bx
    push cx
    xor ax, ax
    mov es, ax
    mov di, VBE_MODE
    mov ax, 0x4F01
    int 0x10
    pop cx
    pop bx
    pop si
    cmp ax, 0x004F
    jne .skip
    mov ax, [VBE_MODE]          ; attributes: graphics + linear fb
    and ax, 0x0090
    cmp ax, 0x0090
    jne .skip
    mov ax, [VBE_MODE + 18]
    cmp ax, [si]
    jne .skip
    mov ax, [VBE_MODE + 20]
    cmp ax, [si + 2]
    jne .skip
    mov al, [VBE_MODE + 25]
    cmp al, [si + 4]
    jne .skip
    jmp .found
.skip:
    add bx, 2
    jmp .mode
.next_pref:
    add si, 5
    jmp .pref
.found:
    mov eax, [VBE_MODE + 40]
    mov [BOOTINFO + 0], eax     ; framebuffer address
    mov ax, [VBE_MODE + 16]
    mov [BOOTINFO + 4], ax      ; pitch
    mov ax, [VBE_MODE + 18]
    mov [BOOTINFO + 6], ax      ; width
    mov ax, [VBE_MODE + 20]
    mov [BOOTINFO + 8], ax      ; height
    mov al, [VBE_MODE + 25]
    mov [BOOTINFO + 10], al     ; bpp
    mov eax, [p_fs_bytes]
    mov [BOOTINFO + 12], eax    ; archive size
    mov bx, cx
    or bx, 0x4000
    xor ax, ax
    mov es, ax
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne video_error
    ret

prefs:
    dw 1024, 768
    db 32
    dw 1024, 768
    db 24
    dw 800, 600
    db 32
    dw 800, 600
    db 24
    dw 640, 480
    db 32
    dw 640, 480
    db 24
    dw 0

; ------------------------------------------------------------------
disk_error:
    mov si, msg_disk
    jmp die
video_error:
    mov si, msg_video
die:
    xor ax, ax
    mov ds, ax
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

msg_disk:  db "FlashOS: cannot read boot media", 0
msg_video: db "FlashOS: no VESA 640x480+ 24/32-bit mode", 0

; ------------------------------------------------------------------
bits 32
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9F000
    mov eax, KERNEL_DEST
    jmp eax

; ------------------------------------------------------------------
align 8
gdt:
    dq 0
    dq 0x00CF9A000000FFFF       ; 0x08 code, flat 4 GB
    dq 0x00CF92000000FFFF       ; 0x10 data, flat 4 GB
gdt_end:
gdt_desc:
    dw gdt_end - gdt - 1
    dd gdt

align 4
dap:        db 16, 0
dap_count:  dw 0
            dw 0x0000, BUF_SEG
dap_lba:    dq 0

boot_drive: db 0
chunk_secs: dw 0
this_cnt:   dw 0
cur_lba:    dd 0
remaining:  dd 0
dest:       dd 0
this_bytes: dd 0

times 2048-($-$$) db 0
