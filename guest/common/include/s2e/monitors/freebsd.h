/// S2E Selective Symbolic Execution Platform
///
/// Copyright (c) 2026 Li-Wen Hsu
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
/// SOFTWARE.

#ifndef S2E_FREEBSD_MONITOR_H
#define S2E_FREEBSD_MONITOR_H

/// Userland helpers for talking to the FreeBSDMonitor plugin.
/// The kernel module has its own copy of this logic (guest/freebsd/s2e.ko).

#include <string.h>
#include <sys/thr.h>
#include <sys/types.h>
#include <unistd.h>

#include <s2e/s2e.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "commands/freebsd.h"

static inline uint64_t s2e_freebsd_gettid(void) {
    long tid = 0;
    thr_self(&tid);
    return (uint64_t) tid;
}

static inline void s2e_freebsd_init_command(struct S2E_FREEBSDMON_COMMAND *cmd, enum S2E_FREEBSDMON_COMMANDS command) {
    memset(cmd, 0, sizeof(*cmd));
    cmd->version = S2E_FREEBSDMON_COMMAND_VERSION;
    cmd->Command = command;
    cmd->CurrentTask.thread = 0;
    cmd->CurrentTask.pid = getpid();
    cmd->CurrentTask.tid = s2e_freebsd_gettid();
}

static inline void s2e_freebsd_load_module(const struct S2E_FREEBSDMON_COMMAND_MODULE_LOAD *m) {
    struct S2E_FREEBSDMON_COMMAND cmd;
    s2e_freebsd_init_command(&cmd, FREEBSD_MODULE_LOAD);
    cmd.ModuleLoad = *m;
    __s2e_touch_string((char *) (uintptr_t) cmd.ModuleLoad.module_path);
    __s2e_touch_buffer((void *) (uintptr_t) cmd.ModuleLoad.phdr, cmd.ModuleLoad.phdr_size);

    s2e_invoke_plugin("FreeBSDMonitor", &cmd, sizeof(cmd));
}

#ifdef __cplusplus
}
#endif

#endif
