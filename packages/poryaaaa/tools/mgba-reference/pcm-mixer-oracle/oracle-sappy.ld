/* Deterministic layout for pinned vanilla Sappy reference fixtures. */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(fixture_entry)

MEMORY
{
    ROM   (rx)  : ORIGIN = 0x08000000, LENGTH = 0x00100000
    EWRAM (rwx) : ORIGIN = 0x02000000, LENGTH = 0x00040000
    IWRAM (rwx) : ORIGIN = 0x03000000, LENGTH = 0x00008000
}

SECTIONS
{
    .text.header 0x08000000 : ALIGN(4)
    {
        KEEP(*(.text.header))
    } > ROM

    .text : ALIGN(4)
    {
        *(.text.fixture)
        *(.rodata*)
    } > ROM

    .fixture_case : ALIGN(4)
    {
        KEEP(*(.fixture_case))
    } > ROM

    .iwram_mixer : ALIGN(4)
    {
        __mixer_start = .;
        *(.iwram_mixer)
        __mixer_end = .;
    } > IWRAM AT > ROM
    __mixer_load_start = LOADADDR(.iwram_mixer);

    .data : ALIGN(4)
    {
        *(.data*)
    } > EWRAM AT > ROM

    .bss (NOLOAD) : ALIGN(4)
    {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > EWRAM

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.ARM.attributes)
    }
}
