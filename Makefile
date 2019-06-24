PROG=	eegviewer

SRCS=	eegviewer.c
NOMAN=
BINDIR=		/usr/local/bin

LDADD+=	-L/usr/X11R6/lib -lxcb
DPADD+=	${LIBXCB}

CFLAGS+=	-I/usr/X11R6/include
CFLAGS+=	-fstack-protector-all
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+=	-Werror-implicit-function-declaration
CFLAGS+=	-Werror # during development phase (breaks some archs)

.include <bsd.prog.mk>
