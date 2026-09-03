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

///
/// Replacement for the Linux UAPI <linux/ioctl.h> on BSD hosts.
///
/// The KVM ioctl numbers never reach the host kernel: libs2e emulates /dev/kvm entirely in
/// user space. They only need to be identical on both sides of the interface (the KVM client,
/// i.e. QEMU, and libs2e), so the native BSD encoding from <sys/ioccom.h> is used. This avoids
/// clashing with the _IO* macros that the system <sys/ioctl.h> defines. Note that the BSD size
/// field is 13 bits wide (up to 8191 bytes); the largest KVM ioctl payload, struct kvm_xsave,
/// is 4096 bytes.
///

#ifndef S2E_LINUX_COMPAT_IOCTL_H
#define S2E_LINUX_COMPAT_IOCTL_H

#pragma GCC system_header

#include <sys/ioccom.h>

#endif
