#pragma once
#include <stdint.h>
namespace controls {
constexpr unsigned packet_size=24;
enum Offset {Joy0,Fire0,Pot0,Pot1,Joy1,Fire1,Pot2,Pot3,LastKey,KeySequence,HeldKey,Help,Start,Select,Option,PortA,Skstat,Kbcode,Pot4,Pot5,Pot6,Pot7,Reserved,Frame};
enum Type : uint8_t { Disabled, Joy, Joy2B };
enum Function : uint8_t { None, A, B, GBStart, GBSelect, LoadState };
struct Port {uint8_t type=Joy;uint8_t fire[3]={A,B,GBStart};uint8_t up=None;};
struct Mapping {Port port[2]={{Joy,{A,B,GBStart},None},{Disabled,{A,B,GBStart},None}};uint8_t space=B;};
struct Input {uint8_t pad=255;bool view_up=false,view_down=false,change_view=false,load_state=false;int8_t view_step=0;};
struct Decoder {
 bool detected[4]={};bool extra[4]={};uint16_t previous_change=0,previous_load=0;
 void update(const uint8_t *p);
 Input read(const uint8_t *p,const Mapping& map);
};
}
