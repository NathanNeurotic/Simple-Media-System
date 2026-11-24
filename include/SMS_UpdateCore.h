/* Parsing and date arithmetic for the clock sync and the updater.
 * Nothing here touches the console, so tools/test_update_core.py runs it on the host.
 * Licensed under the Academic Free License version 2.0. */
#ifndef __SMS_UpdateCore_H
#define __SMS_UpdateCore_H

int UPD_DaysIn     ( int aYear, int aMonth );
/* Moves { year, month, day, hour, minute } by less than a day either way.
 * 0 when the result leaves 2000-2099, which the clock's two-digit year cannot hold. */
int UPD_ClockShift ( int* apT, int aMinutes );
/* The time zone setting, held in one byte: 0 follows the console's own zone and summer time,
 * 1..UPD_ZoneCount () picks a zone from the list, and bit 7 adds an hour of summer time to it. */
int UPD_ZoneCount   ( void );
/* Minutes east of UTC of zone 1..UPD_ZoneCount (). */
int UPD_ZoneMinutes ( int anIdx );
/* Minutes to add to the clock ( Japan time ) to get local time. aConsoleZone is the console's
 * own zone in minutes east of UTC; a setting that names no zone on the list uses it. */
int UPD_ZoneOffset  ( int aSetting, int aConsoleZone, int aConsoleSummer );
/* "Date: Fri, 13 Nov 2026 03:37:42 GMT" out of a reply's headers into
 * { year, month, day, hour, minute, second }, UTC. 0 unless the whole field is there and sane. */
int UPD_ParseDate  ( const char* apHeaders, int* apT );
/* SHA256SUMS.txt: "# build <number> <id>" and "<64 hex>  <name>" lines.
 * Takes the build number, its id and the hash of apName. 1 when all three are found. */
int UPD_ParseSums  ( const char* apText, const char* apName, unsigned char* apSHA,
                     unsigned int* apBuild, char* apID, int anIDSize );
/* "https://host/path" into host and the path. NULL unless it is https to GitHub. */
const char* UPD_SplitURL ( const char* apURL, char* apHost, int aHostSize );
/* A 32-bit little-endian MIPS executable. */
int UPD_IsELF      ( const unsigned char* apData, int aSize );
/* 1 when that ELF is the self-extracting ( packed ) build: its entry point is the
 * unpacker, which sits far above where SMS itself starts. */
int UPD_IsPacked   ( const unsigned char* apData, int aSize );

#endif  /* __SMS_UpdateCore_H */
