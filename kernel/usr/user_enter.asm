; user_enter.asm
BITS 64
global user_enter

; void user_enter(uint64_t rip, uint64_t rsp);
; rdi = rip, rsi = rsp
user_enter:
    mov rcx, rdi        ; rcx = rip
    mov rdx, rsi        ; rdx = rsp

    mov ax, 0x23        ; user data (RPL=3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; ユーザスタックに「戻りフレーム」を積む
    pushq 0x23          ; SS
    pushq rdx           ; RSP
    pushfq
    pop rax
    or rax, (1 << 9)    ; IF=1
    push rax
    pushq 0x2B          ; CS (user code 64)
    pushq rcx           ; RIP

    iretq
