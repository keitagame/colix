#pragma once

// システムコール番号 (カーネル・ユーザー共通)
// Linux 互換に寄せているが完全互換ではない

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