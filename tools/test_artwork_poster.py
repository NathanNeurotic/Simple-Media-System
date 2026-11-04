"""Exercise the exact tiled poster resampler with bounds/aspect controls."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "SMS_Artwork.h"
static unsigned offset(unsigned x,unsigned y) {
 return ((y/64)*2+x/64)*SMS_ARTWORK_BYTES+((y%64)*64+x%64)*4;
}
int main(void) {
 unsigned char *out=malloc(SMS_POSTER_BYTES+32),*rgb=malloc(512*512*3);
 assert(out&&rgb);memset(out,0xa5,SMS_POSTER_BYTES+32);
 for(unsigned y=0;y<192;++y)for(unsigned x=0;x<128;++x) {
  rgb[(y*128+x)*3]=x;rgb[(y*128+x)*3+1]=y;rgb[(y*128+x)*3+2]=37;
 }
 assert(SMS_ArtworkPoster(out,rgb,128,192));
 for(unsigned y=0;y<192;++y)for(unsigned x=0;x<128;++x) {
  unsigned p=offset(x,y);assert(out[p]==x&&out[p+1]==y&&out[p+2]==37&&out[p+3]==0x80);
 }
 for(unsigned i=SMS_POSTER_BYTES;i<SMS_POSTER_BYTES+32;++i)assert(out[i]==0xa5);
 memset(rgb,73,512*512*3);
 assert(SMS_ArtworkPoster(out,rgb,512,256));
 assert(out[offset(0,64)]==73&&out[offset(127,127)]==73);
 assert(out[offset(0,63)]==0&&out[offset(0,128)]==0);
 assert(SMS_ArtworkPoster(out,rgb,256,512));
 assert(out[offset(16,0)]==73&&out[offset(111,191)]==73);
 assert(out[offset(15,0)]==0&&out[offset(112,0)]==0);
 assert(SMS_ArtworkPoster(out,rgb,1,512));
 assert(SMS_ArtworkPoster(out,rgb,512,1));
 assert(!SMS_ArtworkPoster(NULL,rgb,1,1)&&!SMS_ArtworkPoster(out,NULL,1,1));
 assert(!SMS_ArtworkPoster(out,rgb,0,1)&&!SMS_ArtworkPoster(out,rgb,1,0));
 assert(!SMS_ArtworkPoster(out,rgb,513,1)&&!SMS_ArtworkPoster(out,rgb,1,513));
 free(out);free(rgb);puts("Poster resampler: six tiles, edge continuity, aspect padding, bounds and tiny sources passed");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temporary:
    tmp = Path(temporary)
    fixture = tmp / 'poster.c'
    fixture.write_text(source)
    exe = tmp / 'poster'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-g', '-no-pie',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-I', str(root / 'include'), str(fixture), str(root / 'src/SMS_Artwork.c'),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
