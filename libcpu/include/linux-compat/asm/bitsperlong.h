/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/* Copied from the Linux UAPI headers shipped with QEMU; treated as a system header so that
 * the warnings-as-errors builds of libs2e do not trip over it. */
#pragma GCC system_header
#ifndef __ASM_X86_BITSPERLONG_H
#define __ASM_X86_BITSPERLONG_H

#if defined(__x86_64__) && !defined(__ILP32__)
# define __BITS_PER_LONG 64
#else
# define __BITS_PER_LONG 32
#endif

#include <asm-generic/bitsperlong.h>

#endif /* __ASM_X86_BITSPERLONG_H */

