#include "options_ui.hpp"
#include "settings.hpp"
#include "controls.hpp"
#include <string.h>
#include <stdio.h>
static uint8_t code(unsigned c){return c<32?c+64:c<96?c-32:c<128?c:31;}
static void text(uint8_t *s,unsigned y,unsigned x,const char *t,bool inv=false){for(;*t&&x<40;++t,++x)s[y*40+x]=code(uint8_t(*t))|(inv?128:0);}
// One compact field list shared by navigation and rendering. No heap/cache.
static unsigned port_fields(unsigned type){return type==controls::Disabled?1:type==controls::Joy?3:5;}
unsigned OptionsUI::fields()const{
 const unsigned n[]={7,7,3,3,0,0,2};return tab==4?port_fields(settings.controls.port[0].type)+port_fields(settings.controls.port[1].type)+1:n[tab];
}
static uint8_t *control_field(unsigned row,unsigned &count){
 for(unsigned port=0;port<2;++port){auto &p=settings.controls.port[port];unsigned n=port_fields(p.type);
  if(row<=n){if(row==1){count=3;return &p.type;}count=6;
   if(row==n)return &p.up;
   return &p.fire[row-2];}row-=n;
 }
 count=6;return &settings.controls.space;
}
void OptionsUI::update(const uint8_t *p,bool fire){
 unsigned k=p[controls::HeldKey],j=p[controls::Joy0];
 bool u=!(j&1)||k==0x0e,d=!(j&2)||k==0x0f,l=!(j&4)||k==0x06,r=!(j&8)||k==0x07;
 int dir=u&&!d?-1:d&&!u?1:l&&!r?-2:r&&!l?2:0;
 bool repeat=dir!=held;if(repeat){held=dir;ticks=0;}else if(dir)repeat=++ticks>=20&&(ticks-20)%4==0;
 if(tab==6&&confirm){
  if(dir&&repeat&&(dir==2||dir==-2))yes=!yes;
  if(fire){if(yes){if(row==1)optimize=true;else erase=true;}else confirm=false;}
  return;
 }
 if(dir&&repeat){
  message[0]=0;unsigned max=fields();
  if(dir==-1&&row) --row;
  if(dir==1&&row<max)++row;
  if(dir==2||dir==-2){
   int delta=dir==2?1:-1;
   if(!row){tab=(tab+delta+7)%7;}
   else if(tab==1){if(row==1)settings.sound^=1;else if(row==2)settings.volume=(settings.volume+delta+16)%16;else if(row==7){settings.audio_quality^=1;settings.audio_effects=settings.audio_quality?6:4;}else settings.channels^=1u<<(row-3);settings_apply_audio();}
   else if(tab==2){if(row==1)settings.pal_full^=1;else if(row==2)settings.picture=(settings.picture+delta+4)%4;else settings.turbo^=1;}
   else if(tab==3){(row==1?settings.load_latest:row==2?settings.boot_last:settings.advance_slot)^=1;}
   else if(tab==4){unsigned count;uint8_t *v=control_field(row,count);*v=(*v+delta+count)%count;}
   else if(!tab&&row==7){settings.auto_minutes=(settings.auto_minutes+delta+11)%11;}
   else if(!tab&&row<6){uint8_t &v=row==1?settings.hue:settings.brightness[row-2];v=(v+delta+16)%16;}
  }
 }
 if(tab==6&&row&&fire){confirm=true;yes=false;message[0]=0;}
 if(!tab&&row==6&&fire){settings.hue=0;const Settings defaults;memcpy(settings.brightness,defaults.brightness,4);message[0]=0;}
}
void OptionsUI::draw(uint8_t *s)const{
 memset(s,0,960);
 text(s,1,1,"Graph Sound Emu State Ctrl Info Disk");
 const unsigned starts[]={0,6,12,16,22,27,32},widths[]={7,7,5,7,6,6,6};
 unsigned x=starts[tab],width=widths[tab];
 for(unsigned i=0;i<40;++i)s[80+i]=0x52;
 for(unsigned i=x+1;i<x+width-1;++i){s[i]=0x52;}
 s[x]=0x51;s[x+width-1]=0x45;s[40+x]=s[40+x+width-1]=0x7c;
 if(!row)for(unsigned i=x+1;i<x+width-1;++i)s[40+i]|=128;
 auto field=[&](unsigned id,unsigned y,const char *label,unsigned col,const char *value){
  text(s,y,1,label);text(s,y,col,"[");text(s,y,col+1,value,row==id);unsigned end=col+1+strlen(value);text(s,y,end,"]");
  if(row==id){s[y*40+col]=code(30);s[y*40+end]=code(31);}
 };
 char v[8];
 if(tab==6){
  text(s,4,0,"USB transfers slow? Try Optimize disk.");
  text(s,5,0,"It reclaims unused FLASH space.");
  text(s,6,0,"Your files and game are preserved.");
  field(1,8,"",1,"Optimize disk");
  text(s,10,0,"Erase disk deletes ALL files!");
  text(s,11,0,"Back up games, saves and settings.");
  field(2,13,"",1,"Erase disk");
  if(confirm){text(s,15,0,row==1?"Optimize disk now?":"Erase ALL files from disk?");text(s,16,1,yes?"[YES]":"[NO ]",true);}
  if(erase||optimize){text(s,18,0,optimize?"Optimizing disk...":"Erasing disk...",true);text(s,19,0,"This may take a few minutes.",true);text(s,20,0,"Do not turn off the power.",true);}
 }else if(!tab){
  snprintf(v,sizeof v,"%02u",settings.hue);field(1,4,"Color",7,v);
  for(unsigned i=0;i<4;++i){char label[20];snprintf(label,sizeof label,"Brightness %u",i);snprintf(v,sizeof v,"%02u",settings.brightness[i]);field(i+2,6+i,label,14,v);}
  // Every preview band uses PF0. DLI changes PF0 for each band; BK stays black.
  for(unsigned i=0;i<4;++i)memset(s+(10+i)*40,1,40);
  field(6,19,"Reset to default",18,"OK");
  snprintf(v,sizeof v,"%02u",settings.auto_minutes);field(7,21,"Auto color change (min)",26,v);
 }else if(tab==1){
  field(1,4,"Sound",7,settings.sound?"ON ":"OFF");snprintf(v,sizeof v,"%02u",settings.volume);field(2,6,"Volume",8,v);
  const char *names[]={"Channel 1 - Square","Channel 2 - Square","Channel 3 - Wave","Channel 4 - Noise"};
  for(unsigned i=0;i<4;++i)field(3+i,9+i*2,names[i],23,(settings.channels&(1u<<i))?"ON ":"OFF");
  field(7,18,"Audio mode",13,settings.audio_quality?"Quality":"Fast   ");
  text(s,20,1,detected_stereo==1?"Detected: stereo":detected_stereo==0?"Detected: mono":"Detected: waiting for Atari");
 }
 if(tab==2){text(s,4,1,"PAL speed:");field(1,6,"",1,settings.pal_full?"60 fps - Full Speed   ":"50 fps - Smooth frames");
  text(s,10,1,"GB picture mapping:");
  const char *names[]={"$0D MODE, SKIP EVERY 4TH LINE     ","$0D MODE, SHOW 3/4 OF GB IMAGE    ","$0E MODE, NARROW IMAGE            ","$0E MODE, DUPLICATE EVERY 3RD LINE"};
  field(2,12,"",1,names[settings.picture]);
  if(settings.picture==1)text(s,14,1,"MOVE VIEW: UP/DOWN KEYS");
  field(3,18,"CPU clock",13,settings.turbo?"TURBO   ":"STANDARD");
  text(s,20,1,"Applied when returning to game");
 }
 if(tab==3){
  text(s,4,1,"Load latest state on game start");field(1,6,"",1,settings.load_latest?"ON ":"OFF");
  text(s,9,1,"Start last game on power-up");field(2,11,"",1,settings.boot_last?"ON ":"OFF");text(s,12,1,"Global only - save with OPTION");
  text(s,14,1,"Advance slot after saving");field(3,16,"",1,settings.advance_slot?"ON ":"OFF");
 }
 if(tab==4){
  const char *modes[]={"None   ","Joy    ","Joy 2B+"};
  const char *functions[]={"None       ","A          ","B          ","Start      ","Select     ","Load state "};
  const char *names[]={"Fire","Fire 2","Fire 3","Up"};
  unsigned id=1;
  for(unsigned port=0;port<2;++port){const auto &p=settings.controls.port[port];unsigned y=port?12:4;
   field(id++,y,port?"Controls slot 2":"Controls slot 1",18,modes[p.type]);
   if(p.type!=controls::Disabled){
    unsigned n=p.type==controls::Joy?1:3;
    for(unsigned i=0;i<n;++i)field(id++,y+2+i,names[i],18,functions[p.fire[i]]);
    field(id++,y+2+n,"Up",18,functions[p.up]);
   }
  }
  field(id,21,"Space",18,functions[settings.controls.space]);
 }
 if(tab==5){
  const char *lines[]={"Options can be saved globally", "or separately for each game.", "",
   "G = Load global options", "OPTION = Save global options", "L = Load options for this game",
   "S = Save options for this game", "", "Esc = Return without saving", "",
   "Changes made using shortcuts during", "gameplay are not saved automatically.", "",
   "Power-up autostart is global only.", "Use OPTION to save that setting."};
  for(unsigned i=0;i<sizeof(lines)/sizeof(*lines);++i)text(s,4+i,1,lines[i]);
 }
 if(message[0])text(s,22,0,message);
 memset(s+23*40,128,40);
 text(s,23,0,"\x1c\x1d=Select \x1e\x1f=Change",true);
 text(s,23,40-(sizeof("See Info page")-1),"See Info page",true);
}

void OptionsUI::help(uint8_t *s){
 memset(s,0,960);
 text(s,1,1,"Keys available during gameplay:");
 const char *keys[]={"Esc","HELP","OPTION","START","SELECT","P","R","M","N","T","C/V","G","S","L","Z/X","Up/Down","Left/Right","F","Ctrl+D"};
 const char *descriptions[]={"open game browser","help screen","options screen","GB START button","GB SELECT button","pause on/off","reset game","sound on/off","audio Fast/Quality","turbo 250/300 MHz","color change","manage save states","save state","load state","select save slot","move view","change view","FPS on/off","debug info on/off"};
 for(unsigned i=0;i<sizeof(keys)/sizeof(*keys);++i){if(!keys[i][0])continue;text(s,3+i,1,keys[i],true);text(s,3+i,12,"-");text(s,3+i,14,descriptions[i]);}
 memset(s+23*40,128,40);text(s,23,0,"Esc/Help=Return to game",true);
}
