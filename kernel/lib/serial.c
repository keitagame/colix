#include "../include/kernel.h"

// ── シリアルポート (COM1) ───────────────────────────────
void serial_init(void) {
    outb(COM1 + 1, 0x00);   // 割り込み無効
    outb(COM1 + 3, 0x80);   // DLAB有効 (ボーレート設定モード)
    outb(COM1 + 0, 0x03);   // ボーレート分周比 下位 (115200 / 3 = 38400)
    outb(COM1 + 1, 0x00);   // 上位
    outb(COM1 + 3, 0x03);   // 8bit, パリティなし, ストップ1bit (DLAB解除)
    outb(COM1 + 2, 0xC7);   // FIFO有効, 14バイトしきい値
    outb(COM1 + 4, 0x0B);   // IRQ有効, RTS/DSR
}

static bool serial_ready(void) {
    return (inb(COM1 + 5) & 0x20) != 0;
}

void serial_putc(char c) {
    while (!serial_ready());
    outb(COM1, (uint8_t)c);
    if (c == '\n') { while (!serial_ready()); outb(COM1, '\r'); }
}

void serial_puts(const char *s) {
    while (*s) serial_putc(*s++);
}

// ── 最小限のkprintf ──────────────────────────────────────
// 対応: %d %u %x %s %c %p %%
static void print_uint(uint64_t n, int base, int width, char pad) {
    static const char digits[] = "0123456789abcdef";
    char buf[64];
    int i = 0;
    if (n == 0) { buf[i++] = '0'; }
    else { while (n) { buf[i++] = digits[n % base]; n /= base; } }
    // padding
    for (int w = i; w < width; w++) { serial_putc(pad); vga_putc(pad); }
    while (i--) { serial_putc(buf[i]); vga_putc(buf[i]); }
}

static void print_int(int64_t n) {
    if (n < 0) { serial_putc('-'); vga_putc('-'); n = -n; }
    print_uint((uint64_t)n, 10, 0, ' ');
}

static void kputc(char c) { serial_putc(c); vga_putc(c); }
static void kputs(const char *s) {
    if (!s) s = "(null)";
    while (*s) kputc(*s++);
}

void kprintf(const char *fmt, ...) {
    // __builtin_va_list を使う
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') { kputc(*fmt++); continue; }
        fmt++;
        int width = 0;
        char pad = ' ';
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') { width = width*10 + (*fmt-'0'); fmt++; }
        // long prefix
        bool lng = false;
        if (*fmt == 'l') { lng = true; fmt++; if (*fmt == 'l') fmt++; }
        (void)lng;

        switch (*fmt) {
        case 'd': print_int((int64_t)__builtin_va_arg(ap, int64_t)); break;
        case 'u': print_uint((uint64_t)__builtin_va_arg(ap, uint64_t), 10, width, pad); break;
        case 'x': print_uint((uint64_t)__builtin_va_arg(ap, uint64_t), 16, width, pad); break;
        case 'p':
            kputs("0x");
            print_uint((uint64_t)__builtin_va_arg(ap, void*), 16, 16, '0');
            break;
        case 's': kputs(__builtin_va_arg(ap, const char*)); break;
        case 'c': kputc((char)__builtin_va_arg(ap, int)); break;
        case '%': kputc('%'); break;
        default:  kputc('%'); kputc(*fmt); break;
        }
        fmt++;
    }
    __builtin_va_end(ap);
}

// ── パニック ────────────────────────────────────────────
void panic(const char *msg) {
    cli();
    kprintf("\n[PANIC] %s\n", msg);
    kprintf("System halted.\n");
    for(;;) hlt();
}
