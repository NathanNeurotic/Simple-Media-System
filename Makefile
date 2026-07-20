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
           SMS_GUIDevMenu.o  SMS_DirTree.o SMS_GUIFileMenu.o SMS_Locale.o         \
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
           SMS_IconsRGBA.o SMS_LoadingRGBA.o SMS_BallRGBA.o splash_jpg.o main_bg_mini_jpg.o

ifeq ($(BDM),1)
  IRX_DIR = irx/
  IOP_OBJS = bdm_irx.o bdmfs_fatfs_irx.o usbd_irx.o usbmass_bd_irx.o sio2man_irx.o \
             mx4sio_bd_irx.o ata_bd_irx.o iLinkman_irx.o IEEE1394_bd_irx.o \
             mmceman_irx.o ds34usb_irx.o ds34bt_irx.o \
             udpfs_smap_irx.o udpfs_ministack_irx.o udpfs_ioman_irx.o \
             mcman_irx.o mcserv_irx.o   \
             padman_irx.o iomanx_irx.o filexio_irx.o smbman_irx.o
  EE_LIBS += -lmc -lpadx -lfileXio
  EE_OBJS += $(IOP_OBJS)
  EE_OBJS += libds34usb.o libds34bt.o SMS_UDPFSExit.o
  EE_LDFLAGS += -Wl,--wrap=SMS_IOPReset -Wl,--wrap=SMS_EExec -Wl,--wrap=SMS_IOPowerOff \
                -Wl,--wrap=SifIopReset -Wl,--wrap=SifInitRpc -Wl,--wrap=SifIopSync
  EE_CFLAGS += -DBDM
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
	@sed 's/aligned(16)/aligned(16), section("data")/' -i $@

$(EE_OBJ_DIR)%_jpg.c: %.jpg
	bin2c $< $@ $(*F)_jpg
	@sed 's/aligned(16)/aligned(16), section("data")/' -i $@

$(EE_OBJ_DIR)%.o : $(EE_OBJ_DIR)%.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

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
