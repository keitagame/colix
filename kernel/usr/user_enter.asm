BITS 64
global enter_user_mode

enter_user_mode:
    ; rdi = rip
    ; rsi = rsp
    ; rdx = cr3

    ; ページテーブル切替
    mov cr3, rdx

    ; ユーザスタック設定
    mov rsp, rsi

    ; iretqフレーム構築
    push 0x23        ; SS (user data)
    push rsi         ; RSP
    push 0x202       ; RFLAGS (IF=1)
    push 0x1B        ; CS (user code)
    push rdi         ; RIP

    iretq