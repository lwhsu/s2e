# Built by ../Makefile with the FreeBSD system make.
S2E_INCLUDE_DIR?=	${.CURDIR}/../../../guest/common/include

KMOD=	s2epanic
SRCS=	s2epanic.c

CFLAGS+=	-I${S2E_INCLUDE_DIR}

.include <bsd.kmod.mk>
