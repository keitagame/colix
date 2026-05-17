#include "kernel.h"

// GDT: null, kernel code, kernel data, user code, user data, TSS (2エントリ)
#define GDT_ENTRIES 7

static gdt_entry_t     gdt[GDT_ENTRIES];
static gdt_tss_entry_t gdt_tss;   // TSSは16バイト記述子
static tss_t           tss;
static gdtr_t          gdtr;

static void set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access      = access;
}

void gdt_init(void) {
    // 0: null
    set_entry(0, 0, 0, 0, 0);

    // 1: kernel code (64bit) - 0x08
    // access: Present|S|Execute|Read
    // gran  : 64bit flag (L=1)
    set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);

    // 2: kernel data - 0x10
    set_entry(2, 0, 0xFFFFF, 0x92, 0xC0);

    // 3: user code (64bit) - 0x18  (DPL=3)
    set_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);

    // 4: user data - 0x20  (DPL=3)
    set_entry(4, 0, 0xFFFFF, 0xF2, 0xC0);

    // 5-6: TSS (16バイト記述子)
    // GDTの[5]と[6]の位置に配置
    // TSSの初期化
    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;

    gdt_tss.limit_low   = tss_limit & 0xFFFF;
    gdt_tss.base_low    = tss_base & 0xFFFF;
    gdt_tss.base_mid1   = (tss_base >> 16) & 0xFF;
    gdt_tss.access      = 0x89;  // Present | TSS available
    gdt_tss.flags       = ((tss_limit >> 16) & 0x0F);
    gdt_tss.base_mid2   = (tss_base >> 24) & 0xFF;
    gdt_tss.base_high   = (tss_base >> 32) & 0xFFFFFFFF;
    gdt_tss.reserved    = 0;

    // GDTの末尾にTSSを配置 (gdt[5] と gdt[6] の位置に16バイト)
    // 構造体をそのままコピー
    uint8_t *dst = (uint8_t*)&gdt[5];
    uint8_t *src = (uint8_t*)&gdt_tss;
    for (size_t i = 0; i < sizeof(gdt_tss); i++) dst[i] = src[i];

    // GDTRを設定
    // GDT全体のサイズ = gdt[5] + 16バイト = 5*8 + 16 = 56バイト
    gdtr.limit = 5 * sizeof(gdt_entry_t) + sizeof(gdt_tss_entry_t) - 1;
    gdtr.base  = (uint64_t)gdt;

    lgdt(&gdtr);

    // セグメントレジスタのリロード
    __asm__ volatile(
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "pushq $0x08\n\t"
        "lea 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        ::: "rax", "memory"
    );

    // TSSロード (セレクタ = 5*8 = 0x28)
    ltr(0x28);

    kprintf("[GDT] initialized (%d entries)\n", 7);
}

void gdt_set_tss_rsp0(uint64_t rsp0) {
    tss.rsp[0] = rsp0;
}
