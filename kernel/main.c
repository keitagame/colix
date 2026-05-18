#include "kernel.h"
#include "initrd.h"
#include "multiboot2.h"
#include "elf.h"
#include "elf_loader.h"

// Multiboot2 マジック
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289

// Multiboot2 情報構造体 (最小限)
typedef struct {
    uint32_t total_size;
    uint32_t reserved;
    // タグが続く
} PACKED mb2_info_t;

typedef struct {
    uint32_t type;
    uint32_t size;
} PACKED mb2_tag_t;

typedef struct {
    uint32_t type;      // 6
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    // mmap entries...
} PACKED mb2_tag_mmap_t;

typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;      // 1 = available
    uint32_t reserved;
} PACKED mb2_mmap_entry_t;

// テストプロセス: 定期的にメッセージを表示
static void proc_hello(void) {
    kprintf("[proc_hello] pid=%d started\n", current_proc->pid);
    int n = 0;
    while (1) {
        kprintf("[proc_hello] tick=%lu count=%d\n", ticks, n++);
        proc_sleep(1000); // 1秒スリープ
        if (n >= 5) break;
    }
    kprintf("[proc_hello] done\n");
}

// テストプロセス: IPCを使って通信
static void proc_sender(void) {
    kprintf("[proc_sender] pid=%d started\n", current_proc->pid);
    // pid=2 (proc_hello) にメッセージを送る
    for (int i = 0; i < 3; i++) {
        proc_sleep(500);
        char msg[] = "hello from sender";
        ipc_send(2, 0x100, msg, sizeof(msg));
        kprintf("[proc_sender] sent msg %d\n", i);
    }
}

// アイドルプロセス (常にREADYな最低優先度)
static void proc_idle(void) {
    while (1) {
        __asm__ volatile("hlt");
        proc_yield();
    }
}

// Multiboot2メモリマップを解析して最大の空き領域を返す
static void parse_mmap(mb2_info_t *info, uint64_t *best_base, uint64_t *best_size) {
    *best_base = 0x100000;  // デフォルト: 1MBから
    *best_size = 32 * 1024 * 1024;  // デフォルト: 32MB

    if (!info || info->total_size < 8) return;

    uint8_t *p = (uint8_t*)info + 8;
    uint8_t *end = (uint8_t*)info + info->total_size;

    while (p < end) {
        mb2_tag_t *tag = (mb2_tag_t*)p;
        if (tag->type == 0) break;  // 終端

        if (tag->type == 6) {  // メモリマップタグ
            mb2_tag_mmap_t *mm = (mb2_tag_mmap_t*)tag;
            mb2_mmap_entry_t *e = (mb2_mmap_entry_t*)((uint8_t*)mm + sizeof(*mm));
            mb2_mmap_entry_t *ee = (mb2_mmap_entry_t*)((uint8_t*)mm + mm->size);

            while (e < ee) {
                kprintf("[MMAP] base=%p len=%luMB type=%d\n",
                        (void*)e->base_addr, e->length / (1024*1024), e->type);
                if (e->type == 1 && e->length > *best_size) {
                    *best_base = e->base_addr;
                    *best_size = e->length;
                }
                e = (mb2_mmap_entry_t*)((uint8_t*)e + mm->entry_size);
            }
        }
        p += ALIGN_UP(tag->size, 8);
    }
}

// カーネルメインエントリポイント
// boot.asm から呼ばれる: rdi=mb_info_ptr, rsi=magic
void kernel_main(uint32_t magic, mb2_info_t *mb_info) {
    // ── 早期初期化 ──────────────────────────────────────
    serial_init();
    vga_init();

    kprintf("====================================\n");
    kprintf("  MicroKernel x86_64 (C)\n");
    kprintf("====================================\n");

    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        kprintf("[WARN] bad multiboot magic: 0x%x (expected 0x%x)\n",
                magic, MULTIBOOT2_BOOTLOADER_MAGIC);
        // QEMUで直接ロードした場合は無視して続行
    } else {
        kprintf("[BOOT] Multiboot2 OK\n");
    }

    // ── アーキテクチャ初期化 ────────────────────────────
    gdt_init();
    idt_init();
    pic_init();
    pit_init();

    // ── メモリ初期化 ────────────────────────────────────
    uint64_t mem_base, mem_size;
    parse_mmap(mb_info, &mem_base, &mem_size);

    // カーネル自身が占有する領域 (1MB + 余裕) をスキップ
    if (mem_base < 0x200000) {
        uint64_t skip = 0x200000 - mem_base;
        if (skip < mem_size) { mem_base += skip; mem_size -= skip; }
        else { mem_base = 0x200000; mem_size = 16 * 1024 * 1024; }
    }

    pmm_init(mem_base, mem_size);
    vmm_init();
    syscall_init();

    // ── プロセス初期化 ──────────────────────────────────
    proc_init();
   
kprintf("[INITRD] start=%p end=%p size=%u\n",
            initrd_start, initrd_end,
            (unsigned)(initrd_end - initrd_start));
uint8_t *mod_start = NULL;
    uint8_t *mod_end   = NULL;

    struct multiboot_tag *tag;

    // mb_info の先頭 8バイトは total_size / reserved
    uint8_t *p = (uint8_t *)mb_info;
    for (tag = (struct multiboot_tag *)(p + 8);
         tag->type != 0;
         tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7))) {

        if (tag->type == MULTIBOOT2_TAG_TYPE_MODULE) {
            struct multiboot_tag_module *m = (struct multiboot_tag_module *)tag;

            mod_start = (uint8_t *)(uintptr_t)m->mod_start;
            mod_end   = (uint8_t *)(uintptr_t)m->mod_end;

            kprintf("[INITRD] module start=%p end=%p size=%u\n",
                    mod_start, mod_end,
                    (unsigned)(mod_end - mod_start));
            break; // 最初の module を initrd とみなす
        }
    }

    if (!mod_start) {
        kprintf("[INITRD] no module found\n");
        
    }
initrd_init(mod_start, mod_end);
    initrd_list();
    
    // アイドルプロセス (PID=1)
    process_t *idle = proc_create("idle", proc_idle, false);
    if (!idle) panic("failed to create idle process");

    // テストプロセス
    process_t *hello = proc_create("hello", proc_hello, false);
    if (!hello) panic("failed to create hello process");

    process_t *sender = proc_create("sender", proc_sender, false);
    if (!sender) panic("failed to create sender process");

elf_load_result_t elf;
if (elf_load_from_initrd("bin/sh", &elf)) {
    process_t *shell = proc_create_elf("sh", elf.entry);
    
    if (!shell) panic("failed to create shell process");
} else {
    kprintf("[KERN] failed to load shell\n");
}

    // ── スケジューラ起動 ────────────────────────────────
    kprintf("[KERN] starting scheduler...\n");

    // 割り込み有効化 (ここからタイマー割り込みが来る)
    current_proc = idle;
    idle->state  = PROC_RUNNING;
    sti();

    // アイドルループ: スケジューラが他のプロセスを動かす
    kprintf("[KERN] entering idle loop\n");
    uint64_t last_stat = 0;
    while (1) {
        __asm__ volatile("hlt");  // 次の割り込みまで省電力待機

        // 5秒ごとにシステム状態を表示
        if (ticks - last_stat >= 5 * PIT_HZ) {
            last_stat = ticks;
            kprintf("[KERN] ticks=%lu free_pages=%lu\n",
                    ticks, pmm_free_count());
        }
    }
}
