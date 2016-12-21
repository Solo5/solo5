OBJS=ctype.o \
     dtoa.o \
     memchr.o memcmp.o memcpy.o memmove.o memset.o \
     strcmp.o strlen.o strtol.o strchr.o strchrnul.o strncpy.o stpncpy.o \
     stubs.o \
     vfprintf.o vsnprintf.o snprintf.o fprintf.o printf.o sysdeps_solo5.o

UKVM_TARGETS=$(OBJS) test_nolibc.ukvm ukvm-bin
VIRTIO_TARGETS=$(OBJS) test_nolibc.virtio
UKVM_MODULES=

include ../Makefile.tests

LDFLAGS += $(OBJS)
CFLAGS+=-I./include  -Wno-parentheses -Wno-sign-compare

#ifeq ($(FREESTANDING_CFLAGS),)
#    $(error FREESTANDING_CFLAGS not set)
#endif
#ifeq ($(SYSDEP_OBJS),)
#    $(error SYSDEP_OBJS not set)
#endif

#CC=cc
#CFLAGS=-O2 -std=c99 -Wall -Wno-parentheses -Werror
#CFLAGS+=$(FREESTANDING_CFLAGS)

#libnolibc.a: $(OBJS) $(SYSDEP_OBJS)
#	$(AR) rcs $@ $(OBJS) $(SYSDEP_OBJS)

