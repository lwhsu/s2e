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

#ifndef S2E_PLUGINS_FREEBSD_MONITOR_H
#define S2E_PLUGINS_FREEBSD_MONITOR_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/Core/BaseInstructions.h>
#include <s2e/Plugins/Core/Vmi.h>
#include <s2e/Plugins/OSMonitors/ModuleDescriptor.h>
#include <s2e/Plugins/OSMonitors/OSMonitor.h>
#include <s2e/S2EExecutionState.h>

#include <s2e/monitors/commands/freebsd.h>

#include <map>

namespace s2e {
namespace plugins {

///
/// \brief Monitor for FreeBSD guests
///
/// The guest side is the s2e.ko kernel module (process, thread and kld
/// events, kernel panics) and the s2e.so LD_PRELOAD library (shared objects
/// of a process). The kernel module only uses EVENTHANDLER(9) hooks, so there
/// is no context switch notification: the current process and thread are read
/// from the guest's per-CPU data (struct pcpu, reached through the gs base)
/// using the structure offsets sent by the module at load time.
///
class FreeBSDMonitor : public OSMonitor, public IPluginInvoker {
    S2E_PLUGIN

public:
    FreeBSDMonitor(S2E *s2e) : OSMonitor(s2e) {
    }

    void initialize();

    /// Emitted when a process dies with SIGSEGV or SIGBUS
    sigc::signal<void, S2EExecutionState *, uint64_t /* pid */, const S2E_FREEBSDMON_COMMAND_SEGFAULT &> onSegFault;

    /// Emitted when a process dies with another synchronous signal (SIGILL, SIGFPE, SIGTRAP, ...)
    sigc::signal<void, S2EExecutionState *, uint64_t /* pid */, uint64_t /* pc */, int /* trapnr */> onTrap;

    /// Emitted when a kernel module is loaded or unloaded (after onModuleLoad/onModuleUnload)
    sigc::signal<void, S2EExecutionState *, const ModuleDescriptor &> onKldLoad;
    sigc::signal<void, S2EExecutionState *, const ModuleDescriptor &> onKldUnload;

    virtual uint64_t getKernelStart() const {
        return m_kernelStart;
    }

    virtual uint64_t getPid(S2EExecutionState *state);
    virtual uint64_t getTid(S2EExecutionState *state);
    virtual bool getCurrentStack(S2EExecutionState *state, uint64_t *base, uint64_t *size);
    virtual bool getProcessName(S2EExecutionState *state, uint64_t pid, std::string &name);

    virtual void handleOpcodeInvocation(S2EExecutionState *state, uint64_t guestDataPtr, uint64_t guestDataSize);

    /// Whether the guest module sent its INIT command
    bool isGuestInitialized() const {
        return m_guestInitialized;
    }

    const S2E_FREEBSDMON_COMMAND_INIT &getGuestInfo() const {
        return m_init;
    }

private:
    Vmi *m_vmi = nullptr;

    bool m_terminateOnSegfault = true;
    bool m_terminateOnTrap = true;

    /// Start of the kernel address space (VM_MIN_KERNEL_ADDRESS)
    uint64_t m_kernelStart = 0xffff800000000000ULL;

    /// Name of the kernel file in the guestfs (relative to the root)
    std::string m_kernelPath;

    bool m_guestInitialized = false;
    S2E_FREEBSDMON_COMMAND_INIT m_init;

    bool verifyCommand(S2EExecutionState *state, uint64_t guestDataPtr, uint64_t guestDataSize,
                       S2E_FREEBSDMON_COMMAND &cmd);

    bool readGuestPointer(S2EExecutionState *state, uint64_t address, uint64_t &value) const;
    bool readGuestInt32(S2EExecutionState *state, uint64_t address, uint32_t &value) const;
    bool readGuestInt16(S2EExecutionState *state, uint64_t address, uint16_t &value) const;
    bool mapEntrySucc(S2EExecutionState *state, uint64_t entry, uint64_t &after);
    bool lookupMapEntry(S2EExecutionState *state, uint64_t map, uint64_t address, uint64_t *base, uint64_t *size);

    bool isInKernelMode(S2EExecutionState *state) const;
    bool getCurrentThread(S2EExecutionState *state, uint64_t &thread) const;
    bool getCurrentProc(S2EExecutionState *state, uint64_t &proc) const;

    void onPrivilegeChange(S2EExecutionState *state, unsigned previous, unsigned current);
    void checkTaskSwitch(S2EExecutionState *state);

    bool loadSections(S2EExecutionState *state, uint64_t phdr, uint64_t phdrSize,
                      std::vector<SectionDescriptor> &sections);

    void loadKernelImage(S2EExecutionState *state);

    void handleInit(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd);
    void handleProcessLoad(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd);
    void handleModuleLoad(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd);
    void handleProcessExit(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd);
    void handleThreadExit(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd);
    void handleSegfault(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd);
    void handleTrap(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd);
    void handleKernelPanic(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd);
    void handleKldLoad(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd);
    void handleKldUnload(S2EExecutionState *state, const S2E_FREEBSDMON_COMMAND &cmd);
};

} // namespace plugins
} // namespace s2e

#endif
