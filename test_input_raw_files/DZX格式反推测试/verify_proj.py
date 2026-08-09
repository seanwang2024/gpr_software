# -*- coding: utf-8 -*-
"""Verify the cross-validated DZX scheme against PROJ__001-017 (first-version test data)."""
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

def U16(b,o): return struct.unpack_from('<H',b,o)[0] if o+2<=len(b) else 0
def FL(b,o):  return struct.unpack_from('<f',b,o)[0]  if o+4<=len(b) else 0.0

base = 'D:/gpr_software/test_input_raw_files/DZX格式反推测试'
# PROJ settings from the original experiment table (PROJ labels, which we know are HP/LP swapped)
proj_settings = {
    '001': {'gain':'auto 3pt', 'IIR_HP':800, 'IIR_LP':100, 'FIR_HP':'off', 'FIR_LP':'off', 'FIR_stack':'off', 'FIR_bg':0, 'IIR_stack':0, 'IIR_bg':0},
    '002': {'gain':'auto 4pt', 'IIR_HP':800, 'IIR_LP':100},
    '003': {'gain':'auto 3pt', 'IIR_HP':600, 'IIR_LP':100},
    '004': {'gain':'auto 3pt', 'IIR_HP':800, 'IIR_LP':200},
    '005': {'gain':'auto 3pt', 'IIR_HP':800, 'IIR_LP':100, 'IIR_stack':3},
    '006': {'gain':'auto 3pt', 'IIR_HP':800, 'IIR_LP':100, 'IIR_bg':3},
    '007': {'gain':'auto 3pt', 'IIR_HP':800, 'IIR_LP':100},
    '008': {'gain':'auto 3pt', 'IIR_HP':800, 'IIR_LP':100, 'FIR_HP':800},
    '009': {'gain':'auto 3pt', 'IIR_HP':800, 'IIR_LP':100, 'FIR_LP':100},
    '010': {'gain':'auto 3pt', 'IIR_HP':800, 'IIR_LP':100, 'FIR_stack':3},
    '011': {'gain':'auto 3pt', 'IIR_HP':800, 'IIR_LP':100, 'FIR_bg':3},
    '012': {'gain':'manual 3pt [4,37,62]', 'IIR_HP':800, 'IIR_LP':100},
    '013': {'gain':'manual 3pt [4,8,62]', 'IIR_HP':800, 'IIR_LP':100},
    '014': {'gain':'manual 3pt [4,37,4]', 'IIR_HP':800, 'IIR_LP':100},
    '015': {'gain':'manual 3pt [37,37,62]', 'IIR_HP':800, 'IIR_LP':100},
    '016': {'gain':'manual 4pt [4,4,37,62]', 'IIR_HP':800, 'IIR_LP':100},
    '017': {'gain':'manual 4pt [4,37,62,4]', 'IIR_HP':800, 'IIR_LP':100},
}

for num in sorted(proj_settings.keys()):
    fn = os.path.join(base, 'PROJ__%s.DZX' % num)
    if not os.path.exists(fn): continue
    root = ET.parse(fn).getroot()
    procs = []
    for el in root.iter():
        if el.tag.split('}')[-1] != 'BinaryData': continue
        b = uu_decode(el.text or '')
        if len(b) < 9: continue
        tid = b[8]
        if tid == 99: procs.append('TZ(skip)')
        elif tid == 77: procs.append('DC=%.1f'%FL(b,0x0A))
        elif tid == 59:
            npts = b[9]; g = [FL(b,0x0B+i*4) for i in range(npts) if 0x0B+i*4+4<=len(b)]
            procs.append('Gain %dpt %s'%(npts, '/'.join('%.0f'%x for x in g)))
        elif tid == 4: procs.append('IIRv LP=%d HP=%d'%(U16(b,0x20),U16(b,0x22)))
        elif tid == 64: procs.append('FIRvHP=%d'%U16(b,0x20))
        elif tid == 63: procs.append('FIRvLP=%d'%U16(b,0x1E))
        elif tid == 14: procs.append('IIRh=%.0f'%FL(b,0x0A))
        elif tid == 13: procs.append('IIRh13=%.0f'%FL(b,0x0A))
        elif tid == 67: procs.append('FIRhs=%.0f'%FL(b,0x09))
        else: procs.append('tid%d'%tid)
    s = proj_settings[num]
    print('PROJ__%s | %s' % (num, '  '.join(procs)))
    print('         | set: gain=%s IIR_HP=%s IIR_LP=%s FIR_HP=%s FIR_LP=%s' % (
        s.get('gain','?'), s.get('IIR_HP','?'), s.get('IIR_LP','?'),
        s.get('FIR_HP','-'), s.get('FIR_LP','-')))
