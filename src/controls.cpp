#include "controls.hpp"
namespace controls {
void Decoder::update(const uint8_t *p){
 const unsigned offsets[]={Pot0,Pot1,Pot2,Pot3};
 for(unsigned i=0;i<4;++i){unsigned v=p[offsets[i]];
 if(v<228)detected[i]=true;
 extra[i]=detected[i]&&v==228;}
}
Input Decoder::read(const uint8_t *p,const Mapping& m){
 Input out;uint16_t loads=0;unsigned key=p[HeldKey];
 const uint8_t bits[]={0,1,2,8,4};
 auto apply=[&](unsigned fn,unsigned source){
  if(fn>=A&&fn<=GBSelect)out.pad&=~bits[fn];
  if(fn==LoadState)loads|=1u<<source;
 };
 for(unsigned port=0;port<2;++port){
  const Port &cfg=m.port[port];unsigned base=port*4,j=p[base];
  const bool pressed[]={p[base+1]==0,cfg.type==Joy2B&&extra[port*2],cfg.type==Joy2B&&extra[port*2+1],!(j&1)};
  const unsigned fn[]={cfg.fire[0],cfg.fire[1],cfg.fire[2],cfg.up};
  // Port 1 directions control GB; Up bindings remain additive.
  // Port 2 directions no longer change or move the view.
  if(port==0&&cfg.type!=Disabled){
   if(!(j&8))out.pad&=~16;
   if(!(j&4))out.pad&=~32;
   if(!(j&1))out.pad&=~64;
   if(!(j&2))out.pad&=~128;
  }
  if(cfg.type!=Disabled)for(unsigned i=0;i<4;++i)if(pressed[i])apply(fn[i],port*4+i);
 }
 if(key==0x21)apply(m.space,8);
 out.load_state=(loads&~previous_load)!=0;previous_load=loads;
 out.view_up=key==0x0e;out.view_down=key==0x0f;
 uint16_t changes=key==0x06?1:key==0x07?2:0;
 uint16_t edges=changes&~previous_change;previous_change=changes;
 out.view_step=int(bool(edges&2))-int(bool(edges&1));out.change_view=out.view_step!=0;
 if(p[Select]==1)out.pad&=~4;
  if(p[Start]==1)out.pad&=~8;
 return out;
}
}
