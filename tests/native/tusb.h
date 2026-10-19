#pragma once
#include "pico/stdlib.h"
#include <string.h>
#define SCSI_SENSE_NOT_READY 2
#define SCSI_SENSE_ILLEGAL_REQUEST 5
void tud_msc_set_sense(uint8_t,uint8_t,uint8_t,uint8_t);
