#pragma once
#include <stdint.h>

#define MULTIBOOT2_TAG_TYPE_MODULE 3

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_tag_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    char     cmdline[0];
};
