############################################################
# Variables setable from the command line:
#
# CCOMPILER (default: clang)
# DEBUG_SYMBOLS (default: DWARF)
# EXTRA_CFLAGS (default: none)
############################################################

ifndef CCOMPILER
CC=clang
else
CC=$(CCOMPILER)
endif

ifeq ($(OS),Windows_NT)
OSNAME:=$(OS)
else
OSNAME:=$(shell uname -s)
endif

BASENAME=o.expr

LIBOSE_DIR=../libose
OSE_CFILES=\
	ose.c\
	ose_util.c\
	ose_stackops.c\
	ose_match.c\
	ose_context.c\
	ose_print.c\

MOD_FILES=\
	ose_$(BASENAME).c

LIBO_DIR=./libo
LIBO_LIB=libo.a

INCLUDES=-I. -I$(LIBOSE_DIR) -I$(LIBO_DIR)
LIBS=-L$(LIBO_DIR) -lo

DEFINES=-DHAVE_OSE_ENDIAN_H

CFLAGS_DEBUG=-Wall -DOSE_CONF_DEBUG -O0 -g$(DEBUG_SYMBOLS) $(EXTRA_CFLAGS)
CFLAGS_RELEASE=-Wall -O3 $(EXTRA_CFLAGS)

# ifeq ($(shell uname), Darwin)
# LDFLAGS=-fvisibility=hidden -shared -Wl,-exported_symbol,_ose_main
# else
# LDFLAGS=-fvisibility=hidden -shared
# endif

release: CFLAGS+=$(CFLAGS_RELEASE)
release: $(LIBOSE_DIR)/sys/ose_endian.h ose_$(BASENAME).so

debug: CFLAGS+=$(CFLAGS_DEBUG)
debug: $(LIBOSE_DIR)/sys/ose_endian.h ose_$(BASENAME).so

ifeq ($(OS),Windows_NT)
else
CFLAGS:=-fPIC
endif

ose_$(BASENAME).so: $(LIBO_DIR)/$(LIBO_LIB) $(foreach f,$(OSE_CFILES),$(LIBOSE_DIR)/$(f)) $(MOD_FILES)	
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) $(DEFINES) -shared -o o.se.$(BASENAME).so $^ $(LIBS) 

$(LIBOSE_DIR)/sys/ose_endian.h:
	cd $(LIBOSE_DIR) && $(MAKE) sys/ose_endian.h

$(LIBO_DIR)/$(LIBO_LIB):
	cd $(LIBO_DIR) && $(MAKE)

.PHONY: clean
clean:
	rm -rf *.o *.so *.dSYM
