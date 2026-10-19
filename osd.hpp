#pragma once
#include <stdint.h>
void osd_show(const char *text,uint32_t now_us);
void osd_overlay(uint8_t *image,unsigned rows,uint32_t now_us,bool full192=false);
void osd_restart(uint32_t now_us);
void osd_text(uint8_t *image,unsigned rows,unsigned top,const char *text);
