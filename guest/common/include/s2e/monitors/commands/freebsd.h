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

#ifndef S2E_FREEBSD_COMMANDS_H
#define S2E_FREEBSD_COMMANDS_H

#ifdef __cplusplus
extern "C" {
#endif

/// Commands sent by the FreeBSD guest (the s2e.ko kernel module and the s2e.so
/// userland library) to the FreeBSDMonitor plugin.
///
/// The layout mirrors the Linux monitor commands where it makes sense so that
/// the host-side code can share helpers (e.g., the program header descriptors).

#define S2E_FREEBSDMON_COMMAND_VERSION 0x202609041000ULL // date +%Y%m%d%H%M

enum S2E_FREEBSDMON_COMMANDS {
    FREEBSD_INIT,
    FREEBSD_PROCESS_LOAD,
    FREEBSD_MODULE_LOAD,
    FREEBSD_PROCESS_EXIT,
    FREEBSD_THREAD_EXIT,
    FREEBSD_SEGFAULT,
    FREEBSD_TRAP,
    FREEBSD_KERNEL_PANIC,
    FREEBSD_KLD_LOAD,
    FREEBSD_KLD_UNLOAD
};

/// Sent once when s2e.ko is loaded. The struct offsets let the host read the
/// current thread and process from the guest without any context switch hook:
/// pcpu (through the gs base) -> pc_curthread -> td_proc -> p_pid.
struct S2E_FREEBSDMON_COMMAND_INIT {
    uint64_t kernel_start;  // VM_MIN_KERNEL_ADDRESS: addresses above this are kernel space
    uint64_t kernel_base;   // Runtime address of the kernel image (linker_kernel_file->address)
    uint64_t kernel_size;   // Size of the kernel image
    uint64_t kernel_path;   // Zero-terminated path of the kernel file (e.g., /boot/kernel/kernel)
    uint64_t freebsd_version; // __FreeBSD_version
    uint64_t page_size;

    uint64_t pcpu_curthread;      // offsetof(struct pcpu, pc_curthread)
    uint64_t thread_proc;         // offsetof(struct thread, td_proc)
    uint64_t thread_tid;          // offsetof(struct thread, td_tid)
    uint64_t thread_kstack;       // offsetof(struct thread, td_kstack)
    uint64_t thread_kstack_pages; // offsetof(struct thread, td_kstack_pages)
    uint64_t proc_pid;            // offsetof(struct proc, p_pid)
    uint64_t proc_comm;           // offsetof(struct proc, p_comm)
    uint64_t proc_comm_size;      // sizeof(((struct proc *) 0)->p_comm)
    uint64_t proc_vmspace;        // offsetof(struct proc, p_vmspace)
    uint64_t vmspace_maxsaddr;    // offsetof(struct vmspace, vm_maxsaddr)
    uint64_t vmspace_stacktop;    // offsetof(struct vmspace, vm_stacktop)
    uint64_t vmspace_map;         // offsetof(struct vmspace, vm_map)
    uint64_t map_header;          // offsetof(struct vm_map, header)
    uint64_t map_entry_left;      // offsetof(struct vm_map_entry, left)
    uint64_t map_entry_right;     // offsetof(struct vm_map_entry, right)
    uint64_t map_entry_start;     // offsetof(struct vm_map_entry, start)
    uint64_t map_entry_end;       // offsetof(struct vm_map_entry, end)
    uint64_t map_entry_eflags;    // offsetof(struct vm_map_entry, eflags)
    uint64_t map_entry_guard;     // MAP_ENTRY_GUARD
    uint64_t map_entry_stack_gap; // MAP_ENTRY_STACK_GAP
    uint64_t map_entry_grows_down; // MAP_ENTRY_GROWS_DOWN
} __attribute__((packed));

struct S2E_FREEBSDMON_COMMAND_MEMORY_MAP {
    uint64_t address;
    uint64_t size;
    uint64_t prot;
    uint64_t flag;
    uint64_t pgoff;
} __attribute__((packed));

/// Sent when a process is executed (process_exec event handler).
struct S2E_FREEBSDMON_COMMAND_PROCESS_LOAD {
    // Zero-terminated path to the executable
    uint64_t process_path;
} __attribute__((packed));

/// One loadable program header of an ELF object, with its runtime mapping.
struct S2E_FREEBSDMON_PHDR_DESC {
    uint64_t index;
    uint64_t vma;

    // Copy of the program header contents
    uint64_t p_type;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_flags;
    uint64_t p_align;

    struct S2E_FREEBSDMON_COMMAND_MEMORY_MAP mmap;
} __attribute__((packed));

/// Sent for the main executable by s2e.ko at exec time and for every shared
/// object by s2e.so when the process starts.
struct S2E_FREEBSDMON_COMMAND_MODULE_LOAD {
    uint64_t module_path;
    uint64_t entry_point;
    uint64_t phdr;      // Pointer to an array of S2E_FREEBSDMON_PHDR_DESC
    uint64_t phdr_size; // Size of the array in bytes
} __attribute__((packed));

struct S2E_FREEBSDMON_COMMAND_PROCESS_EXIT {
    uint64_t code;
} __attribute__((packed));

struct S2E_FREEBSDMON_COMMAND_THREAD_EXIT {
    uint64_t code;
} __attribute__((packed));

/// Sent when a process dies with SIGSEGV or SIGBUS.
struct S2E_FREEBSDMON_COMMAND_SEGFAULT {
    uint64_t pc;
    uint64_t address;
    uint64_t fault; // Trap error code
} __attribute__((packed));

/// Sent when a process dies with another synchronous signal (SIGILL, SIGFPE, SIGTRAP, ...).
struct S2E_FREEBSDMON_COMMAND_TRAP {
    uint64_t pc;
    int64_t trapnr;
    int64_t signr;
    int64_t error_code;
} __attribute__((packed));

struct S2E_FREEBSDMON_COMMAND_KERNEL_PANIC {
    uint64_t message;
    uint64_t message_size;
} __attribute__((packed));

/// Sent when a kernel module is loaded or unloaded (kld_load / kld_unload event handlers).
struct S2E_FREEBSDMON_COMMAND_KLD_LOAD {
    uint64_t path;    // Zero-terminated path of the module file
    uint64_t name;    // Zero-terminated short name (e.g., "if_em.ko")
    uint64_t address; // Runtime address of the linker file
    uint64_t size;
} __attribute__((packed));

struct S2E_FREEBSDMON_COMMAND_KLD_UNLOAD {
    uint64_t address;
} __attribute__((packed));

struct S2E_FREEBSDMON_TASK {
    uint64_t thread; // struct thread * (kernel commands) or 0
    uint64_t pid;
    uint64_t tid;
} __attribute__((packed));

struct S2E_FREEBSDMON_COMMAND {
    uint64_t version;
    enum S2E_FREEBSDMON_COMMANDS Command;
    struct S2E_FREEBSDMON_TASK CurrentTask;
    union {
        struct S2E_FREEBSDMON_COMMAND_INIT Init;
        struct S2E_FREEBSDMON_COMMAND_PROCESS_LOAD ProcessLoad;
        struct S2E_FREEBSDMON_COMMAND_MODULE_LOAD ModuleLoad;
        struct S2E_FREEBSDMON_COMMAND_PROCESS_EXIT ProcessExit;
        struct S2E_FREEBSDMON_COMMAND_THREAD_EXIT ThreadExit;
        struct S2E_FREEBSDMON_COMMAND_SEGFAULT SegFault;
        struct S2E_FREEBSDMON_COMMAND_TRAP Trap;
        struct S2E_FREEBSDMON_COMMAND_KERNEL_PANIC KernelPanic;
        struct S2E_FREEBSDMON_COMMAND_KLD_LOAD KldLoad;
        struct S2E_FREEBSDMON_COMMAND_KLD_UNLOAD KldUnload;
    };
} __attribute__((packed));

#ifdef __cplusplus
}
#endif

#endif
