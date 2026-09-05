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
/// Fault injection into the bus_dma implementation methods: bus_dmamem_alloc()
/// and bus_dmamap_create() are inline wrappers on x86, the hooks sit on the
/// bus_dma_bounce_impl methods and must catch the indirect calls made here.
/// Triggered by sysctl debug.dmatest.run=1 after the module is loaded.
///
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <s2e/s2e.h>

static void dmatest_run(void) {
    bus_dma_tag_t tag = NULL;
    bus_dmamap_t map = NULL, map2 = NULL;
    void *vaddr = NULL;
    int error;

    s2e_message("dmatest: start");

    error = bus_dma_tag_create(NULL, 16, 0, BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, 4096, 1, 4096, 0,
                               NULL, NULL, &tag);
    if (error != 0) {
        s2e_printf("dmatest: bus_dma_tag_create failed %d", error);
        goto done;
    }

    error = bus_dmamem_alloc(tag, &vaddr, BUS_DMA_NOWAIT | BUS_DMA_ZERO, &map);
    if (error != 0) {
        s2e_printf("dmatest: bus_dmamem_alloc failed %d, taking the error path", error);
        goto destroy;
    }
    s2e_message("dmatest: bus_dmamem_alloc ok");

    error = bus_dmamap_create(tag, 0, &map2);
    if (error != 0) {
        s2e_printf("dmatest: bus_dmamap_create failed %d, taking the error path", error);
    } else {
        s2e_message("dmatest: bus_dmamap_create ok");
        bus_dmamap_destroy(tag, map2);
    }

    bus_dmamem_free(tag, vaddr, map);
destroy:
    bus_dma_tag_destroy(tag);
done:
    s2e_message("dmatest: done");
}

static int dmatest_sysctl_run(SYSCTL_HANDLER_ARGS) {
    int v = 0, error;

    error = sysctl_handle_int(oidp, &v, 0, req);
    if (error != 0 || req->newptr == NULL) {
        return error;
    }
    if (v != 0) {
        dmatest_run();
    }
    return 0;
}

SYSCTL_NODE(_debug, OID_AUTO, dmatest, CTLFLAG_RW | CTLFLAG_MPSAFE, 0, "S2E bus_dma hook test");
SYSCTL_PROC(_debug_dmatest, OID_AUTO, run, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0, dmatest_sysctl_run, "I",
            "Run the test");

static int dmatest_modevent(module_t mod, int type, void *data) {
    (void) mod;
    (void) data;
    switch (type) {
        case MOD_LOAD:
        case MOD_UNLOAD:
            return 0;
        default:
            return EOPNOTSUPP;
    }
}

static moduledata_t dmatest_mod = {"dmatest", dmatest_modevent, NULL};
DECLARE_MODULE(dmatest, dmatest_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(dmatest, 1);
