// initrd.c - cpio newc ベースの簡易 initrd パーサ
#include "stdint.h"
#include "stddef.h"
#include "types.h"
// 必要なら自前の kprintf に差し替え
extern void kprintf(const char *fmt, ...);

// ─────────────────────────────────────────────
// cpio newc ヘッダ定義
// ─────────────────────────────────────────────
typedef struct {
    char c_magic[6];    // "070701"
    char c_ino[8];
    char c_mode[8];
    char c_uid[8];
    char c_gid[8];
    char c_nlink[8];
    char c_mtime[8];
    char c_filesize[8]; // 16進ASCII
    char c_devmajor[8];
    char c_devminor[8];
    char c_rdevmajor[8];
    char c_rdevminor[8];
    char c_namesize[8]; // 16進ASCII（終端NUL含む）
    char c_check[8];
} cpio_newc_header_t;

// ─────────────────────────────────────────────
// initrd 内のファイルエントリ
// ─────────────────────────────────────────────
#define INITRD_MAX_FILES 128

typedef struct {
    const char   *name;  // NUL終端パス
    uint32_t      size;  // バイト数
    const uint8_t *data; // ファイル本体先頭
} initrd_file_t;

static initrd_file_t initrd_files[INITRD_MAX_FILES];
static size_t initrd_file_count = 0;

// ─────────────────────────────────────────────
// ちょいユーティリティ
// ─────────────────────────────────────────────
static int memcmp_simple(const void *a, const void *b, size_t n) {
    const uint8_t *p = (const uint8_t*)a;
    const uint8_t *q = (const uint8_t*)b;
    for (size_t i = 0; i < n; i++) {
        if (p[i] != q[i]) return (int)p[i] - (int)q[i];
    }
    return 0;
}

static int strcmp_simple(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static uint32_t hex8_to_u32(const char *s) {
    uint32_t v = 0;
    for (int i = 0; i < 8; i++) {
        char c = s[i];
        uint32_t d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else d = 0;
        v = (v << 4) | d;
    }
    return v;
}

static uint32_t align4(uint32_t x) {
    return (x + 3) & ~3u;
}

// ─────────────────────────────────────────────
// initrd 初期化：cpio newc を走査してテーブル構築
// ─────────────────────────────────────────────
void initrd_init(uint8_t *start, uint8_t *end) {
    initrd_file_count = 0;

    uint8_t *p = start;
    while (p + sizeof(cpio_newc_header_t) <= end) {
        cpio_newc_header_t *h = (cpio_newc_header_t*)p;

        // マジックチェック
        if (h->c_magic[0] != '0' || h->c_magic[1] != '7' ||
            h->c_magic[2] != '0' || h->c_magic[3] != '7' ||
            h->c_magic[4] != '0' || h->c_magic[5] != '1') {
            kprintf("[INITRD] invalid magic\n");
            break;
        }

        uint32_t namesize = hex8_to_u32(h->c_namesize);
        uint32_t filesize = hex8_to_u32(h->c_filesize);

        p += sizeof(cpio_newc_header_t);

        const char *name = (const char*)p;

        // 終端 "TRAILER!!!"
        if (namesize == 11 && memcmp_simple(name, "TRAILER!!!", 10) == 0) {
            break;
        }

        // 名前部分を4バイト境界に揃える
        uint32_t name_pad = align4(namesize);
        p += name_pad;

        uint8_t *file_data = p;

        // ファイル本体を4バイト境界に揃える
        uint32_t file_pad = align4(filesize);
        p += file_pad;

        if (p > end) break;

        if (initrd_file_count < INITRD_MAX_FILES) {
            initrd_files[initrd_file_count].name = name;
            initrd_files[initrd_file_count].size = filesize;
            initrd_files[initrd_file_count].data = file_data;
            initrd_file_count++;
        }
    }

    kprintf("[INITRD] files=%d\n", (int)initrd_file_count);
}

// ─────────────────────────────────────────────
// ファイル検索 / 読み出し API
// ─────────────────────────────────────────────
const initrd_file_t *initrd_find(const char *name) {
    for (size_t i = 0; i < initrd_file_count; i++) {
        if (strcmp_simple(initrd_files[i].name, name) == 0)
            return &initrd_files[i];
    }
    return NULL;
}

size_t initrd_read(const initrd_file_t *f, size_t off, void *buf, size_t len) {
    if (!f) return 0;
    if (off >= f->size) return 0;
    if (off + len > f->size) len = f->size - off;

    const uint8_t *src = f->data + off;
    uint8_t *dst = (uint8_t*)buf;
    for (size_t i = 0; i < len; i++) dst[i] = src[i];
    return len;
}

// デバッグ用：一覧表示
void initrd_list(void) {
    for (size_t i = 0; i < initrd_file_count; i++) {
        kprintf("[INITRD] %s (%u bytes)\n",
                initrd_files[i].name,
                (unsigned)initrd_files[i].size);
    }
}
