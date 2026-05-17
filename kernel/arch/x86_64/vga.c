#include "../include/kernel.h"

// VGAテキストバッファ: 80列 x 25行
#define VGA_COLS  80
#define VGA_ROWS  25
#define VGA_ADDR  ((volatile uint16_t*)0xB8000)

// カラー属性
#define VGA_COLOR(fg, bg) ((uint8_t)((bg) << 4 | (fg)))
#define VGA_ENTRY(c, attr) ((uint16_t)((attr) << 8 | (uint8_t)(c)))

enum vga_color {
    VGA_BLACK=0, VGA_BLUE, VGA_GREEN, VGA_CYAN,
    VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LGRAY,
    VGA_DGRAY, VGA_LBLUE, VGA_LGREEN, VGA_LCYAN,
    VGA_LRED, VGA_LMAGENTA, VGA_YELLOW, VGA_WHITE
};

static int vga_row = 0;
static int vga_col = 0;
static uint8_t vga_attr = 0;

// VGAカーソル更新 (I/Oポート経由)
static void vga_update_cursor(int row, int col) {
    uint16_t pos = row * VGA_COLS + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_clear(void) {
    uint16_t blank = VGA_ENTRY(' ', vga_attr);
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA_ADDR[i] = blank;
    vga_row = 0;
    vga_col = 0;
    vga_update_cursor(0, 0);
}

void vga_init(void) {
    vga_attr = VGA_COLOR(VGA_LGRAY, VGA_BLACK);
    vga_clear();
}

static void vga_scroll(void) {
    // 1行分上にずらす
    for (int r = 1; r < VGA_ROWS; r++)
        for (int c = 0; c < VGA_COLS; c++)
            VGA_ADDR[(r-1)*VGA_COLS + c] = VGA_ADDR[r*VGA_COLS + c];
    // 最終行をクリア
    uint16_t blank = VGA_ENTRY(' ', vga_attr);
    for (int c = 0; c < VGA_COLS; c++)
        VGA_ADDR[(VGA_ROWS-1)*VGA_COLS + c] = blank;
    vga_row = VGA_ROWS - 1;
}

void vga_putc(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\t') {
        vga_col = (vga_col + 8) & ~7;
    } else if (c == '\b') {
        if (vga_col > 0) vga_col--;
    } else {
        VGA_ADDR[vga_row * VGA_COLS + vga_col] = VGA_ENTRY(c, vga_attr);
        vga_col++;
        if (vga_col >= VGA_COLS) {
            vga_col = 0;
            vga_row++;
        }
    }
    if (vga_row >= VGA_ROWS) vga_scroll();
    vga_update_cursor(vga_row, vga_col);
}

void vga_puts(const char *s) {
    while (*s) vga_putc(*s++);
}
