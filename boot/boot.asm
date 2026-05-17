; boot.asm - Multiboot2 ブートローダー
; x86_64 Long Mode に移行し、カーネルへジャンプする

BITS 32

; Multiboot2 マジックナンバーとヘッダ
MB2_MAGIC    equ 0xE85250D6
MB2_ARCH     equ 0           ; i386
MB2_LENGTH   equ (mb2_header_end - mb2_header)
MB2_CHECKSUM equ -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)

section .multiboot2
align 8
mb2_header:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd MB2_LENGTH
    dd MB2_CHECKSUM
    ; 終端タグ
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
mb2_header_end:

; カーネルスタック (32KB)
section .bss
align 16
stack_bottom:
    resb 32768
stack_top:

; ページングテーブル (各4KB)
align 4096
pml4_table:   resb 4096
pdp_table:    resb 4096
pd_table:     resb 4096
pt_table:     resb 4096

section .text
global _start
extern kernel_main

_start:
    ; マルチブート情報のポインタを保存
    ;mov edi, ebx       ; multiboot info -> edi (第1引数)
    ;mov esi, eax       ; magic -> esi (第2引数)
    mov edi, eax       ; 第1引数: magic
    mov esi, ebx       ; 第2引数: mb_info_ptr 
    ; スタック設定
    mov esp, stack_top

    ; CPUの必要機能チェック
    call check_cpuid
    call check_long_mode

    ; ページング設定 (最初の2MBをアイデンティティマップ)
    call setup_paging

    ; GDT ロード (Long Mode用)
    call load_gdt

    ; Protected Mode のフラグを設定
    ; Long Mode Enable: EFER.LME = 1
    mov ecx, 0xC0000080     ; MSR_EFER
    rdmsr
    or eax, (1 << 8)        ; LME bit
    wrmsr

    ; ページングとProtected Mode有効化
    mov eax, cr0
    or eax, (1 << 31) | (1 << 0)
    mov cr0, eax

    ; 64bitコードセグメントへジャンプ
    jmp 0x08:long_mode_start

; -------------------------------------------------------
; CPUID のサポート確認
check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, (1 << 21)
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    xor eax, ecx
    jz .no_cpuid
    ret
.no_cpuid:
    mov al, '0'
    jmp boot_error

; Long Mode のサポート確認
check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)
    jz .no_long_mode
    ret
.no_long_mode:
    mov al, '1'
    jmp boot_error

; ページテーブルのセットアップ (最初の2MBをアイデンティティマップ)
setup_paging:
    ; PML4 -> PDP
    mov eax, pdp_table
    or eax, 0x3             ; Present + Writable
    mov [pml4_table], eax

    ; PDP -> PD
    mov eax, pd_table
    or eax, 0x3
    mov [pdp_table], eax

    ; PD -> PT
    mov eax, pt_table
    or eax, 0x3
    mov [pd_table], eax

    ; PT: 512エントリ、それぞれ4KBページ
    mov ecx, 0
.map_pt:
    mov eax, 0x1000         ; 4096
    mul ecx
    or eax, 0x3             ; Present + Writable
    mov [pt_table + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jl .map_pt

    ; CR3 にPML4をセット
    mov eax, pml4_table
    mov cr3, eax

    ; PAE有効化
    mov eax, cr4
    or eax, (1 << 5)        ; PAE bit
    mov cr4, eax

    ret

; GDTのロード
load_gdt:
    lgdt [gdt64_ptr]
    ret

; エラー: ポート0x3F8 (シリアル) に文字を出力して停止
boot_error:
    mov byte [0xB8000], al  ; VGAに文字
    mov byte [0xB8001], 0x4F
    hlt

; -------------------------------------------------------
; 64bit GDT
section .data
align 8
gdt64:
    dq 0                    ; Null descriptor
    ; Code segment: Execute/Read, 64bit
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
    ; Data segment: Read/Write
    dq (1 << 44) | (1 << 47) | (1 << 41)
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dq gdt64

; -------------------------------------------------------
; 64bit Long Mode エントリポイント
BITS 64
long_mode_start:
    ; セグメントレジスタをデータセグメントに設定
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; rsp を64bit スタックトップに設定
    mov rsp, stack_top

    ; カーネルメイン呼び出し (rdi=mb_info, rsi=magic は保持済み)
    call kernel_main

    ; 戻ってきたら停止
.halt:
    cli
    hlt
    jmp .halt
