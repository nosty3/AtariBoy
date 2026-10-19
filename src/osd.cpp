#include "osd.hpp"
#include "generated/osd_font.hpp"
#include <string.h>
static char message[21];
static uint32_t started;
static bool shown;
void osd_show(const char *text,uint32_t now){
 strncpy(message,text,20);message[20]=0;started=now;shown=true;
}
void osd_overlay(uint8_t *image,unsigned rows,uint32_t now,bool full192){
 if(!shown||uint32_t(now-started)>=2000000u||rows<8)return;
 if(!full192){osd_text(image,rows,rows-8,message);return;}
 // Eight physical output lines, including independent copies of repeated rows.
 const unsigned map[8]={138,139,140,144,141,142,143,145};
 for(unsigned c=0;c<20&&message[c];++c){unsigned ch=(uint8_t)message[c];if(ch<32||ch>127)ch='?';unsigned glyph=ch<96?ch-32:ch;
  for(unsigned y=0;y<8;++y)for(unsigned b=0;b<2;++b){unsigned pixels=osd::font[glyph][y*2+b];unsigned opaque=((pixels>>1)|~pixels)&0x55u;unsigned mask=opaque|(opaque<<1);uint8_t &dst=image[map[y]*40+c*2+b];dst=(dst&~mask)|(pixels&mask);}
 }
}
void osd_text(uint8_t *image,unsigned rows,unsigned top,const char *text){
 if(rows<8||top>rows-8)return;
 for(unsigned c=0;c<20&&text[c];++c){
  unsigned ch=uint8_t(text[c]);
  if(ch<32||ch>127)ch='?';
  // ATASCII -> screen code (font index). Keep lowercase 96..127 intact.
  unsigned glyph=ch<96?ch-32:ch;
  const uint8_t *src=osd::font[glyph];
  uint8_t *dst=image+top*40+c*2;
  for(unsigned y=0;y<8;++y){
   for(unsigned b=0;b<2;++b){
    unsigned pixels=src[y*2+b];
    // Only 01 means transparent. Expand opaque pair bits into a byte mask.
    unsigned opaque=((pixels>>1)|~pixels)&0x55u;
    unsigned mask=opaque|(opaque<<1);
    dst[b]=uint8_t((dst[b]&~mask)|(pixels&mask));
   }
   dst+=40;
  }
 }
}
void osd_restart(uint32_t now){started=now;}
