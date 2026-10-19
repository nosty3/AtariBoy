#!/usr/bin/env python3
"""Single build-time geometry setting shared by MADS and Pico C++."""
import json
from pathlib import Path
r=Path(__file__).resolve().parents[1]
h=json.loads((r/'config/config.json').read_text())['display_rows']
assert isinstance(h,int) and 64<=h<=120 and h%4==0
(r/'generated/display_config.hpp').write_text(f'#pragma once\n#define A8_DISPLAY_ROWS {h}\n')
# Center within the 240-line ANTIC display interval. Mode D doubles rows.
blank=(240-2*h)//2
instructions=[]
while blank:
 n=min(8,blank);instructions.append((n-1)*16);blank-=n
(r/'generated/display_config.asm').write_text('DISPLAY_ROWS = '+str(h)+'\n')
addresses=[0xa200+y*40 if y<89 else 0xb200+(y-89)*40 for y in range(h)]
assert len(set(addresses))==h
assert all((a&0xfff)+40<=4096 for a in addresses)
assert all(not (a<0xa200 and a+40>0xa100) for a in addresses)
assert all(not (a<0xb200 and a+40>0xb000) for a in addresses)
# Keep one LMS per mode line to preserve the DL DMA workload.
(r/'generated/display_list.asm').write_text(('        dta '+','.join(f'${v:02x}' for v in instructions)+'\n' if instructions else '')+''.join(f'        dta $4d,a(${a:04x})\n' for a in addresses)+'        dta $41,a(dl)\n')
(r/'generated/screen_addresses.json').write_text(json.dumps(addresses))
print(f'Display: {h} rows, {h*40} bytes, {h*2} scanlines')

full_addresses=[0xa200+y*40 if y<89 else 0xb200+(y-89)*40 for y in range(144)]
occupied=set()
for a in full_addresses:
 assert (a&4095)+40<=4096
 for b in range(a,a+40):
  assert b not in occupied
  occupied.add(b)
for a,n in [(0xa000,512),(0xb000,512),(0xbb00,312),(0xbd00,312),(0xbe40,312),(0xbffa,6)]:
 assert not occupied.intersection(range(a,a+n))
for lines,origin in [(144,0x2a00),(192,0x2c00)]:
 label='dl144' if lines==144 else 'dl192'
 blank=(240-lines)//2; data=[]
 while blank:
  n=min(8,blank);data.append(f'        dta ${(n-1)*16:02x}\n');blank-=n
 sequence=list(range(144)) if lines==144 else [y for y in range(144) for _ in range(2 if y%3==2 else 1)]
 assert len(sequence)==lines
 data += [f'        dta $4e,a(${(0xbaa8 if lines==192 and i==187 else 0xbad0 if lines==192 and i==191 else full_addresses[y]):04x})\n' for i,y in enumerate(sequence)]
 data += [f'        dta $41,a({label})\n']
 (r/f'generated/display_list_{lines}.asm').write_text(''.join(data))
(r/'generated/screen_addresses_full.json').write_text(json.dumps(full_addresses))
