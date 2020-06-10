NAME := aeabi

CC := arm-linux-gnueabihf-gcc
AS := arm-linux-gnueabihf-gcc
AR := arm-linux-gnueabihf-ar

CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -g -mthumb -march=armv7
ASFLAGS := -Wall -Wextra -Werror -O2 -g -mthumb -march=armv7

OBJS := divdi3.o divsi3.o divmoddi4.o fixdfdi.o fixunsdfdi.o floatdidf.o floatundidf.o udivsi3.o udivmodsi4.o udivmoddi4.o tp_read.o
OBJS += arm/aeabi_idivmod.o arm/aeabi_ldivmod.o arm/aeabi_uidivmod.o arm/aeabi_uldivmod.o arm/divmodsi4.o
OUTPUT := libaeabi.a
INSTALL_DIR := /usr/lib

all : staticlib

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(AS) $(ASFLAGS) -c $< -o $@

staticlib: $(OBJS)
	$(AR) r $(OUTPUT) $(OBJS)

install:
	install -m 644 $(OUTPUT) $(INSTALL_DIR)

clean:
	@echo "CLEAN $(NAME)"
	rm -f $(OBJS) $(OUTPUT)
