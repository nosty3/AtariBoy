#include "video.hpp"
#include "engine.h"
#include <string.h>
namespace video {
bool convert(const uint8_t *full,uint8_t *dest,const Options& o){
 if(!o.height||o.height>max_rows)return false;
 if((o.method==Method::Full144||o.method==Method::Full192)&&o.height!=144)return false;
 if(o.method==Method::Crop && o.top+o.height>144)return false;
 for(unsigned y=0;y<o.height;++y){
  unsigned src=o.method==Method::Crop?o.top+y:(o.method==Method::Full144||o.method==Method::Full192)?y:y*144/o.height;
  memcpy(dest+y*stride,full+src*stride,stride);
 }
 if(o.method==Method::Full192){memcpy(dest+144*40,dest+140*40,40);memcpy(dest+145*40,dest+143*40,40);}
 return true;
}
}
