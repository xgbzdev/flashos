/* FlashOS stage 2: flat real-mode binary at 0000:9000 (must stay below 0x10000) */
ENTRY(_s2start)
SECTIONS
{
    . = 0x9000;
    .text : { *(.text.entry) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) *(.bss*) *(COMMON) }
    _s2_end = .;
    /DISCARD/ : { *(.eh_frame*) *(.note*) *(.comment) }
}
