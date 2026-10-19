#!/usr/bin/env python3
from pathlib import Path
from PIL import Image
r=Path(__file__).resolve().parent
im=Image.open(r/'assets/title-logo.png').convert('RGB')
assert im.size==(320,64)
assert set(im.getdata())=={(0,0,0),(255,255,255)},'Logo must contain exactly black and white'
b=bytes(sum((im.getpixel((x+i,y))==(0,0,0))<<(7-i) for i in range(8)) for y in range(64) for x in range(0,320,8))
assert len(b)==2560
(r/'generated/title-logo.bin').write_bytes(b)
(r/'generated/title_logo.hpp').write_text('#pragma once\n#include <stdint.h>\nstatic constexpr uint8_t title_logo[]={\n'+','.join(str(v) for v in b)+'};\n')
print('Title logo: 320x64, 2560 bytes, lossless 1bpp')
