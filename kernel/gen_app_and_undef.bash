#!/bin/bash

# Copyright (c) 2015, IBM
# Author(s): Dan Williams <djwillia@us.ibm.com>
#
# Permission to use, copy, modify, and/or distribute this software for
# any purpose with or without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all
# copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
# OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

OBJ=app_solo5.o
H_FILE=app_undefined.h
C_FILE=app_undefined.c

function get_prototype() {
    local f=$1

    for m in 3 2; do
        decl=`/usr/bin/man --pager=cat $m $f |grep -A20 SYNOPSIS|grep -v "#"|grep $f | head -n 1`
        macro=`/usr/bin/man --pager=cat $m $f |grep -A20 Macro |grep ":" | tail -n +2 | grep -o $f | head -n 1`
        
        # is it a macro? if it has a void it is a special case
        if [ -n "$macro" -a -n "`echo $decl |grep -v void`" ]; then
            args=`echo $decl | cut -f 2 -d '(' | cut -f 1 -d ')'`
            argsp=`echo $args |grep -o ' '| wc -l`
            argcm=`echo $args |grep -o ','| wc -l`
            
            # are the arguments typed (eq commas and sp)? if not, it needs to be a macro
            if [ "$argsp" = "$argcm" ]; then
                echo "#define $macro($args) (`echo $decl|awk '{print $1}'`)0"
            else
                echo $decl
            fi
        else
            echo $decl
        fi
    done 2>/dev/null | grep -v "No manual entry" |grep -v "^$" |head -n 1
}


echo "cleaning..."
make clean

rm -f $H_FILE $C_FILE $OBJ
touch $H_FILE $C_FILE 
echo -e "#ifndef __UNDEFINED_GEN_H__\n#define __UNDEFINED_GEN_H__" > /tmp/$H_FILE
echo "#include \"kernel.h\"" > $C_FILE

echo "generating stubs..."

make app_solo5_clean 
# fill in undeclared types
for f in `make $OBJ 2>&1|grep "unknown type"|cut -f 2 -d "'"|sort |uniq`; do
    echo "typedef uint64_t $f;"
done >> /tmp/$H_FILE

echo "checking man..."

make app_solo5_clean
# check man section 3 then 2
# XXX change to use get_prototype function
for m in 3 2; do
    for f in `make $OBJ 2>&1 |grep "implicit declaration" |cut -f 2 -d "'"|sort |uniq`; do
        decl=`/usr/bin/man --pager=cat $m $f |grep -A20 SYNOPSIS|grep -v "#"|grep $f | head -n 1`
        macro=`/usr/bin/man --pager=cat $m $f |grep -A20 Macro |grep ":" | tail -n +2 | grep -o $f | head -n 1`

        # is it a macro? if it has a void it is a special case
        if [ -n "$macro" -a -n "`echo $decl |grep -v void`" ]; then
            args=`echo $decl | cut -f 2 -d '(' | cut -f 1 -d ')'`
            argsp=`echo $args |grep -o ' '| wc -l`
            argcm=`echo $args |grep -o ','| wc -l`

            # are the arguments typed (eq commas and sp)? if not, it needs to be a macro
            if [ "$argsp" = "$argcm" ]; then
                echo "#define $macro($args) (`echo $decl|awk '{print $1}'`)0"
            else
                echo $decl
            fi
        else
            echo $decl
        fi
    done 
done 2>/dev/null >> /tmp/$H_FILE 

echo "declaring constants..."

make app_solo5_clean
# declare constants
for f in `make $OBJ 2>&1 |grep "undeclared" | grep -v "only once"| cut -f 2 -d "'"|sort | uniq`; do
    echo "extern uint64_t $f;" >> /tmp/$H_FILE
done 

# # check for undefined references that are not yet in the header file
# # because of (dangerous/sloppy) system header inclusion
# for f in `make kernel 2>&1 |grep "undefined reference to" |cut -f 2 -d "\\\`" |cut -f 1 -d "'"|sort |uniq`; do
#     decl=`grep "[ \*]$f(" $H_FILE`
#     if [ -z "$decl" ]; then
#         get_prototype $f
#     fi
# done >> /tmp/$H_FILE

echo "#endif" >> /tmp/$H_FILE
mv /tmp/$H_FILE $H_FILE

echo "done with header file"

echo "filling in undef link..."

# fill in undefined link errors
for f in `make kernel 2>&1 |grep "undefined reference to" |cut -f 2 -d "\\\`" |cut -f 1 -d "'"|sort |uniq`; do
    decl=`grep "[ \*]$f(" $H_FILE`
    r=`echo $decl | cut -f 1 -d '(' | sed s/"$f$"//`
    if [ "$r" == "void" ]; then
        echo $decl | sed s/";"/"{ PANIC(\"'$f' unimplemented!\\\n\"); return;}"/
    else
        echo $decl | sed s/"("/"( __attribute__((unused)) "/ | sed s/", \([^\.]\)"/", __attribute__((unused)) \1"/g | sed s/";"/"{ PANIC(\"'$f' unimplemented!\\\n\"); return ($r)0;}"/
    fi
done >> $C_FILE

echo "filling in undef ref..."

# fill in undefined references
for f in `make kernel 2>&1 |grep "undefined reference to" | cut -f 2 -d "\\\`" | tr -d "'"|sort |uniq`; do 
    echo "void $f(void) { PANIC(\"$f\\n\"); }" >> $C_FILE
done

# fill in undefined references
for f in `make kernel 2>&1 |grep "undefined reference to" | cut -f 2 -d "\\\`" | tr -d "'"|sort |uniq`; do 
    echo "uint64_t $f = 0;" >> $C_FILE
done

echo "done..."
