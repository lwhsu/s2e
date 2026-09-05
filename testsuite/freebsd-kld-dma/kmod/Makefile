# Built by ../Makefile with the FreeBSD system make.
S2E_INCLUDE_DIR?=	${.CURDIR}/../../../guest/common/include

KMOD=	dmatest
SRCS=	dmatest.c
SRCS+=	bus_if.h device_if.h

CFLAGS+=	-I${S2E_INCLUDE_DIR}

.include <bsd.kmod.mk>
