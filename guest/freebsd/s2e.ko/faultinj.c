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
/// Multi-path fault injection for FreeBSD kernel modules.
///
/// The GuestCodeHooking plugin redirects the calls that the analyzed modules
/// (moduleNames in s2e-config.lua) make to the kernel APIs below to the hooks
/// in this file. A hook forks the execution: one path calls the original API,
/// the other returns the failure value, so the module's error recovery code
/// gets exercised. Each call site is exercised once (KeyValueStore) unless
/// debug.s2e.faultinj_overapproximate is set.
///
/// Calls made by this module are never redirected (it is not in moduleNames),
/// so the hooks can call the original functions.
///

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>

#include <vm/vm.h>
#include <vm/uma.h>

#include <s2e/s2e.h>
#include <s2e/monitors/support/guest_code_hooking.h>
#include <s2e/monitors/support/key_value_store.h>

#include "s2e_mod.h"

static int s2e_faultinj_active = 0;
static int s2e_faultinj_overapproximate = 0;

SYSCTL_NODE(_debug, OID_AUTO, s2e, CTLFLAG_RW | CTLFLAG_MPSAFE, 0, "S2E guest monitor");
SYSCTL_INT(_debug_s2e, OID_AUTO, faultinj_active, CTLFLAG_RW, &s2e_faultinj_active, 0,
           "Inject faults into the kernel API calls of the analyzed modules");
SYSCTL_INT(_debug_s2e, OID_AUTO, faultinj_overapproximate, CTLFLAG_RW, &s2e_faultinj_overapproximate, 0,
           "Inject faults at every call instead of once per call site");

///
/// Decide whether to inject a fault for this call. Returns 1 on the path
/// that must return the failure value.
///
static int s2e_faultinj_decide(const char *function, void *callsite) {
    char key[96];
    uint64_t exercised = 0;
    uint8_t invoke_original = 1;

    if (!s2e_faultinj_active) {
        return 0;
    }

    snprintf(key, sizeof(key), "FaultInj_%s_%p", function, callsite);

    // Exercise each call site once, in the first state that reaches it
    if (!s2e_faultinj_overapproximate) {
        if (s2e_kvs_get_int(key, &exercised, 0) && exercised) {
            return 0;
        }
        s2e_kvs_put_int(key, 1, 0, NULL);
    }

    snprintf(key, sizeof(key), "FaultInjInvokeOrig_%s_%p", function, callsite);
    s2e_make_symbolic(&invoke_original, sizeof(invoke_original), key);

    if (invoke_original) {
        return 0;
    }

    s2e_printf("s2e: injecting fault into %s called from %p\n", function, callsite);
    return 1;
}

#define CALLSITE __builtin_return_address(0)

static void *s2e_hook_malloc(size_t size, struct malloc_type *type, int flags) {
    // M_WAITOK allocations never fail
    if ((flags & M_NOWAIT) && s2e_faultinj_decide("malloc", CALLSITE)) {
        return NULL;
    }
    return malloc(size, type, flags);
}

static void *s2e_hook_contigmalloc(unsigned long size, struct malloc_type *type, int flags, vm_paddr_t low,
                                   vm_paddr_t high, unsigned long alignment, vm_paddr_t boundary) {
    if (s2e_faultinj_decide("contigmalloc", CALLSITE)) {
        return NULL;
    }
    return contigmalloc(size, type, flags, low, high, alignment, boundary);
}

static void *s2e_hook_uma_zalloc_arg(uma_zone_t zone, void *arg, int flags) {
    if ((flags & M_NOWAIT) && s2e_faultinj_decide("uma_zalloc_arg", CALLSITE)) {
        return NULL;
    }
    return uma_zalloc_arg(zone, arg, flags);
}

/* 16.0-CURRENT passes the resource id by value (__FreeBSD_version 1600005, commit 575639548c). */
#if __FreeBSD_version >= 1600005
typedef int s2e_rid_t;
#else
typedef int *s2e_rid_t;
#endif

static struct resource *s2e_hook_bus_alloc_resource(device_t dev, int type, s2e_rid_t rid, rman_res_t start,
                                                    rman_res_t end, rman_res_t count, u_int flags) {
    if (s2e_faultinj_decide("bus_alloc_resource", CALLSITE)) {
        return NULL;
    }
    return bus_alloc_resource(dev, type, rid, start, end, count, flags);
}

static int s2e_hook_bus_setup_intr(device_t dev, struct resource *r, int flags, driver_filter_t filter,
                                   driver_intr_t handler, void *arg, void **cookiep) {
    if (s2e_faultinj_decide("bus_setup_intr", CALLSITE)) {
        return ENXIO;
    }
    return bus_setup_intr(dev, r, flags, filter, handler, arg, cookiep);
}

static int s2e_hook_bus_dma_tag_create(bus_dma_tag_t parent, bus_size_t alignment, bus_addr_t boundary,
                                       bus_addr_t lowaddr, bus_addr_t highaddr, bus_dma_filter_t *filter,
                                       void *filterarg, bus_size_t maxsize, int nsegments, bus_size_t maxsegsz,
                                       int flags, bus_dma_lock_t *lockfunc, void *lockfuncarg, bus_dma_tag_t *dmat) {
    if (s2e_faultinj_decide("bus_dma_tag_create", CALLSITE)) {
        return ENOMEM;
    }
    return bus_dma_tag_create(parent, alignment, boundary, lowaddr, highaddr, filter, filterarg, maxsize, nsegments,
                              maxsegsz, flags, lockfunc, lockfuncarg, dmat);
}

static int s2e_hook_bus_dmamem_alloc(bus_dma_tag_t dmat, void **vaddr, int flags, bus_dmamap_t *mapp) {
    if (s2e_faultinj_decide("bus_dmamem_alloc", CALLSITE)) {
        return ENOMEM;
    }
    return bus_dmamem_alloc(dmat, vaddr, flags, mapp);
}

static int s2e_hook_bus_dmamap_create(bus_dma_tag_t dmat, int flags, bus_dmamap_t *mapp) {
    if (s2e_faultinj_decide("bus_dmamap_create", CALLSITE)) {
        return ENOMEM;
    }
    return bus_dmamap_create(dmat, flags, mapp);
}

static struct taskqueue *s2e_hook_taskqueue_create(const char *name, int mflags, taskqueue_enqueue_fn enqueue,
                                                   void *context) {
    if (s2e_faultinj_decide("taskqueue_create", CALLSITE)) {
        return NULL;
    }
    return taskqueue_create(name, mflags, enqueue, context);
}

struct s2e_hook {
    const char *name;
    void *original;
    void *hook;
};

static const struct s2e_hook s2e_hooks[] = {
    {"malloc", (void *) malloc, (void *) s2e_hook_malloc},
    {"contigmalloc", (void *) contigmalloc, (void *) s2e_hook_contigmalloc},
    {"uma_zalloc_arg", (void *) uma_zalloc_arg, (void *) s2e_hook_uma_zalloc_arg},
    {"bus_alloc_resource", (void *) bus_alloc_resource, (void *) s2e_hook_bus_alloc_resource},
    {"bus_setup_intr", (void *) bus_setup_intr, (void *) s2e_hook_bus_setup_intr},
    {"bus_dma_tag_create", (void *) bus_dma_tag_create, (void *) s2e_hook_bus_dma_tag_create},
    {"bus_dmamem_alloc", (void *) bus_dmamem_alloc, (void *) s2e_hook_bus_dmamem_alloc},
    {"bus_dmamap_create", (void *) bus_dmamap_create, (void *) s2e_hook_bus_dmamap_create},
    {"taskqueue_create", (void *) taskqueue_create, (void *) s2e_hook_taskqueue_create},
};

void s2e_faultinj_init(void) {
    unsigned i;

    if (!s2e_plugin_loaded("GuestCodeHooking")) {
        printf("s2e: GuestCodeHooking plugin not loaded, fault injection unavailable\n");
        return;
    }

    for (i = 0; i < nitems(s2e_hooks); ++i) {
        s2e_hook_call_site(0, (uintptr_t) s2e_hooks[i].original, (uintptr_t) s2e_hooks[i].hook);
    }

    printf("s2e: fault injection hooks registered (%u kernel APIs), enable with debug.s2e.faultinj_active=1\n",
           (unsigned) nitems(s2e_hooks));
}

void s2e_faultinj_fini(void) {
    unsigned i;

    if (!s2e_plugin_loaded("GuestCodeHooking")) {
        return;
    }

    for (i = 0; i < nitems(s2e_hooks); ++i) {
        s2e_unhook_call_site(0, (uintptr_t) s2e_hooks[i].original);
    }
}
