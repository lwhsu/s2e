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

///
/// StackMonitor bound of a libthr thread whose stack grew past the initial
/// sgrowsiz entry before the monitor first saw the thread: the bound must be
/// the top of the chain of GROWS_DOWN entries, not the end of the entry that
/// held the stack pointer.
///
#include <inttypes.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <string.h>
#include <sys/thr.h>

#include <s2e/monitors/support/thread_execution_detector.h>
#include <s2e/s2e.h>

#define FRAME_BYTES 8192
#define DEPTH 48 /* 48 * 8 KB = 384 KB > sgrowsiz (128 KB): at least two growth entries */

static uintptr_t stack_lo, stack_hi;

static __attribute__((noinline)) void tracked_leaf(int n) {
    volatile char buf[64];
    buf[0] = (char) n;
    (void) buf;
}

static __attribute__((noinline)) void tracked_calls(const char *where) {
    long tid;
    thr_self(&tid);
    s2e_printf("stackgrow: tid=%ld %s sp~%p stack=[%#" PRIxPTR ",%#" PRIxPTR ")", tid, where, __builtin_frame_address(0),
               stack_lo, stack_hi);
    for (int i = 0; i < 4; ++i) {
        tracked_leaf(i);
    }
}

static __attribute__((noinline)) void deep(int n) {
    volatile char buf[FRAME_BYTES];
    memset((char *) buf, n, sizeof(buf));
    if (n > 0) {
        deep(n - 1);
        return;
    }
    /* Deepest frame: sp is far below the initial stack entry. Start tracking here. */
    s2e_thread_exec_enable_current();
    tracked_calls("deep");
}

static void *worker(void *arg) {
    pthread_attr_t attr;
    void *addr;
    size_t size;

    pthread_attr_init(&attr);
    pthread_attr_get_np(pthread_self(), &attr);
    pthread_attr_getstack(&attr, &addr, &size);
    pthread_attr_destroy(&attr);
    stack_lo = (uintptr_t) addr;
    stack_hi = (uintptr_t) addr + size;

    deep(DEPTH);
    /* Back near the top: the stack pointer is above the entry that held it before */
    tracked_calls("top");
    s2e_thread_exec_disable_current();
    return NULL;
}

int main(int argc, char **argv) {
    pthread_t t;
    if (pthread_create(&t, NULL, worker, NULL) != 0) {
        s2e_kill_state(1, "pthread_create failed");
    }
    pthread_join(t, NULL);
    s2e_kill_state(0, "stackgrow done");
    return 0;
}
