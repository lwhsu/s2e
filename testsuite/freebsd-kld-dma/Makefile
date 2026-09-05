# Copyright (c) 2026 Li-Wen Hsu
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# GNU makefile driven by s2e testsuite; the module itself is built by the
# FreeBSD system make with bsd.kmod.mk against the kernel sources in
# S2E_FREEBSD_SYSDIR (default /usr/src/sys).

TARGET=freebsd64-kld-dma.ko
S2E_FREEBSD_SYSDIR?=/usr/src/sys
S2ESRC?=$(realpath ../..)

ifeq ($(shell uname -s),FreeBSD)
TARGETS=$(TARGET)
else
TARGETS=
endif

$(TARGET): kmod/dmatest.c kmod/Makefile
	# MK_REPRODUCIBLE_BUILD=no keeps the real source paths in the debug info (coverage needs them)
	env -u MAKEFLAGS -u MFLAGS make -C kmod SYSDIR=$(S2E_FREEBSD_SYSDIR) \
	    S2E_INCLUDE_DIR=$(S2ESRC)/guest/common/include DEBUG_FLAGS=-g MK_REPRODUCIBLE_BUILD=no
	# bsd.kmod.mk strips the debug info into a separate file; the coverage tools need the full object
	if [ -f kmod/dmatest.ko.full ]; then cp kmod/dmatest.ko.full "$@"; else cp kmod/dmatest.ko "$@"; fi

all: $(TARGETS)

clean:
	-env -u MAKEFLAGS -u MFLAGS make -C kmod SYSDIR=$(S2E_FREEBSD_SYSDIR) clean
	rm -f $(TARGET)
