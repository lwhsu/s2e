# Built by ../Makefile with the FreeBSD system make; the source is shared with freebsd-kld-basic.
S2E_INCLUDE_DIR?=	${.CURDIR}/../../../guest/common/include

.PATH:	${.CURDIR}/../../freebsd-kld-basic/kmod

KMOD=	s2etest
SRCS=	s2etest.c
SRCS+=	bus_if.h device_if.h

CFLAGS+=	-I${S2E_INCLUDE_DIR}

.include <bsd.kmod.mk>
