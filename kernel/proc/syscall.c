#include "kernel.h"

void syscall_init(void) {
    // INT 0x80 方式はIDTで登録済み
    // 将来: SYSCALL命令を使う場合は MSR_STAR/LSTAR を設定
    kprintf("[SYSCALL] handler via INT 0x80\n");
}

// システムコールディスパッチャ
// 割り込みフレームから呼ばれる: num=rax, a1=rdi, a2=rsi, a3=rdx, a4=r10, a5=r8
uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2,
                         uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a3; (void)a4; (void)a5;

    switch (num) {

    case SYS_EXIT:
        proc_exit((int)a1);
        return 0; // 到達しない

    case SYS_WRITE: {
        // a1=fd, a2=buf, a3=len
        // fd=1(stdout),fd=2(stderr) → シリアル/VGAに出力
        int fd = (int)a1;
        kprintf("[SYSCALL] SYS_WRITE\n");
        const char *buf = (const char*)a2;
        uint64_t len = a3;
        if (fd == 1 || fd == 2) {
            for (uint64_t i = 0; i < len; i++) {
                serial_putc(buf[i]);
                vga_putc(buf[i]);
            }
            return len;
        }
        return (uint64_t)-9; /* EBADF */
    }

    case SYS_GETPID:
        return current_proc ? (uint64_t)current_proc->pid : 0;

    case SYS_GETPPID:
        return current_proc ? (uint64_t)current_proc->ppid : 0;

    case SYS_YIELD:
        proc_yield();
        return 0;

    case SYS_SLEEP:
        // a1 = ミリ秒
        proc_sleep(a1);
        return 0;

    case SYS_IPC_SEND: {
        // a1=dst_pid, a2=type, a3=data_ptr, a4=len
        pid_t dst = (pid_t)a1;
        uint32_t type = (uint32_t)a2;
        const void *data = (const void*)a3;
        uint32_t len = (uint32_t)a4;
        return (uint64_t)ipc_send(dst, type, data, len);
    }

    case SYS_IPC_RECV: {
        // a1 = ipc_message_t* (ユーザー空間)
        ipc_message_t *out = (ipc_message_t*)a1;
        return (uint64_t)ipc_recv(out);
    }

    default:
        kprintf("[SYSCALL] unknown syscall %lu\n", num);
        return (uint64_t)-ENOSYS;
    }
}

// EBADF は標準Unixエラーコード
