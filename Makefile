.SILENT:

BDM ?= 1

EE_OBJ_DIR = obj/
EE_INC_DIR = include/
EE_BIN_DIR = bin/
EE_SRC_DIR = src/

EE_BIN = $(EE_BIN_DIR)SMS.elf

EE_INCS    = -I$(EE_INC_DIR) -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2SDK)/ports/include -I$(PS2SDK)/sbv/include
EE_LDFLAGS = -L$(PS2SDK)/sbv/lib -L$(PS2SDK)/ee/lib -L$(PS2SDK)/ports/lib -L$(EE_SRC_DIR)/lzma2
EE_LIBS    = -lpatches -lc -lkernel -lmf
EE_CFLAGS := -Dmemset=mips_memset -Dmemcpy=mips_memcpy -D_EE -O2 -G8192 -mgpopt -Wall -mno-check-zero-division

# Short commit id shown at the end of the About screen so a tester can tell builds apart.
# CI passes the commit; a local build says "local".
SMS_BUILD_ID ?= local
EE_CFLAGS += -DSMS_BUILD_ID=\"$(SMS_BUILD_ID)\"

EE_OBJS  = main.o SMS_OS.o SMS_GS_0.o SMS_GS_1.o SMS_GS_2.o SMS_Timer.o           \
           SMS_MP123Core.o SMS_FileContext.o  SMS_H263.o                          \
           SMS_DSP.o SMS_DSP_MMI.o SMS_MPEG.o SMS_MPEG4.o SMS_VLC.o SMS_IPU.o     \
           SMS_AAC.o SMS_Utils.o SMS_MP123.o SMS_AC3.o SMS_SPU.o                  \
           SMS_Player.o  SMS_AC3_imdct.o SMS_MSMPEG4.o SMS_Codec.o                \
           SMS_VideoBuffer.o SMS_PlayerControl.o SMS_CDVD.o SMS_CDDA.o            \
           SMS_EE.o SMS_IOP.o SMS_PAD.o SMS_RC.o SMS_RC_0.o SMS_MC.o              \
           SMS_RingBuffer.o SMS_Container.o SMS_ContainerAVI.o SMS_ContainerMP3.o \
           SMS_ContainerM3U.o SMS_List.o SMS_Config.o About.o SMS_Data.o          \
           SMS_GSFont.o About_Data.o SMS_GUIcons.o SMS_ConfigIcon.o SMS_GUI.o     \
           SMS_GUIDevMenu.o  SMS_DirTree.o SMS_GUIFileMenu.o SMS_Artwork.o SMS_Locale.o \
           SMS_FileDir.o SMS_GUIMenu.o SMS_DTS.o SMS_SubtitleContext.o            \
           SMS_GUICmdProc.o SMS_GUIDesktop.o SMS_PlayerMenu.o SMS_Sounds.o        \
           SMS_DSP_QPel.o SMS_GUIMiniBrowser.o SMS_DSP_FFT.o SMS_Spectrum.o       \
           SMS_DMA_0.o SMS_IPU_0.o SMS_IPU_1.o SMS_GUIFileCtxMenu.o               \
           SMS_InverseCodePages.o SMS_ContainerMPEG_PS.o SMS_MPEG12.o libmpeg.o   \
           libmpeg_core.o SMS_DXSB.o SMS_ContainerOGG.o SMS_OGG.o SMS_CopyTree.o  \
           SMS_GUIMenuSMS.o SMS_GUISMBrowser.o SMS_WMA.o mbstring.o SMS_PCM.o     \
           SMS_ContainerASF.o SMS_GUInfoPanel.o SMS_ContainerMOV.o                \
           SMS_ContainerAAC.o SMS_ContainerFLAC.o SMS_FLAC.o SMS_ContainerAC3.o   \
           SMS_History.o SMS_PgInd.o SMS_VSync.o SMS_GUIClock.o SMS_DateTime.o    \
           SMS_PlayerBallSim.o SMS_SIF.o SMS_ContainerJPG.o SMS_FileMapping.o     \
           SMS_JPEGData.o SMS_JPEG.o SMS_Rescale.o SMS_MPEGInit.o                 \
           lzma2.o xz_crc32.o xz_dec_lzma2.o xz_dec_stream.o jellyfish_jpg.o     \
           SMS_IconsRGBA.o SMS_LoadingRGBA.o SMS_BallRGBA.o splash_jpg.o main_bg_mini_jpg.o \
           SMS_Update.o SMS_UpdateCore.o

ifeq ($(BDM),1)
  IRX_DIR = irx/
  IOP_OBJS = bdm_irx.o bdmfs_fatfs_irx.o usbd_irx.o usbmass_bd_irx.o sio2man_irx.o \
             mx4sio_bd_irx.o ata_bd_irx.o iLinkman_irx.o IEEE1394_bd_irx.o \
             mmceman_irx.o ds34usb_irx.o ds34bt_irx.o \
             udpfs_smap_irx.o udpfs_ministack_irx.o udpfs_ioman_irx.o libsd_irx.o \
             mcman_irx.o mcserv_irx.o   \
             padman_irx.o iomanx_irx.o filexio_irx.o smbman_irx.o smsnet_irx.o
  EE_LIBS += -lmc -lpadx -lfileXio
  EE_OBJS += $(IOP_OBJS)
  EE_OBJS += libds34usb.o libds34bt.o
  EE_CFLAGS += -DBDM

  # Set the clock from the Internet and Check for updates: the EE side of the socket service
  # ( iop/smsnet ), one verified HTTPS download ( SMS_TLS.c ) and the library it runs on.
  # mbedTLS 3.6.7 is kept as the 34 source files this build uses plus their headers, packed
  # in one archive ( tools/pack_mbedtls.py ) and unpacked into obj/ on the first build.
  UPD_OBJS     = SMS_Net.o SMS_TLS.o \
                 sectigo_e46_der.o sectigo_r46_der.o isrgrootx1_der.o isrg_root_x2_der.o
  MBEDTLS_TAR  = third_party/mbedtls-3.6.7-src.tar.gz
  MBEDTLS_DIR  = $(EE_OBJ_DIR)mbedtls/
  MBEDTLS_SRCS = aes asn1parse asn1write bignum bignum_core cipher cipher_wrap constant_time \
                 ctr_drbg ecdh ecdsa ecp ecp_curves entropy gcm md oid pk pk_ecc pk_wrap \
                 pkparse platform platform_util rsa rsa_alt_helpers sha256 sha512 \
                 ssl_ciphersuites ssl_client ssl_msg ssl_tls ssl_tls12_client x509 x509_crt
  MBEDTLS_OBJS = $(MBEDTLS_SRCS:%=tls_%.o)
  EE_OBJS += $(UPD_OBJS) $(MBEDTLS_OBJS)
endif

# When the build was made, as the commit's time in seconds: Check for updates offers a
# published build only when its number is higher. CI passes it; a local build is 0, so it
# is offered whatever is published. SMS_BUILD_STABLE=1 ( a tagged release ) follows the
# latest release instead of the rolling build.
SMS_BUILD_TIME ?= 0
UPD_DEFS = -DSMS_BUILD_TIME=$(SMS_BUILD_TIME)
ifeq ($(SMS_BUILD_STABLE),1)
  UPD_DEFS += -DSMS_BUILD_STABLE
endif

# Diagnostic build: `make SMS_DIAG=1 all`.
#
# Turns SMS_ExitCrumb back on, so each stage of the boot / exit / UDPFS-start paths latches
# an "E<nn> <what>" code on screen just BEFORE the call it names. If that call never returns,
# the code stays on screen -- which is how a hard hang gets localised on hardware that has no
# network and no memory card to log to. A tester photographs or films the screen and the last
# code is the blocking call.
#
# Deliberately OFF by default: release builds must not flash diagnostic codes at users.
ifeq ($(SMS_DIAG),1)
  # -G4096, NOT -G0. Re-enabling the crumb bodies pushes this tree past the GP-relative
  # addressing window at the release setting of -G8192, so the threshold has to come down --
  # but only as far as it must. -G0 was tried first and disables small-data entirely, which is
  # a large codegen change from the release build; a tester then reported the diagnostic ELF
  # black-screening on a PSX while the release ELF booted, and a build that does not start is
  # useless as a diagnostic. -G4096 is the largest value that still links, so objects under
  # 4 KB stay GP-addressed exactly as in release and the two builds differ as little as
  # possible. NOT confirmed as the black-screen cause -- it is the variable worth removing.
  EE_CFLAGS += -DSMS_DIAG -G4096
endif

EE_OBJS := $(EE_OBJS:%=$(EE_OBJ_DIR)%)

all: $(EE_OBJ_DIR) $(EE_BIN_DIR) $(EE_BIN)
	@$(EE_STRIP) --remove-section=.comment $(EE_BIN)

$(EE_OBJ_DIR):
	@$(MKDIR) -p $(EE_OBJ_DIR)

$(EE_BIN_DIR):
	@$(MKDIR) -p $(EE_BIN_DIR)

vpath %.irx.xz $(IRX_DIR)
vpath %.jpg images/

# IRX modules are embedded XZ-compressed (irx/*.irx.xz, made by tools/compress_irx.py)
# and loaded via SifExecDecompModuleBuffer. bin2c still names the array <name>_irx.
$(EE_OBJ_DIR)%_irx.c: %.irx.xz
	bin2c $< $@ $(*F)_irx
	@sed 's/aligned(16)/aligned(16), section(\"data\")/' -i $@

# The release build's small-data window is full to the byte, so this module's size word goes
# to .data like its body ( SMS_IOP.c declares it the same way ).
$(EE_OBJ_DIR)smsnet_irx.c: smsnet.irx.xz
	bin2c $< $@ smsnet_irx
	@sed -e 's/aligned(16)/aligned(16), section(\"data\")/' \
	     -e 's/size_smsnet_irx/size_smsnet_irx __attribute__((section(\".data\")))/' -i $@

$(EE_OBJ_DIR)%_jpg.c: %.jpg
	bin2c $< $@ $(*F)_jpg
	@sed 's/aligned(16)/aligned(16), section(\"data\")/' -i $@

$(EE_OBJ_DIR)%.o : $(EE_OBJ_DIR)%.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

# The clock, network and update code is compiled -G0: the release build's small-data window
# ( -G8192 ) is full, so these files keep their data out of it and export functions only.
UPD_CFLAGS = $(filter-out -G8192 -G4096,$(EE_CFLAGS)) -G0 -fno-common $(UPD_DEFS)
TLS_INCS   = -Ithird_party/mbedtls-port -I$(MBEDTLS_DIR)include -DMBEDTLS_CONFIG_FILE=\"sms_mbedtls_config.h\"

$(EE_OBJ_DIR)SMS_UpdateCore.o : $(EE_SRC_DIR)SMS_UpdateCore.c
	$(EE_CC) $(UPD_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJ_DIR)SMS_Net.o : $(EE_SRC_DIR)SMS_Net.c
	$(EE_CC) $(UPD_CFLAGS) $(EE_INCS) -Iiop/smsnet/include -c $< -o $@

ifeq ($(BDM),1)
$(EE_OBJ_DIR)SMS_Update.o : $(EE_SRC_DIR)SMS_Update.c $(MBEDTLS_DIR).unpacked
	$(EE_CC) $(UPD_CFLAGS) -std=gnu99 $(TLS_INCS) $(EE_INCS) -c $< -o $@

$(EE_OBJ_DIR)SMS_TLS.o : $(EE_SRC_DIR)SMS_TLS.c $(MBEDTLS_DIR).unpacked
	$(EE_CC) $(UPD_CFLAGS) -std=gnu99 $(TLS_INCS) $(EE_INCS) -c $< -o $@

$(MBEDTLS_DIR).unpacked : $(MBEDTLS_TAR) | $(EE_OBJ_DIR)
	rm -rf $(MBEDTLS_DIR)
	tar -xzf $(MBEDTLS_TAR) -C $(EE_OBJ_DIR)
	touch $@

$(MBEDTLS_SRCS:%=$(MBEDTLS_DIR)library/%.c) : $(MBEDTLS_DIR).unpacked

$(EE_OBJ_DIR)tls_%.o : $(MBEDTLS_DIR)library/%.c
	$(EE_CC) $(UPD_CFLAGS) -std=gnu99 -w $(TLS_INCS) -I$(MBEDTLS_DIR)library $(EE_INCS) -c $< -o $@

# The four certificate roots GitHub's two hosts chain to ( third_party/ca-roots/README.md ).
$(EE_OBJ_DIR)sectigo_e46_der.c : third_party/ca-roots/sectigo-e46.der
	bin2c $< $@ sectigo_e46_der
$(EE_OBJ_DIR)sectigo_r46_der.c : third_party/ca-roots/sectigo-r46.der
	bin2c $< $@ sectigo_r46_der
$(EE_OBJ_DIR)isrgrootx1_der.c : third_party/ca-roots/isrgrootx1.der
	bin2c $< $@ isrgrootx1_der
$(EE_OBJ_DIR)isrg_root_x2_der.c : third_party/ca-roots/isrg-root-x2.der
	bin2c $< $@ isrg_root_x2_der

$(EE_OBJ_DIR)%_der.o : $(EE_OBJ_DIR)%_der.c
	$(EE_CC) $(UPD_CFLAGS) $(EE_INCS) -c $< -o $@
else
$(EE_OBJ_DIR)SMS_Update.o : $(EE_SRC_DIR)SMS_Update.c
	$(EE_CC) $(UPD_CFLAGS) $(EE_INCS) -c $< -o $@
endif

$(EE_OBJ_DIR)%.o : $(EE_SRC_DIR)%.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJ_DIR)SMS_MPEG4.o : $(EE_SRC_DIR)SMS_MPEG4.c $(EE_INC_DIR)SMS_MPEG.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJ_DIR)SMS_MSMPEG4.o : $(EE_SRC_DIR)SMS_MSMPEG4.c $(EE_INC_DIR)SMS_MPEG.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJ_DIR)SMS_MPEG.o : $(EE_SRC_DIR)SMS_MPEG.c $(EE_INC_DIR)SMS_MPEG.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJ_DIR)SMS_H263.o : $(EE_SRC_DIR)SMS_H263.c $(EE_INC_DIR)SMS_MPEG.h
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

# The C/assembly motion layout is shared even though the core is handwritten.
$(EE_OBJ_DIR)libmpeg.o $(EE_OBJ_DIR)libmpeg_core.o: $(EE_INC_DIR)libmpeg_internal.h $(EE_INC_DIR)libmpeg.h

$(EE_OBJ_DIR)SMS_GS_1.o : $(EE_SRC_DIR)SMS_GS_1.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJ_DIR)SMS_GS_2.o : $(EE_SRC_DIR)SMS_GS_2.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJ_DIR)%.o : $(EE_SRC_DIR)%.s
	$(EE_AS) $(EE_ASFLAGS) $< -o $@

$(EE_OBJ_DIR)%.o : $(EE_SRC_DIR)%.S
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_BIN) : $(EE_OBJS) $(PS2SDK)/ee/startup/crt0.o
	$(EE_CC) -mno-crt0 -T$(PS2SDK)/ee/startup/linkfile $(EE_LDFLAGS) \
		     -o $(EE_BIN) $(PS2SDK)/ee/startup/crt0.o $(EE_OBJS) $(EE_LIBS) -Xlinker -Map -Xlinker ./obj/SMS.map

rebuild: clean all

# Self-extracting packed ELF ( ps2-packer, lzma stub ) built ALONGSIDE the plain ELF for
# the release. bin/SMS.elf stays the canonical / fallback download; bin/SMS-packed.elf is
# the same program compressed ~48% smaller ( 1.77MB -> ~0.9MB ) that decompresses itself
# into RAM at boot. Depends on `all` so it packs the fully linked + stripped ELF.
# ps2-packer ships in the ps2dev toolchain image ( /usr/local/ps2dev/bin ).
pack: all
	ps2-packer $(EE_BIN) $(EE_BIN_DIR)SMS-packed.elf

clean:
	@rm -f -r $(EE_BIN_DIR) $(EE_OBJ_DIR)

include $(PS2SDK)/Defs.make
