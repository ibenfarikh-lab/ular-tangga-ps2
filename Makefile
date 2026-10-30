# Makefile untuk build GitHub Actions.
# Penyesuaian vs Makefile panduan: Makefile panduan tidak menambah path ports/,
# sehingga gsKit.h tidak ketemu di SDK ini (terbukti 2x di build lokal).
# main.c & build.yml sesuai panduan, tidak diubah. Pola path sama dengan build lokal.
PS2DEV ?= $(HOME)/workspace/ps2dev
PS2SDK ?= $(PS2DEV)/ps2sdk-install

EE_CC      = $(PS2DEV)/ee/bin/mips64r5900el-ps2-elf-gcc
EE_CFLAGS  = -D_EE -O2 -G0 -Wall \
             -I$(PS2SDK)/ee/include \
             -I$(PS2SDK)/common/include \
             -I$(PS2SDK)/ports/include
EE_LDFLAGS = -L$(PS2SDK)/ee/lib \
             -L$(PS2SDK)/ports/lib
EE_LIBS    = -lgskit -ldmakit -lpad -lmc -laudsrv -lkernel -lc -lm

EE_BIN  = ulartangga_slim.elf
EE_OBJS = main.o

all: $(EE_BIN)

$(EE_BIN): $(EE_OBJS)
	$(EE_CC) $(EE_LDFLAGS) -o $@ $(EE_OBJS) $(EE_LIBS)

main.o: main.c
	$(EE_CC) $(EE_CFLAGS) -c $< -o $@

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

.PHONY: all clean
