#include "state_ui.hpp"
#include "controls.hpp"
#include <string.h>
#include <stdio.h>
static void text(uint8_t *s,unsigned y,unsigned x,const char *t){for(;*t&&x<40;++x,++t){unsigned c=(uint8_t)*t;if(c<28||c>127)c=' ';s[y*40+x]=c<32?c+64:c<96?c-32:c;}}
static const char unshift[64]={
 'l','j',';',0,0,'k','+', '*','o',0,'p','u',0,'i','-','=',
 'v',0,'c',0,0,'b','x','z','4',0,'3','6',0,'5','2','1',
 ',', ' ','.','n',0,'m','/',0,'r',0,'e','y',0,'t','w','q',
 '9',0,'0','7',0,'8','<','>','f','h','d',0,0,'g','s','a'};
static unsigned character(unsigned raw){unsigned k=raw&63;if(raw&128)return 0;unsigned c=unshift[k];if(!(raw&64))return c;
 if(c>='a'&&c<='z')return c-32;
 switch(c){case '1':return '!';case '2':return '"';case '3':return '#';case '4':return '$';case '5':return '%';case '6':return '&';case '7':return '\'';case '8':return '@';case '9':return '(';case '0':return ')';case ';':return ':';case ',':return '[';case '.':return ']';case '/':return '?';case '=':return '|';case '-':return '_';case '+':return '\\';case '*':return '^';default:return c;}}
void StateUI::refresh(){SaveResult r=saves_list(slots);if(r!=SAVE_OK)strcpy(message,"READ FAILED!");saves_preview(row+1,preview);}
void StateUI::open(){clipboard=false;editing=false;operation=0;message[0]=0;previous=255;direction=0;repeat=0;refresh();bool found=false;row=0;for(unsigned i=0;i<10;++i)if(slots[i].present&&(!found||slots[i].number>slots[row].number)){row=i;found=true;}saves_preview(row+1,preview);}
void StateUI::move(int d){row=(row+10+d)%10;message[0]=0;saves_preview(row+1,preview);}
bool StateUI::update(const uint8_t *p){
 unsigned k=p[controls::HeldKey],raw=p[controls::Kbcode];bool edge=k!=previous;previous=k;
 if(operation)return false;
 if(editing){
  // KBCODE retains the last key after release; HeldKey=255 means no key.
  if(k==255){repeat=0;return false;}
  bool act=edge;if(k==0x34||k==0x06||k==0x07){if(edge)repeat=0;else if(++repeat>=20&&(repeat-20)%3==0)act=true;}
  if(act){if(k==0x1c){editing=false;return false;}if(k==0x0c){operation=5;strcpy(message,"SAVING DESCRIPTION...");return false;}
   if(k==0x06){if(cursor) --cursor;}else if(k==0x07){if(cursor<29)++cursor;}
   else if(k==0x34){if(cursor){--cursor;memmove(edit+cursor,edit+cursor+1,29-cursor);edit[29]=' ';}}
   else {unsigned c=character(raw);if(c&&cursor<30){edit[cursor++]=c;}}
  }return false;
 }
 if(edge&&(k==0x1c||k==0x3d))return true;
 int d=(!(p[controls::Joy0]&1)||k==0x0e)?-1:(!(p[controls::Joy0]&2)||k==0x0f)?1:0;
 if(d!=direction){direction=d;repeat=0;if(d)move(d);}else if(d&&++repeat>=20&&(repeat-20)%4==0)move(d);
 if(!edge)return false;
 if(k==0x23||k==0x08){bool found=false;unsigned best=0;for(unsigned i=0;i<10;++i)if(slots[i].present&&(!found||(k==0x23?slots[i].number>slots[best].number:slots[i].number<slots[best].number))){best=i;found=true;}if(found){row=best;saves_preview(row+1,preview);}}
 else if(k==0x3e){operation=1;strcpy(message,"SAVING...");}
 else if(k==0x00&&slots[row].present){operation=2;strcpy(message,"LOADING...");}
 else if(k==0x3a&&slots[row].present){operation=3;strcpy(message,"DELETING...");}
 else if(k==0x12&&slots[row].present){operation=4;strcpy(message,"COPYING...");}
 else if(k==0x0a&&clipboard){operation=6;strcpy(message,"PASTING...");}
 else if(k==0x2a&&slots[row].present){editing=true;memcpy(edit,slots[row].description,30);edit[30]=0;cursor=0;repeat=0;message[0]=0;}
 return false;
}
SaveResult StateUI::perform(){unsigned op=operation;operation=0;unsigned old=saves_slot();saves_set_slot(row+1);
 SaveResult r=op==1?saves_write():op==2?saves_load():op==3?saves_delete(row+1):op==4?saves_copy(row+1):op==5?saves_describe(row+1,edit):saves_paste(row+1);
 if(op!=1&&op!=2)saves_set_slot(old);
 if(r==SAVE_OK){if(op==4)clipboard=true;if(op==5)editing=false;strcpy(message,op==4?"COPIED - SELECT SLOT AND PRESS P":"DONE");}
 else strcpy(message,r==SAVE_FULL?"NO SPACE ON DISC!":op==2?"LOAD FAILED!":"OPERATION FAILED!");
 refresh();return r;
}
void StateUI::draw(uint8_t *s){
 memset(s,0,87*40);text(s,0,0,saves_game_name());for(unsigned x=0;x<40;++x)s[x]|=128;text(s,2,0,"Slot No.  Description");
 int newest=-1,oldest=-1;for(unsigned i=0;i<10;++i)if(slots[i].present){if(newest<0||slots[i].number>slots[newest].number)newest=i;if(oldest<0||slots[i].number<slots[oldest].number)oldest=i;}
 for(unsigned i=0;i<10;++i){char t[64];if(slots[i].present)snprintf(t,sizeof t,"%c %02u %04llu %.30s",int(i)==newest?'N':int(i)==oldest?'O':' ',i+1,(unsigned long long)slots[i].number,editing&&i==row?edit:slots[i].description);else snprintf(t,sizeof t,"  %02u ---- ",i+1);text(s,3+i,0,t);if(i==row)for(unsigned x=2;x<40;++x)s[(3+i)*40+x]^=128;}
 if(editing)s[(3+row)*40+10+(cursor<30?cursor:29)]^=128;
 if(slots[row].present)for(unsigned y=0;y<64;++y)for(unsigned x=0;x<53;++x){unsigned px=(preview[y*14+x/4]>>(6-2*(x%4)))&3;unsigned dx=53+x;uint8_t &b=s[(13+4+y)*40+dx/4];unsigned sh=6-2*(dx%4);b=(b&~(3<<sh))|(px<<sh);}
 if(editing){text(s,85,0,"Type description (max 30 characters)");text(s,86,0,"Return=Confirm  Esc=Cancel");}
 else{char line[41]="N=Newest O=Oldest";if(slots[row].present)strcat(line," C=Copy E=Edit D=Delete");text(s,85,0,line);
  strcpy(line,"\x1c\x1d=Select");if(slots[row].present)strcat(line," L=Load");strcat(line," S=Save");if(clipboard)strcat(line," P=Paste");strcat(line," Esc=Exit");text(s,86,0,line);
 }
 if(message[0])text(s,1,0,message);
 for(unsigned i=85*40;i<87*40;++i)s[i]^=128;
}
