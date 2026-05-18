#pragma once
#include "types.h"

// カーネルへ渡すブート情報 (Multiboot2 / EFI 両対応)
#define BOOT_INFO_MAGIC  0xB007B007ULL

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;     // 1=利用可能, 2=予約済み, 3=ACPI, 4=NVS, 5=不良
    uint32_t _pad;
} boot_mmap_entry_t;

#define MMAP_USABLE     1
#define MMAP_RESERVED   2
#define MMAP_ACPI_RECL  3
#define MMAP_ACPI_NVS   4
#define MMAP_BADRAM     5

// フレームバッファピクセルフォーマット
typedef enum {
    FB_FMT_RGB  = 0,
    FB_FMT_BGR  = 1,
    FB_FMT_RGBX = 2,
    FB_FMT_TEXT = 3,   // VGA テキストモード
} fb_format_t;

typedef struct {
    uint32_t magic;                     // BOOT_INFO_MAGIC
    // メモリマップ
    boot_mmap_entry_t *mmap;
    uint32_t           mmap_count;
    uint32_t          _pad0;
    // フレームバッファ (EFI GOP / VBE)
    uint64_t   fb_addr;
    uint32_t   fb_width;
    uint32_t   fb_height;
    uint32_t   fb_pitch;                // 1行のバイト数
    uint32_t   fb_bpp;                  // bits per pixel
    fb_format_t fb_format;
    uint32_t  _pad1;
    // initrd (カーネルモジュール)
    uint64_t   initrd_start;
    uint64_t   initrd_size;
    // カーネルコマンドライン
    char       cmdline[256];
    // ACPI RSDP 物理アドレス
    uint64_t   rsdp_addr;
} boot_info_t;