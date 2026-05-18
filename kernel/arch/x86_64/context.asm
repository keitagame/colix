; context.asm - プロセスコンテキストスイッチ
; void switch_context(cpu_context_t *old, cpu_context_t *new)
;
; cpu_context_t オフセット (kernel.h と一致):
;   0:  r15
;   8:  r14
;  16:  r13
;  24:  r12
;  32:  rbx
;  40:  rbp
;  48:  rip   (ret 先アドレス)
;  56:  rsp   (スタックポインタ)

BITS 64
global switch_context

switch_context:
    ; rdi = old context*, rsi = new context*

    ; ── 現在コンテキスト保存 ──────────────────────────────
    mov [rdi + 0],  r15
    mov [rdi + 8],  r14
    mov [rdi + 16], r13
    mov [rdi + 24], r12
    mov [rdi + 32], rbx
    mov [rdi + 40], rbp

    ; RIP: スタックトップにある呼び出し元 ret アドレスを保存
    mov rax, [rsp]
    mov [rdi + 48], rax
    ; RSP: ret アドレスが入っている位置 (= 現在の rsp) を保存
    mov [rdi + 56], rsp

    ; ── 新コンテキスト復元 ────────────────────────────────
    mov r15, [rsi + 0]
    mov r14, [rsi + 8]
    mov r13, [rsi + 16]
    mov r12, [rsi + 24]
    mov rbx, [rsi + 32]
    mov rbp, [rsi + 40]

    ; スタック切り替え
    mov rsp, [rsi + 56]     ; 新スタックポインタ
    ; [rsp] に rip を書き込んで ret で飛ぶ
    mov rax, [rsi + 48]
    mov [rsp], rax

    ret