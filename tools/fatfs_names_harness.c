/* Host harness: the PS2SDK FatFs sources (the ones tools/build_bdmfs_fatfs.sh pins) over a
 * file-backed disk image, to show what bdmfs_fatfs hands SMS for accented names.
 *   make <img> fat32|exfat : format the image and create the test names (UTF-8 config only)
 *   list <img>             : list the root, print each name, re-open it by that name
 * Build once per ffconf.h copy (the module's, with FF_FS_REENTRANT 0, FF_FS_NORTC 1 and
 * FF_USE_MKFS 1 for the host; then the same with FF_LFN_UNICODE 2), plus a module_debug.h
 * stub defining u64/u32, M_DEBUG(...), M_PRINTF(...) and DEBUG_U64_2XU32(x) as empty:
 *   gcc -I. -I<conf dir> fatfs_names_harness.c <fatfs>/source/ff.c <fatfs>/source/ffunicode.c
 * Stock config: FAT32 lists 8.3 aliases ("01TR_T~1.AVI", "ANIMES~1") and exFAT lists "?"
 * (and cannot open it); UTF-8 config lists and opens every name. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ff.h"
#include "diskio.h"

static FILE *img;
DSTATUS disk_status(BYTE d) { return 0; }
DSTATUS disk_initialize(BYTE d) { return 0; }
DRESULT disk_read(BYTE d, BYTE *b, LBA_t s, UINT n) { fseek(img, (long)s * 512, SEEK_SET); return fread(b, 512, n, img) == n ? RES_OK : RES_ERROR; }
DRESULT disk_write(BYTE d, const BYTE *b, LBA_t s, UINT n) { fseek(img, (long)s * 512, SEEK_SET); return fwrite(b, 512, n, img) == n ? RES_OK : RES_ERROR; }
DRESULT disk_ioctl(BYTE d, BYTE c, void *b) {
	if (c == GET_SECTOR_COUNT) { fseek(img, 0, SEEK_END); *(LBA_t *)b = ftell(img) / 512; }
	if (c == GET_SECTOR_SIZE) *(WORD *)b = 512;
	if (c == GET_BLOCK_SIZE) *(DWORD *)b = 1;
	return RES_OK;
}

static const char *names[] = {
	"01 Trátame Suavemente.avi", "05 Cuándo.avi", "Animes en Español", "08 Cómo.avi",
	"07.Quiero Dejar Esta Vida tan Aparte.avi", "¡Ñandú! Plain ASCII-free.avi", "Plain ASCII name.avi", NULL };

int main(int argc, char **argv) {
	FATFS fs; DIR dir; FILINFO fi; FIL f; BYTE work[4096]; int i, bad = 0;
	img = fopen(argv[2], strcmp(argv[1], "make") ? "rb" : "r+b");
	if (!strcmp(argv[1], "make")) {
		MKFS_PARM p = { strcmp(argv[3], "exfat") ? (FM_FAT32 | FM_SFD) : (FM_EXFAT | FM_SFD), 0, 0, 0, 512 };
		if (f_mkfs("0:", &p, work, sizeof work)) return puts("mkfs failed"), 1;
		{ FRESULT m = f_mount(&fs, "0:", 1); if (m) return printf("mount %d\n", m), 1; }
		for (i = 0; names[i]; i++) {
			if (!strstr(names[i], ".avi")) { if (f_mkdir(names[i])) return printf("mkdir %s failed\n", names[i]), 1; continue; }
			{ FRESULT r = f_open(&f, names[i], FA_CREATE_NEW | FA_WRITE); if (r) return printf("create %s failed %d\n", names[i], r), 1; }
			f_close(&f);
		}
		return 0;
	}
	f_mount(&fs, "0:", 1);
	f_opendir(&dir, "/");
	while (f_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
		FRESULT r = (fi.fattrib & AM_DIR) ? f_opendir(&(DIR){0}, fi.fname) : f_open(&f, fi.fname, FA_READ);
		if (!(fi.fattrib & AM_DIR) && r == FR_OK) f_close(&f);
		printf("  %-45s reopen=%s\n", fi.fname, r == FR_OK ? "ok" : "FAILED");
		bad |= r != FR_OK;
	}
	return bad;
}
