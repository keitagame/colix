#pragma once
#include "types.h"
#include "arch/x86_64.h"
#include "syscall_num.h"

// ── GDT セレクタ ────────────────────────────────────────
// SYSCALL/SYSRET に合わせた配置:
//  STAR[47:32]=0x08 → syscall:  CS=0x08, SS=0x10
//  STAR[63:48]=0x18 → sysret64: CS=(0x18+16)|3=0x2B, SS=(0x18+8)|3=0x23
#define GDT_NULL         0x00
#define GDT_KERNEL_CODE  0x08   // ring 0 code (64-bit)
#define GDT_KERNEL_DATA  0x10   // ring 0 data
#define GDT_USER_CODE32  0x18   // ring 3 code (32-bit, SYSRET 用プレースホルダ)
#define GDT_USER_DATA    0x20   // ring 3 data  (RPL=3 → 0x23)
#define GDT_USER_CODE64  0x28   // ring 3 code (64-bit, RPL=3 → 0x2B)
#define GDT_TSS          0x30   // TSS (16 バイト記述子)

// ring 3 実行時セレクタ
#define SEL_USER_CODE  (GDT_USER_CODE64 | 3)   // 0x2B
#define SEL_USER_DATA  (GDT_USER_DATA   | 3)   // 0x23

// ── GDT ────────────────────────────────────────────────
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} PACKED gdt_entry_t;

typedef struct {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} PACKED tss_t;

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
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} PACKED idt_entry_t;

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no;
    uint64_t err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} PACKED interrupt_frame_t;

void idt_init(void);
void idt_set_gate(uint8_t num, uint64_t handler, uint8_t ist, uint8_t dpl);

// ── PIC / PIT ───────────────────────────────────────────
void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);

#define PIT_HZ   100
void pit_init(void);

// ── シリアル / VGA ──────────────────────────────────────
#define COM1  0x3F8
void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
void kprintf(const char *fmt, ...);
void panic(const char *msg) NORETURN;

void vga_init(void);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_clear(void);

// ── キーボード ──────────────────────────────────────────
#include "keyboard.h"

// ── 物理メモリマネージャ ────────────────────────────────
#define PMM_BLOCK_SIZE PAGE_SIZE
void  pmm_init(uint64_t mem_base, uint64_t mem_size);
void *pmm_alloc(void);
void  pmm_free(void *p);
uint64_t pmm_free_count(void);
void *pmm_alloc_page(void);
void  pmm_free_page(void *page);
extern uint64_t pmm_base;
extern uint64_t pmm_total_pages;

// ── 仮想メモリマネージャ ────────────────────────────────
typedef uint64_t* page_table_t;
extern page_table_t kernel_pml4;

void         vmm_init(void);
page_table_t vmm_new_pagedir(void);
void         vmm_free_pagedir(page_table_t pml4);
int          vmm_map(page_table_t pml4, uintptr_t virt, uintptr_t phys, uint64_t flags);
void         vmm_unmap(page_table_t pml4, uintptr_t virt);
uintptr_t    vmm_resolve(page_table_t pml4, uintptr_t virt);
void         vmm_switch(page_table_t pml4);
void         vmm_map_page(page_table_t pml4, uint64_t va, uint64_t pa, uint64_t flags);

void *kmalloc(size_t size);
void  kfree(void *p);

// ── ファイル記述子テーブル ──────────────────────────────
#define FD_STDIN   0
#define FD_STDOUT  1
#define FD_STDERR  2
#define MAX_FDS    16

struct initrd_file;  // forward decl
typedef struct {
    bool                    open;
    uint32_t                offset;
    const struct initrd_file *file;   // NULL = stdin/stdout/stderr
} fd_entry_t;
#define USER_STACK_TOP  0x0000000000800000ULL  // 好きな高位アドレス
#define USER_STACK_SIZE 0x0000000000001000ULL  // とりあえず 1 ページ(4KB)
#define PROC_USER_SP_OFFSET  offsetof(process_t, user_sp)
// ── プロセス管理 ────────────────────────────────────────
#define PROC_MAX      64
#define STACK_SIZE    (PAGE_SIZE * 8)
#define KSTACK_SIZE   (PAGE_SIZE * 4)   // 16KB カーネルスタック
#define USER_STACK_TOP  0x7FFFFF000ULL  // ユーザースタック先頭
#define USER_STACK_SIZE (PAGE_SIZE * 8) // 32KB ユーザースタック
struct trapframe64 {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rsi, rdi, rbp, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
};

typedef enum {
    PROC_UNUSED = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_SLEEPING,
    PROC_WAITING,   // waitpid でブロック中
    PROC_ZOMBIE,
} proc_state_t;

typedef struct {
    uint64_t r15, r14, r13, r12;
    uint64_t rbx, rbp;
    uint64_t rip;
    uint64_t rsp;
} cpu_context_t;

typedef struct process {
    pid_t          pid;
    pid_t          ppid;
    proc_state_t   state;
    int            exit_code;
    bool           user_mode;        // ring 3 プロセスか

    page_table_t   pml4;

    cpu_context_t  context;          // カーネルスケジューラコンテキスト
    uint64_t       kernel_rsp;       // カーネルスタックトップ (TSS/SYSCALL 用)
    //uint64_t       user_rsp;         // ユーザースタック初期値

    uint64_t       sleep_ticks;
    pid_t          wait_pid;         // waitpid 待機中の子 PID

    // IPC
    struct ipc_message *msg_queue;
    struct ipc_message *msg_tail;
    uint32_t            msg_count;

    // ファイル記述子
    fd_entry_t     fds[MAX_FDS];
 uint8_t *kstack;
    // ヒープ (sbrk 用)
    uint64_t       brk;
struct trapframe64 *tf;
    char name[16];
    uint64_t cr3;
    bool          is_user;
    bool          started;      // まだ一度もユーザモードで動いてないか

    // ユーザモードでの開始位置
    uint64_t      user_rip;
    uint64_t      user_rsp;
    uint64_t user_sp;
} process_t;

extern process_t  proc_table[PROC_MAX];
extern process_t *current_proc;
extern uint64_t   ticks;

void       proc_init(void);
process_t *proc_create(const char *name, void (*entry)(void), bool user);
process_t *proc_create_user(const char *name, uint64_t entry, uint64_t user_stack_top,
                             page_table_t pml4);
void       proc_exit(int code);
void       proc_yield(void);
void       proc_sleep(uint64_t ms);
void       proc_wakeup(process_t *p);
process_t *proc_find(pid_t pid);
pid_t      proc_waitpid(pid_t pid, int *exit_code_out);

void switch_context(cpu_context_t *old, cpu_context_t *new);
void sched_tick(void);
void schedule(void);

// ユーザーモードエントリ (usermode.asm)
void enter_usermode(uint64_t entry, uint64_t user_rsp) NORETURN;

// ── IPC ────────────────────────────────────────────────
#define IPC_MSG_MAX  256

typedef struct ipc_message {
    pid_t    from;
    uint32_t type;
    uint32_t len;
    uint8_t  data[IPC_MSG_MAX];
    struct ipc_message *next;
} ipc_message_t;

int ipc_send(pid_t dst, uint32_t type, const void *data, uint32_t len);
int ipc_recv(ipc_message_t *out);

// ── システムコール ──────────────────────────────────────
void     syscall_init(void);
uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2,
                         uint64_t a3, uint64_t a4, uint64_t a5);
// SYSCALL asm エントリ
extern void syscall_entry(void);

// ── Per-CPU データ (SYSCALL/SYSRET 用) ─────────────────
typedef struct {
    uint64_t kernel_rsp;      // [gs:0]  ← SYSCALL 時に切り替えるカーネル RSP
    uint64_t user_rsp_save;   // [gs:8]  ← ユーザー RSP 保存領域
} cpu_local_t;
extern cpu_local_t cpu_local;

// ── ELF ローダー ────────────────────────────────────────
// initrd データから ELF を pml4 にロード
// entry_out: エントリポイント, load_base_out: ロードベース
int elf_load(page_table_t pml4, const uint8_t *data, size_t size,
             uint64_t *entry_out);

// ── initrd ─────────────────────────────────────────────
#include "initrd.h"