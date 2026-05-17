#include "kernel.h"

process_t  proc_table[PROC_MAX];
process_t *current_proc = NULL;
uint64_t   ticks = 0;

// 次のPIDカウンタ
static pid_t next_pid = 1;

void proc_init(void) {
    for (int i = 0; i < PROC_MAX; i++) {
        proc_table[i].state = PROC_UNUSED;
        proc_table[i].pid   = 0;
    }
    kprintf("[PROC] initialized (max %d)\n", PROC_MAX);
}

static process_t *alloc_proc(void) {
    for (int i = 0; i < PROC_MAX; i++) {
        if (proc_table[i].state == PROC_UNUSED) {
            return &proc_table[i];
        }
    }
    return NULL;
}

// カーネルプロセス/スレッドの生成
// user=false: カーネルスタックのみ使用
process_t *proc_create(const char *name, void (*entry)(void), bool user) {
    (void)user; // 現在はカーネルモードのみ

    process_t *p = alloc_proc();
    if (!p) {
        kprintf("[PROC] table full!\n");
        return NULL;
    }

    // カーネルスタックを確保
    void *kstack = kmalloc(KSTACK_SIZE);
    if (!kstack) {
        kprintf("[PROC] no memory for stack\n");
        return NULL;
    }

    uint64_t kstack_top = (uint64_t)kstack + KSTACK_SIZE;

    // コンテキストの初期化
    // switch_context が ret したときに entry が呼ばれるよう設定
    p->context.r15 = 0;
    p->context.r14 = 0;
    p->context.r13 = 0;
    p->context.r12 = 0;
    p->context.rbx = 0;
    p->context.rbp = 0;
    p->context.rip = (uint64_t)entry;  // retの戻り先 = エントリポイント
    p->context.rsp = kstack_top;
    // カーネルスタックにダミーのリターンアドレスを積む
    // switch_contextはRIPをスタックから取るので[rsp]にエントリを書く
    // switch_context内で `mov [rsp], rax; ret` するため、
    // RSPをentry直前のアドレスに設定する
    uint64_t *sp = (uint64_t*)kstack_top;
    sp--;
    *sp = (uint64_t)entry;    // 最初の ret の戻り先
    // context の rip はこの [rsp] 値として使われる
    // switch_context: mov rax,[rsi+48]; mov [rsp],rax; ret
    // なので context.rip は実際には使われず [rsp] が使われる
    // 上のスタック設定を使うため context 側はスタックアドレスで上書き
    // ただし switch_context は `mov [rsp], rax` で上書きするので
    // 実際には context.rip を直接スタックのretに置く必要がある
    // → p->kernel_rsp を設定し、rsp レジスタを変えてから ret する方式に

    p->kernel_rsp = (uint64_t)sp;

    // 実際の switch_context は rsp を変更しないため、
    // 初回起動時は別途スタックを設定する必要がある
    // ここでは「初回はproc_startを経由」する方式
    // context.rip = proc_start; proc_start が entry を呼ぶ
    // しかし proc_start は上で設定した entry をどこから参照する?
    // 簡単な解決: context に entry を保存し、汎用レジスタで渡す
    p->context.r12 = (uint64_t)entry;  // proc_entrypoint に渡す引数

    // proc_entrypoint を初回起動先にする
    extern void proc_entrypoint(void);
    p->context.rip = (uint64_t)proc_entrypoint;

    // TSSのRSP0 (カーネルスタックトップ = 割り込み時に使用)
    p->kernel_rsp = kstack_top;

    p->pid   = next_pid++;
    p->ppid  = current_proc ? current_proc->pid : 0;
    p->state = PROC_READY;
    p->exit_code = 0;
    p->sleep_ticks = 0;
    p->msg_queue = NULL;
    p->msg_tail  = NULL;
    p->msg_count = 0;

    // 名前コピー
    size_t i = 0;
    while (i < 15 && name[i]) { p->name[i] = name[i]; i++; }
    p->name[i] = '\0';

    kprintf("[PROC] created '%s' pid=%d\n", p->name, p->pid);
    return p;
}

process_t *proc_find(pid_t pid) {
    for (int i = 0; i < PROC_MAX; i++)
        if (proc_table[i].state != PROC_UNUSED && proc_table[i].pid == pid)
            return &proc_table[i];
    return NULL;
}

void proc_exit(int code) {
    if (!current_proc) return;
    current_proc->state = PROC_ZOMBIE;
    current_proc->exit_code = code;
    kprintf("[PROC] pid=%d exited code=%d\n", current_proc->pid, code);
    schedule(); // 他のプロセスへ
}

void proc_yield(void) {
    schedule();
}

void proc_sleep(uint64_t ms) {
    if (!current_proc) return;
    current_proc->sleep_ticks = (ms * PIT_HZ) / 1000;
    if (current_proc->sleep_ticks == 0) current_proc->sleep_ticks = 1;
    current_proc->state = PROC_SLEEPING;
    schedule();
}

void proc_wakeup(process_t *p) {
    if (p && p->state == PROC_SLEEPING) {
        p->state = PROC_READY;
    }
}

// ── スケジューラ (ラウンドロビン) ──────────────────────
// タイマー割り込み毎に呼ばれる
void sched_tick(void) {
    // スリーププロセスのカウントダウン
    for (int i = 0; i < PROC_MAX; i++) {
        process_t *p = &proc_table[i];
        if (p->state == PROC_SLEEPING) {
            if (p->sleep_ticks > 0) p->sleep_ticks--;
            if (p->sleep_ticks == 0) p->state = PROC_READY;
        }
    }
    // タイムスライス終了でスケジュール (10tick = 100ms)
    if (current_proc && (ticks % 10) == 0) schedule();
}

// 次のREADYプロセスを選択してスイッチ
void schedule(void) {
    if (!current_proc) return;

    // 現在のプロセスがRUNNINGならREADYに戻す
    if (current_proc->state == PROC_RUNNING)
        current_proc->state = PROC_READY;

    process_t *old = current_proc;
    int start = old - proc_table;

    // ラウンドロビン: 次のREADYを探す
    for (int n = 1; n <= PROC_MAX; n++) {
        int idx = (start + n) % PROC_MAX;
        process_t *p = &proc_table[idx];
        if (p->state == PROC_READY) {
            current_proc = p;
            p->state = PROC_RUNNING;
            gdt_set_tss_rsp0(p->kernel_rsp);
            switch_context(&old->context, &p->context);
            return;
        }
    }

    // READYなし = アイドル (現在プロセスを継続)
    if (old->state == PROC_READY) {
        old->state = PROC_RUNNING;
    }
}
