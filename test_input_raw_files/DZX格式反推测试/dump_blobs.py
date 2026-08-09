# -*- coding: utf-8 -*-
import struct, re, os
import xml.etree.ElementTree as ET

def uu_decode(text):
    text = (text.replace("&amp;","&").replace("&lt;","<").replace("&gt;",">")
                 .replace("&quot;",'"').replace("&apos;","'"))
    text = re.sub(r"[\r\n\t ]","",text)
    out = bytearray(); pos = 0
    while pos < len(text):
        n = (ord(text[pos])-32) & 0x3F; pos += 1
        if n == 0: break
        nc = ((n+2)//3)*4
        for i in range(0,nc,4):
            v = [(ord(text[pos+i+j])-32)&0x3F if pos+i+j<len(text) else 0 for j in range(4)]
            b = (v[0]<<18)|(v[1]<<12)|(v[2]<<6)|v[3]
            out += bytes([(b>>16)&0xFF,(b>>8)&0xFF,b&0xFF])
        pos += nc
    d = struct.unpack_from('<H',out,0)[0] if len(out)>=2 else len(out)
    return bytes(out[:d]) if 0<d<=len(out) else bytes(out)

def FL(b,off): return struct.unpack_from('<f',b,off)[0] if off+4<=len(b) else 0.0

base='D:/gpr_software/test_input_raw_files/测试样本/测试样本/DZXDEMO'
for num in ['002','004','010']:
    dzxf = os.path.join(base, 'DZXDEMO__%s.DZX'%num)
    root = ET.parse(dzxf).getroot()
    print('=== DZXDEMO__%s ==='%num)
    for el in root.iter():
        if el.tag.split('}')[-1] != 'BinaryData': continue
        b = uu_decode(el.text or '')
        if len(b) < 9: continue
        tid = b[8]
        if tid in (67, 13, 14):
            print('  typeId %d (%d B): %s' % (tid, len(b), ' '.join('%02x'%x for x in b[:28])))
            for off in range(8, min(len(b)-3, 22)):
                v = FL(b, off)
                if 0.5 < v < 500:
                    print('    float @0x%02x = %.2f' % (off, v))
