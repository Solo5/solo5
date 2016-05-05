all: ukvm

ukvm: ukvm.c ukvm.h misc.h gdb-stub.c
	gcc -o ukvm ukvm.c gdb-stub.c -lpthread -lrt

clean:
	rm -f ukvm
