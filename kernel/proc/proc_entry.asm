; proc_entry.asm - 新プロセスの初回起動
; switch_context が rip=proc_entrypoint で ret してくる
; その時 r12 に entry 関数ポインタが入っている

BITS 64
global proc_entrypoint

extern proc_exit
extern current_proc
%define PROC_USER_SP_OFFSET  0x48   ; 例：自分の struct layout から計算

proc_entrypoint:
    ;mov     rax, [rel current_proc]   ; rax = current_proc
    ;mov     rsp, [rax + PROC_USER_SP_OFFSET] ; rsp = current_proc->user_sp
    ; r12 = entry 関数ポインタ (proc_create で設定)
    mov rdi, r12
    ; 割り込み有効化
    sti
    ; エントリ関数を呼ぶ
    call r12
    ; 戻ってきたらexit(0)
    mov rdi, 0
    call proc_exit
.hang:
    hlt
    jmp .hang
