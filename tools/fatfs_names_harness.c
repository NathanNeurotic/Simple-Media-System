/* Host harness: the PS2SDK FatFs sources (the ones tools/build_bdmfs_fatfs.sh pins) over a
 * file-backed disk image, to show what bdmfs_fatfs hands SMS for accented names.
 *   make <img> fat32|exfat : format the image and create the test names (UTF-8 config only)
 *   list <img>             : list the root, print each name, re-open it by that name
 * Build once per ffconf.h copy (the module's, with FF_FS_REENTRANT 0, FF_FS_NORTC 1 and
 * FF_USE_MKFS 1 for the host; then the same with build_bdmfs_fatfs.sh's three changes), plus a module_debug.h
 * stub defining u64/u32, M_DEBUG(...), M_PRINTF(...) and DEBUG_U64_2XU32(x) as empty:
 *   gcc -I. -I<conf dir> fatfs_names_harness.c <fatfs>/source/ff.c <fatfs>/source/ffunicode.c
 * Build the "make" binary with the UTF-8 copy (it writes the 8.3 aliases in codepage 850).
 * Stock config: FAT32 lists 8.3 aliases ("01TRAT~1.AVI", "ANIMES~1") and exFAT lists "?"
 * (and cannot open it); UTF-8 config lists and opens every name, including 8.3-only ones. */
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
	"07.Quiero Dejar Esta Vida tan Aparte.avi", "¡Ñandú! Plain ASCII-free.avi", "Plain ASCII name.avi",
	"AÑO.AVI", "ÑÑÑÑÑÑÑÑ.AVI",	/* made 8.3-only below, the way Spanish Windows stores them */
	NULL };

/* Drop the long-name entries of an upper-case 8.3 name so only its alias (OEM bytes) is left. */
static int sfn_only(const char *sfn11) {
	static BYTE d[1 << 20]; size_t n, i; long at;
	for (at = 0; fseek(img, at, SEEK_SET) == 0 && (n = fread(d, 1, sizeof d, img)) > 0; at += n)
		for (i = 0; i + 32 <= n; i += 32)
			if (!memcmp(d + i, sfn11, 11) && i >= 32 && d[i - 32 + 11] == 0x0F) {
				for (; i >= 32 && d[i - 32 + 11] == 0x0F; i -= 32) d[i - 32] = 0xE5;
				fseek(img, at, SEEK_SET);
				return fwrite(d, 1, n, img) != n;
			}
	return 1;
}

int main(int argc, char **argv) {
	FATFS fs; DIR dir; FILINFO fi; FIL f; BYTE work[4096]; int i, mk, bad = 0; FRESULT r;
	if (argc < 3 || (strcmp(argv[1], "list") && (strcmp(argv[1], "make") || argc < 4)))
		return fputs("usage: harness make <img> fat32|exfat | harness list <img>\n", stderr), 2;
	mk = !strcmp(argv[1], "make");
	if (!(img = fopen(argv[2], mk ? "r+b" : "rb"))) return perror(argv[2]), 2;
	if (mk) {
		MKFS_PARM p = { strcmp(argv[3], "exfat") ? (FM_FAT32 | FM_SFD) : (FM_EXFAT | FM_SFD), 0, 0, 0, 512 };
		if (f_mkfs("0:", &p, work, sizeof work)) return puts("mkfs failed"), 1;
		if ((r = f_mount(&fs, "0:", 1))) return printf("mount %d\n", r), 1;
		for (i = 0; names[i]; i++) {
			if (!strchr(names[i], '.')) { if (f_mkdir(names[i])) return printf("mkdir %s failed\n", names[i]), 1; continue; }
			if ((r = f_open(&f, names[i], FA_CREATE_NEW | FA_WRITE))) return printf("create %s failed %d\n", names[i], r), 1;
			f_close(&f);
		}
		f_unmount("0:");
		if (strcmp(argv[3], "exfat") && (sfn_only("A\xA5O     AVI") || sfn_only("\xA5\xA5\xA5\xA5\xA5\xA5\xA5\xA5" "AVI")))
			return puts("8.3-only patch failed"), 1;
		return 0;
	}
	if ((r = f_mount(&fs, "0:", 1)) || (r = f_opendir(&dir, "/"))) return printf("mount/opendir %d\n", r), 1;
	while ((r = f_readdir(&dir, &fi)) == FR_OK && fi.fname[0]) {
		FRESULT o = (fi.fattrib & AM_DIR) ? f_opendir(&(DIR){0}, fi.fname) : f_open(&f, fi.fname, FA_READ);
		if (!(fi.fattrib & AM_DIR) && o == FR_OK) f_close(&f);
		printf("  %-45s reopen=%s\n", fi.fname, o == FR_OK ? "ok" : "FAILED");
		bad |= o != FR_OK;
	}
	if (r) return printf("readdir %d\n", r), 1;
	return bad;
}
