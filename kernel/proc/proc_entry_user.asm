; proc_entry_user.asm
BITS 64
global proc_entry_user
extern current_proc
extern enter_user_mode
%define USER_RIP_OFFSET  0x58
%define USER_SP_OFFSET   0x50
%define CR3_OFFSET       0x60

proc_entry_user:
    ; current_proc を読む
    mov rax, [rel current_proc]

    ; rdi = user_rip
    mov rdi, [rax + USER_RIP_OFFSET]

    ; rsi = user_rsp
    mov rsi, [rax + USER_SP_OFFSET]

    ; rdx = cr3
    mov rdx, [rax + CR3_OFFSET]

    ; ユーザモードへ遷移
    call enter_user_mode

.hang:
    hlt
    jmp .hang
