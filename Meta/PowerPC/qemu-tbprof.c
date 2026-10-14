/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// QEMU TCG plugin that counts executed guest instructions per translation block.
// Unlike sampling NIP through the monitor, which is biased towards instructions that end a
// translation block (mtmsr, tlbie, MMIO), this gives an exact instruction-count profile.
//
// Build (macOS with Homebrew QEMU and glib):
//   clang -O2 -shared -fPIC -undefined dynamic_lookup \
//       -I"$(brew --prefix qemu)/include" $(pkg-config --cflags glib-2.0) \
//       Meta/PowerPC/qemu-tbprof.c -o tbprof.dylib $(pkg-config --libs glib-2.0)
// Use:
//   qemu-system-ppc ... -plugin ./tbprof.dylib,ctl=/tmp/serenity-tbprof
//   mkdir -p /tmp/serenity-tbprof
//   touch /tmp/serenity-tbprof/reset    # zero the counters (consumed by the plugin)
//   ... exercise the guest ...
//   touch /tmp/serenity-tbprof/dump     # writes profile.txt, then creates "done"
// profile.txt lines are "<vaddr> <instructions per block> <executions>"; see symbolize-tbprof.py.

#include <qemu-plugin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

typedef struct {
    uint64_t vaddr;
    uint64_t insns;
    uint64_t execs;
} Block;

static GHashTable *blocks;
static GMutex lock;
static uint64_t ticks;
static const char *control_dir = "/tmp/serenity-tbprof";

static gint by_cost(gconstpointer a, gconstpointer b)
{
    const Block *x = *(Block *const *)a, *y = *(Block *const *)b;
    uint64_t cx = x->execs * x->insns, cy = y->execs * y->insns;
    return cx < cy ? 1 : cx > cy ? -1 : 0;
}

static void dump(const char *path)
{
    GPtrArray *list = g_ptr_array_new();
    GHashTableIter it;
    gpointer key, value;
    g_hash_table_iter_init(&it, blocks);
    while (g_hash_table_iter_next(&it, &key, &value))
        if (((Block *)value)->execs)
            g_ptr_array_add(list, value);
    g_ptr_array_sort(list, by_cost);
    FILE *f = fopen(path, "w");
    if (f) {
        for (guint i = 0; i < list->len; i++) {
            Block *b = list->pdata[i];
            fprintf(f, "%08" PRIx64 " %" PRIu64 " %" PRIu64 "\n", b->vaddr, b->insns, b->execs);
        }
        fclose(f);
    }
    g_ptr_array_free(list, TRUE);
}

static void check_control(void)
{
    char path[256];
    snprintf(path, sizeof path, "%s/reset", control_dir);
    if (access(path, F_OK) == 0) {
        GHashTableIter it;
        gpointer key, value;
        g_hash_table_iter_init(&it, blocks);
        while (g_hash_table_iter_next(&it, &key, &value))
            ((Block *)value)->execs = 0;
        unlink(path);
    }
    snprintf(path, sizeof path, "%s/dump", control_dir);
    if (access(path, F_OK) == 0) {
        char out[256];
        snprintf(out, sizeof out, "%s/profile.txt", control_dir);
        dump(out);
        unlink(path);
        snprintf(out, sizeof out, "%s/done", control_dir);
        FILE *f = fopen(out, "w");
        if (f)
            fclose(f);
    }
}

static void exec_cb(unsigned int vcpu, void *udata)
{
    ((Block *)udata)->execs++;
    if ((++ticks & ((1u << 18) - 1)) == 0) {
        g_mutex_lock(&lock);
        check_control();
        g_mutex_unlock(&lock);
    }
}

static void trans_cb(struct qemu_plugin_tb *tb, void *unused)
{
    uint64_t vaddr = qemu_plugin_tb_vaddr(tb);
    uint64_t insns = qemu_plugin_tb_n_insns(tb);
    uint64_t key = vaddr ^ (insns << 48);
    g_mutex_lock(&lock);
    Block *b = g_hash_table_lookup(blocks, &key);
    if (!b) {
        b = g_new0(Block, 1);
        b->vaddr = vaddr;
        b->insns = insns;
        uint64_t *k = g_new(uint64_t, 1);
        *k = key;
        g_hash_table_insert(blocks, k, b);
    }
    g_mutex_unlock(&lock);
    qemu_plugin_register_vcpu_tb_exec_cb(tb, exec_cb, QEMU_PLUGIN_CB_NO_REGS, b);
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc, char **argv)
{
    for (int i = 0; i < argc; ++i) {
        if (strncmp(argv[i], "ctl=", 4) == 0)
            control_dir = strdup(argv[i] + 4);
    }
    blocks = g_hash_table_new(g_int64_hash, g_int64_equal);
    qemu_plugin_register_vcpu_tb_trans_cb(id, trans_cb, NULL);
    return 0;
}
