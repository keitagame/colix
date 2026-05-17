; context.asm - プロセスコンテキストスイッチ
; void switch_context(cpu_context_t *old, cpu_context_t *new)
;
; cpu_context_t のレイアウト (kernel.h と一致させること):
;   offset 0:  r15
;   offset 8:  r14
;   offset 16: r13
;   offset 24: r12
;   offset 32: rbx
;   offset 40: rbp
;   offset 48: rip  (このreturnアドレスで再開)

BITS 64
global switch_context

switch_context:
    ; rdi = old context, rsi = new context
    ; 呼び出し規約: rdi, rsi, rdx, rcx, r8, r9 は caller-saved → 保存不要
    ; callee-saved: rbx, rbp, r12-r15 を保存

    ; ── 現在のコンテキストを保存 ──
    mov [rdi + 0],  r15
    mov [rdi + 8],  r14
    mov [rdi + 16], r13
    mov [rdi + 24], r12
    mov [rdi + 32], rbx
    mov [rdi + 40], rbp
    ; RIPは、このret命令のアドレス (スタックトップ) を保存
    mov rax, [rsp]
    mov [rdi + 48], rax
    mov [rdi + 56], rsp

    ; ── 新しいコンテキストを復元 ──
    mov r15, [rsi + 0]
    mov r14, [rsi + 8]
    mov r13, [rsi + 16]
    mov r12, [rsi + 24]
    mov rbx, [rsi + 32]
    mov rbp, [rsi + 40]
    ; 新しいRIPをスタックに積んでretで飛ぶ
    mov rsp, [rsi + 56]
    mov rax, [rsi + 48]
    mov [rsp], rax

    ret
