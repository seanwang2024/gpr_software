# -*- coding: utf-8 -*-
"""Diff a given typeId's blob across PROJ files to locate parameter byte offsets."""
import sys, struct
import xml.etree.ElementTree as ET
import re

def uu_decode(text):
    text = (text.replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">")
                 .replace("&quot;", '"').replace("&apos;", "'"))
    text = re.sub(r"[\r\n\t ]", "", text)
    out = bytearray(); pos = 0
    while pos < len(text):
        n = (ord(text[pos]) - 32) & 0x3F; pos += 1
        if n == 0: break
        nchars = ((n + 2) // 3) * 4
        for i in range(0, nchars, 4):
            v = [(ord(text[pos+i+j])-32)&0x3F if pos+i+j < len(text) else 0 for j in range(4)]
            b = (v[0]<<18)|(v[1]<<12)|(v[2]<<6)|v[3]
            out += bytes([(b>>16)&0xFF,(b>>8)&0xFF,b&0xFF])
        pos += nchars
    decl = struct.unpack_from("<H", out, 0)[0] if len(out) >= 2 else len(out)
    return bytes(out[:decl]) if 0 < decl <= len(out) else bytes(out)

def get_blobs(path):
    root = ET.parse(path).getroot()
    res = {}
    for el in root.iter():
        if el.tag.split('}')[-1] == 'BinaryData':
            b = uu_decode(el.text or "")
            tid = b[8] if len(b) > 8 else -1
            res.setdefault(tid, []).append(b)
    return res

def show(label, b):
    print("  %s (%d bytes):" % (label, len(b)))
    print("    hex: " + " ".join("%02x"%x for x in b))
    # print u16 and float at each offset
    for off in range(8, len(b)-1):
        u16 = struct.unpack_from("<H", b, off)[0] if off+2 <= len(b) else None
        fl  = struct.unpack_from("<f", b, off)[0] if off+4 <= len(b) else None
        # only print interesting offsets (non-zero u16 in a sane range, or clean float)
        if u16 and 10 < u16 < 20000:
            print("      off 0x%02x u16=%d" % (off, u16))
        if fl and 1.0 < abs(fl) < 5000 and (off % 1 == 0):
            # avoid printing every float; only if looks like a freq
            if abs(fl) > 1.0:
                pass

base = r"D:\gpr_software\test_input_raw_files\DZX格式反推测试"
tid = int(sys.argv[1]) if len(sys.argv) > 1 else 4
files = sys.argv[2:] or ["PROJ__001.DZX","PROJ__003.DZX","PROJ__004.DZX"]
print("=== typeId %d across %s ===" % (tid, files))
blobs_by_file = {}
for fn in files:
    blobs_by_file[fn] = get_blobs(base + "\\" + fn)
    bl = blobs_by_file[fn].get(tid, [])
    print("\n[%s] typeId %d: %d occurrence(s)" % (fn, tid, len(bl)))
    for i,b in enumerate(bl):
        show("  blob#%d"%i, b)

# byte-by-byte diff across first blob of each file
print("\n=== byte diffs (offset: values) ===")
ref = blobs_by_file[files[0]].get(tid, [None])[0]
if ref:
    for off in range(len(ref)):
        vals = []
        for fn in files:
            bl = blobs_by_file[fn].get(tid, [[0xff]*len(ref)])
            b = bl[0] if bl else b"\x00"*len(ref)
            vals.append(b[off] if off < len(b) else None)
        if len(set(vals)) > 1:
            u16s = []
            for fn in files:
                bl = blobs_by_file[fn].get(tid, [b"\x00"*len(ref)])
                b = bl[0] if bl else b"\x00"*len(ref)
                u16s.append(struct.unpack_from("<H", b, off)[0] if off+2<=len(b) else -1)
            print("  off 0x%02x (%d): bytes=%s  u16@here=%s" % (off, off, vals, u16s))
