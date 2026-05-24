BITS 64
global enter_user_mode

enter_user_mode:
    ; rdi = user_rip
    ; rsi = user_rsp
    ; rdx = cr3

    ; 1. ページテーブル切替 (ユーザー空間のマップを含むCR3へ)
    mov cr3, rdx

    ; 2. カーネルスタックのまま、iretqフレームを構築する！
    ; (mov rsp, rsi は絶対にここでやらない)
    
    push 0x23        ; SS (User Data Segment Selector, RPL=3)
    push rsi         ; RSP (ユーザーモード移行後に適用されるスタックポインタ)
    push 0x202       ; RFLAGS (IF=1)
    push 0x1B        ; CS (User Code Segment Selector, RPL=3)
    push rdi         ; RIP (ユーザーモード移行後に実行するアドレス)

    ; 3. 運命のジャンプ
    ; CPUがスタックから上記5つの値を自動でポップし、
    ; 特権レベルをRing 3に落とすと同時に、RSPとRIPをユーザー空間のものに切り替えます。
    iretq