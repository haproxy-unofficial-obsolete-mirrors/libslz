TOPDIR     := $(PWD)
DESTDIR    :=
PREFIX     := /usr/local

CC         := gcc
OPT_CFLAGS := -O3
CPU_CFLAGS := -fomit-frame-pointer -DCONFIG_REGPARM=3
DEB_CFLAGS := -Wall -g
DEF_CFLAGS :=
USR_CFLAGS :=
INC_CFLAGS := -I$(TOPDIR)/include
CFLAGS     := $(OPT_CFLAGS) $(CPU_CFLAGS) $(DEB_CFLAGS) $(DEF_CFLAGS) $(USR_CFLAGS) $(INC_CFLAGS)

LD         := $(CC)
DEB_LFLAGS := -g
USR_LFLAGS :=
LIB_LFLAGS :=
LDFLAGS    := $(DEB_LFLAGS) $(USR_LFLAGS) $(LIB_LFLAGS)

STRIP      := strip
BINS       := zdec zenc
STATIC     := libslz.a
OBJS       :=
OBJS       += $(patsubst %.c,%.o,$(wildcard src/*.c))
OBJS       += $(patsubst %.S,%.o,$(wildcard src/*.S))

all: $(BINS) $(STATIC)

static: $(STATIC)

zdec: src/zdec.o
	$(LD) $(LDFLAGS) -o $@ $^

zenc: src/zenc.o src/slz.o
	$(LD) $(LDFLAGS) -o $@ $^

libslz.a: src/slz.o
	$(AR) rv $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

install:
	[ -d "$(DESTDIR)$(PREFIX)/include/." ] || mkdir -p -m 0755 $(DESTDIR)$(PREFIX)/include
	[ -d "$(DESTDIR)$(PREFIX)/lib/." ]     || mkdir -p -m 0755 $(DESTDIR)$(PREFIX)/lib
	[ -d "$(DESTDIR)$(PREFIX)/bin/." ]     || mkdir -p -m 0755 $(DESTDIR)$(PREFIX)/bin
	cp src/slz.h $(DESTDIR)$(PREFIX)/include/ && chmod 644 $(DESTDIR)$(PREFIX)/include/slz.h
	if [ -e libslz.a ]; then cp libslz.a $(DESTDIR)$(PREFIX)/lib/ && chmod 644 $(DESTDIR)$(PREFIX)/lib/libslz.a; fi
	if [ -e zenc ]; then $(STRIP) zenc; cp zenc $(DESTDIR)$(PREFIX)/bin/ && chmod 755 $(DESTDIR)$(PREFIX)/bin/zenc; fi

clean:
	-rm -f $(BINS) $(OBJS) $(STATIC) *.[oa] *~ */*.[oa] */*~
