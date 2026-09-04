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
/// s2e.ko: report process, thread and kernel module events of a FreeBSD guest
/// to the FreeBSDMonitor plugin. Only stable EVENTHANDLER(9) hooks are used,
/// so the module works with an unmodified kernel.
///

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/eventhandler.h>
#include <sys/elf.h>
#include <sys/imgact.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/stdint.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>

#include <machine/frame.h>
#include <machine/pte.h>
#include <machine/trap.h>

#include <s2e/s2e.h>
#include <s2e/monitors/commands/freebsd.h>

#include "s2e_mod.h"

static MALLOC_DEFINE(M_S2E, "s2e", "S2E guest monitor");

static eventhandler_tag s2e_exec_tag;
static eventhandler_tag s2e_exit_tag;
static eventhandler_tag s2e_thread_dtor_tag;
static eventhandler_tag s2e_kld_load_tag;
static eventhandler_tag s2e_kld_unload_tag;
static eventhandler_tag s2e_shutdown_tag;

static void s2e_init_command(struct S2E_FREEBSDMON_COMMAND *cmd, enum S2E_FREEBSDMON_COMMANDS command) {
    memset(cmd, 0, sizeof(*cmd));
    cmd->version = S2E_FREEBSDMON_COMMAND_VERSION;
    cmd->Command = command;
    cmd->CurrentTask.thread = (uintptr_t) curthread;
    cmd->CurrentTask.pid = curproc->p_pid;
    cmd->CurrentTask.tid = curthread->td_tid;
}

static void s2e_send(struct S2E_FREEBSDMON_COMMAND *cmd) {
    s2e_invoke_plugin("FreeBSDMonitor", cmd, sizeof(*cmd));
}

///
/// Report the main executable of a newly executed process. The ELF header and
/// program headers are read from the first page of the file that exec already
/// mapped (imgp->image_header).
///
static void s2e_report_main_module(struct image_params *imgp, const char *path) {
    const Elf_Ehdr *ehdr = (const Elf_Ehdr *) imgp->image_header;
    const Elf_Phdr *phdr;
    struct S2E_FREEBSDMON_PHDR_DESC *desc;
    struct S2E_FREEBSDMON_COMMAND cmd;
    unsigned loadable = 0, j = 0;
    int i;

    if (!IS_ELF(*ehdr) || ehdr->e_ident[EI_CLASS] != ELF_TARG_CLASS) {
        return;
    }

    // The process_exec handlers run from exec_new_vmspace(), before the segments are loaded. With ASLR the
    // load base of a PIE is not known yet (ET_DYN_ADDR_RAND sentinel), so the module cannot be described.
    if (imgp->et_dyn_addr == 1) {
        printf("s2e: %s uses a randomized load base, not reporting the module (disable kern.elf64.aslr)\n", path);
        return;
    }

    if (ehdr->e_phentsize != sizeof(Elf_Phdr) ||
        ehdr->e_phoff + (size_t) ehdr->e_phnum * sizeof(Elf_Phdr) > PAGE_SIZE) {
        printf("s2e: program headers of %s are not in the first page, not reporting the module\n", path);
        return;
    }

    phdr = (const Elf_Phdr *) (imgp->image_header + ehdr->e_phoff);
    for (i = 0; i < ehdr->e_phnum; ++i) {
        if (phdr[i].p_type == PT_LOAD) {
            ++loadable;
        }
    }

    if (!loadable) {
        return;
    }

    desc = malloc(loadable * sizeof(*desc), M_S2E, M_WAITOK | M_ZERO);

    for (i = 0; i < ehdr->e_phnum; ++i) {
        const Elf_Phdr *p = &phdr[i];
        uintptr_t start, page_start, end, page_end;

        if (p->p_type != PT_LOAD) {
            continue;
        }

        // et_dyn_addr is the load base of position-independent executables (0 otherwise)
        start = imgp->et_dyn_addr + p->p_vaddr;
        page_start = trunc_page(start);
        end = start + p->p_memsz;
        page_end = round_page(end);

        desc[j].index = i;
        desc[j].vma = page_start;
        desc[j].p_type = p->p_type;
        desc[j].p_offset = p->p_offset;
        desc[j].p_vaddr = p->p_vaddr;
        desc[j].p_paddr = p->p_paddr;
        desc[j].p_filesz = p->p_filesz;
        desc[j].p_memsz = p->p_memsz;
        desc[j].p_flags = p->p_flags;
        desc[j].p_align = p->p_align;

        desc[j].mmap.address = page_start;
        desc[j].mmap.size = page_end - page_start;
        desc[j].mmap.prot = ((p->p_flags & PF_R) ? VM_PROT_READ : 0) | ((p->p_flags & PF_W) ? VM_PROT_WRITE : 0) |
                            ((p->p_flags & PF_X) ? VM_PROT_EXECUTE : 0);
        desc[j].mmap.flag = 0;
        desc[j].mmap.pgoff = trunc_page(p->p_offset);
        ++j;
    }

    s2e_init_command(&cmd, FREEBSD_MODULE_LOAD);
    cmd.ModuleLoad.module_path = (uintptr_t) path;
    // imgp->entry_addr is not set yet at this point
    cmd.ModuleLoad.entry_point = imgp->et_dyn_addr + ehdr->e_entry;
    cmd.ModuleLoad.phdr = (uintptr_t) desc;
    cmd.ModuleLoad.phdr_size = loadable * sizeof(*desc);
    s2e_send(&cmd);

    free(desc, M_S2E);
}

static void s2e_process_exec(void *arg, struct proc *p, struct image_params *imgp) {
    struct S2E_FREEBSDMON_COMMAND cmd;
    const char *path;

    (void) arg;
    (void) p;

    path = imgp->execpath;
    if (path == NULL) {
        path = imgp->args != NULL ? imgp->args->fname : NULL;
    }
    if (path == NULL) {
        path = "";
    }

    s2e_init_command(&cmd, FREEBSD_PROCESS_LOAD);
    cmd.ProcessLoad.process_path = (uintptr_t) path;
    s2e_send(&cmd);

    s2e_report_main_module(imgp, path);
}

///
/// Whether the fatal signal came from a hardware trap whose frame is still in
/// td_frame. A signal delivered through a system call (abort(), kill(2),
/// thr_kill(2)) leaves the syscall frame there: fast_syscall only sets
/// tf_err to 2 and keeps the tf_trapno/tf_addr of an earlier trap, so those
/// fields must not be reported.
///
static int s2e_signal_from_trap(int sig, const struct trapframe *tf) {
    switch (sig) {
        case SIGSEGV:
        case SIGBUS:
            switch (tf->tf_trapno) {
                case T_PAGEFLT:
                    // A user page fault always has PGEX_U; the syscall marker (2) does not
                    return (tf->tf_err & PGEX_U) != 0;
                case T_PROTFLT:
                case T_SEGNPFLT:
                case T_STKFLT:
                case T_ALIGNFLT:
                    return tf->tf_err != 2;
                default:
                    return 0;
            }
        case SIGILL:
            return tf->tf_trapno == T_PRIVINFLT && tf->tf_err != 2;
        case SIGFPE:
            return (tf->tf_trapno == T_DIVIDE || tf->tf_trapno == T_ARITHTRAP || tf->tf_trapno == T_XMMFLT) &&
                   tf->tf_err != 2;
        case SIGTRAP:
            return (tf->tf_trapno == T_BPTFLT || tf->tf_trapno == T_TRCTRAP) && tf->tf_err != 2;
        default:
            // SIGABRT, SIGSYS and the rest never come from a trap
            return 0;
    }
}

static void s2e_process_exit(void *arg, struct proc *p) {
    struct S2E_FREEBSDMON_COMMAND cmd;
    struct trapframe *tf = curthread->td_frame;
    int sig = p->p_xsig;

    (void) arg;

    if (sig != 0 && !s2e_signal_from_trap(sig, tf)) {
        // Killed by a signal that was not a hardware trap: only the exit is reported
    } else if (sig == SIGSEGV || sig == SIGBUS) {
        s2e_init_command(&cmd, FREEBSD_SEGFAULT);
        cmd.SegFault.pc = tf->tf_rip;
        // The fault address is only known for page faults
        cmd.SegFault.address = tf->tf_trapno == T_PAGEFLT ? tf->tf_addr : 0;
        cmd.SegFault.fault = tf->tf_err;
        s2e_send(&cmd);
    } else if (sig == SIGILL || sig == SIGFPE || sig == SIGTRAP) {
        s2e_init_command(&cmd, FREEBSD_TRAP);
        cmd.Trap.pc = tf->tf_rip;
        cmd.Trap.trapnr = tf->tf_trapno;
        cmd.Trap.signr = sig;
        cmd.Trap.error_code = tf->tf_err;
        s2e_send(&cmd);
    }

    s2e_init_command(&cmd, FREEBSD_PROCESS_EXIT);
    cmd.ProcessExit.code = sig != 0 ? (uint64_t) (128 + sig) : (uint64_t) p->p_xexit;
    s2e_send(&cmd);
}

///
/// thread_dtor fires when the thread structure is reaped (thread_reap,
/// thread_free or the parent's wait4), not when the thread exits: the event
/// may arrive late and while another process is current. The kernel has no
/// EVENTHANDLER for the exit itself.
///
static void s2e_thread_dtor(void *arg, struct thread *td) {
    struct S2E_FREEBSDMON_COMMAND cmd;

    (void) arg;

    s2e_init_command(&cmd, FREEBSD_THREAD_EXIT);
    cmd.CurrentTask.thread = (uintptr_t) td;
    cmd.CurrentTask.tid = td->td_tid;
    cmd.CurrentTask.pid = td->td_proc != NULL ? td->td_proc->p_pid : 0;
    cmd.ThreadExit.code = 0;
    s2e_send(&cmd);
}

static void s2e_kld_load(void *arg, linker_file_t lf) {
    struct S2E_FREEBSDMON_COMMAND cmd;

    (void) arg;

    s2e_init_command(&cmd, FREEBSD_KLD_LOAD);
    cmd.KldLoad.path = (uintptr_t) (lf->pathname != NULL ? lf->pathname : "");
    cmd.KldLoad.name = (uintptr_t) (lf->filename != NULL ? lf->filename : "");
    cmd.KldLoad.address = (uintptr_t) lf->address;
    cmd.KldLoad.size = lf->size;
    s2e_send(&cmd);
}

static void s2e_kld_unload(void *arg, const char *filename, caddr_t address, size_t size) {
    struct S2E_FREEBSDMON_COMMAND cmd;

    (void) arg;
    (void) filename;
    (void) size;

    s2e_init_command(&cmd, FREEBSD_KLD_UNLOAD);
    cmd.KldUnload.address = (uintptr_t) address;
    s2e_send(&cmd);
}

static void s2e_shutdown_pre_sync(void *arg, int howto) {
    struct S2E_FREEBSDMON_COMMAND cmd;
    const char *msg;

    (void) arg;
    (void) howto;

    if (!KERNEL_PANICKED()) {
        return;
    }

    msg = panicstr != NULL ? panicstr : "panic";
    s2e_init_command(&cmd, FREEBSD_KERNEL_PANIC);
    cmd.KernelPanic.message = (uintptr_t) msg;
    cmd.KernelPanic.message_size = strlen(msg);
    s2e_send(&cmd);
}

///
/// Report a module that was loaded before s2e.ko (kld_load only fires for
/// later loads). The kernel and s2e.ko itself are skipped.
///
static int s2e_report_linker_file(linker_file_t lf, void *ctx) {
    struct S2E_FREEBSDMON_COMMAND cmd;

    if (lf == linker_kernel_file || lf == (linker_file_t) ctx) {
        return 0;
    }

    s2e_init_command(&cmd, FREEBSD_KLD_LOAD);
    cmd.KldLoad.path = (uintptr_t) (lf->pathname != NULL ? lf->pathname : "");
    cmd.KldLoad.name = (uintptr_t) (lf->filename != NULL ? lf->filename : "");
    cmd.KldLoad.address = (uintptr_t) lf->address;
    cmd.KldLoad.size = lf->size;
    s2e_send(&cmd);
    return 0;
}

static void s2e_send_init(linker_file_t self) {
    struct S2E_FREEBSDMON_COMMAND cmd;
    linker_file_t kf = linker_kernel_file;

    s2e_init_command(&cmd, FREEBSD_INIT);
    cmd.Init.kernel_start = VM_MIN_KERNEL_ADDRESS;
    cmd.Init.kernel_base = kf != NULL ? (uintptr_t) kf->address : 0;
    cmd.Init.kernel_size = kf != NULL ? kf->size : 0;
    cmd.Init.kernel_path = (uintptr_t) (kf != NULL && kf->pathname != NULL ? kf->pathname : "");
    cmd.Init.freebsd_version = __FreeBSD_version;
    cmd.Init.page_size = PAGE_SIZE;

    cmd.Init.pcpu_curthread = offsetof(struct pcpu, pc_curthread);
    cmd.Init.thread_proc = offsetof(struct thread, td_proc);
    cmd.Init.thread_tid = offsetof(struct thread, td_tid);
    cmd.Init.thread_kstack = offsetof(struct thread, td_kstack);
    cmd.Init.thread_kstack_pages = offsetof(struct thread, td_kstack_pages);
    cmd.Init.proc_pid = offsetof(struct proc, p_pid);
    cmd.Init.proc_comm = offsetof(struct proc, p_comm);
    cmd.Init.proc_comm_size = sizeof(((struct proc *) 0)->p_comm);
    cmd.Init.proc_vmspace = offsetof(struct proc, p_vmspace);
    cmd.Init.vmspace_maxsaddr = offsetof(struct vmspace, vm_maxsaddr);
    cmd.Init.vmspace_stacktop = offsetof(struct vmspace, vm_stacktop);
    cmd.Init.vmspace_map = offsetof(struct vmspace, vm_map);
    cmd.Init.map_header = offsetof(struct vm_map, header);
    cmd.Init.map_entry_left = offsetof(struct vm_map_entry, left);
    cmd.Init.map_entry_right = offsetof(struct vm_map_entry, right);
    cmd.Init.map_entry_start = offsetof(struct vm_map_entry, start);
    cmd.Init.map_entry_end = offsetof(struct vm_map_entry, end);
    cmd.Init.map_entry_eflags = offsetof(struct vm_map_entry, eflags);
    cmd.Init.map_entry_guard = MAP_ENTRY_GUARD;
    cmd.Init.map_entry_stack_gap = MAP_ENTRY_STACK_GAP;
    cmd.Init.map_entry_grows_down = MAP_ENTRY_GROWS_DOWN;

    s2e_send(&cmd);

    // Modules loaded before this one (preloaded or kldload'ed earlier)
    linker_file_foreach(s2e_report_linker_file, self);
}

static int s2e_modevent(module_t mod, int type, void *data) {
    (void) data;

    switch (type) {
        case MOD_LOAD:
            if (!s2e_check()) {
                printf("s2e: not running in S2E, the monitor will not be active\n");
                return 0;
            }

            s2e_send_init(module_file(mod));

            s2e_exec_tag = EVENTHANDLER_REGISTER(process_exec, s2e_process_exec, NULL, EVENTHANDLER_PRI_ANY);
            s2e_exit_tag = EVENTHANDLER_REGISTER(process_exit, s2e_process_exit, NULL, EVENTHANDLER_PRI_ANY);
            s2e_thread_dtor_tag = EVENTHANDLER_REGISTER(thread_dtor, s2e_thread_dtor, NULL, EVENTHANDLER_PRI_ANY);
            s2e_kld_load_tag = EVENTHANDLER_REGISTER(kld_load, s2e_kld_load, NULL, EVENTHANDLER_PRI_ANY);
            s2e_kld_unload_tag = EVENTHANDLER_REGISTER(kld_unload, s2e_kld_unload, NULL, EVENTHANDLER_PRI_ANY);
            s2e_shutdown_tag = EVENTHANDLER_REGISTER(shutdown_pre_sync, s2e_shutdown_pre_sync, NULL, SHUTDOWN_PRI_FIRST);
            printf("s2e: monitor loaded\n");

            s2e_faultinj_init();
            return 0;

        case MOD_UNLOAD:
            if (s2e_exec_tag != NULL) {
                s2e_faultinj_fini();
                EVENTHANDLER_DEREGISTER(process_exec, s2e_exec_tag);
                EVENTHANDLER_DEREGISTER(process_exit, s2e_exit_tag);
                EVENTHANDLER_DEREGISTER(thread_dtor, s2e_thread_dtor_tag);
                EVENTHANDLER_DEREGISTER(kld_load, s2e_kld_load_tag);
                EVENTHANDLER_DEREGISTER(kld_unload, s2e_kld_unload_tag);
                EVENTHANDLER_DEREGISTER(shutdown_pre_sync, s2e_shutdown_tag);
            }
            return 0;

        default:
            return EOPNOTSUPP;
    }
}

static moduledata_t s2e_mod = {"s2e", s2e_modevent, NULL};

DECLARE_MODULE(s2e, s2e_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(s2e, 1);
