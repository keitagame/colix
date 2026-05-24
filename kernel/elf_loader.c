#include "elf.h"
#include "initrd.h"
#include "pmm.h"
#include "vmm.h"



// 超雑: カーネルと同じアドレス空間に PT_LOAD をそのままマップ
bool elf_load_from_initrd(const char *path, elf_load_result_t *out) {
    const initrd_file_t *f = initrd_find(path);
    if (!f) {
        kprintf("[ELF] file not found: %s\n", path);
        return false;
    }

    if (f->size < sizeof(elf64_ehdr_t)) {
        kprintf("[ELF] too small: %s\n", path);
        return false;
    }
/*
    const uint8_t *base = f->data;
    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)base;
*/
const uint8_t *base = f->data;

kprintf("base = %p\n", base);
kprintf("magic = %x\n", *(uint32_t*)base);

const elf64_ehdr_t *eh = (const elf64_ehdr_t *)base;

kprintf("e_entry = %p\n", eh->e_entry);
kprintf("e_phoff = %lx\n", eh->e_phoff);
    uint32_t magic = *(const uint32_t *)eh->e_ident;
    if (magic != ELF_MAGIC) {
        kprintf("[ELF] bad magic: %x\n", magic);
        return false;
    }

    if (eh->e_machine != 0x3E) { // EM_X86_64
        kprintf("[ELF] unsupported machine: %u\n", eh->e_machine);
        return false;
    }

    const elf64_phdr_t *ph = (const elf64_phdr_t *)(base + eh->e_phoff);

    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;

        uint64_t vaddr   = ph[i].p_vaddr;
        uint64_t memsz   = ph[i].p_memsz;
        uint64_t filesz  = ph[i].p_filesz;
        uint64_t offset  = ph[i].p_offset;

        // ページ境界に丸める
        uint64_t page_start = vaddr & ~0xFFFULL;
        uint64_t page_end   = (vaddr + memsz + 0xFFF) & ~0xFFFULL;
        uint64_t pages      = (page_end - page_start) >> 12;

        // 物理ページ確保してマップ
        for (uint64_t p = 0; p < pages; p++) {
            uint64_t pa = pmm_alloc_page();
            if (!pa) {
                kprintf("[ELF] out of memory\n");
                return false;
            }
            vmm_map(page_start + p * 0x1000, pa, /*flags=*/0x3); // RW + present
        }

        // ファイル内容をコピー
        uint8_t *dst = (uint8_t *)vaddr;
        const uint8_t *src = base + offset;
        for (uint64_t j = 0; j < filesz; j++) {
            dst[j] = src[j];
        }
        // BSS 埋め
        for (uint64_t j = filesz; j < memsz; j++) {
            dst[j] = 0;
        }
    }

    out->entry = eh->e_entry;
    kprintf("[ELF] loaded %s entry=%p\n", path, (void*)out->entry);
    return true;
}