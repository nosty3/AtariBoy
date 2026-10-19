#!/usr/bin/env python3
"""Rasterize Early GameBoy at its native 8px size, retaining Atari UI glyphs."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
r=Path(__file__).resolve().parents[1]
font=ImageFont.truetype(str(r/'assets/early-gameboy.ttf'),8)
data=bytearray((r/'assets/atari-font.fnt').read_bytes())
assert len(data)==1024
chars='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789,.;:\"?!-'
for c in chars:
 im=Image.new('1',(8,8));ImageDraw.Draw(im).text((1,7),c,font=font,fill=1,anchor='ls',stroke_width=0)
 code=ord(c)-32 if 32<=ord(c)<96 else ord(c)
 for y in range(8):data[code*8+y]=sum(int(bool(im.getpixel((x,y))))<<(7-x) for x in range(8))
(r/'assets/menu-font.fnt').write_bytes(data)
