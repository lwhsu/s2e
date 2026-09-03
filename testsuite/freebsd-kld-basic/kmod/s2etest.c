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

///
/// Minimal kernel module exercising the three mechanisms used to test FreeBSD
/// drivers under S2E: symbolic port I/O, symbolic MMIO (SymbolicHardware) and
/// fault injection into kernel API calls (s2e.ko + GuestCodeHooking).
///
/// Expected paths when everything is enabled: 2 (port) x 2 (MMIO) x 2 (malloc
/// fault, first call site only) = 8.
///

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cpufunc.h>

#include <s2e/s2e.h>

#define S2ETEST_PORT 0x1004
#define S2ETEST_MMIO_PADDR 0xfebf0000UL

static MALLOC_DEFINE(M_S2ETEST, "s2etest", "S2E kld test");

static void s2etest_run(void) {
    uint8_t port_value;
    volatile uint32_t *mmio;
    uint32_t mmio_value;
    void *buffer;

    s2e_message("s2etest: start");

    // 1. Symbolic port I/O
    port_value = inb(S2ETEST_PORT);
    if (port_value == 0x42) {
        s2e_message("s2etest: port value is 0x42");
    } else {
        s2e_message("s2etest: port value is not 0x42");
    }

    // 2. Symbolic MMIO
    mmio = pmap_mapdev(S2ETEST_MMIO_PADDR, PAGE_SIZE);
    if (mmio == NULL) {
        s2e_kill_state(1, "s2etest: could not map the MMIO region");
        return;
    }
    mmio_value = *mmio;
    if (mmio_value & 1) {
        s2e_message("s2etest: mmio bit 0 is set");
    } else {
        s2e_message("s2etest: mmio bit 0 is clear");
    }
    pmap_unmapdev((void *) (uintptr_t) mmio, PAGE_SIZE);

    // 3. Fault injection into malloc(M_NOWAIT)
    buffer = malloc(64, M_S2ETEST, M_NOWAIT);
    if (buffer == NULL) {
        s2e_message("s2etest: malloc failed, taking the error path");
    } else {
        s2e_message("s2etest: malloc succeeded");
        free(buffer, M_S2ETEST);
    }

    s2e_message("s2etest: done");
    s2e_kill_state(0, "s2etest done");
}

///
/// The work is triggered by writing 1 to debug.s2etest.run instead of running
/// in MOD_LOAD: the kernel only announces a module (kld_load) after its
/// MOD_LOAD handler returned, and the analysis wants the module known to S2E
/// (coverage, fault injection hooks) when its code runs. Real drivers are
/// handled the same way: load them with their device disabled (devctl
/// disable), then devctl enable/attach.
///
static int s2etest_sysctl_run(SYSCTL_HANDLER_ARGS) {
    int value = 0, error;

    error = sysctl_handle_int(oidp, &value, 0, req);
    if (error != 0 || req->newptr == NULL) {
        return error;
    }

    if (value != 0) {
        s2etest_run();
    }

    return 0;
}

SYSCTL_NODE(_debug, OID_AUTO, s2etest, CTLFLAG_RW | CTLFLAG_MPSAFE, 0, "S2E kld test");
SYSCTL_PROC(_debug_s2etest, OID_AUTO, run, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0, s2etest_sysctl_run, "I",
            "Run the test");

static int s2etest_modevent(module_t mod, int type, void *data) {
    (void) mod;
    (void) data;

    switch (type) {
        case MOD_LOAD:
            printf("s2etest: loaded, trigger with sysctl debug.s2etest.run=1\n");
            return 0;
        case MOD_UNLOAD:
            return 0;
        default:
            return EOPNOTSUPP;
    }
}

static moduledata_t s2etest_mod = {"s2etest", s2etest_modevent, NULL};

DECLARE_MODULE(s2etest, s2etest_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(s2etest, 1);
