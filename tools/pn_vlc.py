# -*- coding: utf-8 -*-
"""VLC格式逆向 + P_N(载入vlc偏移)proc解码"""
import struct
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'

# 1) VLC 文件
d = open(BASE + '/1103_010.VLC', 'rb').read()
print('VLC 大小:', len(d))
print('前128字节hex:')
for i in range(0, min(128, len(d)), 16):
    print('  %04x: %s' % (i, d[i:i+16].hex(' ')))
# 尝试 f32 流解码
n = len(d) // 4
f = np.frombuffer(d[:n*4], dtype='<f4')
print('f32流前40:', np.round(f[:40], 4))
print('f32统计: min=%.4f max=%.4f 非零比例=%.2f' % (f.min(), f.max(), (f != 0).mean()))
# u16 流视角
u = np.frombuffer(d[:len(d)//2*2], dtype='<u2')
print('u16流前24:', u[:24])

# 2) P_N proc
p = BASE + '/Proc/1103_010 P_N.DZT'
h = open(p, 'rb').read(1024)
off, = struct.unpack('<h', h[48:50]); sz, = struct.unpack('<h', h[50:52])
blob = h[off:off+sz]
print('\nP_N proc:', blob.hex(' '))
for i in range(6, len(blob)-3):
    v, = struct.unpack('<f', blob[i:i+4])
    if 1e-6 < abs(v) < 200:
        print('  f32@%d = %.6f' % (i, v))
