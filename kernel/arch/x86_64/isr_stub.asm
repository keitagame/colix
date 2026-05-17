; isr_stub.asm - 割り込みスタブ (全レジスタを保存してCハンドラへ)
BITS 64
global isr_stub_table

extern interrupt_dispatch

; エラーコードなし割り込みのスタブマクロ
%macro ISR_NOERR 1
isr_%1:
    push 0          ; ダミーエラーコード
    push %1         ; 割り込み番号
    jmp isr_common
%endmacro

; エラーコードあり割り込みのスタブマクロ
%macro ISR_ERR 1
isr_%1:
    push %1         ; 割り込み番号 (エラーコードはすでにスタックにある)
    jmp isr_common
%endmacro

; 例外ハンドラ (0-19)
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8     ; double fault (エラーコードあり)
ISR_NOERR 9
ISR_ERR   10    ; invalid TSS
ISR_ERR   11    ; segment not present
ISR_ERR   12    ; stack fault
ISR_ERR   13    ; GPF
ISR_ERR   14    ; page fault
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17    ; alignment check
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; IRQ (0x20-0x2F) = PIC IRQ 0-15
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47

; システムコール (0x80)
ISR_NOERR 128

; 共通ハンドラ: レジスタ保存 → Cに委譲 → 復元
isr_common:
    ; 汎用レジスタを保存 (interrupt_frame_t に対応)
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; セグメントをカーネルに
    mov ax, 0x10
    mov ds, ax
    mov es, ax

    ; C ディスパッチャを呼ぶ (第1引数 = &frame)
    mov rdi, rsp
    call interrupt_dispatch

    ; レジスタ復元
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; int_no と err_code を取り除く
    add rsp, 16

    iretq

; スタブアドレステーブル
section .rodata
isr_stub_table:
    dq isr_0,  isr_1,  isr_2,  isr_3,  isr_4,  isr_5,  isr_6,  isr_7
    dq isr_8,  isr_9,  isr_10, isr_11, isr_12, isr_13, isr_14, isr_15
    dq isr_16, isr_17, isr_18, isr_19, isr_20, isr_21, isr_22, isr_23
    dq isr_24, isr_25, isr_26, isr_27, isr_28, isr_29, isr_30, isr_31
    dq isr_32, isr_33, isr_34, isr_35, isr_36, isr_37, isr_38, isr_39
    dq isr_40, isr_41, isr_42, isr_43, isr_44, isr_45, isr_46, isr_47
    ; 0x30 - 0x7F (空き: ダミー0で埋める) 80エントリ
    times 80 dq 0
    dq isr_128   ; 0x80 syscall
