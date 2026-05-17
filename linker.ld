/* linker.ld - x86_64マイクロカーネル リンカスクリプト */

ENTRY(_start)

/* 物理アドレス 1MB から配置 */
KERNEL_PHYS = 0x100000;

SECTIONS
{
    . = KERNEL_PHYS;

    /* Multiboot2 ヘッダ - 先頭に配置 */
    .multiboot2 :
    {
        KEEP(*(.multiboot2))
    }

    /* テキストセクション (実行コード) */
    .text ALIGN(4096) :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    }

    /* 初期化済みデータ */
    .data ALIGN(4096) :
    {
        *(.data)
        *(.data.*)
    }

    /* 未初期化データ (ゼロクリア) */
    .bss ALIGN(4096) :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* カーネル終端 */
    kernel_end = .;
    .initrd ALIGN(4096) :
    {
        initrd_start = .;
        KEEP(*(.initrd))   /* GRUB module2 をここに置く */
        initrd_end = .;
    }
    /* デバッグ情報は削除 */
    /DISCARD/ :
    {
        *(.eh_frame)
        *(.note*)
        *(.comment)
    }
}
