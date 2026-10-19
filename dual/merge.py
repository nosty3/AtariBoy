"""Merge three separately linked firmware images; only GB reset vector changes.
No payload at or beyond FAT +1 MiB. Complete every touched 4 KiB sector (RP2040-E14); whole unused sectors stay untouched.
"""
from pathlib import Path
import sys,struct,subprocess,json,hashlib,os
from uf2_flash import complete_sectors
r=Path(__file__).resolve().parents[1]
bases=[0x10000000,0x100bc000,0x100c0000];limits=[752*1024,0x3f00,256*1024]
names=['atariboy','dual_selector','a8_pico_cart'];dirs=list(map(Path,sys.argv[1:4]));blobs=[];syms=[]
for folder,name,limit,base in zip(dirs,names,limits,bases):
 data=bytearray((folder/f'{name}.bin').read_bytes());assert 0x108<len(data)<=limit
 listing=subprocess.check_output([os.environ.get('NM','arm-none-eabi-nm'),'-n',str(folder/f'{name}.elf')],text=True)
 symbols={a[2]:int(a[0],16) for l in listing.splitlines() if len(a:=l.split())==3}
 assert symbols['__vectors']==base+0x100,(name,hex(symbols['__vectors']))
 syms.append(symbols);blobs.append(data)
absp,abpc=struct.unpack_from('<II',blobs[0],0x100);a8sp,a8pc=struct.unpack_from('<II',blobs[2],0x100)
entry=syms[1]['selector_entry']|1
struct.pack_into('<I',blobs[0],0x104,entry)
manifest=struct.pack('<6I',0x314c5544,1,absp,abpc,a8sp,a8pc)
segments=list(zip(bases,blobs))+[(0x100bff00,manifest)]
blocks={}
for addr,data in segments:
 for i in range(0,len(data),256):
  assert addr+i not in blocks and addr+i+256<=0x10100000
  blocks[addr+i]=bytes(data[i:i+256]).ljust(256,b'\0')
blocks=complete_sectors(blocks,0x10000000,0x10100000)
uf2=bytearray();total=len(blocks)
for n,(addr,data) in enumerate(sorted(blocks.items())):
 uf2+=struct.pack('<8I',0x0a324655,0x9e5d5157,0x2000,addr,256,n,total,0xe48bff56)+data+bytes(220)+struct.pack('<I',0x0ab16f30)
release=r/'Build';release.mkdir(exist_ok=True)
(release/'atariboy-v090b-dual.uf2').write_bytes(uf2)
f=r/'out/firmware';f.mkdir(parents=True,exist_ok=True)
info={'package_version':'0.90b','firmware_version':'0.90b','uf2_sector_padding':'RP2040-E14: every touched sector has all 16 pages','flash_slots':dict(zip(names,[hex(b) for b in bases])),'sizes':dict(zip(names,map(len,blobs))),'selector_entry':hex(entry),'gb_original_reset':hex(abpc),'a8_reset':hex(a8pc),'fat_start':'0x10100000','uf2_bytes':len(uf2),'uf2_sha256':hashlib.sha256(uf2).hexdigest()}
(f/'layout.json').write_text(json.dumps(info,indent=2)+'\n')
for folder,name in zip(dirs,names):
 for ext in ['elf','bin']:(f/f'{name}.{ext}').write_bytes((folder/f'{name}.{ext}').read_bytes())
print(json.dumps(info,indent=2))
