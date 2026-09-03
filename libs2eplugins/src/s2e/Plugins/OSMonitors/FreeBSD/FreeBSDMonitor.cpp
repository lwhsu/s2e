///
/// Copyright (C) 2026, Li-Wen Hsu
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

#include <s2e/ConfigFile.h>
#include <s2e/FastReg.h>
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/Utils.h>
#include <s2e/cpu.h>

#include <llvm/Support/Path.h>

#include <sys/mman.h>

#include "FreeBSDMonitor.h"

using namespace klee;

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(FreeBSDMonitor, "Monitor for FreeBSD guests (s2e.ko and s2e.so)", "OSMonitor", "BaseInstructions",
                  "Vmi");

namespace {

///
/// The thread that was running the last time the monitor looked, per state.
/// Used to emit onProcessOrThreadSwitch without a context switch hook.
///
class FreeBSDMonitorState : public PluginState {
public:
    uint64_t m_lastThread = 0;

    virtual FreeBSDMonitorState *clone() const {
        return new FreeBSDMonitorState(*this);
    }

    static PluginState *factory(Plugin *p) {
        return new FreeBSDMonitorState();
    }
};

template <typename T> T &operator<<(T &stream, const S2E_FREEBSDMON_COMMANDS &c) {
    switch (c) {
        case FREEBSD_INIT:
            stream << "INIT";
            break;
        case FREEBSD_PROCESS_LOAD:
            stream << "PROCESS_LOAD";
            break;
        case FREEBSD_MODULE_LOAD:
            stream << "MODULE_LOAD";
            break;
        case FREEBSD_PROCESS_EXIT:
            stream << "PROCESS_EXIT";
            break;
        case FREEBSD_THREAD_EXIT:
            stream << "THREAD_EXIT";
            break;
        case FREEBSD_SEGFAULT:
            stream << "SEGFAULT";
            break;
        case FREEBSD_TRAP:
            stream << "TRAP";
            break;
        case FREEBSD_KERNEL_PANIC:
            stream << "KERNEL_PANIC";
            break;
        case FREEBSD_KLD_LOAD:
            stream << "KLD_LOAD";
            break;
        case FREEBSD_KLD_UNLOAD:
            stream << "KLD_UNLOAD";
            break;
        default:
            stream << "INVALID(" << (int) c << ")";
            break;
    }
    return stream;
}

} // namespace

void FreeBSDMonitor::initialize() {
    ConfigFile *cfg = s2e()->getConfig();

    m_vmi = s2e()->getPlugin<Vmi>();

    m_terminateOnSegfault = cfg->getBool(getConfigKey() + ".terminateOnSegfault", true);
    m_terminateOnTrap = cfg->getBool(getConfigKey() + ".terminateOnTrap", true);
    m_kernelPath = cfg->getString(getConfigKey() + ".kernelPath", "/boot/kernel/kernel");

    memset(&m_init, 0, sizeof(m_init));

    s2e()->getCorePlugin()->onPrivilegeChange.connect(sigc::mem_fun(*this, &FreeBSDMonitor::onPrivilegeChange));
}

///
/// There is no context switch notification from the guest. A switch to another
/// thread always goes through the kernel, so the current thread is compared
/// with the previous one whenever the CPU returns to user mode.
///
void FreeBSDMonitor::onPrivilegeChange(S2EExecutionState *state, unsigned previous, unsigned current) {
    if (current != 3 || !m_guestInitialized) {
        return;
    }

    checkTaskSwitch(state);
}

void FreeBSDMonitor::checkTaskSwitch(S2EExecutionState *state) {
    uint64_t thread = 0;
    if (!getCurrentThread(state, thread)) {
        return;
    }

    DECLARE_PLUGINSTATE(FreeBSDMonitorState, state);
    if (plgState->m_lastThread == thread) {
        return;
    }

    plgState->m_lastThread = thread;
    onProcessOrThreadSwitch.emit(state);
}

/*****************************************************************************/
/* Guest memory and register helpers                                         */
/*****************************************************************************/

bool FreeBSDMonitor::readGuestPointer(S2EExecutionState *state, uint64_t address, uint64_t &value) const {
    return state->mem()->read(address, &value, sizeof(value));
}

bool FreeBSDMonitor::readGuestInt32(S2EExecutionState *state, uint64_t address, uint32_t &value) const {
    return state->mem()->read(address, &value, sizeof(value));
}

///
/// Kernel data structures may live in the direct map (0xfffff800...), which is
/// below VM_MIN_KERNEL_ADDRESS, so pointers are only checked against the
/// canonical upper half of the address space.
///
static inline bool isKernelPointer(uint64_t p) {
    return p >= 0xffff800000000000ULL;
}

bool FreeBSDMonitor::isInKernelMode(S2EExecutionState *state) const {
    uint32_t cs = s2e_read_register_concrete_fast<uint32_t>(CPU_OFFSET(segs[R_CS].selector));
    return (cs & 3) == 0;
}

///
/// The kernel keeps the address of the per-CPU data in the gs base while in
/// kernel mode and in MSR_KERNELGSBASE while in user mode (swapgs).
///
bool FreeBSDMonitor::getCurrentThread(S2EExecutionState *state, uint64_t &thread) const {
    if (!m_guestInitialized) {
        return false;
    }

    uint64_t pcpu = 0;
    if (isInKernelMode(state)) {
        pcpu = s2e_read_register_concrete_fast<target_ulong>(CPU_OFFSET(segs[R_GS].base));
    } else {
#ifdef TARGET_X86_64
        pcpu = s2e_read_register_concrete_fast<target_ulong>(CPU_OFFSET(kernelgsbase));
#else
        // Only 64-bit FreeBSD guests are supported
        return false;
#endif
    }

    if (!isKernelPointer(pcpu)) {
        return false;
    }

    return readGuestPointer(state, pcpu + m_init.pcpu_curthread, thread) && isKernelPointer(thread);
}

bool FreeBSDMonitor::getCurrentProc(S2EExecutionState *state, uint64_t &proc) const {
    uint64_t thread;
    if (!getCurrentThread(state, thread)) {
        return false;
    }

    return readGuestPointer(state, thread + m_init.thread_proc, proc) && isKernelPointer(proc);
}

uint64_t FreeBSDMonitor::getPid(S2EExecutionState *state) {
    uint64_t proc;
    uint32_t pid;
    if (!getCurrentProc(state, proc) || !readGuestInt32(state, proc + m_init.proc_pid, pid)) {
        return -1;
    }
    return pid;
}

uint64_t FreeBSDMonitor::getTid(S2EExecutionState *state) {
    uint64_t thread;
    uint32_t tid;
    if (!getCurrentThread(state, thread) || !readGuestInt32(state, thread + m_init.thread_tid, tid)) {
        return -1;
    }
    return tid;
}

///
/// In kernel mode the stack is the kernel stack of the current thread. In user
/// mode it is the main thread's stack region of the vmspace (other threads'
/// stacks are allocated by libthr with mmap, which is not tracked yet).
///
bool FreeBSDMonitor::getCurrentStack(S2EExecutionState *state, uint64_t *base, uint64_t *size) {
    if (isInKernelMode(state)) {
        uint64_t thread, kstack;
        uint32_t pages;
        if (!getCurrentThread(state, thread) || !readGuestPointer(state, thread + m_init.thread_kstack, kstack) ||
            !readGuestInt32(state, thread + m_init.thread_kstack_pages, pages)) {
            return false;
        }

        *base = kstack;
        *size = (uint64_t) pages * m_init.page_size;
        return true;
    }

    uint64_t proc, vmspace, maxsaddr, stacktop;
    if (!getCurrentProc(state, proc) || !readGuestPointer(state, proc + m_init.proc_vmspace, vmspace) ||
        !readGuestPointer(state, vmspace + m_init.vmspace_maxsaddr, maxsaddr) ||
        !readGuestPointer(state, vmspace + m_init.vmspace_stacktop, stacktop)) {
        return false;
    }

    if (stacktop <= maxsaddr) {
        return false;
    }

    auto sp = state->regs()->getSp();
    if (sp >= maxsaddr && sp < stacktop) {
        *base = maxsaddr;
        *size = stacktop - maxsaddr;
        return true;
    }

    // Not the main stack (e.g., a libthr thread): find the mapping that holds sp
    return lookupMapEntry(state, vmspace + m_init.vmspace_map, sp, base, size);
}

// vm_map_entry_succ() of sys/vm/vm_map.h: the entries form a threaded binary tree
bool FreeBSDMonitor::mapEntrySucc(S2EExecutionState *state, uint64_t entry, uint64_t &after) {
    uint64_t left, leftStart, entryStart;
    if (!readGuestPointer(state, entry + m_init.map_entry_right, after) ||
        !readGuestPointer(state, after + m_init.map_entry_left, left) ||
        !readGuestPointer(state, left + m_init.map_entry_start, leftStart) ||
        !readGuestPointer(state, entry + m_init.map_entry_start, entryStart)) {
        return false;
    }

    if (leftStart > entryStart) {
        unsigned depth = 0;
        do {
            after = left;
            if (!readGuestPointer(state, after + m_init.map_entry_left, left)) {
                return false;
            }
            if (++depth > 1024) {
                return false;
            }
        } while (left != entry);
    }

    return true;
}

bool FreeBSDMonitor::lookupMapEntry(S2EExecutionState *state, uint64_t map, uint64_t address, uint64_t *base,
                                    uint64_t *size) {
    const uint64_t header = map + m_init.map_header;
    uint64_t entry = header;

    for (unsigned i = 0; i < 65536; ++i) {
        if (!mapEntrySucc(state, entry, entry) || entry == header) {
            return false;
        }

        uint64_t start, end;
        uint32_t eflags;
        if (!readGuestPointer(state, entry + m_init.map_entry_start, start) ||
            !readGuestPointer(state, entry + m_init.map_entry_end, end) ||
            !readGuestInt32(state, entry + m_init.map_entry_eflags, eflags)) {
            return false;
        }

        if (address >= start && address < end) {
            if (eflags & m_init.map_entry_guard) {
                // A stack guard: the stack pointer is below its stack
                return false;
            }
            *base = start;
            *size = end - start;
            return true;
        }

        if (start > address) {
            // Entries are sorted
            return false;
        }
    }

    return false;
}

bool FreeBSDMonitor::getProcessName(S2EExecutionState *state, uint64_t pid, std::string &name) {
    uint64_t proc;
    uint32_t curPid;
    if (!getCurrentProc(state, proc) || !readGuestInt32(state, proc + m_init.proc_pid, curPid)) {
        return false;
    }

    // Only the current process is reachable without walking allproc
    if (curPid != pid) {
        return false;
    }

    return state->mem()->readString(proc + m_init.proc_comm, name, m_init.proc_comm_size);
}

/*****************************************************************************/
/* Command handling                                                          */
/*****************************************************************************/

bool FreeBSDMonitor::verifyCommand(S2EExecutionState *state, uint64_t guestDataPtr, uint64_t guestDataSize,
                                   S2E_FREEBSDMON_COMMAND &cmd) {
    s2e_assert(state, guestDataSize == sizeof(cmd),
               "Invalid command size " << guestDataSize << " != " << sizeof(cmd)
                                       << " from pagedir=" << hexval(state->regs()->getPageDir())
                                       << " pc=" << hexval(state->regs()->getPc()));

    std::ostringstream symbolicBytes;
    for (unsigned i = 0; i < guestDataSize; ++i) {
        ref<Expr> t = state->mem()->read(guestDataPtr + i);
        if (t && !isa<ConstantExpr>(t)) {
            symbolicBytes << "  " << hexval(i, 2) << "\n";
        }
    }

    if (symbolicBytes.str().length()) {
        getWarningsStream(state) << "Command has symbolic bytes at " << symbolicBytes.str() << "\n";
    }

    bool ok = state->mem()->read(guestDataPtr, &cmd, sizeof(cmd));
    s2e_assert(state, ok, "Failed to read command memory");

    s2e_assert(state, cmd.version == S2E_FREEBSDMON_COMMAND_VERSION,
               "Invalid command version " << hexval(cmd.version) << " != " << hexval(S2E_FREEBSDMON_COMMAND_VERSION)
                                          << " from pagedir=" << hexval(state->regs()->getPageDir())
                                          << " pc=" << hexval(state->regs()->getPc())
                                          << " (rebuild the guest tools)");

    return true;
}

void FreeBSDMonitor::handleOpcodeInvocation(S2EExecutionState *state, uint64_t guestDataPtr, uint64_t guestDataSize) {
    S2E_FREEBSDMON_COMMAND cmd;
    if (!verifyCommand(state, guestDataPtr, guestDataSize, cmd)) {
        return;
    }

    switch (cmd.Command) {
        case FREEBSD_INIT:
            handleInit(state, cmd);
            break;
        case FREEBSD_PROCESS_LOAD:
            handleProcessLoad(state, cmd);
            break;
        case FREEBSD_MODULE_LOAD:
            handleModuleLoad(state, cmd);
            break;
        case FREEBSD_PROCESS_EXIT:
            handleProcessExit(state, cmd);
            break;
        case FREEBSD_THREAD_EXIT:
            handleThreadExit(state, cmd);
            break;
        case FREEBSD_SEGFAULT:
            handleSegfault(state, cmd);
            break;
        case FREEBSD_TRAP:
            handleTrap(state, cmd);
            break;
        case FREEBSD_KERNEL_PANIC:
            handleKernelPanic(state, cmd);
            break;
        case FREEBSD_KLD_LOAD:
            handleKldLoad(state, cmd);
            break;
        case FREEBSD_KLD_UNLOAD:
            handleKldUnload(state, cmd);
            break;
        default:
            getWarningsStream(state) << "Unknown command " << cmd.Command << "\n";
            break;
    }
}

bool FreeBSDMonitor::loadSections(S2EExecutionState *state, uint64_t phdr, uint64_t phdrSize,
                                  std::vector<SectionDescriptor> &sections) {
    using T = S2E_FREEBSDMON_PHDR_DESC;

    if (phdrSize % sizeof(T)) {
        getWarningsStream(state) << "Invalid phdr_size " << phdrSize << "\n";
        return false;
    }

    auto count = phdrSize / sizeof(T);
    auto headers = std::unique_ptr<T[]>{new T[count]};

    if (!state->mem()->read(phdr, headers.get(), phdrSize)) {
        getWarningsStream(state) << "Could not read program headers\n";
        return false;
    }

    for (unsigned i = 0; i < count; ++i) {
        if (headers[i].mmap.address == 0 && headers[i].mmap.size == 0) {
            continue;
        }

        SectionDescriptor sd;
        sd.nativeLoadBase = headers[i].p_vaddr;
        sd.runtimeLoadBase = headers[i].vma + (headers[i].p_vaddr & 0xfff);
        sd.size = headers[i].p_filesz;
        sd.executable = headers[i].mmap.prot & PROT_EXEC;
        sd.readable = headers[i].mmap.prot & PROT_READ;
        sd.writable = headers[i].mmap.prot & PROT_WRITE;
        sections.push_back(sd);
    }

    return true;
}

///
/// The kernel is loaded at its link address (FreeBSD does not randomize the
/// kernel), so the sections of the ELF file in the guestfs are used as is.
///
void FreeBSDMonitor::loadKernelImage(S2EExecutionState *state) {
    auto kernelName = std::string(llvm::sys::path::filename(m_kernelPath));

    auto exe = m_vmi->getFromDisk(m_kernelPath, kernelName, false);
    if (!exe) {
        getWarningsStream(state) << "Could not load the kernel image " << m_kernelPath
                                 << " from the guestfs, kernel code will not be attributed to a module\n";
        return;
    }

    std::vector<SectionDescriptor> sections;
    for (const auto &s : exe->getSections()) {
        if (!s.loadable) {
            continue;
        }

        SectionDescriptor sd;
        sd.readable = s.readable;
        sd.writable = s.writable;
        sd.executable = s.executable;
        sd.size = s.size;
        sd.nativeLoadBase = s.start;
        sd.runtimeLoadBase = s.start;
        sections.push_back(sd);
    }

    auto kernel = ModuleDescriptor::get(m_kernelPath, kernelName, 0, 0, exe->getEntryPoint(), sections);
    getDebugStream(state) << kernel << '\n';

    onModuleLoad.emit(state, kernel);
}

void FreeBSDMonitor::handleInit(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd) {
    std::string kernelPath;
    state->mem()->readString(cmd.Init.kernel_path, kernelPath);

    getDebugStream(state) << "Received guest init: FreeBSD " << cmd.Init.freebsd_version
                          << " kernel_start=" << hexval(cmd.Init.kernel_start)
                          << " kernel_base=" << hexval(cmd.Init.kernel_base)
                          << " kernel_size=" << hexval(cmd.Init.kernel_size) << " kernel_path=" << kernelPath
                          << " page_size=" << cmd.Init.page_size << "\n"
                          << "  offsets: pcpu.curthread=" << cmd.Init.pcpu_curthread
                          << " thread.proc=" << cmd.Init.thread_proc << " thread.tid=" << cmd.Init.thread_tid
                          << " thread.kstack=" << cmd.Init.thread_kstack
                          << " thread.kstack_pages=" << cmd.Init.thread_kstack_pages
                          << " proc.pid=" << cmd.Init.proc_pid << " proc.comm=" << cmd.Init.proc_comm
                          << " proc.vmspace=" << cmd.Init.proc_vmspace
                          << " vmspace.maxsaddr=" << cmd.Init.vmspace_maxsaddr
                          << " vmspace.stacktop=" << cmd.Init.vmspace_stacktop << "\n";

    m_init = cmd.Init;
    m_kernelStart = cmd.Init.kernel_start;
    m_guestInitialized = true;

    completeInitialization(state);

    loadKernelImage(state);

    {
        uint64_t gsbase = s2e_read_register_concrete_fast<target_ulong>(CPU_OFFSET(segs[R_GS].base));
        uint64_t kgsbase = 0;
#ifdef TARGET_X86_64
        kgsbase = s2e_read_register_concrete_fast<target_ulong>(CPU_OFFSET(kernelgsbase));
#endif
        uint64_t thread = 0, proc = 0;
        bool okThread = getCurrentThread(state, thread);
        bool okProc = getCurrentProc(state, proc);
        getDebugStream(state) << "Registers: cs=" << hexval(s2e_read_register_concrete_fast<uint32_t>(CPU_OFFSET(segs[R_CS].selector)))
                              << " gsbase=" << hexval(gsbase) << " kernelgsbase=" << hexval(kgsbase)
                              << " curthread=" << hexval(thread) << (okThread ? "" : " (failed)")
                              << " proc=" << hexval(proc) << (okProc ? "" : " (failed)") << "\n";
    }

    checkTaskSwitch(state);

    uint64_t pid = getPid(state), tid = getTid(state);
    getDebugStream(state) << "Current process from pcpu: pid=" << pid << " tid=" << tid
                          << " (guest says pid=" << cmd.CurrentTask.pid << " tid=" << cmd.CurrentTask.tid << ")\n";
    if (pid != cmd.CurrentTask.pid || tid != cmd.CurrentTask.tid) {
        getWarningsStream(state) << "pcpu-based pid/tid lookup disagrees with the guest, check the struct offsets\n";
    }
}

void FreeBSDMonitor::handleProcessLoad(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd) {
    completeInitialization(state);

    std::string processPath;
    if (!state->mem()->readString(cmd.ProcessLoad.process_path, processPath)) {
        getWarningsStream(state) << "could not read process path of pid " << cmd.CurrentTask.pid << "\n";
    }

    getDebugStream(state) << "Process " << processPath << " loaded pid=" << cmd.CurrentTask.pid
                          << " tid=" << cmd.CurrentTask.tid << " pagedir=" << hexval(state->regs()->getPageDir())
                          << "\n";

    llvm::StringRef file(processPath);
    onProcessLoad.emit(state, state->regs()->getPageDir(), cmd.CurrentTask.pid,
                       std::string(llvm::sys::path::filename(file)));
}

void FreeBSDMonitor::handleModuleLoad(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd) {
    std::string modulePath;
    if (!state->mem()->readString(cmd.ModuleLoad.module_path, modulePath)) {
        getWarningsStream(state) << "could not read module path\n";
        return;
    }

    auto moduleName = std::string(llvm::sys::path::filename(modulePath));

    std::vector<SectionDescriptor> sections;
    if (!loadSections(state, cmd.ModuleLoad.phdr, cmd.ModuleLoad.phdr_size, sections)) {
        return;
    }

    auto module = ModuleDescriptor::get(modulePath, moduleName, cmd.CurrentTask.pid, state->regs()->getPageDir(),
                                        cmd.ModuleLoad.entry_point, sections);

    getDebugStream(state) << module << '\n';

    onModuleLoad.emit(state, module);
}

void FreeBSDMonitor::handleProcessExit(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd) {
    auto pd = state->regs()->getPageDir();
    getDebugStream(state) << "Process exit pid=" << cmd.CurrentTask.pid << " tid=" << cmd.CurrentTask.tid
                          << " cr3=" << hexval(pd) << " exitCode=" << cmd.ProcessExit.code << "\n";

    onProcessUnload.emit(state, pd, cmd.CurrentTask.pid, cmd.ProcessExit.code);
}

void FreeBSDMonitor::handleThreadExit(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd) {
    getDebugStream(state) << "Thread exit pid=" << cmd.CurrentTask.pid << " tid=" << cmd.CurrentTask.tid << "\n";

    ThreadDescriptor desc;
    desc.Pid = cmd.CurrentTask.pid;
    desc.Tid = cmd.CurrentTask.tid;
    onThreadExit.emit(state, desc);
}

void FreeBSDMonitor::handleSegfault(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd) {
    getWarningsStream(state) << "Received segfault"
                             << " err=" << hexval(cmd.SegFault.fault) << " pagedir=" << hexval(state->regs()->getPageDir())
                             << " pid=" << cmd.CurrentTask.pid << " pc=" << hexval(cmd.SegFault.pc)
                             << " addr=" << hexval(cmd.SegFault.address) << "\n";

    // Do not switch states until the state is killed by the bootstrap script
    getDebugStream(state) << "Blocking searcher until state is terminated\n";
    state->setStateSwitchForbidden(true);

    state->disassemble(getDebugStream(state), cmd.SegFault.pc, 256);

    onSegFault.emit(state, cmd.CurrentTask.pid, cmd.SegFault);

    if (m_terminateOnSegfault) {
        getDebugStream(state) << "Terminating state: received segfault\n";
        s2e()->getExecutor()->terminateState(state, "Segfault");
    }
}

void FreeBSDMonitor::handleTrap(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd) {
    getWarningsStream(state) << "Received trap"
                             << " pid=" << cmd.CurrentTask.pid << " pc=" << hexval(cmd.Trap.pc)
                             << " trapnr=" << cmd.Trap.trapnr << " signr=" << cmd.Trap.signr
                             << " err_code=" << cmd.Trap.error_code << "\n";

    getDebugStream(state) << "Blocking searcher until state is terminated\n";
    state->setStateSwitchForbidden(true);

    onTrap.emit(state, cmd.CurrentTask.pid, cmd.Trap.pc, (int) cmd.Trap.trapnr);

    if (m_terminateOnTrap) {
        getDebugStream(state) << "Terminating state: received trap\n";
        s2e()->getExecutor()->terminateState(state, "Trap");
    }
}

void FreeBSDMonitor::handleKernelPanic(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd) {
    std::string str = "kernel panic";
    state->mem()->readString(cmd.KernelPanic.message, str, cmd.KernelPanic.message_size);
    getWarningsStream(state) << "Kernel panic: " << str << "\n";
    s2e()->getExecutor()->terminateState(state, "Kernel panic: " + str);
}

///
/// Kernel modules on amd64 are relocatable objects: the in-kernel linker places
/// their allocated sections one after the other in a single block. libvmi
/// reproduces that layout from the .ko file in the guestfs (section "addresses"
/// are offsets in the block), so the runtime address of a section is the
/// linker file address plus that offset. Without the file, the module is
/// described by a single section covering the whole linker file.
///
void FreeBSDMonitor::handleKldLoad(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd) {
    std::string path, name;
    state->mem()->readString(cmd.KldLoad.path, path);
    state->mem()->readString(cmd.KldLoad.name, name);
    if (name.empty()) {
        name = std::string(llvm::sys::path::filename(path));
    }

    getDebugStream(state) << "kld load " << name << " path=" << path << " address=" << hexval(cmd.KldLoad.address)
                          << " size=" << hexval(cmd.KldLoad.size) << "\n";

    std::vector<SectionDescriptor> sections;

    auto exe = m_vmi->getFromDisk(path, name, false);
    if (exe) {
        for (const auto &s : exe->getSections()) {
            if (!s.loadable) {
                continue;
            }

            SectionDescriptor sd;
            sd.readable = s.readable;
            sd.writable = s.writable;
            sd.executable = s.executable;
            sd.size = s.virtualSize;
            sd.nativeLoadBase = s.start;
            sd.runtimeLoadBase = cmd.KldLoad.address + s.start;
            sd.name = s.name;
            sections.push_back(sd);
        }
    }

    if (sections.empty()) {
        getWarningsStream(state) << "kld " << name << " not found in the guestfs, using a single section\n";
        SectionDescriptor sd;
        sd.readable = true;
        sd.writable = true;
        sd.executable = true;
        sd.size = cmd.KldLoad.size;
        sd.nativeLoadBase = 0;
        sd.runtimeLoadBase = cmd.KldLoad.address;
        sd.name = name;
        sections.push_back(sd);
    }

    auto module = ModuleDescriptor::get(path, name, 0, 0, 0, sections);
    m_klds[cmd.KldLoad.address] = module;

    onModuleLoad.emit(state, module);
    onKldLoad.emit(state, module);
}

void FreeBSDMonitor::handleKldUnload(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd) {
    auto it = m_klds.find(cmd.KldUnload.address);
    if (it == m_klds.end()) {
        getWarningsStream(state) << "kld unload of unknown module at " << hexval(cmd.KldUnload.address) << "\n";
        return;
    }

    getDebugStream(state) << "kld unload " << it->second.Name << "\n";

    onKldUnload.emit(state, it->second);
    onModuleUnload.emit(state, it->second);
    m_klds.erase(it);
}

} // namespace plugins
} // namespace s2e
