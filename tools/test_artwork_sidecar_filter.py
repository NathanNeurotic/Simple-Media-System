"""Execute the production sidecar filter under ASan/UBSan; no PS2 claim."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
browser = (root / 'src/SMS_GUIFileMenu.c').read_text()
icons = '\n'.join(line for line in (root / 'include/SMS_GUIcons.h').read_text().splitlines()
                  if re.match(r'#define GUICON_(?:FOLDER|AVI|MP3|M3U|PARTITION|SHARE|AVIS|PICTURE)\s+\d', line))


def function(name):
    match = re.search(r'^static int ' + name + r'\s*\([^;{}]*\)\s*\{', browser, re.M)
    assert match, name
    end, depth = match.end(), 1
    while depth:
        depth += (browser[end] == '{') - (browser[end] == '}')
        end += 1
    return browser[match.start():end]


prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include "SMS_List.h"
#include "SMS_Artwork.h"
#define stricmp strcasecmp
#define SMS_BF_HDLP 1
#define SMS_BF_SYSP 2
#define SMS_BF_AVIF 4
static struct { unsigned m_BrowserFlags; } g_Config;
static char s_pPPHDL[]="PP.HDL.",s_pHDLdr[]="HDLoade";
static SMS_List listing, *g_pFileList=&listing;
'''
post = r'''
static SMS_ListNode *add(const char *name,int type) {
 SMS_ListNode *p=calloc(1,sizeof(*p)+strlen(name)+1);assert(p);
 strcpy(_STR(p),name);p->m_Param=type;p->m_pPrev=listing.m_pTail;
 if(listing.m_pTail)listing.m_pTail->m_pNext=p;else listing.m_pHead=p;
 listing.m_pTail=p;++listing.m_Size;return p;
}
int main(void) {
 SMS_ListNode *movie=add("Movie.avi",GUICON_AVI);
 SMS_ListNode *jpg=add("MOVIE.JPG",GUICON_PICTURE);
 SMS_ListNode *jpeg=add("movie.JpEg",GUICON_PICTURE);
 SMS_ListNode *photo=add("Holiday.jpg",GUICON_PICTURE);
 SMS_ListNode *near=add("Movie extra.jpg",GUICON_PICTURE);
 SMS_ListNode *folder=add("Folder.avi",GUICON_FOLDER);
 SMS_ListNode *folder_pic=add("Folder.jpg",GUICON_PICTURE);
 SMS_ListNode *song=add("Song.mp3",GUICON_MP3);
 SMS_ListNode *song_pic=add("Song.jpg",GUICON_PICTURE);
 SMS_ListNode *playlist=add("List.m3u",GUICON_M3U);
 SMS_ListNode *playlist_pic=add("List.jpg",GUICON_PICTURE);
 SMS_ListNode *png=add("Movie.png",GUICON_PICTURE);
 SMS_ListNode *subtitle_movie=add("Subtitled.mp4",GUICON_AVIS);
 SMS_ListNode *subtitle_pic=add("subtitled.jpeg",GUICON_PICTURE);
 SMS_ListNode *cyrillic_movie=add("\320\250\321\200\320\265\320\272 (2001).avi",GUICON_AVI);
 SMS_ListNode *cyrillic_pic=add("\320\250\321\200\320\265\320\272 (2001).jpg",GUICON_PICTURE);
 SMS_ListNode *part=add("Film - Part 2.mpg",GUICON_AVI);
 SMS_ListNode *part_pic=add("Film - Part 2.jpg",GUICON_PICTURE);
 SMS_ListNode *other_part=add("Film - Part 1.jpg",GUICON_PICTURE);
 char long_video[340],long_pic[340];memset(long_video,'x',330);strcpy(long_video+330,".avi");
 memset(long_pic,'x',330);strcpy(long_pic+330,".jpg");
 SMS_ListNode *long_movie=add(long_video,GUICON_AVI),*long_art=add(long_pic,GUICON_PICTURE);
 unsigned count=listing.m_Size;
 for(unsigned flags=0;flags<=SMS_BF_AVIF;flags+=SMS_BF_AVIF) {
  g_Config.m_BrowserFlags=flags;
  assert(_filter_item(jpg) && _filter_item(jpeg));
  assert(_filter_item(subtitle_pic) && _filter_item(cyrillic_pic) && _filter_item(part_pic));
  assert(!_filter_item(movie) && !_filter_item(subtitle_movie) && !_filter_item(cyrillic_movie));
  assert(!_filter_item(photo) && !_filter_item(near) && !_filter_item(folder));
  assert(!_filter_item(folder_pic) && !_filter_item(song_pic) && !_filter_item(playlist_pic));
  assert(!_filter_item(song) && !_filter_item(playlist) && !_filter_item(png));
  assert(!_filter_item(other_part) && !_filter_item(long_movie) && !_filter_item(long_art));
  assert(listing.m_Size==count && movie->m_pNext==jpg && jpg->m_pNext==jpeg);
 }
 /* The unfiltered names needed by _artwork_load still exist after filtering. */
 assert(!strcmp(_STR(jpg),"MOVIE.JPG") && !strcmp(_STR(jpeg),"movie.JpEg"));
 movie->m_Param=GUICON_FOLDER;assert(!_filter_item(jpg));movie->m_Param=GUICON_AVI;
 g_pFileList=NULL;assert(!_filter_item(jpg));g_pFileList=&listing;
 while(listing.m_pHead) { SMS_ListNode *next=listing.m_pHead->m_pNext;free(listing.m_pHead);listing.m_pHead=next; }
 puts("Sidecar filter: video-only, JPEG case, Cyrillic, parts, photos, audio, bounds and retained listing passed");
 return 0;
}
'''

with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    (tmp / 'tamtypes.h').write_text('#include <stdint.h>\ntypedef uint64_t u64;typedef int64_t s64;\n')
    source = tmp / 'sidecar_filter.c'
    source.write_text(prefix + icons + '\n' + function('_is_video_artwork') + function('_filter_item') + post)
    exe = tmp / 'sidecar_filter'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-g', '-no-pie',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-I', str(tmp), '-I', str(root / 'include'), str(source),
                    str(root / 'src/SMS_Artwork.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
