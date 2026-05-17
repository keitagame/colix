#include "kernel.h"

// ── PIC (8259A) ─────────────────────────────────────────
void pic_init(void) {
    // ICW1: 初期化開始
    outb(PIC_MASTER_CMD,  0x11);
    outb(PIC_SLAVE_CMD,   0x11);
    io_wait();

    // ICW2: ベクタオフセット
    outb(PIC_MASTER_DATA, IDT_IRQ_BASE);        // Master: 0x20
    outb(PIC_SLAVE_DATA,  IDT_IRQ_BASE + 8);   // Slave:  0x28
    io_wait();

    // ICW3: カスケード接続
    outb(PIC_MASTER_DATA, 0x04);  // IRQ2にスレーブ接続
    outb(PIC_SLAVE_DATA,  0x02);  // スレーブID=2
    io_wait();

    // ICW4: 8086モード
    outb(PIC_MASTER_DATA, 0x01);
    outb(PIC_SLAVE_DATA,  0x01);
    io_wait();

    // すべてのIRQをマスク (タイマーのみ有効にする)
    outb(PIC_MASTER_DATA, 0xFE);  // IRQ0(timer)のみ許可
    outb(PIC_SLAVE_DATA,  0xFF);

    kprintf("[PIC] remapped to 0x%02x-0x%02x\n",
            IDT_IRQ_BASE, IDT_IRQ_BASE + 15);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC_SLAVE_CMD, 0x20);
    outb(PIC_MASTER_CMD, 0x20);
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC_MASTER_DATA : PIC_SLAVE_DATA;
    if (irq >= 8) irq -= 8;
    uint8_t mask = inb(port) | (1 << irq);
    outb(port, mask);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC_MASTER_DATA : PIC_SLAVE_DATA;
    if (irq >= 8) irq -= 8;
    uint8_t mask = inb(port) & ~(1 << irq);
    outb(port, mask);
}

// ── PIT (8253/8254 タイマー) ────────────────────────────
#define PIT_CH0   0x40
#define PIT_CMD   0x43
#define PIT_BASE_HZ 1193182UL

void pit_init(void) {
    uint32_t divisor = PIT_BASE_HZ / PIT_HZ;

    // チャンネル0, ローバイト/ハイバイト, モード2 (レートジェネレータ)
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));

    kprintf("[PIT] %d Hz (divisor=%d)\n", PIT_HZ, divisor);
}
