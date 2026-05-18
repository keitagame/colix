// user/syscall.h
#pragma once
#define SYS_EXIT        1
#define SYS_READ        3    // read(fd, buf, len)
#define SYS_WRITE       4    // write(fd, buf, len)
#define SYS_OPEN        5    // open(path, flags) -> fd
#define SYS_CLOSE       6    // close(fd)
#define SYS_LSEEK       19   // lseek(fd, offset, whence)
#define SYS_GETPID      20
#define SYS_GETPPID     64
#define SYS_YIELD       24
#define SYS_SLEEP       162  // sleep(ms) ※ 本来は nanosleep だが ms で簡略化
#define SYS_IPC_SEND    200
#define SYS_IPC_RECV    201
#define SYS_SPAWN       250  // spawn(path, argv) -> child_pid  (fork+exec 簡略版)
#define SYS_WAITPID     251  // waitpid(pid) -> exit_code
#define SYS_GETDENTS    252  // getdents(buf, max) -> count  (initrd ファイル一覧)
#define SYS_SBRK        253  // sbrk(incr) -> old_brk

// open フラグ
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2

// lseek origin
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

// getdents エントリ
typedef struct {
    char     name[64];
    uint32_t size;
    uint32_t type;   // 0=file, 1=dir
} dirent_t;
#include <stdint.h>


static inline long write(int fd, const void *buf, unsigned long len) {
    long ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE),  // rax = SYS_WRITE
          "D"(fd),         // rdi = fd
          "S"(buf),        // rsi = buf
          "d"(len)         // rdx = len
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline void exit(int code) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYS_EXIT),   // rax = SYS_EXIT
          "D"(code)        // rdi = code
        : "rcx", "r11", "memory"
    );
    __builtin_unreachable();
}
