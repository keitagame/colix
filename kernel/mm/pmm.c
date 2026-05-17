#include "kernel.h"

// ビットマップベースの物理メモリマネージャ
// 最大512MB / 4KB = 131072ページ → 16KBのビットマップ
#define PMM_MAX_PAGES (512 * 1024 * 1024 / PAGE_SIZE)
#define BITMAP_SIZE   (PMM_MAX_PAGES / 64)  // uint64_t単位

static uint64_t bitmap[BITMAP_SIZE];
uint64_t pmm_base;
uint64_t pmm_total_pages;
static uint64_t pmm_free_pages;
static uint64_t pmm_last_alloc;  // 次回探索の開始位置 (高速化)

static void pmm_set_used(uint64_t page) {
    bitmap[page / 64] |= (1ULL << (page % 64));
}
static void pmm_set_free(uint64_t page) {
    bitmap[page / 64] &= ~(1ULL << (page % 64));
}
static bool pmm_is_used(uint64_t page) {
    return (bitmap[page / 64] & (1ULL << (page % 64))) != 0;
}

void pmm_init(uint64_t mem_base, uint64_t mem_size) {
    pmm_base = PAGE_ALIGN(mem_base);
    pmm_total_pages = mem_size / PAGE_SIZE;
    if (pmm_total_pages > PMM_MAX_PAGES)
        pmm_total_pages = PMM_MAX_PAGES;

    // 全ページを使用中にする
    for (uint64_t i = 0; i < BITMAP_SIZE; i++) bitmap[i] = ~0ULL;

    // 有効なページを空きに
    for (uint64_t i = 0; i < pmm_total_pages; i++) pmm_set_free(i);

    pmm_free_pages = pmm_total_pages;
    pmm_last_alloc = 0;

    kprintf("[PMM] base=%p pages=%lu (%luMB)\n",
            (void*)pmm_base, pmm_total_pages, pmm_total_pages * PAGE_SIZE / (1024*1024));
}

void *pmm_alloc(void) {
    kprintf("[PMM] alloc: base=%p total=%lu free=%lu\n",
            (void*)pmm_base, pmm_total_pages, pmm_free_pages);
    // last_alloc から線形スキャン (最初の空きビットを探す)
    for (uint64_t i = 0; i < pmm_total_pages; i++) {
        uint64_t page = (pmm_last_alloc + i) % pmm_total_pages;
        if (!pmm_is_used(page)) {
            pmm_set_used(page);
            pmm_free_pages--;
            pmm_last_alloc = (page + 1) % pmm_total_pages;
            void *addr = (void*)(pmm_base + page * PAGE_SIZE);
            // ゼロクリア
            //uint64_t *p = (uint64_t*)addr;
            //for (size_t j = 0; j < PAGE_SIZE / 8; j++) p[j] = 0;
            return addr;
        }
    }
    return NULL; // out of memory
}

void pmm_free(void *ptr) {
    if (!ptr) return;
    uint64_t addr = (uint64_t)ptr;
    if (addr < pmm_base) return;
    uint64_t page = (addr - pmm_base) / PAGE_SIZE;
    if (page >= pmm_total_pages) return;
    if (!pmm_is_used(page)) return; // double free 検出
    pmm_set_free(page);
    pmm_free_pages++;
}

uint64_t pmm_free_count(void) { return pmm_free_pages; }

void* pmm_alloc_page(void) {
    return pmm_alloc();   // 既存の pmm_alloc をそのまま使う
}

void pmm_free_page(void* page) {
    pmm_free(page);       // 既存の pmm_free をそのまま使う
}
