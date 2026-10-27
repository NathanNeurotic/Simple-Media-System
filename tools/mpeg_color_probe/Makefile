COLOR16 ?= 0
OUT = obj$(COLOR16)
EE_BIN = $(OUT)/probe.elf
EE_OBJS = $(OUT)/main.o $(OUT)/libmpeg.o $(OUT)/libmpeg_core.o $(OUT)/input.o
EE_INCS = -I../../include
EE_LIBS = -lc -lkernel
EE_CFLAGS = -O2 -G8192 -D_EE -DCOLOR16=$(COLOR16)

all: $(OUT) $(EE_BIN)
$(OUT):
	mkdir -p $@
$(OUT)/main.o: main.c | $(OUT)
	$(EE_CC) -I../../include $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
$(OUT)/libmpeg.o: ../../src/libmpeg.c | $(OUT)
	$(EE_CC) -I../../include $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
$(OUT)/libmpeg_core.o: ../../src/libmpeg_core.S | $(OUT)
	$(EE_CC) -I../../include $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
$(OUT)/input.c: probe.m2v | $(OUT)
	bin2c $< $@ probe_m2v
$(OUT)/input.o: $(OUT)/input.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
