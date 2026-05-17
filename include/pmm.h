#pragma once
#include "stdint.h"

// 1ページ = 4096 bytes
//#define PAGE_SIZE 4096

// 物理メモリ管理の初期化
void pmm_init(uint64_t base, uint64_t size);

// 1ページ確保（4096バイト）
void* pmm_alloc_page(void);

// ページを解放（必要なら）
void pmm_free_page(void* page);

// デバッグ用：空きページ数
uint64_t pmm_free_count(void);

extern uint64_t pmm_base;
extern uint64_t pmm_total_pages;
