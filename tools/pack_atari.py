"""Lossless stream codec: literals plus the repeated stereo sample template.
No sliding window or dynamic allocation. Decoder needs only 45 SRAM bytes.
"""
TEMPLATE=bytes.fromhex('8d0ad4 ad00bb 8d01d2 8d03d2 ad00bd 8d05d2 ad40be 8d07d2 ad00b0 8d11d2 8d13d2 ad38b1 8d15d2 ad7800 8d17d2')
OFFSETS=(4,13,19,25,34,40)
assert len(TEMPLATE)==45
def matches(s):
 return len(s)==45 and all(s[i]==v for i,v in enumerate(TEMPLATE) if i not in {j for p in OFFSETS for j in (p,p+1)})
def pack(data):
 out=bytearray();literal=bytearray();previous=None;p=0
 def flush():
  if literal:out.append(len(literal)-1);out.extend(literal);literal.clear()
 while p<len(data):
  sample=data[p:p+45]
  if matches(sample):
   flush();addresses=[int.from_bytes(sample[i:i+2],'little') for i in OFFSETS]
   if previous is not None and addresses==[(a+1)&65535 for a in previous]:out.append(128)
   else:
    out.append(129)
    for a in addresses:out.extend(a.to_bytes(2,'little'))
   previous=addresses;p+=45
  else:
   literal.append(data[p]);p+=1
   if len(literal)==128:flush()
 flush();return bytes(out)
def unpack(data):
 out=bytearray();sample=bytearray(TEMPLATE);p=0
 while p<len(data):
  tag=data[p];p+=1
  if tag<128:out.extend(data[p:p+tag+1]);p+=tag+1
  else:
   for i in OFFSETS:
    if tag==128:a=(int.from_bytes(sample[i:i+2],'little')+1)&65535
    else:a=int.from_bytes(data[p:p+2],'little');p+=2
    sample[i:i+2]=a.to_bytes(2,'little')
   out.extend(sample)
 return bytes(out)
