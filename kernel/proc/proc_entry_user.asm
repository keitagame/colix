BITS 64
global proc_entry_user
extern current_proc
extern enter_user_mode

%define USER_RIP_OFFSET  0x1c8
%define USER_SP_OFFSET   0x1d8
%define CR3_OFFSET       0x1b8   ; さっき出した値

proc_entry_user:
    mov rbx, [rel current_proc]

    ; 引数セット
    mov rdi, [rbx + USER_RIP_OFFSET]  ; RIP
    mov rsi, [rbx + USER_SP_OFFSET]   ; RSP
    mov rdx, [rbx + CR3_OFFSET]       ; CR3

    ; 直接ジャンプ（call禁止）
    jmp enter_user_mode

.hang:
    hlt
    jmp .hang