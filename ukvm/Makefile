all: ukvm

include Makefile.ukvm

CC?=cc
UKVM_FLAGS=-D__UKVM_HOST__ $(UKVM_MODULE_FLAGS)
CFLAGS=-Wall -Werror -std=c99 -O2 -g $(UKVM_FLAGS)
OBJS=ukvm.o eventfd-aio.o $(UKVM_MODULE_OBJS)
HEADERS=ukvm.h misc.h processor-flags.h

ukvm: $(OBJS) $(HEADERS)
	$(CC) -o $@ $(CFLAGS) $(OBJS) -lpthread -lrt

clean:
	rm -f ukvm $(OBJS)
