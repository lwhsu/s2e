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

# The driver comes from a FreeBSD kernel build (make buildkernel KERNCONF=S2E
# MODULES_OVERRIDE=if_em). S2E_FREEBSD_KERNEL_OBJ points at the kernel build
# directory (e.g., $MAKEOBJDIRPREFIX/<src>/amd64.amd64/sys/S2E); the unstripped
# object keeps the debug info for coverage.

TARGET=freebsd64-if_em.ko
S2E_FREEBSD_KERNEL_OBJ_MAIN?=

IF_EM=$(shell [ -d "$(S2E_FREEBSD_KERNEL_OBJ_MAIN)/modules" ] && find $(S2E_FREEBSD_KERNEL_OBJ_MAIN)/modules -name if_em.ko.full | head -1)

ifeq ($(shell uname -s),FreeBSD)
ifneq ($(IF_EM),)
TARGETS=$(TARGET)
else
TARGETS=
endif
else
TARGETS=
endif

$(TARGET): $(IF_EM)
	cp "$(IF_EM)" "$@"

all: $(TARGETS)

clean:
	rm -f $(TARGET)
