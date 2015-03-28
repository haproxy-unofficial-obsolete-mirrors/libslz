TOPDIR     := $(PWD)

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

BINS       := zdec zenc
OBJS       :=
OBJS       += $(patsubst %.c,%.o,$(wildcard src/*.c))
OBJS       += $(patsubst %.S,%.o,$(wildcard src/*.S))

all: $(BINS)

static: libslz.a

zdec: src/zdec.o
	$(LD) $(LDFLAGS) -o $@ $^

zenc: src/zenc.o src/slz.o
	$(LD) $(LDFLAGS) -o $@ $^

libslz.a: src/slz.o
	$(AR) rv $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	-rm -f $(BINS) $(OBJS) *.[oa] *~ */*.[oa] */*~
