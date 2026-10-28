/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2006/7 Eugene Plotnikov <e-plotnikov@operamail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#ifndef __SMS_GUI_H
#define __SMS_GUI_H

#include <tamtypes.h>

#define GUI_MSG_MOUNT_BIT  0x0000000000100000L
#define GUI_MSG_MOUNT_MASK 0x00000000001F0000L
#define GUI_MSG_PAD_MASK   0x000000000000FFFFL
#define GUI_MSG_MENU_BIT   0x0000000000200000L
#define GUI_MSG_USER_MASK  0xFFFFFFFFFFC00000L

#define GUI_MSG_USB    0x0000000000010000L
#define GUI_MSG_CDROM  0x0000000000020000L
#define GUI_MSG_HDD    0x0000000000030000L
#define GUI_MSG_CDDA   0x0000000000040000L
#define GUI_MSG_HOST   0x0000000000050000L
#define GUI_MSG_DVD    0x0000000000060000L
#define GUI_MSG_SMB    0x0000000000070000L
#define GUI_MSG_LOGIN  0x0000000000080000L
/* MMCE ( device id 7 ). Its natural mount value (7+1)<<16 == 0x80000 collides with
 * GUI_MSG_LOGIN, so it uses the next free mount slot (0x90000, decodes to raw id 8)
 * and GUIDevMenu_HandleMount remaps that back to device id 7. Still inside
 * GUI_MSG_MOUNT_MASK (0x1F0000). */
#define GUI_MSG_MMCE   0x0000000000090000L
/* UDPFS network drive ( device id 8 ). Follows MMCE's displaced numbering: the slots
 * are now one ahead of the device ids, so id 8 takes the next free mount slot
 * (0xA0000, decodes to raw id 9) and GUIDevMenu_HandleMount remaps it back to 8.
 * Still inside GUI_MSG_MOUNT_MASK (0x1F0000); the mount bit is 0x100000, so mount
 * slots run to 0xF0000 -- there is room, but note the ceiling. Device id 8 is
 * deliberately EVEN: several playback/browser sites test `g_CMedia & 1` to decide
 * whether the media is a disc and spin up the CDVD, so an odd id would make the drive
 * spin during network playback. */
#define GUI_MSG_UDPFS  0x00000000000A0000L

#define GUI_MSG_MEDIA_SELECTED 0x8000000000000000L
#define GUI_MSG_MEDIA_REMOVED  0x7000000000000000L
#define GUI_MSG_UPDATE_STATUS  0x6000000000000000L
#define GUI_MSG_REFILL_BROWSER 0x5000000000000000L
#define GUI_MSG_QUIT           0x4000000000000000L
#define GUI_MSG_FILE           0x3000000000000000L
#define GUI_MSG_FOLDER_MP3     0x2000000000000000L
#define GUI_MSG_RELOAD_BROWSER 0x1000000000000000L

#define DECLARE_GUI_OBJECT()                                \
 void ( *Render      ) ( struct GUIObject*, int           ); \
 int  ( *HandleEvent ) ( struct GUIObject*, u64           ); \
 void ( *SetFocus    ) ( struct GUIObject*, int           ); \
 void ( *Cleanup     ) ( struct GUIObject*                ); \
 u64*           m_pGSPacket;

typedef enum GUIHResult {

 GUIHResult_Void        = 0,
 GUIHResult_Handled     = 1,
 GUIHResult_ChangeFocus = 2

} GUIHResult;

typedef struct GUIObject {

 DECLARE_GUI_OBJECT()

} GUIObject;

void GUIObject_Cleanup ( GUIObject* );
/* Rebuild browser display packets while retaining its decoded thumbnails. */
void GUIFileMenu_CleanupPackets ( GUIObject* );

typedef enum GUIRedrawMethod {

 GUIRedrawMethod_InitClearAll = 0,
 GUIRedrawMethod_InitClearObj = 1,
 GUIRedrawMethod_Redraw       = 2,
 GUIRedrawMethod_RedrawClear  = 3,
 GUIRedrawMethod_ClearScreen  = 4

} GUIRedrawMethod;

extern GUIObject* g_pDesktop;
extern GUIObject* g_pStatusLine;
extern GUIObject* g_pDevMenu;
extern GUIObject* g_pFileMenu;
extern void*      g_pActiveNode;
extern int        g_SMBUnit;
extern int        g_SMBError;
extern int        g_SMBServerError;

extern int ( *GUI_ReadButtons  ) ( void );

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

void          GUI_Initialize   ( int                      );
int           GUI_WaitButtons  ( int, unsigned*, int      );
void          GUI_AddObject    ( const char*, GUIObject*  );
void          GUI_Redraw       ( GUIRedrawMethod          );
void          GUI_Status       ( char*                    );
void          GUI_Error        ( char*                    );
int           GUI_Question     ( char*                    );
void          GUI_Progress     ( char*, int, int          );
void          GUI_Run          ( void                     );
void          GUI_Suspend      ( void                     );
void          GUI_Resume       ( void                     );
u64           GUI_WaitEvent    ( void                     );
void          GUI_PostMessage  ( u64                      );
void          GUI_DeleteObject ( const char*              );
void          GUI_UpdateStatus ( void                     );
void          GUI_SetColors    ( void                     );
int           GUI_QuitPosted   ( void                     );
#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* __SMS_GUI_H */
