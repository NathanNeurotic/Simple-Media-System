/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2006 Eugene Plotnikov <e-plotnikov@operamail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#ifndef __SMS_SMB_H
#define __SMS_SMB_H

#define SMB_IOCTL_LOGIN  0x00000000
#define SMB_IOCTL_LOGOUT 0x00000001
#define SMB_IOCTL_MOUNT  0x00000002
#define SMB_IOCTL_UMOUNT 0x00000003
#define SMB_IOCTL_SENUM  0x00000004
#define SMB_IOCTL_ECHO   0x00000005
#define SMB_IOCTL_STOPC  0x00000006
#define SMB_IOCTL_SETCP  0x00000007

#define SMB_ERROR_NEGOTIATE 0x00000001
#define SMB_ERROR_LOGIN     0x00000002
#define SMB_ERROR_COMM      0x00000003

#define SMB_SENUM_SIZE 8096

/* ----------------------------------------------------------------------------
 * smbman ( modern ps2sdk SMB1 client ) devctl interface.
 *
 * smbman registers an iomanX device "smb" ( single connection, one open share
 * at a time ). File reads go through fileXio; directory listing through fio*;
 * every control op is a synchronous fileXioDevctl ( "smb:", SMB_DEVCTL_*, ... ).
 * Definitions mirror ps2sdk common/include/ps2smb.h.
 * ------------------------------------------------------------------------- */
#define NO_PASSWORD        -1
#define PLAINTEXT_PASSWORD  0
#define HASHED_PASSWORD     1

#define SMB_DEVCTL_GETPASSWORDHASHES 0xC0DE0001
#define SMB_DEVCTL_LOGON             0xC0DE0002
#define SMB_DEVCTL_LOGOFF            0xC0DE0003
#define SMB_DEVCTL_GETSHARELIST      0xC0DE0004
#define SMB_DEVCTL_OPENSHARE         0xC0DE0005
#define SMB_DEVCTL_CLOSESHARE        0xC0DE0006
#define SMB_DEVCTL_ECHO              0xC0DE0007
#define SMB_DEVCTL_QUERYDISKINFO     0xC0DE0008
/* SMS-private extension: int 0 = legacy byte mapping, 1 = Windows-1251. */
#define SMB_DEVCTL_SMS_CP1251        0xC0DE1251

#define SMB_DEVCTL_LOGON_ERR_CONN  0x1001
#define SMB_DEVCTL_LOGON_ERR_PROT  0x1002
#define SMB_DEVCTL_LOGON_ERR_LOGON 0x1003

typedef struct {  /* size = 536 */
 char serverIP[ 16 ];
 int  serverPort;
 char User[ 256 ];
 char Password[ 256 ];
 int  PasswordType;
} smbLogOn_in_t;

typedef struct {  /* size = 8 */
 void* EE_addr;
 int   maxent;
} smbGetShareList_in_t;

typedef struct {  /* size = 520 */
 char ShareName[ 256 ];
 char Password[ 256 ];
 int  PasswordType;
} smbOpenShare_in_t;

typedef struct {  /* size = 260 */
 char echo[ 256 ];
 int  len;
} smbEcho_in_t;

typedef struct {  /* size = 512 */
 char ShareName[ 256 ];
 char ShareComment[ 256 ];
} ShareEntry_t;

typedef struct SMBLoginInfo {

 char m_ServerIP  [ 16 ];
 char m_ServerName[ 16 ];
 char m_ClientName[ 16 ];
 char m_UserName  [ 32 ];
 char m_Password  [ 64 ];
 char m_fAsync;
 int  m_Port;          /* TCP port, default 1445 ( PS2-Servers ); 0 -> 1445 */
 char m_Share [ 64 ];  /* optional pre-set share name ( empty -> browse )    */

} SMBLoginInfo;

typedef struct SMBMountInfo {

 int  m_Unit        __attribute__(  ( packed )  );
 char m_Path[ 512 ];

} SMBMountInfo;

typedef struct SMBShareInfo {

          char  m_Name[ 13 ];
 unsigned char  m_Pad;
 unsigned short m_Type;
          char* m_pRemark;

} SMBShareInfo;

typedef struct SMBSEnumInfo {

 int           m_Unit  __attribute__(  ( packed )  );
 SMBShareInfo* m_pInfo __attribute__(  ( packed )  );

} SMBSEnumInfo;
#endif  /* __SMS_SMB_H */
