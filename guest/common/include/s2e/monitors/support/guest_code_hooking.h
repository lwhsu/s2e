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

#ifndef S2E_GUEST_CODE_HOOKING_H
#define S2E_GUEST_CODE_HOOKING_H

/// C interface to the GuestCodeHooking plugin (the Windows guest tools have
/// their own copy in guest/windows/libcommon). Usable from kernel code.

#include <s2e/s2e.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum S2E_GUEST_HOOK_PLUGIN_COMMANDS {
    REGISTER_DIRECT_HOOK,
    UNREGISTER_DIRECT_HOOK,
    REGISTER_CALL_SITE_HOOK,
    UNREGISTER_CALL_SITE_HOOK,
    REGISTER_CALL_SITE_HOOK_ADDRESS,
    UNREGISTER_CALL_SITE_HOOK_ADDRESS
} S2E_GUEST_HOOK_PLUGIN_COMMANDS;

typedef struct S2E_GUEST_HOOK_DIRECT {
    uint64_t Pid;
    uint64_t OriginalPc;
    uint64_t HookPc;
} S2E_GUEST_HOOK_DIRECT;

typedef struct S2E_GUEST_HOOK_LIBRARY_FCN {
    uint64_t Pid;
    uint64_t LibraryName;
    uint64_t FunctionName;
    uint64_t HookPc;
    uint64_t HookReturn64;
} S2E_GUEST_HOOK_LIBRARY_FCN;

typedef struct S2E_GUEST_HOOK_PLUGIN_COMMAND {
    S2E_GUEST_HOOK_PLUGIN_COMMANDS Command;
    union {
        S2E_GUEST_HOOK_DIRECT DirectHook;
        S2E_GUEST_HOOK_LIBRARY_FCN CallSiteHook;
    };
} S2E_GUEST_HOOK_PLUGIN_COMMAND;

///
/// Redirect the calls that the modules listed in the plugin's moduleNames make
/// to the function at original_pc to the function at hook_pc. The hook has the
/// same signature as the original and may call it (calls from modules that are
/// not in moduleNames are not redirected).
///
static inline void s2e_hook_call_site(uint64_t pid, uintptr_t original_pc, uintptr_t hook_pc) {
    S2E_GUEST_HOOK_PLUGIN_COMMAND cmd;
    cmd.Command = REGISTER_CALL_SITE_HOOK_ADDRESS;
    cmd.DirectHook.Pid = pid;
    cmd.DirectHook.OriginalPc = original_pc;
    cmd.DirectHook.HookPc = hook_pc;
    s2e_invoke_plugin("GuestCodeHooking", &cmd, sizeof(cmd));
}

static inline void s2e_unhook_call_site(uint64_t pid, uintptr_t original_pc) {
    S2E_GUEST_HOOK_PLUGIN_COMMAND cmd;
    cmd.Command = UNREGISTER_CALL_SITE_HOOK_ADDRESS;
    cmd.DirectHook.Pid = pid;
    cmd.DirectHook.OriginalPc = original_pc;
    cmd.DirectHook.HookPc = 0;
    s2e_invoke_plugin("GuestCodeHooking", &cmd, sizeof(cmd));
}

#ifdef __cplusplus
}
#endif

#endif
