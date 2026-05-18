// elf_loader.h
#pragma once
#include "stdint.h"
#include "stdbool.h"
#include "elf.h"

bool elf_load_from_initrd(const char *path, elf_load_result_t *out);
