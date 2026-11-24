IOP_CC = iop-gcc
IOP_OBJS = obj/smsnet.o obj/dns.o obj/imports.o
IOP_CFLAGS = -Wall -Os -G0 -fno-builtin -D_IOP -Isrc -Iinclude -I$(PS2SDK)/iop/include -I$(PS2SDK)/common/include

all: bin/smsnet.irx

.PHONY: all clean
clean:
	rm -f $(IOP_OBJS) obj/imports.c bin/smsnet.irx

$(IOP_OBJS): $(wildcard src/*.h) $(wildcard include/*.h)

obj/ bin/:
	mkdir -p $@

obj/%.o: src/%.c | obj/
	$(IOP_CC) $(IOP_CFLAGS) -c $< -o $@

obj/imports.c: src/imports.lst | obj/
	printf '%s\n' '#include "irx_imports.h"' > $@
	cat $< >> $@

obj/imports.o: obj/imports.c
	$(IOP_CC) $(IOP_CFLAGS) -c $< -o $@

# The v1.0 image has the legacy IOP linker, which emits IRX directly using
# its built-in script ( same as iop/smbman-sms ).
bin/smsnet.irx: $(IOP_OBJS) | bin/
	$(IOP_CC) -nostdlib -s -L$(PS2SDK)/iop/lib -o $@ $(IOP_OBJS) -lkernel -lgcc
