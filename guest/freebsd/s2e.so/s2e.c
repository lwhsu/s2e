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


#include <elf.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <s2e/monitors/freebsd.h>
#include <s2e/s2e.h>

#include "s2e_so.h"

#define s2e_printf printf

struct load_modules_ctx {
    char exe_path[PATH_MAX];
    int index;
};

/// Full path of the running executable, as the kernel reported it to the monitor at exec time.
static int get_exe_path(char *buf, size_t size) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    size_t len = size;
    if (sysctl(mib, 4, buf, &len, NULL, 0) < 0) {
        return -1;
    }
    return 0;
}

static unsigned pflags_to_prot(unsigned p_flags) {
    unsigned prot = 0;
    if (p_flags & PF_R) {
        prot |= 1; // PROT_READ
    }
    if (p_flags & PF_W) {
        prot |= 2; // PROT_WRITE
    }
    if (p_flags & PF_X) {
        prot |= 4; // PROT_EXEC
    }
    return prot;
}

/// Report one ELF object (from dl_iterate_phdr) to the FreeBSDMonitor plugin.
static void s2e_load_module(const struct dl_phdr_info *info, const char *path) {
    unsigned loadable = 0;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        if (info->dlpi_phdr[i].p_type == PT_LOAD) {
            ++loadable;
        }
    }

    if (!loadable) {
        return;
    }

    size_t phdr_size = loadable * sizeof(struct S2E_FREEBSDMON_PHDR_DESC);
    struct S2E_FREEBSDMON_PHDR_DESC *phdr = calloc(1, phdr_size);
    if (!phdr) {
        s2e_printf("Could not allocate memory for the module load command\n");
        return;
    }

    // The ELF header lives at the start of the segment that maps file offset 0
    uint64_t entry_point = 0;
    unsigned j = 0;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const Elf_Phdr *p = &info->dlpi_phdr[i];
        if (p->p_type != PT_LOAD) {
            continue;
        }

        uintptr_t start = info->dlpi_addr + p->p_vaddr;
        uintptr_t page_start = start & ~(uintptr_t) (PAGE_SIZE - 1);
        uintptr_t end = start + p->p_memsz;
        uintptr_t page_end = (end + PAGE_SIZE - 1) & ~(uintptr_t) (PAGE_SIZE - 1);

        if (p->p_offset == 0 && !entry_point) {
            const Elf_Ehdr *ehdr = (const Elf_Ehdr *) start;
            entry_point = info->dlpi_addr + ehdr->e_entry;
        }

        phdr[j].index = i;
        phdr[j].vma = page_start;
        phdr[j].p_type = p->p_type;
        phdr[j].p_offset = p->p_offset;
        phdr[j].p_vaddr = p->p_vaddr;
        phdr[j].p_paddr = p->p_paddr;
        phdr[j].p_filesz = p->p_filesz;
        phdr[j].p_memsz = p->p_memsz;
        phdr[j].p_flags = p->p_flags;
        phdr[j].p_align = p->p_align;

        phdr[j].mmap.address = page_start;
        phdr[j].mmap.size = page_end - page_start;
        phdr[j].mmap.prot = pflags_to_prot(p->p_flags);
        phdr[j].mmap.flag = 0;
        phdr[j].mmap.pgoff = p->p_offset & ~(uint64_t) (PAGE_SIZE - 1);
        ++j;
    }

    struct S2E_FREEBSDMON_COMMAND_MODULE_LOAD load;
    load.module_path = (uintptr_t) path;
    load.entry_point = entry_point;
    load.phdr = (uintptr_t) phdr;
    load.phdr_size = phdr_size;

    s2e_printf("Loading module %s base=%#lx entry=%#lx segments=%u\n", path, (unsigned long) info->dlpi_addr,
               (unsigned long) entry_point, loadable);

    s2e_freebsd_load_module(&load);

    free(phdr);
}

static int load_modules_cb(struct dl_phdr_info *info, size_t size, void *data) {
    struct load_modules_ctx *ctx = data;
    (void) size;

    const char *path = info->dlpi_name;
    int is_main = ctx->index == 0;
    ++ctx->index;

    if (!path || !path[0]) {
        path = is_main ? ctx->exe_path : NULL;
    }

    if (!path) {
        return 0;
    }

    // The kernel module already reported the main executable at exec time
    if (is_main || !strcmp(path, ctx->exe_path)) {
        s2e_printf("Skipping %s, because it was already notified by the kernel\n", path);
        return 0;
    }

    s2e_load_module(info, path);
    return 0;
}

/// Report every ELF object mapped in the process (except the main executable) to the monitor.
void s2e_load_modules(void) {
    struct load_modules_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));

    if (get_exe_path(ctx.exe_path, sizeof(ctx.exe_path)) < 0) {
        s2e_printf("Could not read the current process path\n");
    }

    dl_iterate_phdr(load_modules_cb, &ctx);
}
