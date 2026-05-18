#pragma once
#include "types.h"

// ELF64 マジックナンバー・定数
#define ELF_MAGIC       0x464C457FU   // 0x7F 'E' 'L' 'F'
#define ELFCLASS64      2
#define ELFDATA2LSB     1             // リトルエンディアン
#define ET_EXEC         2             // 実行可能ファイル
#define ET_DYN          3             // 共有オブジェクト (PIE)
#define EM_X86_64       62
#define PT_LOAD         1             // ロードセグメント
#define PT_INTERP       3
#define PT_PHDR         6
#define PF_X            (1 << 0)      // 実行可
#define PF_W            (1 << 1)      // 書き込み可
#define PF_R            (1 << 2)      // 読み取り可

// ELF64 ヘッダ (64 バイト)
typedef struct {
    uint8_t  e_ident[16];   // マジック, クラス, エンディアン, バージョン...
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;       // エントリポイント仮想アドレス
    uint64_t e_phoff;       // プログラムヘッダテーブルオフセット
    uint64_t e_shoff;       // セクションヘッダテーブルオフセット
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;   // 各 Phdr サイズ
    uint16_t e_phnum;       // Phdr 数
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} PACKED Elf64_Ehdr;

// ELF64 プログラムヘッダ (56 バイト)
typedef struct {
    uint32_t p_type;        // セグメント種別
    uint32_t p_flags;       // 権限フラグ
    uint64_t p_offset;      // ファイル内オフセット
    uint64_t p_vaddr;       // 仮想アドレス
    uint64_t p_paddr;       // 物理アドレス (通常未使用)
    uint64_t p_filesz;      // ファイル内サイズ
    uint64_t p_memsz;       // メモリ上のサイズ (>= filesz, 差分は0埋め)
    uint64_t p_align;       // アライメント
} PACKED Elf64_Phdr;

// バリデーション
static inline bool elf64_valid(const uint8_t *data, size_t size) {
    if (size < sizeof(Elf64_Ehdr)) return false;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)data;
    if (*(uint32_t *)eh->e_ident != ELF_MAGIC)   return false;
    if (eh->e_ident[4] != ELFCLASS64)             return false;
    if (eh->e_ident[5] != ELFDATA2LSB)            return false;
    if (eh->e_machine   != EM_X86_64)             return false;
    return true;
}