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

### begin mirage stuff ###

include ../mirage.config  # global mirage platform configuration
include ../mirage.mk      # app-specific mirage configuration

SOLO5_DIR_4_SED=$(subst /,\/,$(SOLO5_DIR))
OPAM_DIR_4_SED=$(subst /,\/,$(CFG_M_OPAM_D))

MIRAGE_OBJS=\
$(CFG_M_OPAM_D)/lib/mirage-console/libmirage_console_solo5_stubs.a \
$(CFG_M_OPAM_D)/lib/mirage-block-solo5/libmirage_block_solo5_stubs.a \
$(CFG_M_OPAM_D)/lib/mirage-net-solo5/libmirage_net_solo5_stubs.a \
$(CFG_M_PLATFORM_D)/solo5-bindings/libsolo5camlbindings.a \
$(CFG_M_PLATFORM_D)/xen-ocaml/ocaml-src/asmrun/libasmrun.a \
$(CFG_M_PLATFORM_D)/xen-ocaml/ocaml-src/libxenotherlibs.a \
$(CFG_M_PLATFORM_D)/xen-posix/libxenposix.a \
$(CFG_M_OPAM_D)/lib/libopenlibm.a \

MIRAGE_APP_DEPS += \
$(CFG_M_OPAM_D)/lib/mirage \
$(CFG_M_OPAM_D)/lib/mirage-solo5 \
$(CFG_M_OPAM_D)/lib/mirage-console/libmirage_console_solo5_stubs.a \
$(CFG_M_OPAM_D)/lib/mirage-block-solo5/libmirage_block_solo5_stubs.a \
$(CFG_M_OPAM_D)/lib/mirage-net-solo5/libmirage_net_solo5_stubs.a \
$(CFG_MIRAGE_APP_DIR)/Makefile $(CFG_MIRAGE_APP_FILES) $(CFG_MIRAGE_APP_DEPS)

MIRAGE_CONSOLE_FILES=\
$(CFG_M_CONSOLE_D)/solo5/console_solo5.ml \
$(CFG_M_CONSOLE_D)/solo5/console_solo5.mli \
$(CFG_M_CONSOLE_D)/solo5/solo5_stubs.c \
$(CFG_M_CONSOLE_D)/Makefile \
$(CFG_M_CONSOLE_D)/_oasis

MIRAGE_BLOCK_FILES=\
$(CFG_M_BLOCK_D)/lib/block.ml \
$(CFG_M_BLOCK_D)/lib/block.mli \
$(CFG_M_BLOCK_D)/lib/solo5_stubs.c \
$(CFG_M_BLOCK_D)/Makefile \
$(CFG_M_BLOCK_D)/_oasis

MIRAGE_NET_FILES=\
$(CFG_M_NET_D)/lib/netif.ml \
$(CFG_M_NET_D)/lib/netif.mli \
$(CFG_M_NET_D)/lib/solo5_stubs.c \
$(CFG_M_NET_D)/Makefile \
$(CFG_M_NET_D)/_oasis

MIRAGE_TOOL_FILES=\
$(CFG_M_TOOL_D)/lib/main.ml \
$(CFG_M_TOOL_D)/lib/mirage.ml \
$(CFG_M_TOOL_D)/lib/mirage.mli

MIRAGE_PLATFORM_FILES=\
$(CFG_M_PLATFORM_D)/solo5-bindings/Makefile \
$(CFG_M_PLATFORM_D)/solo5-bindings/atomic_stubs.c \
$(CFG_M_PLATFORM_D)/solo5-bindings/barrier_stubs.c \
$(CFG_M_PLATFORM_D)/solo5-bindings/build.sh \
$(CFG_M_PLATFORM_D)/solo5-bindings/clock_stubs.c \
$(CFG_M_PLATFORM_D)/solo5-bindings/cstruct_stubs.c \
$(CFG_M_PLATFORM_D)/solo5-bindings/eventchn_stubs.c \
$(CFG_M_PLATFORM_D)/solo5-bindings/exit_stubs.c \
$(CFG_M_PLATFORM_D)/solo5-bindings/gnttab_stubs.c \
$(CFG_M_PLATFORM_D)/solo5-bindings/install.sh \
$(CFG_M_PLATFORM_D)/solo5-bindings/main.c \
$(CFG_M_PLATFORM_D)/solo5-bindings/mirage-xen-ocaml-bindings.pc \
$(CFG_M_PLATFORM_D)/solo5-bindings/mirage-solo5.pc \
$(CFG_M_PLATFORM_D)/solo5-bindings/sched_stubs.c \
$(CFG_M_PLATFORM_D)/solo5-bindings/start_info_stubs.c \
$(CFG_M_PLATFORM_D)/solo5-bindings/uninstall.sh \
$(CFG_M_PLATFORM_D)/solo5-bindings/xb_stubs.c

MIRAGE_XEN_OCAML_FILES=

MIRAGE_XEN_POSIX_FILES=\
$(CFG_M_PLATFORM_D)/xen-posix/src/mini_libc.c \
$(CFG_M_PLATFORM_D)/xen-posix/Makefile

# net driver for solo5
$(CFG_M_OPAM_D)/lib/mirage-net-solo5/libmirage_net_solo5_stubs.a: $(MIRAGE_NET_FILES)
	(cd $(CFG_M_NET_D) \
	&& sed s/"CCOpt.*"/"CCOpt:\t$(S5_CFLAGS) -I$(SOLO5_DIR_4_SED)"/g -i _oasis \
	&& oasis setup \
	&& opam reinstall -y mirage-net-solo5 )

# && opam pin add mirage-net-solo5 . -y \
# && opam update mirage-net-solo5	\
# && opam upgrade mirage-net-solo5 )

# block driver for solo5
$(CFG_M_OPAM_D)/lib/mirage-block-solo5/libmirage_block_solo5_stubs.a: $(MIRAGE_BLOCK_FILES)
	(cd $(CFG_M_BLOCK_D) \
	&& sed s/"CCOpt.*"/"CCOpt:\t$(S5_CFLAGS) -I$(SOLO5_DIR_4_SED)"/g -i _oasis \
	&& oasis setup \
	&& opam reinstall -y mirage-block-solo5 )

# && opam pin add mirage-block-solo5 . -y \
# && opam update mirage-block-solo5 \
# && opam upgrade mirage-block-solo5 )

# console driver for solo5
$(CFG_M_OPAM_D)/lib/mirage-console/libmirage_console_solo5_stubs.a: $(MIRAGE_CONSOLE_FILES)
	(cd $(CFG_M_CONSOLE_D) \
	&& sed s/"CCOpt.*"/"CCOpt:\t$(S5_CFLAGS) -I$(SOLO5_DIR_4_SED)"/g -i _oasis \
	&& oasis setup \
	&& opam reinstall -y mirage-console )

# && opam pin add mirage-console . -y \
# && opam update mirage-console \
# && opam upgrade mirage-console )

# the tool to do the build
$(CFG_M_OPAM_D)/lib/mirage: $(MIRAGE_TOOL_FILES)
	(cd $(CFG_M_TOOL_D) \
	&& oasis setup \
	&& opam reinstall -y mirage )

# && opam pin add mirage . -y \
# && opam update mirage \
# && opam upgrade mirage )

$(CFG_M_OPAM_D)/lib/mirage-solo5:
	opam reinstall -y mirage-solo5

# the platform bindings
$(CFG_M_PLATFORM_D)/solo5-bindings/libsolo5camlbindings.a: $(MIRAGE_PLATFORM_FILES)
	(cd $(CFG_M_PLATFORM_D) \
	&& sed s/"^SOLO5_DIR=.*"/"SOLO5_DIR=$(SOLO5_DIR_4_SED)"/g -i solo5-bindings/Makefile \
	&& sed s/"^OPAM_DIR=.*"/"OPAM_DIR=$(OPAM_DIR_4_SED)"/g -i solo5-bindings/Makefile \
	&& make -C solo5-bindings )


$(CFG_M_PLATFORM_D)/xen-ocaml/ocaml-src/asmrun/libasmrun.a: $(MIRAGE_XEN_OCAML_FILES)
	(cd $(CFG_M_PLATFORM_D) && make -C xen-ocaml)

$(CFG_M_PLATFORM_D)/xen-ocaml/ocaml-src/libxenotherlibs.a: $(MIRAGE_XEN_OCAML_FILES)
	(cd $(CFG_M_PLATFORM_D) && make -C xen-ocaml)

$(CFG_M_PLATFORM_D)/xen-posix/libxenposix.a: $(MIRAGE_XEN_POSIX_FILES)
	(cd $(CFG_M_PLATFORM_D) && make -C xen-posix)

# app configuration
$(CFG_MIRAGE_APP_DIR)/Makefile: $(CFG_M_OPAM_D)/lib/mirage
	(cd $(CFG_MIRAGE_APP_DIR) && mirage configure --solo5)

# the application
$(CFG_MIRAGE_APP_DIR)/_build/main.native.o: $(MIRAGE_APP_DEPS)
	(cd $(CFG_MIRAGE_APP_DIR) && make clean && make main.native.o)

# cleaning
MIRAGE_CLEAN=\
mirage-app-clean\
mirage-block-clean\
mirage-net-clean\
mirage-console-clean\
mirage-platform-clean\
mirage-clean

mirage-net-clean: 
	-opam remove mirage-net-solo5 -y

mirage-block-clean: 
	-opam remove mirage-block-solo5 -y

mirage-console-clean: 
	-opam remove mirage-console -y

mirage-platform-clean:
	-opam remove mirage-solo5 -y

# (cd $(CFG_M_PLATFORM_D) \
# && make -C solo5-bindings clean \
# && make -C xen-ocaml clean\
# && make -C xen-posix clean)

mirage-app-clean: $(CFG_M_OPAM_D)/lib/mirage $(CFG_MIRAGE_APP_DIR)/Makefile
	(cd $(CFG_MIRAGE_APP_DIR) \
	&& make clean \
	&& mirage clean)

mirage-clean:
	-opam remove mirage

mirage-app: $(CFG_MIRAGE_APP_DIR)/_build/main.native.o

# Horrible hack below, but all this should go away when Solo5 is a
# proper backend
.PHONY: mirage_libs.mk
mirage_libs.mk: $(MIRAGE_APP_DEPS)
	@echo "CFG_MIRAGE_APP_LIBS=\\" >> ../mirage.mk
	@cat $(CFG_MIRAGE_APP_DIR)/Makefile |grep "\-L" | sed s/tcpip_xen_stubs/tcpip_stubs/ >> ../mirage.mk
	@echo >> ../mirage.mk

### end mirage stuff ###

app_solo5_clean: $(MIRAGE_CLEAN)

.PHONY: app_solo5.o # relink app_solo5.o every time in case CFG_MIRAGE_APP_DIR changed
app_solo5.o: $(CFG_MIRAGE_APP_DIR)/_build/main.native.o 
	ln -f -s $(CFG_MIRAGE_APP_DIR)/_build/main.native.o app_solo5.o

# this file must set the following in order to fit in with the solo5
# build (Makefile)

APP_OBJS += app_solo5.o $(MIRAGE_OBJS)
APP_CLEAN += app_solo5_clean
APP_LIBS += $(CFG_MIRAGE_APP_LIBS)


