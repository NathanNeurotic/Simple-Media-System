/* The console clock ( read, set, set from the Internet ) and Check for updates.
 * Licensed under the Academic Free License version 2.0. */
#ifndef __SMS_Update_H
#define __SMS_Update_H

/* argv[ 0 ]: the file Check for updates overwrites. */
void SMS_UpdateSetBootPath ( const char* apPath );
/* Minutes to add to the console clock ( Japan time ) to get local time: the zone picked in
 * "Date, time and updates", or the console's own zone and summer time. */
int  SMS_ZoneOffset        ( void );
/* The console clock in the user's time zone: { year, month, day, hour, minute }.
 * 0 when the clock cannot be read. */
int  SMS_ClockGet          ( int* apT );
/* Sets the clock from the same five local values ( seconds = aSec ). 1 when the console took it. */
int  SMS_ClockSet          ( const int* apT, int aSec );
#ifdef BDM
/* Both talk to the user themselves ( status line, questions, errors ).
 * SMS_ClockFromInternet returns 1 when the clock was set. */
int  SMS_ClockFromInternet ( void );
void SMS_UpdateCheck       ( void );
#endif  /* BDM */

#endif  /* __SMS_Update_H */
