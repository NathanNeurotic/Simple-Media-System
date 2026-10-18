/* AFL-3.0. Native harness: function bodies are extracted from the actual driver. */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "smb.h"
#define ZERO_PKT_ALIGNED(p, size) memset((p), 0, ((size) + 3) & ~3)
#define O_DIROPEN 0x1000
#define SMB_NAME_MAX 256
#define MAX_FDHANDLES 32
typedef struct { void *privdata; } iop_file_t;
/* HANDLE_TYPE */
static FHANDLE smbman_fdhandles[MAX_FDHANDLES];
static int UID = 1, TID = 2, locks, close_calls, close_result, socket_closes;
static int main_socket = 1, smb_reply_remaining;
static unsigned char wire[200000];
static int wire_size, wire_pos, largest_receive;
static struct {
    u32 sessionHeader;
    union {
        u8 u8buff[MAX_SMB_BUF + MAX_SMB_BUF_HDR];
        u16 u16buff[(MAX_SMB_BUF + MAX_SMB_BUF_HDR) / 2];
        s16 s16buff[(MAX_SMB_BUF + MAX_SMB_BUF_HDR) / 2];
        ReadAndXRequest_t readAndXRequest;
        ReadAndXResponse_t readAndXResponse;
        NetShareEnumRequest_t netShareEnumRequest;
        NetShareEnumResponse_t netShareEnumResponse;
    } smb;
    unsigned char canary[16];
} SMB_buf;
static int checks;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)
static void smb_io_lock(void) { ++locks; }
static void smb_io_unlock(void) { --locks; }
int smb_Close(int uid, int tid, int fid)
{ (void)uid; (void)tid; (void)fid; ++close_calls; return close_result; }
int smb_ReadFile(int uid, int tid, int fid, s64 offset, void *buf, int size)
{ (void)uid; (void)tid; (void)fid; (void)offset; (void)buf; return size; }
int smb_WriteFile(int uid, int tid, int fid, s64 offset, void *buf, int size)
{ (void)uid; (void)tid; (void)fid; (void)offset; (void)buf; return size; }
static int lwip_close(int socket) { (void)socket; ++socket_closes; return 0; }
static int lwip_send(int socket, const void *p, int n, int flags)
{ (void)socket; (void)p; (void)flags; return n; }
static int RecvTimeout(int socket, void *p, int n, int timeout)
{
    int count;
    (void)socket; (void)timeout;
    if (n > largest_receive) largest_receive = n;
    count = wire_size - wire_pos;
    if (count > n) count = n;
    if (count <= 0) return -1;
    memcpy(p, wire + wire_pos, count);
    wire_pos += count;
    return count;
}
/* DRIVER_FUNCTIONS */

static void reset(void)
{
    int i;
    memset(&SMB_buf, 0, sizeof(SMB_buf));
    memset(SMB_buf.canary, 0xa5, sizeof(SMB_buf.canary));
    memset(smbman_fdhandles, 0, sizeof(smbman_fdhandles));
    for (i = 0; i < MAX_FDHANDLES; ++i) smbman_fdhandles[i].smb_fid = -1;
    wire_size = wire_pos = largest_receive = 0;
    smb_reply_remaining = locks = close_calls = close_result = socket_closes = 0;
    main_socket = UID = TID = 1;
}
static void guards(void)
{
    unsigned int i;
    for (i = 0; i < sizeof(SMB_buf.canary); ++i) CHECK(SMB_buf.canary[i] == 0xa5);
    CHECK(locks == 0);
}
static void enqueue(const void *body, unsigned int size)
{
    CHECK(wire_size + size + 4 <= sizeof(wire));
    wire[wire_size++] = 0;
    wire[wire_size++] = size >> 16;
    wire[wire_size++] = size >> 8;
    wire[wire_size++] = size;
    memcpy(wire + wire_size, body, size);
    wire_size += size;
}
static unsigned char packet[70000];
static void read_packet(unsigned int offset, unsigned int length, unsigned int bytes)
{
    ReadAndXResponse_t *reply = (ReadAndXResponse_t *)packet;
    memset(packet, 0, sizeof(packet));
    reply->smbH.Magic = SMB_MAGIC;
    reply->DataOffset = offset;
    reply->DataLengthLow = length;
    reply->DataLengthHigh = length >> 16;
    if (offset < bytes) memset(packet + offset, 'D', bytes - offset);
    enqueue(packet, bytes);
}
static void test_read(void)
{
    unsigned char out[24];
    unsigned int i, h = sizeof(ReadAndXResponse_t);
    reset(); memset(out, 0xa5, sizeof(out));
    read_packet(h + 4, 4, h + 11); /* padding + payload + trailing bytes */
    CHECK(smb_ReadAndX(1, 1, 1, 0, out + 8, 4) == 4);
    CHECK(!memcmp(out + 8, "DDDD", 4));
    CHECK(wire_pos == wire_size && smb_reply_remaining == 0); guards();
    for (i = 0; i < 8; ++i) CHECK(out[i] == 0xa5 && out[12 + i] == 0xa5);
    for (i = 0; i < h; ++i) {
        reset(); memset(packet, 0, h); enqueue(packet, i);
        read_packet(h, 4, h + 4);
        CHECK(smb_ReadAndX(1, 1, 1, 0, out + 8, 4) < 0);
        CHECK(smb_ReadAndX(1, 1, 1, 0, out + 8, 4) == 4);
        CHECK(wire_pos == wire_size); guards();
    }
    for (i = 0; i < 512; ++i) {
        unsigned int offset = (i * 127) & 65535;
        unsigned int length = i % 23;
        int expected;
        reset(); memset(out, 0xa5, sizeof(out));
        read_packet(offset, length, h + 16);
        expected = offset >= h && offset <= h + 16 && length <= 4 && length <= h + 16 - offset;
        CHECK((smb_ReadAndX(1, 1, 1, 0, out + 8, 4) >= 0) == expected);
        CHECK(wire_pos == wire_size && !smb_reply_remaining);
        for (unsigned int n = 0; n < 8; ++n) CHECK(out[n] == 0xa5 && out[12 + n] == 0xa5);
        guards();
    }
    reset(); memset(out, 0xa5, sizeof(out));
    read_packet(h, 65536, h + 4);
    CHECK(smb_ReadAndX(1, 1, 1, 0, out + 8, 4) < 0); guards();
    reset(); read_packet(65535, 4, 65539);
    CHECK(smb_ReadAndX(1, 1, 1, 0, out + 8, 4) == 4);
    CHECK(largest_receive <= 128); guards();
    reset(); read_packet(h, 4, h + 4); wire_size -= 2;
    CHECK(smb_ReadAndX(1, 1, 1, 0, out + 8, 4) < 0);
    CHECK(socket_closes == 1 && main_socket == -1 && !smb_reply_remaining); guards();
    reset(); memset(packet, 0, 5000); enqueue(packet, 5000);
    nb_SetSessionMessage(0);
    CHECK(GetSMBServerReply(0, NULL, 0) < 0);
    CHECK(wire_pos == wire_size && largest_receive <= 128); guards();
}
static unsigned int rap_packet(unsigned int comment_size, unsigned int converter)
{
    NetShareEnumResponse_t *reply = (NetShareEnumResponse_t *)packet;
    unsigned int po = sizeof(*reply) + 2, offset = po + 9, bytes = 20 + comment_size;
    memset(packet, 0, sizeof(packet));
    reply->smbH.Magic = SMB_MAGIC;
    reply->smbTrans.ParamOffset = po;
    reply->smbTrans.ParamCount = reply->smbTrans.TotalParamCount = 8;
    reply->smbTrans.DataOffset = offset;
    reply->smbTrans.DataCount = reply->smbTrans.TotalDataCount = bytes;
    packet[po + 2] = converter;
    packet[po + 3] = converter >> 8;
    packet[po + 4] = 1;
    packet[po + 6] = packet[po + 7] = 255; /* TotalAvailable is not EntriesRead. */
    memcpy(packet + offset, "MEDIA", 6);
    packet[offset + 16] = 20 + converter;
    packet[offset + 17] = (20 + converter) >> 8;
    memset(packet + offset + 20, 'c', comment_size);
    packet[offset + bytes - 1] = 0;
    return offset + bytes;
}
static void test_rap(void)
{
    struct { unsigned char before[8]; ShareEntry_t entry; unsigned char after[8]; } out;
    unsigned int i, size, po, offset;
    reset(); memset(&out, 0xa5, sizeof(out)); size = rap_packet(4, 100); enqueue(packet, size);
    CHECK(smb_NetShareEnum(1, 1, &out.entry, 0, 1) == 1);
    CHECK(!strcmp(out.entry.ShareName, "MEDIA") && !strcmp(out.entry.ShareComment, "ccc"));
    for (i = 0; i < 8; ++i) CHECK(out.before[i] == 0xa5 && out.after[i] == 0xa5);
    guards();
    reset(); size = rap_packet(500, 0); enqueue(packet, size);
    CHECK(smb_NetShareEnum(1, 1, &out.entry, 0, 1) == 1);
    CHECK(strlen(out.entry.ShareComment) == 255); guards();
    reset(); size = rap_packet(4, 0); enqueue(packet, size);
    CHECK(smb_NetShareEnum(1, 1, NULL, 0, 0) == 1); guards();
    for (i = 0; i < 9; ++i) {
        NetShareEnumResponse_t *reply;
        reset(); size = rap_packet(4, 100); reply = (NetShareEnumResponse_t *)packet;
        po = reply->smbTrans.ParamOffset; offset = reply->smbTrans.DataOffset;
        switch (i) {
        case 0: reply->smbTrans.ParamOffset = 65535; break;
        case 1: reply->smbTrans.DataOffset = 65535; break;
        case 2: packet[po + 4] = packet[po + 5] = 255; break;
        case 3: memset(packet + offset, 'X', 13); break;
        case 4: packet[offset + 16] = 1; break; /* converter underflow */
        case 5: packet[offset + 16] = packet[offset + 17] = 255; break;
        case 6: packet[size - 1] = 'c'; break; /* unterminated comment */
        case 7: reply->smbTrans.DataDisplacement = 1; break;
        case 8: size = sizeof(*reply) - 1; break;
        }
        enqueue(packet, size);
        CHECK(smb_NetShareEnum(1, 1, &out.entry, 0, 1) < 0); guards();
    }
}
static void test_handles(void)
{
    iop_file_t files[MAX_FDHANDLES];
    int round, i;
    reset();
    CHECK(smb_close(NULL) == -EBADF);
    memset(files, 0, sizeof(files)); CHECK(smb_close(files) == -EBADF);
    for (round = 0; round < 64; ++round) {
        for (i = 0; i < MAX_FDHANDLES; ++i) {
            FHANDLE *fh = smbman_getfilefreeslot(); CHECK(fh != NULL);
            files[i].privdata = fh; fh->f = files + i; fh->mode = O_DIROPEN;
        }
        CHECK(smbman_getfilefreeslot() == NULL);
        UID = TID = round & 1 ? -1 : 1;
        for (i = 0; i < MAX_FDHANDLES; ++i) {
            CHECK(smb_close(files + i) == 0); CHECK(files[i].privdata == NULL);
        }
    }
    CHECK(close_calls == 0); guards();
    reset(); files[0].privdata = smbman_fdhandles;
    smbman_fdhandles[0].f = files; smbman_fdhandles[0].smb_fid = 3;
    close_result = -EIO;
    CHECK(smb_close(files) == -EIO); CHECK(files[0].privdata == NULL);
    CHECK(smbman_getfilefreeslot() == smbman_fdhandles); guards();
    reset();
    for (i = 0; i < MAX_FDHANDLES; ++i) {
        files[i].privdata = smbman_fdhandles + i;
        smbman_fdhandles[i].f = files + i;
        smbman_fdhandles[i].smb_fid = i;
        smbman_fdhandles[i].mode = i & 1 ? O_DIROPEN : 0;
    }
    smb_closeAll(); CHECK(close_calls == 16);
    for (i = 0; i < MAX_FDHANDLES; ++i) CHECK(files[i].privdata == NULL);
    for (i = 0; i < MAX_FDHANDLES; ++i) {
        CHECK(smb_read(files + i, packet, 1) == -EBADF);
        CHECK(smb_write(files + i, packet, 1) == -EBADF);
        CHECK(smb_lseek64(files + i, 0, SEEK_SET) == -EBADF);
    }
    CHECK(smb_read(NULL, packet, 1) == -EBADF);
    CHECK(smb_write(NULL, packet, 1) == -EBADF);
    CHECK(smb_lseek64(NULL, 0, SEEK_SET) == -EBADF);
    CHECK(smbman_getfilefreeslot() == smbman_fdhandles); guards();
}
int main(void)
{
    test_read(); test_rap(); test_handles();
    printf("PASS: %d checks on exact SMB transport/read/RAP/close functions\n", checks);
    return 0;
}
