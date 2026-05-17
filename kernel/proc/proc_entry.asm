; proc_entry.asm - 新プロセスの初回起動
; switch_context が rip=proc_entrypoint で ret してくる
; その時 r12 に entry 関数ポインタが入っている

BITS 64
global proc_entrypoint

extern proc_exit

proc_entrypoint:
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
