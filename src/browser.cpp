// Directory ordering adapted from A8PicoCart (Robin Edwards, 2023).
#include "browser.hpp"
#include "storage.h"
#include "saves.h"
#include "settings.hpp"
#include "storage/fatfs/ff.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
static bool visible(const FILINFO& f){
 if((f.fattrib&(AM_HID|AM_SYS)) || f.fname[0]=='.')return false;
 if(f.fattrib&AM_DIR)return true;
 const char *ext=strrchr(f.fname,'.');return ext&&!strcasecmp(ext,".gb");
}
static int compare(const Browser::Entry& a,const Browser::Entry& b){
 if(a.dir!=b.dir)return a.dir?-1:1;
 int n=strcasecmp(a.name,b.name);return n?n:strcmp(a.name,b.name);
}
void Browser::init(){ready=storage_mount();if(!ready)snprintf(message,sizeof message,"%s",storage_error());else scan();}
void Browser::scan(){
 count=0;selected=top=0;message[0]=0;DIR d;FILINFO f;
 if(f_opendir(&d,path)!=FR_OK){strcpy(message,"Cannot open directory");page_count=0;return;}
 FRESULT res;while((res=f_readdir(&d,&f))==FR_OK&&f.fname[0])if(visible(f))++count;
 f_closedir(&d);if(res!=FR_OK)strcpy(message,"Directory read error");refresh();
}
void Browser::refresh(){
 // Bounded SRAM, no 255-item ceiling: select the next smallest name per rank.
 // This trades directory scan time for RAM and is only run on menu changes.
 page_count=0;if(!ready)return;
 Entry previous={},best={};bool have_previous=false;
 for(unsigned rank=0;rank<count&&rank<top+15;++rank){
  DIR d;FILINFO f;bool found=false;if(f_opendir(&d,path)!=FR_OK)break;
  while(f_readdir(&d,&f)==FR_OK&&f.fname[0]){
   if(!visible(f))continue;
   Entry e={};strcpy(e.name,f.fname);e.dir=(f.fattrib&AM_DIR)!=0;
   if(have_previous&&compare(e,previous)<=0)continue;
   if(!found||compare(e,best)<0){best=e;found=true;}
  }
  f_closedir(&d);if(!found)break;previous=best;have_previous=true;
  if(rank>=top)page[page_count++]=best;
 }
 refresh_saves();ticks=0;
}
void Browser::refresh_saves(){
 for(unsigned i=0;i<page_count;++i){page[i].has_save=!page[i].dir&&saves_has_game(path,page[i].name);page[i].has_options=!page[i].dir&&settings_has_game(page[i].name);}
}
void Browser::note_save(const char *base){
 for(unsigned i=0;i<page_count;++i){auto &e=page[i];const char *dot=strrchr(e.name,'.');
  unsigned n=dot?unsigned(dot-e.name):strlen(e.name);
  if(!e.dir&&strlen(base)==n&&!strncasecmp(base,e.name,n))e.has_save=true;
 }
}
bool Browser::neighbour(const Entry& anchor,int direction,Entry& best){
 DIR d;FILINFO f;bool found=false;if(f_opendir(&d,path)!=FR_OK)return false;
 while(f_readdir(&d,&f)==FR_OK&&f.fname[0]){
  if(!visible(f))continue;
  Entry e={};strcpy(e.name,f.fname);e.dir=(f.fattrib&AM_DIR)!=0;
  int relation=compare(e,anchor);if(direction>0?relation<=0:relation>=0)continue;
  if(!found||(direction>0?compare(e,best)<0:compare(e,best)>0)){best=e;found=true;}
 }
 f_closedir(&d);return found;
}
void Browser::move(int delta){
 if(!count)return;
 unsigned old=top;
 if(delta<0){if(selected)--selected;}else if(selected+1<count)++selected;
 if(selected<top)top=selected;
 if(selected>=top+15)top=selected-14;
 ticks=0;message[0]=0;if(top==old)return;
 Entry next={};bool down=top>old;
 if(page_count==15&&neighbour(page[down?14:0],down?1:-1,next)){
  next.has_save=!next.dir&&saves_has_game(path,next.name);next.has_options=!next.dir&&settings_has_game(next.name);
  if(down){memmove(page,page+1,14*sizeof(Entry));page[14]=next;}
  else {memmove(page+1,page,14*sizeof(Entry));page[0]=next;}
 }else refresh();
}
void Browser::back(){
 if(!strcmp(path,"/"))return;
 char *p=strrchr(path,'/');if(p==path)path[1]=0;else if(p)*p=0;scan();
}
bool Browser::choose(char *file,unsigned capacity){
 if(!ready||selected<top||selected-top>=page_count)return false;
 const Entry& e=page[selected-top];
 int n=snprintf(file,capacity,"%s%s%s",path,strcmp(path,"/")?"/":"",e.name);
 if(n<0||(unsigned)n>=capacity){strcpy(message,"Path too long");return false;}
 if(e.dir){if((unsigned)n>=sizeof path){strcpy(message,"Path too long");return false;}strcpy(path,file);scan();return false;}
 return true;
}
static uint8_t code(unsigned c){if(c<32)return c+64;if(c<96)return c-32;return c<128?c:31;}
static void text(uint8_t *s,unsigned y,unsigned x,const char *t,unsigned max,bool inverse=false){
 for(unsigned i=0;i<max&&t[i]&&x+i<40;++i)s[y*40+x+i]=code((unsigned char)t[i])|(inverse?128:0);
}
void Browser::draw(uint8_t *s,bool resume){
 memset(s,0,960);++ticks;
 text(s,0,13," SELECT GAME ",13,true);
 text(s,2,0,"D - directory",40);
 text(s,3,0,"X - game has saves",40);s[3*40]=0x60;
 text(s,4,0,"X - game has its own options file",40);s[4*40]=0x50;
 memset(s+5*40,0x52,40); // screen code $52 = horizontal line (ATASCII $12)
 for(unsigned i=0;i<page_count;++i){
  unsigned row=7+i;const Entry& e=page[i];bool chosen=top+i==selected;
  if(e.dir)s[row*40]=code('D');
  else if(e.has_save)s[row*40]=0x60;
  if(!e.dir&&e.has_options)s[row*40+1]=0x50;
  unsigned offset=0,len=strlen(e.name);
  if(chosen&&len>37&&ticks>50){unsigned span=len-37;unsigned phase=(ticks-50)/5%(span+21);offset=phase>span?span:phase;}
  text(s,row,3,e.name+offset,37);
  if(chosen)for(unsigned x=3;x<40;++x)s[row*40+x]|=128;
 }
 if(!count&&ready)text(s,7,3,"No .gb files in this directory",37);
 if(message[0])text(s,22,0,message,40);
 for(unsigned x=0;x<40;++x)s[23*40+x]=128;
 const char *select="Up/Dn/Ret=Sel";
 text(s,23,0,select,40,true);unsigned x=strlen(select);
 if(strcmp(path,"/")){x+=2;s[23*40+x++]=code(30)|128;text(s,23,x,"=Back",40,true);x+=5;}
 if(resume)text(s,23,x+2,"Esc=Resume",40,true);
}

void Browser::note_options(const char *base){
 for(unsigned i=0;i<page_count;++i){auto &e=page[i];const char *dot=strrchr(e.name,'.');
  unsigned n=dot?unsigned(dot-e.name):strlen(e.name);
  if(!e.dir&&strlen(base)==n&&!strncasecmp(base,e.name,n))e.has_options=true;
 }
}
