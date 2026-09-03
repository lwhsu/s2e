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
/// A module that panics on one of two paths. The test checks that S2E learns
/// about the panic (FreeBSDMonitor terminates the state with "Kernel panic")
/// instead of the guest waiting at the ddb prompt, and that the other path
/// completes.
///

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <s2e/s2e.h>

static void s2epanic_run(void) {
    int value = 0;

    s2e_make_symbolic(&value, sizeof(value), "s2epanic_value");

    if (value == 0x42) {
        panic("s2epanic: deliberate panic");
    }

    s2e_message("s2epanic: no panic on this path");
    s2e_kill_state(0, "s2epanic: done");
}

static int s2epanic_sysctl_run(SYSCTL_HANDLER_ARGS) {
    int value = 0, error;

    error = sysctl_handle_int(oidp, &value, 0, req);
    if (error != 0 || req->newptr == NULL) {
        return error;
    }

    if (value != 0) {
        s2epanic_run();
    }

    return 0;
}

SYSCTL_NODE(_debug, OID_AUTO, s2epanic, CTLFLAG_RW | CTLFLAG_MPSAFE, 0, "S2E kld panic test");
SYSCTL_PROC(_debug_s2epanic, OID_AUTO, run, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0, s2epanic_sysctl_run,
            "I", "Run the test");

static int s2epanic_modevent(module_t mod, int type, void *data) {
    (void) mod;
    (void) data;

    switch (type) {
        case MOD_LOAD:
            printf("s2epanic: loaded, trigger with sysctl debug.s2epanic.run=1\n");
            return 0;
        case MOD_UNLOAD:
            return 0;
        default:
            return EOPNOTSUPP;
    }
}

static moduledata_t s2epanic_mod = {"s2epanic", s2epanic_modevent, NULL};

DECLARE_MODULE(s2epanic, s2epanic_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(s2epanic, 1);
