#pragma once
#include "stdint.h"

typedef uint64_t* page_table_t;

extern page_table_t kernel_pml4;

void vmm_init(void);

// 仮想アドレス va → 物理アドレス pa をマップ
void vmm_map_page(page_table_t pml4,
                  uint64_t va,
                  uint64_t pa,
                  uint64_t flags);
