INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O0 -g $(INCLUDE_DIRS) $(CDEFS)
LIBS= -lrt

HFILES= capture.h mem.h sharpen.h 
CFILES= capture.c mem.c sharpen.c main.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	final

clean:
	-rm -f *.o *.d
	-rm -f final

distclean:
	-rm -f *.o *.d

final: ${OBJS}
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ main.o capture.o mem.o sharpen.o $(LIBS)

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
