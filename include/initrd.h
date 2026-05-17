#pragma once
#include "stdint.h"
#include "stddef.h"
#include "types.h"
// initrd 内のファイルエントリ
typedef struct {
    const char   *name;   // NUL終端パス
    uint32_t      size;   // バイト数
    const uint8_t *data;  // ファイル本体先頭
} initrd_file_t;

// initrd.c が提供する API
void initrd_init(uint8_t *start, uint8_t *end);
const initrd_file_t *initrd_find(const char *name);
size_t initrd_read(const initrd_file_t *f, size_t off, void *buf, size_t len);
void initrd_list(void);

// Multiboot2 module から渡される領域
extern uint8_t initrd_start[];
extern uint8_t initrd_end[];
