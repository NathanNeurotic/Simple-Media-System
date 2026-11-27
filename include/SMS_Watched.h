/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#ifndef __SMS_Watched_H
#define __SMS_Watched_H

/* A video counts as watched when playback ends on its own, or when the user stops it
 * at or after this percentage of its duration ( end credits ). */
#define SMS_WATCHED_PERCENT 90

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

void SMS_WatchedLoad   ( void        );
int  SMS_WatchedLook   ( const char* );
void SMS_WatchedAdd    ( const char* );
int  SMS_WatchedRemove ( const char* );
void SMS_WatchedSave   ( void        );

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* __SMS_Watched_H */
