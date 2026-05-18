#include "kernel.h"
// x86_64 4段ページング: PML4 -> PDP -> PD -> PT -> 物理アドレス
// 各テーブルは512エントリ、エントリは8バイト
#include "pmm.h"
#include "vmm.h"
// 仮想アドレスのインデックス抽出
#define PML4_IDX(va)  (((va) >> 39) & 0x1FF)
#define PDP_IDX(va)   (((va) >> 30) & 0x1FF)
#define PD_IDX(va)    (((va) >> 21) & 0x1FF)
#define PT_IDX(va)    (((va) >> 12) & 0x1FF)

// エントリから物理アドレスを取得
#define PTE_PHYS(e)   ((e) & PTE_ADDR_MASK)
#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITE    (1ULL << 1)
// カーネルのPML4 (boot.asm が作ったものを置き換え)
page_table_t kernel_pml4;
// ページテーブル用の静的プール（4KB × 64 = 256KB）
static uint8_t pt_pool[PAGE_SIZE * 64] __attribute__((aligned(4096)));
static size_t  pt_pool_used = 0;

// ページテーブルを1段確保・初期化
/*static uint64_t *alloc_table(void) {
    uint64_t *t = pmm_alloc();
    if (!t) panic("vmm: out of memory for page table");
    return t;
}*/
static uint64_t *alloc_table(void) {
    // まずはカーネル .bss 内の静的プールから確保
    if (pt_pool_used + PAGE_SIZE <= sizeof(pt_pool)) {
        uint64_t *t = (uint64_t*)(pt_pool + pt_pool_used);
        pt_pool_used += PAGE_SIZE;

        // ゼロクリア
        for (size_t j = 0; j < PAGE_SIZE / 8; j++) {
            t[j] = 0;
        }
        return t;
    }

    // プールが尽きたら物理メモリから確保（この時点では pmm_base もマップ済み想定）
    uint64_t *t = pmm_alloc();
    if (!t) panic("vmm: out of memory for page table");
    return t;
}
void vmm_map_user_stack(process_t *p, uint64_t base, uint64_t size) {
    for (uint64_t off = 0; off < size; off += 0x1000) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) panic("no memory for user stack");

        vmm_map_page(
            p->cr3,                 // プロセスのページテーブル
            base + off,             // 仮想アドレス
            phys,                   // 物理アドレス
            PAGE_WRITE  // ★ユーザ権限
        );
    }
}

// 指定PML4にマッピングを追加
int vmm_map(page_table_t pml4, uintptr_t virt, uintptr_t phys, uint64_t flags) {
    // flags は PTE_PRESENT | PTE_WRITABLE | PTE_USER など
    uint64_t p4i = PML4_IDX(virt);
    uint64_t p3i = PDP_IDX(virt);
    uint64_t p2i = PD_IDX(virt);
    uint64_t p1i = PT_IDX(virt);

    // PML4 -> PDP
    if (!(pml4[p4i] & PTE_PRESENT)) {
        uint64_t *pdp = alloc_table();
        pml4[p4i] = (uintptr_t)pdp | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    }
    uint64_t *pdp = (uint64_t*)PTE_PHYS(pml4[p4i]);

    // PDP -> PD
    if (!(pdp[p3i] & PTE_PRESENT)) {
        uint64_t *pd = alloc_table();
        pdp[p3i] = (uintptr_t)pd | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    }
    uint64_t *pd = (uint64_t*)PTE_PHYS(pdp[p3i]);

    // PD -> PT
    if (!(pd[p2i] & PTE_PRESENT)) {
        uint64_t *pt = alloc_table();
        pd[p2i] = (uintptr_t)pt | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    }
    uint64_t *pt = (uint64_t*)PTE_PHYS(pd[p2i]);

    // PT エントリ設定
    pt[p1i] = (phys & PTE_ADDR_MASK) | (flags | PTE_PRESENT);
    tlb_flush(virt);
    return 0;
}

void vmm_unmap(page_table_t pml4, uintptr_t virt) {
    uint64_t p4i = PML4_IDX(virt);
    uint64_t p3i = PDP_IDX(virt);
    uint64_t p2i = PD_IDX(virt);
    uint64_t p1i = PT_IDX(virt);

    if (!(pml4[p4i] & PTE_PRESENT)) return;
    uint64_t *pdp = (uint64_t*)PTE_PHYS(pml4[p4i]);
    if (!(pdp[p3i] & PTE_PRESENT)) return;
    uint64_t *pd  = (uint64_t*)PTE_PHYS(pdp[p3i]);
    if (!(pd[p2i] & PTE_PRESENT)) return;
    uint64_t *pt  = (uint64_t*)PTE_PHYS(pd[p2i]);

    pt[p1i] = 0;
    tlb_flush(virt);
}

uintptr_t vmm_resolve(page_table_t pml4, uintptr_t virt) {
    uint64_t p4i = PML4_IDX(virt);
    uint64_t p3i = PDP_IDX(virt);
    uint64_t p2i = PD_IDX(virt);
    uint64_t p1i = PT_IDX(virt);

    if (!(pml4[p4i] & PTE_PRESENT)) return 0;
    uint64_t *pdp = (uint64_t*)PTE_PHYS(pml4[p4i]);
    if (!(pdp[p3i] & PTE_PRESENT)) return 0;
    uint64_t *pd  = (uint64_t*)PTE_PHYS(pdp[p3i]);
    if (!(pd[p2i] & PTE_PRESENT)) return 0;
    uint64_t *pt  = (uint64_t*)PTE_PHYS(pd[p2i]);
    if (!(pt[p1i] & PTE_PRESENT)) return 0;

    return PTE_PHYS(pt[p1i]) | (virt & 0xFFF);
}

void vmm_switch(page_table_t pml4) {
    write_cr3((uint64_t)pml4);
}

// 新しいページディレクトリを生成 (カーネル空間をコピー)
page_table_t vmm_new_pagedir(void) {
    page_table_t pml4 = alloc_table();
    // カーネル領域のPML4エントリをコピー
    // PML4の上半分 (256-511) = カーネル空間
    if (kernel_pml4) {
        for (int i = 256; i < 512; i++)
            pml4[i] = kernel_pml4[i];
    }
    return pml4;
}

// PML4と下位テーブル (ユーザー空間のみ) を解放
void vmm_free_pagedir(page_table_t pml4) {
    // ユーザー空間のみ解放 (0-255)
    for (int p4 = 0; p4 < 256; p4++) {
        if (!(pml4[p4] & PTE_PRESENT)) continue;
        uint64_t *pdp = (uint64_t*)PTE_PHYS(pml4[p4]);
        for (int p3 = 0; p3 < 512; p3++) {
            if (!(pdp[p3] & PTE_PRESENT)) continue;
            uint64_t *pd = (uint64_t*)PTE_PHYS(pdp[p3]);
            for (int p2 = 0; p2 < 512; p2++) {
                if (!(pd[p2] & PTE_PRESENT)) continue;
                pmm_free((void*)PTE_PHYS(pd[p2]));
            }
            pmm_free(pd);
        }
        pmm_free(pdp);
    }
    pmm_free(pml4);
}

// ── カーネルヒープ (簡易バンプアロケータ) ──────────────
// boot.asm がアイデンティティマップした物理メモリ内で動作
// 将来: slab/buddy allocatorに置き換え可能

#define KHEAP_START 0x400000ULL  // 4MB開始
#define KHEAP_SIZE  (4 * 1024 * 1024)  // 4MB
static uint64_t heap_ptr = KHEAP_START;

// 簡易free-listヘッダ
typedef struct heap_block {
    size_t size;
    bool   free;
    struct heap_block *next;
} heap_block_t;

static heap_block_t *heap_head = NULL;

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    size = ALIGN_UP(size, 16);  // 16バイトアライン

    // 空きブロックを探す (first-fit)
    heap_block_t *blk = heap_head;
    while (blk) {
        if (blk->free && blk->size >= size) {
            blk->free = false;
            return (void*)(blk + 1);
        }
        blk = blk->next;
    }

    // 新規割り当て
    size_t total = sizeof(heap_block_t) + size;
    if (heap_ptr + total > KHEAP_START + KHEAP_SIZE)
        return NULL;  // ヒープ枯渇

    blk = (heap_block_t*)heap_ptr;
    heap_ptr += total;
    blk->size = size;
    blk->free = false;
    blk->next = heap_head;
    heap_head = blk;
    return (void*)(blk + 1);
}

void kfree(void *ptr) {
    if (!ptr) return;
    heap_block_t *blk = (heap_block_t*)ptr - 1;
    blk->free = true;
}
/*
void vmm_init(void) {
    // 現在のCR3をカーネルPML4として保存
    kernel_pml4 = (page_table_t)read_cr3();
    kprintf("[VMM] kernel PML4=%p\n", (void*)kernel_pml4);
    kprintf("[VMM] heap %p - %p\n",
            (void*)KHEAP_START, (void*)(KHEAP_START + KHEAP_SIZE));
}*/
void vmm_init(void) {
    // すでに bootloader が作った PML4 が CR3 に入っている
    kernel_pml4 = (page_table_t)read_cr3();

    kprintf("[VMM] kernel PML4=%p\n", (void*)kernel_pml4);
    kprintf("[VMM] heap %p - %p\n",
            (void*)KHEAP_START, (void*)(KHEAP_START + KHEAP_SIZE));
    uint64_t pmm_end = pmm_base + pmm_total_pages * PAGE_SIZE;
    for (uint64_t va = pmm_base; va < pmm_end; va += PAGE_SIZE) {
        vmm_map_page(kernel_pml4, va, va, PAGE_PRESENT | PAGE_WRITE);
    }

    // 2. ヒープ領域を物理ページに割り当ててマップ
    for (uint64_t va = KHEAP_START; va < KHEAP_START + KHEAP_SIZE; va += PAGE_SIZE) {
        void *pa = pmm_alloc_page();
        if (!pa) panic("vmm_init: pmm_alloc_page failed");

        vmm_map_page(kernel_pml4, va, (uint64_t)pa,
                     PAGE_PRESENT | PAGE_WRITE);
    }
    

    

    // ============================================================
    // 3. CR3 を再ロード（TLB flush）
    // ============================================================
    write_cr3((uint64_t)kernel_pml4);
}
void vmm_map_page(page_table_t pml4,
                  uint64_t va,
                  uint64_t pa,
                  uint64_t flags)
{
    vmm_map(pml4, va, pa, flags);
}
