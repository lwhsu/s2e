# S2E guest kernel module for FreeBSD.
#
# Build with the system make against a kernel source tree:
#   make SYSDIR=/path/to/src/sys S2E_INCLUDE_DIR=/path/to/s2e/guest/common/include

S2E_INCLUDE_DIR?=	${.CURDIR}/../../common/include

KMOD=	s2e
SRCS=	s2e_mod.c
SRCS+=	bus_if.h device_if.h

CFLAGS+=	-I${S2E_INCLUDE_DIR}

.include <bsd.kmod.mk>
