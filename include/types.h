#pragma once

// 基本型
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;
typedef uint64_t           size_t;
typedef int64_t            ssize_t;
typedef uint64_t           uintptr_t;
typedef int64_t            intptr_t;
typedef int32_t            pid_t;
typedef int32_t            uid_t;
typedef int32_t            gid_t;

#define NULL  ((void*)0)
#define true  1
#define false 0
typedef int bool;

// 便利マクロ
#define ARRAY_SIZE(x)     (sizeof(x) / sizeof((x)[0]))
#define ALIGN_UP(x, a)    (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x, a)  ((x) & ~((a) - 1))
#define PAGE_SIZE         4096ULL
#define PAGE_MASK         (~(PAGE_SIZE - 1))
#define PAGE_ALIGN(x)     ALIGN_UP(x, PAGE_SIZE)

// ビット操作
#define BIT(n)            (1ULL << (n))
#define SET_BIT(x, n)     ((x) |= BIT(n))
#define CLR_BIT(x, n)     ((x) &= ~BIT(n))
#define TEST_BIT(x, n)    (!!((x) & BIT(n)))

// コンパイラヒント
#define likely(x)         __builtin_expect(!!(x), 1)
#define unlikely(x)       __builtin_expect(!!(x), 0)
#define NORETURN          __attribute__((noreturn))
#define PACKED            __attribute__((packed))
#define ALIGNED(x)        __attribute__((aligned(x)))
#define UNUSED            __attribute__((unused))

// エラーコード (Unix互換)
#define E_OK       0
#define EPERM      1
#define ENOENT     2
#define ESRCH      3
#define EINTR      4
#define EIO        5
#define ENOMEM    12
#define EACCES    13
#define EFAULT    14
#define EBUSY     16
#define EEXIST    17
#define EINVAL    22
#define ENFILE    23
#define EMFILE    24
#define ENOSYS    38
#define ENOMSG    42
#define EOVERFLOW 75
#define ETIMEDOUT 110
