#include "kernel.h"

#define IDT_SIZE 256

static idt_entry_t idt[IDT_SIZE];
static idtr_t idtr;

extern uint64_t isr_stub_table[];

static void set_gate(uint8_t num, uint64_t handler, uint8_t ist, uint8_t dpl) {
    // type_attr: Present(7) | DPL(6:5) | 0(4) | Type(3:0)
    // type 0xE = 64bit interrupt gate
    uint8_t attr = 0x8E | ((dpl & 3) << 5);
    idt[num].offset_low  = handler & 0xFFFF;
    idt[num].selector    = GDT_KERNEL_CODE;
    idt[num].ist         = ist & 0x7;
    idt[num].type_attr   = attr;
    idt[num].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].reserved    = 0;
}

void idt_set_gate(uint8_t num, uint64_t handler, uint8_t ist, uint8_t dpl) {
    set_gate(num, handler, ist, dpl);
}

void idt_init(void) {
    // 例外と IRQ (0-47)
    for (int i = 0; i < 48; i++) {
        if (isr_stub_table[i])
            set_gate((uint8_t)i, isr_stub_table[i], 0, 0);
    }
    // システムコール (0x80) - DPL=3でユーザーから呼べる
    if (isr_stub_table[128])
        set_gate(0x80, isr_stub_table[128], 0, 3);

    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)idt;
    lidt(&idtr);

    kprintf("[IDT] initialized\n");
}

// ── 例外/IRQディスパッチャ ──────────────────────────────
static const char *exception_names[] = {
    "Divide Error",       "Debug",              "NMI",
    "Breakpoint",         "Overflow",           "Bound Range",
    "Invalid Opcode",     "Device Not Available","Double Fault",
    "Coprocessor Seg",    "Invalid TSS",        "Segment Not Present",
    "Stack Fault",        "General Protection", "Page Fault",
    "Reserved",           "FPU Error",          "Alignment Check",
    "Machine Check",      "SIMD Error",
};

void interrupt_dispatch(interrupt_frame_t *frame) {
    uint64_t no = frame->int_no;
    

    if (no < 32) {
        // CPU例外
        const char *name = (no < ARRAY_SIZE(exception_names))
                           ? exception_names[no] : "Unknown";
        kprintf("\n[EXCEPTION #%d] %s\n", (int)no, name);
        kprintf("r12=%p\n", frame->r12);
        kprintf("  err=%lx rip=%p rsp=%p\n",
                frame->err_code, (void*)frame->rip, (void*)frame->rsp);
        if (no == IDT_PAGE_FAULT)
            kprintf("  cr2=%p (fault addr)\n", (void*)read_cr2());

        // カーネル例外 = パニック; ユーザー例外は将来はSIGNAL送出
        panic("unhandled exception");

    } else if (no == 0x80) {
        // システムコール: rax=番号, rdi rsi rdx r10 r8 r9 = 引数
        // interrupt_frameのレジスタから取得
        uint64_t ret = syscall_handler(
            frame->rax,
            frame->rdi, frame->rsi, frame->rdx,
            frame->r10, frame->r8);
        frame->rax = ret;

    } else if (no >= IDT_IRQ_BASE && no < IDT_IRQ_BASE + 16) {
        uint8_t irq = (uint8_t)(no - IDT_IRQ_BASE);

        if (irq == 0) {
            // タイマー割り込み
            ticks++;
            sched_tick();
        }
        // ※ 他のIRQ (keyboard等) はユーザードライバで処理予定
        pic_send_eoi(irq);
    }
}
