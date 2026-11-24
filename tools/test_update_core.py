"""Clock sync and Check for updates, compiled from production code and run on the host.

Part 1: src/SMS_UpdateCore.c ( dates, SHA256SUMS.txt, redirect hosts, ELF checks ) and
        iop/smsnet/src/dns.c ( name lookup packets ).
Part 2: src/SMS_TLS.c with the mbedTLS sources and configuration the PS2 build uses, against
        a local HTTPS server with a throwaway certificate authority: a good download, and
        every way a download must be refused.

Neither part runs PS2 code: the EE build of the same sources is not exercised here.
"""
import http.server
import os
from pathlib import Path
import shutil
import ssl
import subprocess
import sys
import tarfile
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
CC = os.environ.get("CC", "gcc")

CORE = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "SMS_UpdateCore.h"
#include "dns.h"

static void shift(int y,int mo,int d,int h,int mi,int by,int ok,int ey,int emo,int ed,int eh,int emi) {
 int t[5]={y,mo,d,h,mi};
 assert(UPD_ClockShift(t,by)==ok);
 if(ok) assert(t[0]==ey&&t[1]==emo&&t[2]==ed&&t[3]==eh&&t[4]==emi);
}
#define A "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
#define B "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"
int main(void) {
 int t[6]; unsigned char sha[32]; unsigned int build=0; char id[16], host[64]; const char* p;
 unsigned char elf[64], q[64], ip[4]; int n, i;

 assert(UPD_DaysIn(2026,2)==28 && UPD_DaysIn(2028,2)==29 && UPD_DaysIn(2026,12)==31 && UPD_DaysIn(2026,4)==30);
 shift(2026,12,31,23,30, 540,1, 2027,1,1,8,30);     /* UTC to Japan time over a year end */
 shift(2027,1,1,0,10,    -60,1, 2026,12,31,23,10);
 shift(2028,2,28,23,0,   120,1, 2028,2,29,1,0);     /* leap day */
 shift(2026,3,1,0,0,     -1, 1, 2026,2,28,23,59);
 shift(2026,6,15,12,0,   0,  1, 2026,6,15,12,0);
 shift(2099,12,31,23,0,  120,0, 0,0,0,0,0);         /* past what the clock can hold */
 shift(2000,1,1,0,0,     -1, 0, 0,0,0,0,0);

 assert(UPD_ParseDate("HTTP/1.1 301 Moved\r\nServer: x\r\nDate: Fri, 13 Nov 2026 03:37:42 GMT\r\n\r\n",t));
 assert(t[0]==2026&&t[1]==11&&t[2]==13&&t[3]==3&&t[4]==37&&t[5]==42);
 assert(UPD_ParseDate("HTTP/1.1 200 OK\r\ndate: Tue, 29 Feb 2028 23:59:59 GMT\r\n",t) && t[1]==2 && t[2]==29);
 assert(!UPD_ParseDate("HTTP/1.1 200 OK\r\nDate: Fri, 13 Nov 2026 03:37:4",t));          /* cut short */
 assert(!UPD_ParseDate("HTTP/1.1 200 OK\r\nDate: Fri, 13 Nov 2026 03:37:42 GMT",t));      /* line not ended yet */
 assert(!UPD_ParseDate("HTTP/1.1 200 OK\r\nServer: x\r\n\r\n",t));
 assert(!UPD_ParseDate("Date: Fri, 13 Nov 2026 03:37:42 GMT\r\n",t));                     /* never the first line */
 assert(!UPD_ParseDate("HTTP/1.1 200 OK\r\nDate: Fri, 30 Feb 2026 03:37:42 GMT\r\n",t));
 assert(!UPD_ParseDate("HTTP/1.1 200 OK\r\nDate: Fri, 13 Nov 1999 03:37:42 GMT\r\n",t));
 assert(!UPD_ParseDate("HTTP/1.1 200 OK\r\nDate: Fri, 13 Xyz 2026 03:37:42 GMT\r\n",t));
 assert(!UPD_ParseDate("HTTP/1.1 200 OK\r\nDate: Fri, 13 Nov 2026 24:37:42 GMT\r\n",t));
 assert(!UPD_ParseDate("HTTP/1.1 200 OK\r\nX-Date: Fri, 13 Nov 2026 03:37:42 GMT\r\n",t));

 #define SUMS "# build 1795000000 abc1234\n" A "  Simple-Media-System.elf\n" B "  Simple-Media-System-packed.elf\n"
 assert(UPD_ParseSums(SUMS,"Simple-Media-System.elf",sha,&build,id,sizeof id));
 assert(build==1795000000u && !strcmp(id,"abc1234") && sha[0]==0x01 && sha[31]==0xef);
 assert(UPD_ParseSums(SUMS,"Simple-Media-System-packed.elf",sha,&build,id,sizeof id) && sha[0]==0xfe && sha[31]==0x10);
 assert(!UPD_ParseSums(SUMS,"Simple-Media-System-DIAG.elf",sha,&build,id,sizeof id));
 assert(!UPD_ParseSums(SUMS,"Media-System.elf",sha,&build,id,sizeof id));                 /* a whole name only */
 assert(!UPD_ParseSums(A "  Simple-Media-System.elf\n","Simple-Media-System.elf",sha,&build,id,sizeof id));   /* no build line */
 assert(!UPD_ParseSums("# build 1795000000 abc1234\n","Simple-Media-System.elf",sha,&build,id,sizeof id));
 assert(!UPD_ParseSums("# build  abc1234\n" A "  x.elf\n","x.elf",sha,&build,id,sizeof id));
 assert(!UPD_ParseSums("# build 12 ab c\n" A "  x.elf\n","x.elf",sha,&build,id,sizeof id));
 assert(!UPD_ParseSums("# build 12 abc\n0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeg  x.elf\n","x.elf",sha,&build,id,sizeof id));
 assert(!UPD_ParseSums("# build 12 abc\n0123456789abcdef  x.elf\n","x.elf",sha,&build,id,sizeof id));
 assert(UPD_ParseSums("# build 12 abc\r\n" A " *x.elf\r\n","x.elf",sha,&build,id,sizeof id) && build==12);
 assert(!UPD_ParseSums("<html>Not Found</html>","x.elf",sha,&build,id,sizeof id));
 /* a later, overlong build line must not write over the accepted ID */
 assert(UPD_ParseSums("# build 12 abc\n# build 13 0123456789012345678901234567890123456789012345678901234567890123456789\n" A "  x.elf\n","x.elf",sha,&build,id,sizeof id) && build==12 && !strcmp(id,"abc"));

 p=UPD_SplitURL("https://github.com/a/b",host,sizeof host); assert(p && !strcmp(p,"/a/b") && !strcmp(host,"github.com"));
 p=UPD_SplitURL("https://release-assets.githubusercontent.com/x?y=1&z=2",host,sizeof host); assert(p && !strcmp(p,"/x?y=1&z=2"));
 assert(UPD_SplitURL("https://objects.githubusercontent.com/x",host,sizeof host));
 assert(!UPD_SplitURL("http://github.com/a",host,sizeof host));                            /* never plain HTTP */
 assert(!UPD_SplitURL("https://github.com.example.org/a",host,sizeof host));
 assert(!UPD_SplitURL("https://example.org/github.com/",host,sizeof host));
 assert(!UPD_SplitURL("https://github.com@example.org/a",host,sizeof host));
 assert(!UPD_SplitURL("https://github.com:444/a",host,sizeof host));
 assert(!UPD_SplitURL("https://github.com",host,sizeof host));
 assert(!UPD_SplitURL("https:///a",host,sizeof host));
 assert(!UPD_SplitURL("/a/b",host,sizeof host));                                           /* relative */
 assert(!UPD_SplitURL("https://release-assets.githubusercontent.com/x",host,16));          /* does not fit */

 memset(elf,0,sizeof elf); memcpy(elf,"\x7f" "ELF\x01\x01\x01",7); elf[16]=2; elf[18]=8;
 elf[24]=0xd8; elf[25]=0x00; elf[26]=0x10; elf[27]=0x00;                                    /* entry 0x001000d8 */
 assert(UPD_IsELF(elf,sizeof elf) && !UPD_IsPacked(elf,sizeof elf));
 elf[24]=0x1c; elf[25]=0x00; elf[26]=0xd0; elf[27]=0x01;                                    /* entry 0x01d0001c */
 assert(UPD_IsPacked(elf,sizeof elf));
 assert(!UPD_IsELF(elf,40));
 elf[18]=3; assert(!UPD_IsELF(elf,sizeof elf) && !UPD_IsPacked(elf,sizeof elf)); elf[18]=8;  /* not MIPS */
 elf[0]='M'; assert(!UPD_IsELF(elf,sizeof elf));

 n=smsdns_query(q,sizeof q,"github.com",0x1234);
 assert(n==28 && q[0]==0x12 && q[1]==0x34 && q[2]==1 && q[5]==1 && q[12]==6 && !memcmp(q+13,"github",6) && q[19]==3 && q[23]==0 && q[25]==1 && q[27]==1);
 {
  /* The question, a CNAME answer, then the address. */
  unsigned char r[96]; int e=n;
  memcpy(r,q,n); r[2]=0x81; r[3]=0x80; r[7]=2;
  r[e++]=0xC0; r[e++]=12; r[e++]=0; r[e++]=5; r[e++]=0; r[e++]=1; e+=4; r[e++]=0; r[e++]=2; r[e++]=0xC0; r[e++]=12;
  r[e++]=0xC0; r[e++]=12; r[e++]=0; r[e++]=1; r[e++]=0; r[e++]=1; e+=4; r[e++]=0; r[e++]=4; r[e++]=140; r[e++]=82; r[e++]=112; r[e++]=3;
  assert(smsdns_answer(r,e,0x1234,ip) && ip[0]==140 && ip[1]==82 && ip[2]==112 && ip[3]==3);
  assert(!smsdns_answer(r,e,0x1235,ip));                                                    /* another question's reply */
  for(i=0;i<e;i++) assert(!smsdns_answer(r,i,0x1234,ip));                                   /* cut short anywhere */
  r[3]=0x83; assert(!smsdns_answer(r,e,0x1234,ip)); r[3]=0x80;                              /* "no such name" */
  r[2]=0x01; assert(!smsdns_answer(r,e,0x1234,ip)); r[2]=0x81;                              /* not a reply */
  r[7]=1; assert(!smsdns_answer(r,e,0x1234,ip));                                            /* only the CNAME */
  assert(!smsdns_query(q,20,"github.com",1) && !smsdns_query(q,sizeof q,"",1) && !smsdns_query(q,sizeof q,"a..b",1));
 }
 puts("ok");
 return 0;
}
"""

TLS = r"""
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "SMS_TLS.h"
#include "mbedtls/ssl.h"
#include "mbedtls/platform_time.h"

static long long g_Now;
static long long now(void) { return g_Now; }
mbedtls_ms_time_t mbedtls_ms_time(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1000LL+t.tv_nsec/1000000; }
struct tm* mbedtls_platform_gmtime_r(const mbedtls_time_t* t, struct tm* b) { return gmtime_r(t,b); }
int mbedtls_hardware_poll(void* d, unsigned char* o, size_t l, size_t* ol) {
 FILE* f=fopen("/dev/urandom","rb"); (void)d;
 if(!f || fread(o,1,l,f)!=l) return -1;
 fclose(f); *ol=l; return 0;
}
static int bsend(void* c,const unsigned char* b,size_t l) { int n=send(*(int*)c,b,l,0); return n<0?MBEDTLS_ERR_SSL_INTERNAL_ERROR:n; }
static int brecv(void* c,unsigned char* b,size_t l) { int n=recv(*(int*)c,b,l,0); return n<0?MBEDTLS_ERR_SSL_INTERNAL_ERROR:n; }
static char g_Body[4096]; static int g_Len;
static int sink(void* c,const char* d,int l) { (void)c; if(g_Len+l>(int)sizeof g_Body) return 1; memcpy(g_Body+g_Len,d,l); g_Len+=l; return 0; }

/* port host path root.der now max_body */
int main(int argc,char** argv) {
 static unsigned char der[4096]; static char buf[16384], loc[512];
 struct sockaddr_in to; ratls_anchor_t root; ratls_io_t io={bsend,brecv,NULL,now};
 FILE* f=fopen(argv[4],"rb"); int s, rc;
 (void)argc;
 root.der=der; root.len=fread(der,1,sizeof der,f); fclose(f);
 g_Now=atoll(argv[5]);
 s=socket(AF_INET,SOCK_STREAM,0);
 memset(&to,0,sizeof to); to.sin_family=AF_INET; to.sin_port=htons(atoi(argv[1])); to.sin_addr.s_addr=htonl(0x7F000001);
 if(connect(s,(struct sockaddr*)&to,sizeof to)) return 2;
 io.ctx=&s;
 rc=raTlsDownload(&io,argv[2],argv[3],"SMS-updater",&root,1,buf,sizeof buf,atoi(argv[6]),sink,NULL,loc,sizeof loc);
 close(s);
 printf("%d|%d|%.*s|%s\n",rc,raTlsBodyLength,g_Len,g_Body,loc);
 return 0;
}
"""

MBEDTLS = ("aes asn1parse asn1write bignum bignum_core cipher cipher_wrap constant_time ctr_drbg ecdh ecdsa ecp "
           "ecp_curves entropy gcm md oid pk pk_ecc pk_wrap pkparse platform platform_util rsa rsa_alt_helpers "
           "sha256 sha512 ssl_ciphersuites ssl_client ssl_msg ssl_tls ssl_tls12_client x509 x509_crt").split()


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)


def part1(tmp):
    (tmp / "core.c").write_text(CORE)
    run([CC, "-std=gnu99", "-Wall", "-Werror", "-I" + str(ROOT / "include"), "-I" + str(ROOT / "iop/smsnet/src"),
         str(tmp / "core.c"), str(ROOT / "src/SMS_UpdateCore.c"), str(ROOT / "iop/smsnet/src/dns.c"), "-o", str(tmp / "core")])
    assert run([str(tmp / "core")]).stdout.strip() == "ok"
    print("PASS: dates, SHA256SUMS.txt, redirect hosts, ELF checks and name lookup packets")


def makefile_sources():
    text = (ROOT / "Makefile").read_text()
    block = text.split("MBEDTLS_SRCS = ", 1)[1].split("MBEDTLS_OBJS", 1)[0]
    return block.replace("\\", " ").split()


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        body = b"hello from the release\n" * 40
        if self.path == "/ok":
            head = "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n" % len(body)
        elif self.path == "/redirect":
            head, body = "HTTP/1.1 302 Found\r\nLocation: https://github.com/next\r\nContent-Length: 0\r\n", b""
        elif self.path == "/chunked":
            head, body = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n", b"5\r\nhello\r\n0\r\n\r\n"
        elif self.path == "/gzip":
            head = "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: %d\r\n" % len(body)
        elif self.path == "/short":
            head = "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n" % (len(body) + 100)   # then the server hangs up
        elif self.path == "/nolength":
            head = "HTTP/1.1 200 OK\r\n"
        elif self.path == "/http10":
            head = "HTTP/2 200\r\nContent-Length: %d\r\n" % len(body)
        else:
            head, body = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n", b"Not Found"
        self.wfile.write((head + "Connection: close\r\n\r\n").encode() + body)
        self.close_connection = True


def part2(tmp):
    if not shutil.which("openssl"):
        print("SKIP: no openssl on this machine; the HTTPS checks were not run")
        return
    assert makefile_sources() == MBEDTLS, "the Makefile builds a different set of mbedTLS sources than this test"
    with tarfile.open(ROOT / "third_party/mbedtls-3.6.7-src.tar.gz") as tar:
        tar.extractall(tmp, filter="data")
    # The PS2 configuration as it is. Only the two stand-in C headers are left out: the host has real ones.
    shutil.copy(ROOT / "third_party/mbedtls-port/sms_mbedtls_config.h", tmp / "sms_mbedtls_config.h")
    (tmp / "tls.c").write_text(TLS)
    run([CC, "-std=gnu99", "-O1", "-w", "-DMBEDTLS_CONFIG_FILE=\"sms_mbedtls_config.h\"", "-I" + str(tmp),
         "-I" + str(tmp / "mbedtls/include"), "-I" + str(tmp / "mbedtls/library"), "-I" + str(ROOT / "include"),
         str(tmp / "tls.c"), str(ROOT / "src/SMS_TLS.c")] + [str(tmp / "mbedtls/library" / (n + ".c")) for n in MBEDTLS]
        + ["-o", str(tmp / "tls")])

    def ca(name):
        run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "30", "-subj", "/CN=" + name,
             "-keyout", name + ".key", "-out", name + ".pem", "-addext", "basicConstraints=critical,CA:TRUE",
             "-addext", "keyUsage=critical,keyCertSign"], cwd=tmp)
        run(["openssl", "x509", "-in", name + ".pem", "-outform", "DER", "-out", name + ".der"], cwd=tmp)
    ca("root")
    ca("other")
    (tmp / "ext").write_text("subjectAltName=DNS:localhost\nkeyUsage=digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\n")
    run(["openssl", "req", "-newkey", "rsa:2048", "-nodes", "-subj", "/CN=localhost", "-keyout", "srv.key", "-out", "srv.csr"], cwd=tmp)
    run(["openssl", "x509", "-req", "-in", "srv.csr", "-CA", "root.pem", "-CAkey", "root.key", "-CAcreateserial",
         "-days", "30", "-extfile", "ext", "-out", "srv.pem"], cwd=tmp)

    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(tmp / "srv.pem", tmp / "srv.key")
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.socket = ctx.wrap_socket(server.socket, server_side=True)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    port = str(server.server_address[1])
    today = int(time.time())

    def get(path, host="localhost", root="root.der", now=today, max_body=4096):
        out = run([str(tmp / "tls"), port, host, path, str(tmp / root), str(now), str(max_body)]).stdout.strip()
        rc, length, body, loc = out.split("|", 3) if "\n" not in out else (out.split("|", 1)[0], "", out, "")
        return int(rc), body, loc, out

    body = "hello from the release\n" * 40
    out = run([str(tmp / "tls"), port, "localhost", "/ok", str(tmp / "root.der"), str(today), "4096"]).stdout
    assert out == "%d|%d|%s|\n" % (len(body), len(body), body), out                  # verified, whole, in order
    assert get("/redirect")[0] == -9 and get("/redirect")[2] == "https://github.com/next"
    for path in ("/missing", "/chunked", "/gzip", "/nolength", "/http10", "/short"):
        rc, got, _, raw = get(path)
        assert rc == -5, (path, raw)                                                 # refused as a bad answer
    assert get("/ok", max_body=100)[0] == -5                                         # larger than allowed
    assert get("/ok", root="other.der")[0] == -4                                     # not signed by a root we hold
    assert get("/ok", host="github.com")[0] == -4                                    # certificate for another name
    assert get("/ok", now=today + 400 * 86400)[0] == -4                              # certificate expired by our clock
    assert get("/ok", now=today - 400 * 86400)[0] == -4                              # certificate not valid yet
    rc, got, _, raw = get("/ok", now=-1)
    assert rc == -2 and got == "", raw                                               # clock not set: nothing sent
    server.shutdown()
    print("PASS: verified HTTPS download; 12 refusals ( bad answers, size, wrong root, wrong name, clock )")


with tempfile.TemporaryDirectory(prefix="sms-update-") as td:
    part1(Path(td))
    part2(Path(td))
