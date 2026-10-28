OUT ?= obj
DECODER_SOURCE ?= ../../src/libmpeg.c
DECODER_CORE ?= ../../src/libmpeg_core.S
VECTOR_MIN ?= -17
EE_BIN = $(OUT)/probe.elf
EE_OBJS = $(OUT)/main.o $(OUT)/core.o
EE_INCS = -I../../include
EE_LIBS = -lc -lkernel
EE_CFLAGS = -O2 -G8192 -D_EE -DVECTOR_MIN=$(VECTOR_MIN)
EE_CFLAGS += -DDECODER_SOURCE='"$(DECODER_SOURCE)"'

all: $(OUT) $(EE_BIN)
$(OUT):
	mkdir -p $@
$(OUT)/main.o: main.c $(DECODER_SOURCE) ../../include/libmpeg_internal.h | $(OUT)
	$(EE_CC) -I../../include $(EE_INCS) $(EE_CFLAGS) -c $< -o $@
$(OUT)/core.o: $(DECODER_CORE) | $(OUT)
	$(EE_CC) -I../../include $(EE_INCS) $(EE_CFLAGS) -c $< -o $@

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
