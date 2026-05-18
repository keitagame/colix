#include "kernel.h"

extern void enter_user_mode(uint64_t rip, uint64_t rsp, uint64_t cr3);

void user_trampoline(void) {
    process_t *p = current_proc;

    enter_user_mode(p->user_rip, p->user_sp, p->cr3);

    for (;;) __asm__ volatile("hlt");
}
