COLOR16 ?= 0
POOL ?= 0
CAPTURE_FRAMES ?= 5
INPUT ?= probe.m2v
OUT = obj$(COLOR16)
ifneq ($(CAPTURE_FRAMES),5)
OUT = obj$(COLOR16)-$(CAPTURE_FRAMES)
endif
MAIN_SOURCE = main.c
ifeq ($(POOL),1)
OUT = pool$(COLOR16)
MAIN_SOURCE = pool.c
endif
EE_BIN = $(OUT)/probe.elf
EE_OBJS = $(OUT)/main.o $(OUT)/libmpeg.o $(OUT)/libmpeg_core.o $(OUT)/input.o
ifeq ($(POOL),1)
EE_OBJS += $(OUT)/ring.o $(OUT)/ipu.o
endif
EE_INCS = -I../../include
EE_LIBS = -lc -lkernel
EE_CFLAGS = -O2 -G8192 -D_EE -DCOLOR16=$(COLOR16) -DCAPTURE_FRAMES=$(CAPTURE_FRAMES)

all: $(OUT) $(EE_BIN)
$(OUT):
	mkdir -p $@
$(OUT)/main.o: $(MAIN_SOURCE) ../../src/SMS_MPEG12.c ../../include/libmpeg_internal.h | $(OUT)
	$(EE_CC) -I../../include $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
$(OUT)/libmpeg.o: ../../src/libmpeg.c ../../include/libmpeg_internal.h | $(OUT)
	$(EE_CC) -I../../include $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
$(OUT)/libmpeg_core.o: ../../src/libmpeg_core.S | $(OUT)
	$(EE_CC) -I../../include $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
$(OUT)/input.c: $(INPUT) | $(OUT)
	bin2c $< $@ probe_m2v
$(OUT)/input.o: $(OUT)/input.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
$(OUT)/ring.o: ../../src/SMS_RingBuffer.c | $(OUT)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
$(OUT)/ipu.o: ../../src/SMS_IPU_0.s | $(OUT)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
