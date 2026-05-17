#include "kernel.h"

// IPC メッセージプール (静的確保)
#define IPC_POOL_SIZE 128
static ipc_message_t msg_pool[IPC_POOL_SIZE];
static bool          msg_used[IPC_POOL_SIZE];

static ipc_message_t *msg_alloc(void) {
    for (int i = 0; i < IPC_POOL_SIZE; i++) {
        if (!msg_used[i]) {
            msg_used[i] = true;
            msg_pool[i].next = NULL;
            return &msg_pool[i];
        }
    }
    return NULL;
}

static void msg_free(ipc_message_t *m) {
    if (!m) return;
    int i = (int)(m - msg_pool);
    if (i >= 0 && i < IPC_POOL_SIZE)
        msg_used[i] = false;
}

// メッセージを送信: dst宛のキューに積む
int ipc_send(pid_t dst, uint32_t type, const void *data, uint32_t len) {
    process_t *target = proc_find(dst);
    if (!target) return -ESRCH;

    if (len > IPC_MSG_MAX) return -EINVAL;

    ipc_message_t *m = msg_alloc();
    if (!m) return -ENOMEM;

    m->from = current_proc ? current_proc->pid : 0;
    m->type = type;
    m->len  = len;
    if (len > 0 && data) {
        // memcpy
        uint8_t *dst8 = m->data;
        const uint8_t *src8 = (const uint8_t*)data;
        for (uint32_t i = 0; i < len; i++) dst8[i] = src8[i];
    }

    // 割り込み禁止でキューに追加
    uint64_t flags = save_flags(); cli();

    m->next = NULL;
    if (!target->msg_tail) {
        target->msg_queue = target->msg_tail = m;
    } else {
        target->msg_tail->next = m;
        target->msg_tail = m;
    }
    target->msg_count++;

    // スリープ中のターゲットを起こす
    if (target->state == PROC_SLEEPING)
        proc_wakeup(target);

    restore_flags(flags);
    return 0;
}

// メッセージを受信 (キューが空ならスリープ)
int ipc_recv(ipc_message_t *out) {
    if (!current_proc || !out) return -EINVAL;

    while (true) {
        uint64_t flags = save_flags(); cli();

        if (current_proc->msg_queue) {
            ipc_message_t *m = current_proc->msg_queue;
            current_proc->msg_queue = m->next;
            if (!current_proc->msg_queue)
                current_proc->msg_tail = NULL;
            current_proc->msg_count--;

            // outにコピー
            out->from = m->from;
            out->type = m->type;
            out->len  = m->len;
            uint8_t *dst = out->data;
            uint8_t *src = m->data;
            for (uint32_t i = 0; i < m->len; i++) dst[i] = src[i];
            out->next = NULL;

            msg_free(m);
            restore_flags(flags);
            return 0;
        }

        restore_flags(flags);
        // キューが空: スリープして待つ (1tick)
        proc_sleep(10);
    }
}
