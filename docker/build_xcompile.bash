#!/bin/bash

cd $HOME
mkdir build-xcompile-src
cd build-xcompile-src/

wget ftp://ftp.gnu.org/gnu/gcc/gcc-5.2.0/gcc-5.2.0.tar.bz2
wget ftp://ftp.gnu.org/gnu/binutils/binutils-2.25.tar.bz2
tar xjf binutils-2.25.tar.bz2 
tar xjf gcc-5.2.0.tar.bz2 

export PREFIX="$HOME/opt/cross"
export PATH="$PREFIX/bin:$PATH"

function build_binutils {
    mkdir build-binutils-$1
    (cd build-binutils-$1/ \
	    && ../binutils-2.25/configure --target=$1-elf --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror \
	    && make \
	    && make install)
    rm -rf build-binutils-$1
}

function build_gcc {
    mkdir build-gcc-$1
    (cd build-gcc-$1/ \
	    && ../gcc-5.2.0/configure --target=$1-elf --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers \
	    && make all-gcc \
	    && make all-target-libgcc \
	    && make install-gcc \
	    && make install-target-libgcc)
    rm -rf build-gcc-$1
}    

build_binutils i686
build_binutils x86_64
build_gcc i686
build_gcc x86_64

cd ..
rm -rf build-xcompile-src
