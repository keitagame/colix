#pragma once
#include "types.h"
#include "arch/x86_64.h"

// ── GDT ────────────────────────────────────────────────
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;   // limit_high | flags
    uint8_t  base_high;
} PACKED gdt_entry_t;

// TSS (Task State Segment) 64bit版
typedef struct {
    uint32_t reserved0;
    uint64_t rsp[3];        // RSP0-2
    uint64_t reserved1;
    uint64_t ist[7];        // IST1-7
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} PACKED tss_t;

// TSS用の16バイトGDT記述子
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid1;
    uint8_t  access;
    uint8_t  flags;
    uint8_t  base_mid2;
    uint32_t base_high;
    uint32_t reserved;
} PACKED gdt_tss_entry_t;

void gdt_init(void);
void gdt_set_tss_rsp0(uint64_t rsp0);

// ── IDT ────────────────────────────────────────────────
typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;           // IST index (0 = disabled)
    uint8_t  type_attr;     // type | DPL | Present
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} PACKED idt_entry_t;

// 割り込みフレーム (スタックに積まれる)
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no;        // 割り込み番号
    uint64_t err_code;      // エラーコード (なければ0)
    uint64_t rip, cs, rflags, rsp, ss;
} PACKED interrupt_frame_t;

void idt_init(void);
void idt_set_gate(uint8_t num, uint64_t handler, uint8_t ist, uint8_t dpl);

// ── PIC ────────────────────────────────────────────────
void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);

// ── PIT (タイマー) ──────────────────────────────────────
#define PIT_HZ   100        // 10ms間隔
void pit_init(void);

// ── シリアル (デバッグ出力) ─────────────────────────────
#define COM1  0x3F8
void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
void kprintf(const char *fmt, ...);
void panic(const char *msg) NORETURN;

// ── 物理メモリマネージャ ────────────────────────────────
#define PMM_BLOCK_SIZE PAGE_SIZE

void  pmm_init(uint64_t mem_base, uint64_t mem_size);
void *pmm_alloc(void);       // 4KBブロックを1つ確保
void  pmm_free(void *p);     // 返却
uint64_t pmm_free_count(void);

// ── 仮想メモリマネージャ ────────────────────────────────
typedef uint64_t* page_table_t;  // PML4のポインタ

page_table_t vmm_new_pagedir(void);
void         vmm_free_pagedir(page_table_t pml4);
int          vmm_map(page_table_t pml4, uintptr_t virt, uintptr_t phys, uint64_t flags);
void         vmm_unmap(page_table_t pml4, uintptr_t virt);
uintptr_t    vmm_resolve(page_table_t pml4, uintptr_t virt);
void         vmm_switch(page_table_t pml4);
void         vmm_init(void);

// カーネルヒープ (slab風簡易版)
void *kmalloc(size_t size);
void  kfree(void *p);

// ── プロセス管理 ────────────────────────────────────────
#define PROC_MAX      64
#define STACK_SIZE    (PAGE_SIZE * 8)  // 32KB
#define KSTACK_SIZE   (PAGE_SIZE * 2)  // 8KB

typedef enum {
    PROC_UNUSED = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_SLEEPING,
    PROC_ZOMBIE,
} proc_state_t;

// CPU レジスタコンテキスト (スケジューラが保存)
typedef struct {
    uint64_t r15, r14, r13, r12;
    uint64_t rbx, rbp;
    uint64_t rip;           // リターン先 (switch_context のret)
    uint64_t rsp;
} cpu_context_t;

typedef struct process {
    pid_t          pid;
    pid_t          ppid;
    proc_state_t   state;
    int            exit_code;

    page_table_t   pml4;           // ページディレクトリ

    cpu_context_t  context;        // スケジューラコンテキスト
    uint64_t       kernel_rsp;     // カーネルスタックトップ (TSS用)
    uint64_t       user_rsp;       // ユーザースタックトップ (未使用はカーネルのみ)

    uint64_t       sleep_ticks;    // スリープ残りtick数

    // IPC
    struct ipc_message *msg_queue; // 受信メッセージキュー先頭
    struct ipc_message *msg_tail;  // キュー末尾
    uint32_t            msg_count;

    char name[16];
} process_t;

extern process_t  proc_table[PROC_MAX];
extern process_t *current_proc;
extern uint64_t   ticks;

void proc_init(void);
process_t *proc_create(const char *name, void (*entry)(void), bool user);
void proc_exit(int code);
void proc_yield(void);
void proc_sleep(uint64_t ms);
void proc_wakeup(process_t *p);
process_t *proc_find(pid_t pid);

// コンテキストスイッチ (arch/context.asm)
void switch_context(cpu_context_t *old, cpu_context_t *new);

// スケジューラ
void sched_tick(void);      // タイマー割り込みから呼ぶ
void schedule(void);        // 次プロセスを選択して切り替え

// ── IPC ────────────────────────────────────────────────
#define IPC_MSG_MAX  256    // メッセージ本文最大バイト

typedef struct ipc_message {
    pid_t    from;
    uint32_t type;
    uint32_t len;
    uint8_t  data[IPC_MSG_MAX];
    struct ipc_message *next;
} ipc_message_t;

int ipc_send(pid_t dst, uint32_t type, const void *data, uint32_t len);
int ipc_recv(ipc_message_t *out);   // ブロッキング

// ── システムコール ──────────────────────────────────────
// 番号定義 (Linux互換に近い)
#define SYS_EXIT       1
#define SYS_FORK       2
#define SYS_READ       3
#define SYS_WRITE      4
#define SYS_GETPID     20
#define SYS_GETPPID    64
#define SYS_SLEEP      162   // nanosleep相当
#define SYS_YIELD      24
#define SYS_IPC_SEND   200
#define SYS_IPC_RECV   201

void syscall_init(void);
uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2,
                         uint64_t a3, uint64_t a4, uint64_t a5);

// ── VGA テキスト ────────────────────────────────────────
void vga_init(void);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_clear(void);
