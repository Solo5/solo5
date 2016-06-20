all: ukvm

CC=gcc
CFLAGS=-Wall -Werror
OBJS=ukvm.o gdb-stub.o
HEADERS=ukvm.h misc.h processor-flags.h

ukvm: $(OBJS) $(HEADERS)
	gcc -o $@ $(OBJS) -lpthread -lrt

clean:
	rm -f ukvm $(OBJS)
