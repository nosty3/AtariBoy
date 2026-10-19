#!/usr/bin/env python3
import pathlib,subprocess,sys,shutil,binascii
r=pathlib.Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(r/'tools/build_font.py')],check=True)
g=r/'generated';g.mkdir(exist_ok=True)
subprocess.run([sys.executable,str(r/'tools/build_menu_font.py')],check=True)
subprocess.run([sys.executable,str(r/'tools/build_title.py')],check=True)
subprocess.run([sys.executable,str(r/'tools/build_config.py')],check=True)
m=shutil.which(sys.argv[1]) or str(pathlib.Path(sys.argv[1]).resolve())
def asm(n,ext='bin'):
 subprocess.run([m,str(r/'atari'/f'{n}.asm'),f'-o:{g/n}.{ext}',f'-l:{g/n}.lst',f'-t:{g/n}.lab'],check=True)
 return (g/f'{n}.{ext}').read_bytes()
l=asm('loader')
(g/'loader_crc.asm').write_text(f'LOADER_CRC = ${binascii.crc_hqx(l,0xffff):04x}\n')
b=asm('boot')
from pack_atari import pack,unpack
images={}
for standard,hz,lines in [('ntsc',60,262),('pal',50,312)]:
 (g/'timing_config.asm').write_text(f'TV_HZ = {hz}\nLAST_VCOUNT = {lines//2-1}\n')
 subprocess.run([sys.executable,str(r/'tools/build_audio_atari.py')]+(['--ntsc'] if standard=='ntsc' else []),check=True)
 x=asm('program','xex');packed=pack(x);assert unpack(packed)==x
 images[standard]=(x,packed)
 for ext in ['xex','lst','lab']:
  shutil.copyfile(g/f'program.{ext}',g/f'program_{standard}.{ext}')
 print(standard,'XEX',len(x),'packed',len(packed))

labels={p[2]:int(p[1],16) for line in (g/'boot.lab').read_text().splitlines() if len(p:=line.split())==3 and p[0]=='00'}
from pack_atari import pack,unpack
packed=pack(x);assert unpack(packed)==x
print('Packed Atari XEX',len(x),'->',len(packed))
v=bytes([0,0xb0,0,4,labels['INIT']&255,labels['INIT']>>8])
assert len(l)==256 and x[:2]==b'\xff\xff' and len(b)<=0x200
with (g/'atari_images.hpp').open('w') as f:
 f.write('#pragma once\n#include <cstdint>\nnamespace atari {\n')
 for n,d in [('boot',b),('loader',l),('vectors',v)]:
  f.write(f'inline constexpr uint8_t {n}[] = {{\n')
  for i in range(0,len(d),16):f.write(','.join(f'0x{c:02x}' for c in d[i:i+16])+',\n')
  f.write('};\n')
 for name,(data,packed) in images.items():
  f.write(f'inline const uint8_t program_{name}[] __attribute__((section(".atari_xip"))) = {{\n')
  for i in range(0,len(packed),16):f.write(','.join(f'0x{c:02x}' for c in packed[i:i+16])+',\n')
  f.write('};\n'+f'inline constexpr unsigned program_{name}_size = {len(data)};\n')
 f.write(f'inline uint8_t program[{max(len(p) for x,p in images.values())}];\ninline unsigned program_size = program_pal_size;\n}}\n')
print('Boot',len(b),'loader',len(l),'XEX',len(x))
