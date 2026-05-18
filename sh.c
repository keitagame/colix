// sh.c

// x86_64 Linux のシステムコール番号
#define SYS_write 1
#define SYS_exit 60

// アセンブリを使用したシステムコールの定義
long syscall3(long num, long arg1, long arg2, long arg3) {
    long ret;
    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

void write(int fd, const void *buf, unsigned long count) {
    syscall3(SYS_write, fd, (long)buf, count);
}

void exit(int status) {
    syscall3(SYS_exit, status, 0, 0);
}

// エントリーポイント
void _start(void) {
    write(1, "[user sh] hello from ELF\n", 25); // 文字数は25文字です

    // 無限ループ
    for (;;) {
        write(1, ".", 1);
        
        // 簡易的なウェイト（CPU負荷高め、将来 sleep に置き換え）
        for (volatile int i = 0; i < 10000000; i++);
    }

    exit(0);
}
