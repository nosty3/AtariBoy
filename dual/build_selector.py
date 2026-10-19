from pathlib import Path
import re,sys,subprocess
r=Path(__file__).resolve().parents[1];s=r/'dual/selector';g=s/'generated'
g.mkdir(exist_ok=True)
a8bytes=bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})', (r/'dual/a8picocart/rom.h').read_text()))
assert len(a8bytes)==8192
# Original AtariBoy boot vectors, as assembled (never guess INIT addresses).
labels={v[2]:int(v[1],16) for l in (r/'generated/boot.lab').read_text().splitlines() if len(v:=l.split())==3 and v[0]=='00'}
ab=bytes([0,0xb0,0,4,labels['INIT']&255,labels['INIT']>>8])
(g/'vectors.asm').write_text('        dta '+','.join(f'${x:02x}' for x in ab+a8bytes[-6:])+'\n')
for asm,out in [('ram','ram'),('cart','cart')]:subprocess.run([sys.argv[1],str(s/f'{asm}.asm'),f'-o:{g/out}.bin',f'-l:{g/out}.lst',f'-t:{g/out}.lab'],check=True,cwd=s)
rom=(g/'cart.bin').read_bytes();assert len(rom)==8192 and rom[-6:]!=ab and rom[-6:]!=a8bytes[-6:]
(g/'selector_rom.h').write_text('static const unsigned char selector_boot[512]={\n'+','.join(str(b) for b in rom[0x1700:0x1900])+'};\nstatic const unsigned char selector_vectors[6]={'+','.join(str(b) for b in rom[-6:])+'};\n')
print('Selector Atari vectors',rom[-6:].hex(),'AtariBoy',ab.hex(),'A8',a8bytes[-6:].hex())
