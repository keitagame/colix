#pragma once
#include "../types.h"

// ページテーブルエントリのフラグ
#define PTE_PRESENT    BIT(0)
#define PTE_WRITABLE   BIT(1)
#define PTE_USER       BIT(2)
#define PTE_WRITETHROUGH BIT(3)
#define PTE_NOCACHE    BIT(4)
#define PTE_ACCESSED   BIT(5)
#define PTE_DIRTY      BIT(6)
#define PTE_HUGE       BIT(7)    // PD: 2MB page
#define PTE_GLOBAL     BIT(8)
#define PTE_NX         BIT(63)   // No Execute

#define PTE_ADDR_MASK  0x000FFFFFFFFFF000ULL

// 仮想アドレス空間レイアウト
#define KERNEL_BASE    0xFFFFFFFF80000000ULL  // カーネルは-2GBにリンク
#define KERNEL_HEAP    0xFFFFFF8000000000ULL  // カーネルヒープ
#define USER_MAX       0x0000800000000000ULL  // ユーザー空間の上限

// GDTセレクタ
#define GDT_NULL       0x00
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE  0x1B   // | RPL=3
#define GDT_USER_DATA  0x23   // | RPL=3
#define GDT_TSS        0x28

// MSRアドレス
#define MSR_EFER       0xC0000080
#define MSR_STAR       0xC0000081
#define MSR_LSTAR      0xC0000082
#define MSR_SFMASK     0xC0000084
#define MSR_FS_BASE    0xC0000100
#define MSR_GS_BASE    0xC0000101

// EFERビット
#define EFER_SCE       BIT(0)   // syscall enable
#define EFER_LME       BIT(8)   // long mode enable
#define EFER_LMA       BIT(10)  // long mode active
#define EFER_NXE       BIT(11)  // NX enable

// RFLAGS
#define RFLAGS_CF      BIT(0)
#define RFLAGS_PF      BIT(2)
#define RFLAGS_AF      BIT(4)
#define RFLAGS_ZF      BIT(6)
#define RFLAGS_SF      BIT(7)
#define RFLAGS_TF      BIT(8)
#define RFLAGS_IF      BIT(9)   // 割り込み有効
#define RFLAGS_DF      BIT(10)
#define RFLAGS_OF      BIT(11)
#define RFLAGS_IOPL    (BIT(12)|BIT(13))

// 割り込みベクタ
#define IDT_DIVIDE_ERROR    0
#define IDT_DEBUG           1
#define IDT_NMI             2
#define IDT_BREAKPOINT      3
#define IDT_OVERFLOW        4
#define IDT_BOUND           5
#define IDT_INVALID_OPCODE  6
#define IDT_DEVICE_NA       7
#define IDT_DOUBLE_FAULT    8
#define IDT_INVALID_TSS     10
#define IDT_SEGMENT_NP      11
#define IDT_STACK_FAULT     12
#define IDT_GPF             13
#define IDT_PAGE_FAULT      14
#define IDT_FPU_ERROR       16
#define IDT_ALIGNMENT       17
#define IDT_MACHINE_CHECK   18
#define IDT_SIMD_ERROR      19
#define IDT_SYSCALL         0x80
#define IDT_IRQ_BASE        0x20  // PIC IRQ0 = ベクタ 0x20
#define IDT_TIMER           (IDT_IRQ_BASE + 0)
#define IDT_KEYBOARD        (IDT_IRQ_BASE + 1)

// IRQマスクなし
#define PIC_MASTER_CMD  0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_CMD   0xA0
#define PIC_SLAVE_DATA  0xA1

// I/Oポート
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port) : "memory");
}
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" :: "a"(val), "Nd"(port) : "memory");
}
static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}
static inline void io_wait(void) { outb(0x80, 0); }

// CR操作
static inline uint64_t read_cr0(void) {
    uint64_t v; __asm__ volatile("mov %%cr0, %0" : "=r"(v)); return v;
}
static inline void write_cr0(uint64_t v) {
    __asm__ volatile("mov %0, %%cr0" :: "r"(v) : "memory");
}
static inline uint64_t read_cr2(void) {
    uint64_t v; __asm__ volatile("mov %%cr2, %0" : "=r"(v)); return v;
}
static inline uint64_t read_cr3(void) {
    uint64_t v; __asm__ volatile("mov %%cr3, %0" : "=r"(v)); return v;
}
static inline void write_cr3(uint64_t v) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(v) : "memory");
}
static inline uint64_t read_cr4(void) {
    uint64_t v; __asm__ volatile("mov %%cr4, %0" : "=r"(v)); return v;
}
static inline void write_cr4(uint64_t v) {
    __asm__ volatile("mov %0, %%cr4" :: "r"(v) : "memory");
}

// MSR操作
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t val) {
    __asm__ volatile("wrmsr" :: "c"(msr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32)));
}

// 割り込み制御
static inline void cli(void) { __asm__ volatile("cli" ::: "memory"); }
static inline void sti(void) { __asm__ volatile("sti" ::: "memory"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }
static inline uint64_t save_flags(void) {
    uint64_t f;
    __asm__ volatile("pushfq; popq %0" : "=r"(f) :: "memory");
    return f;
}
static inline void restore_flags(uint64_t f) {
    __asm__ volatile("pushq %0; popfq" :: "r"(f) : "memory", "cc");
}
static inline void tlb_flush_all(void) { write_cr3(read_cr3()); }
static inline void tlb_flush(uintptr_t addr) {
    __asm__ volatile("invlpg (%0)" :: "r"(addr) : "memory");
}
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// GDT/IDT記述子
typedef struct { uint16_t limit; uint64_t base; } PACKED gdtr_t;
typedef struct { uint16_t limit; uint64_t base; } PACKED idtr_t;

static inline void lgdt(gdtr_t *p) {
    __asm__ volatile("lgdt %0" :: "m"(*p) : "memory");
}
static inline void lidt(idtr_t *p) {
    __asm__ volatile("lidt %0" :: "m"(*p) : "memory");
}
static inline void ltr(uint16_t sel) {
    __asm__ volatile("ltr %0" :: "r"(sel));
}
