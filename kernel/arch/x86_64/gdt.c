#include "kernel.h"

// GDT レイアウト (SYSCALL/SYSRET の STAR MSR 要件に合わせる):
//  STAR[47:32] = 0x08  syscall  → CS=0x08(kernel code), SS=0x10(kernel data)
//  STAR[63:48] = 0x18  sysret64 → CS=(0x18+16)|3=0x2B, SS=(0x18+8)|3=0x23
//
// [0] 0x00  null
// [1] 0x08  kernel code 64-bit
// [2] 0x10  kernel data
// [3] 0x18  user code 32-bit  (SYSRET 互換用プレースホルダ)
// [4] 0x20  user data          (RPL=3 → 0x23)
// [5] 0x28  user code 64-bit  (RPL=3 → 0x2B)
// [6-7] 0x30 TSS 記述子 (16 バイト)

#define GDT_NORMAL_ENTRIES 6

static gdt_entry_t     gdt[GDT_NORMAL_ENTRIES];
static gdt_tss_entry_t gdt_tss;
static tss_t           tss;
static gdtr_t          gdtr;

static void set_entry(int i, uint32_t base, uint32_t limit,
                      uint8_t access, uint8_t gran) {
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

    // 1: kernel code 64-bit (0x08) - Present|S|Execute|Read, L=1
    set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);

    // 2: kernel data (0x10) - Present|S|Write
    set_entry(2, 0, 0xFFFFF, 0x92, 0xC0);

    // 3: user code 32-bit (0x18, DPL=3) - SYSRET プレースホルダ
    set_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);

    // 4: user data (0x20, DPL=3)
    set_entry(4, 0, 0xFFFFF, 0xF2, 0xC0);

    // 5: user code 64-bit (0x28, DPL=3) - L=1
    set_entry(5, 0, 0xFFFFF, 0xFA, 0xA0);

    // TSS 記述子 (16 バイト) → GDT の末尾 [6-7] に配置
    uint64_t tss_base  = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;

    gdt_tss.limit_low  = tss_limit & 0xFFFF;
    gdt_tss.base_low   = tss_base & 0xFFFF;
    gdt_tss.base_mid1  = (tss_base >> 16) & 0xFF;
    gdt_tss.access     = 0x89;   // Present | TSS Available
    gdt_tss.flags      = ((tss_limit >> 16) & 0x0F);
    gdt_tss.base_mid2  = (tss_base >> 24) & 0xFF;
    gdt_tss.base_high  = (uint32_t)(tss_base >> 32);
    gdt_tss.reserved   = 0;

    // TSS を gdt[6] の位置に 16 バイトコピー
    uint8_t *dst = (uint8_t *)&gdt[GDT_NORMAL_ENTRIES]; // = &gdt[6]
    uint8_t *src = (uint8_t *)&gdt_tss;
    for (size_t i = 0; i < sizeof(gdt_tss); i++) dst[i] = src[i];

    // GDTR: 6×8 + 16 = 64 バイト
    gdtr.limit = GDT_NORMAL_ENTRIES * sizeof(gdt_entry_t)
               + sizeof(gdt_tss_entry_t) - 1;
    gdtr.base  = (uint64_t)gdt;

    lgdt(&gdtr);

    // セグメントレジスタ再ロード
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

    // TSS ロード (セレクタ 0x30)
    ltr(0x30);

    kprintf("[GDT] initialized (kcode=0x08 kdata=0x10 "
            "udata=0x23 ucode64=0x2B TSS=0x30)\n");
}

void gdt_set_tss_rsp0(uint64_t rsp0) {
    tss.rsp[0] = rsp0;
}