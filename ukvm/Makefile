all: ukvm

ukvm: ukvm.c ukvm.h misc.h
	gcc -o ukvm ukvm.c -lpthread -lrt

clean:
	rm -f ukvm
