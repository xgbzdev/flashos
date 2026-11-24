/* FlashKernel memory layout
 *   0x00100000  kernel code + data (loaded by the boot loader)
 *   0x00500000  .bss (zeroed by entry.asm)
 *   0x00800000  heap (24 MB)
 *   0x02000000  FlashFS archive (loaded by the boot loader)
 */
ENTRY(_start)
SECTIONS
{
    . = 0x100000;
    .text : { *(.text.entry) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    _kernel_end = .;

    . = 0x500000;
    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*) *(COMMON)
        _bss_end = .;
    }
    /DISCARD/ : { *(.eh_frame*) *(.note*) *(.comment) }
}
