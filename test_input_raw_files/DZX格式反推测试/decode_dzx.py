# -*- coding: utf-8 -*-
"""Decode a DZX's BinaryData processes (uuencode) to match RADAN processing records.
Mirrors MainWindow::uuDecode + parseDzxProcesses logic.
Usage: python decode_dzx.py <file.dzx>
"""
import sys, struct, re
import xml.etree.ElementTree as ET

def uu_decode(text):
    text = (text.replace("&amp;", "&").replace("&lt;", "<")
                 .replace("&gt;", ">").replace("&quot;", '"').replace("&apos;", "'"))
    text = re.sub(r"[\r\n\t ]", "", text)
    out = bytearray()
    pos = 0
    while pos < len(text):
        n = (ord(text[pos]) - 32) & 0x3F   # 该行数据字节数
        pos += 1
        if n == 0:
            break
        nchars = ((n + 2) // 3) * 4
        for i in range(0, nchars, 4):
            v = []
            for j in range(4):
                if pos + i + j < len(text):
                    v.append((ord(text[pos + i + j]) - 32) & 0x3F)
                else:
                    v.append(0)
            b = (v[0] << 18) | (v[1] << 12) | (v[2] << 6) | v[3]
            out.append((b >> 16) & 0xFF)
            out.append((b >> 8) & 0xFF)
            out.append(b & 0xFF)
        pos += nchars
    # 不再按"最后一个 n"截断;按首两字节声明的记录长度对齐(多余补码字节丢弃)
    decl = struct.unpack_from("<H", out, 0)[0] if len(out) >= 2 else len(out)
    return bytes(out[:decl]) if 0 < decl <= len(out) else bytes(out)

def hexdump(b):
    return " ".join("%02x" % x for x in b)

def floats(b, off, count):
    res = []
    for i in range(count):
        p = off + i * 4
        if p + 4 <= len(b):
            res.append(struct.unpack_from("<f", b, p)[0])
    return res

def u16(b, off):
    if off + 2 <= len(b):
        return struct.unpack_from("<H", b, off)[0]
    return None

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else r"D:\gpr_software\test_input_raw_files\测试样本\测试样本\DZXDEMO\Proc\DZXDEMO__002 P_2.DZX"
    tree = ET.parse(path)
    root = tree.getroot()
    # strip namespace
    bins = [el for el in root.iter() if el.tag.split('}')[-1] == 'BinaryData']
    print("=== %s : %d BinaryData processes ===" % (path, len(bins)))
    for idx, el in enumerate(bins):
        raw = el.text or ""
        blob = uu_decode(raw)
        tid = blob[8] if len(blob) > 8 else None
        print("\n[#%d] blob=%d bytes  typeId(0x08)=%s (0x%02x)" % (idx+1, len(blob), tid, tid if tid is not None else 0))
        print("  hex: %s" % hexdump(blob))
        # show some interpretations
        if len(blob) >= 10:
            print("  u16@0x09=%s  u16@0x0A=%s  u16@0x0B=%s" % (
                u16(blob,9), u16(blob,0x0A), u16(blob,0x0B)))
        # floats from offset 0x0A and 0x0B
        for base in (0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x10, 0x12):
            fs = floats(blob, base, 6)
            pretty = " ".join("%.3f" % f for f in fs)
            print("  f@0x%02x: %s" % (base, pretty))

if __name__ == "__main__":
    main()
