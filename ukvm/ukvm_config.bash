#!/bin/bash

if [ "a$1" == "a--help" ]; then
    echo -n "Usage: $0 ["
    echo -n `ls ukvm-*.c` | sed s/"ukvm-"//g | sed s/"\.c"//g
    echo "] > Makefile.ukvm"
    exit
fi

echo "UKVM_MODULE_OBJS= \\"
for m in $@; do
    echo "ukvm-$m.o \\"
done
echo 
echo "UKVM_MODULE_FLAGS= \\"
for m in $@; do
    echo "-DUKVM_MODULE_${m^^} \\"
done
echo 
