#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe18.py — 信息源切换: P_S/P_T/Raw 的 DZT头 diff + DZX 完整dump
"""
import os, struct

CAL = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   'test_input_raw_files', 'process标定')

def hdr(p):
    return open(p, 'rb').read(512)

files = {
    'RAW': os.path.join(CAL, '1103_010.DZT'),
    'P_S': os.path.join(CAL, 'Proc', '1103_010 P_S.DZT'),
    'P_T': os.path.join(CAL, 'Proc', '1103_010 P_T.DZT'),
    'P_R': os.path.join(CAL, 'Proc', '1103_010 P_R.DZT'),
}
hs = {k: hdr(v) for k, v in files.items()}
print('[DZT头diff vs RAW] (非零差异字节段)')
for name in ('P_S', 'P_T', 'P_R'):
    d = [(i, hs['RAW'][i], hs[name][i]) for i in range(512) if hs['RAW'][i] != hs[name][i]]
    # 合并成段
    segs = []
    for i, a, b in d:
        if segs and segs[-1][0]+segs[-1][1] == i:
            segs[-1][1] += 1; segs[-1][2].append((a, b))
        else:
            segs.append([i, 1, [(a, b)]])
    print(' %s: %d字节差异, %d段' % (name, len(d), len(segs)))
    for s in segs[:20]:
        raw = hs['RAW'][s[0]:s[0]+s[1]]
        new = hs[name][s[0]:s[0]+s[1]]
        fv = struct.unpack('<f', new[:4].ljust(4, b'\0'))[0] if len(new) >= 4 else 0
        print('   @0x%03x len%d  %s → %s   (f32=%g)' % (s[0], s[1], raw.hex(' '), new.hex(' '), fv))

# DZX dump
print('\n[DZX]')
for name in ('P_S', 'P_T', 'P_R'):
    p = os.path.join(CAL, 'Proc', '1103_010 %s.DZX' % name[2:])
    if not os.path.exists(p):
        p = os.path.join(CAL, 'Proc', '1103_010 P_%s.DZX' % name[2:])
    data = open(p, 'rb').read()
    print(' %s: %d bytes' % (name, len(data)))
    # 打印关键元素
    for tag in (b'<ProcessInfo', b'BinaryData', b'GainPoint', b'gain', b'Gain'):
        i = data.find(tag)
        if i >= 0:
            print('   %s @%d: %s' % (tag, i, data[i:i+220].decode('utf-8', 'replace').split('>')[0][:200]))
