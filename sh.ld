ENTRY(_start)

SECTIONS {
    . = 0x400000;   /* ユーザ空間のロード先（あなたの ELF ローダに合わせる） */

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(.bss*)
        *(COMMON)
    }
}
