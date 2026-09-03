# -*- coding: utf-8 -*-
"""PRV01__072 P_1 偏移标定: 头/vlc/深部能量/理论k模型"""
import os
import struct
import numpy as np

root = [f for f in os.listdir(r'D:/gpr_software/test_input_raw_files') if 'process' in f][0]
BASE = os.path.join(r'D:/gpr_software/test_input_raw_files', root)
SRC = os.path.join(BASE, 'PRV01__072.DZT')
REF = os.path.join(BASE, 'Proc', 'PRV01__072 P_1.DZT')

def header(path):
    h = open(path, 'rb').read(1024)
    nsamp, = struct.unpack('<h', h[10:12])
    bits = h[12]
    rng, = struct.unpack('<f', h[26:30])
    off, = struct.unpack('<h', h[48:50]); sz, = struct.unpack('<h', h[50:52])
    return nsamp, bits, rng, h[off:off+sz]

ns, bits, rng, proc0 = header(SRC)
ns2, bits2, rng2, proc1 = header(REF)
print('原始: nsamp=%d bits=%d range=%.3f proc=%s' % (ns, bits, rng, proc0.hex(' ')))
print('P_1 : nsamp=%d bits=%d range=%.3f proc=%s' % (ns2, bits2, rng2, proc1.hex(' ')))
for i in range(6, len(proc1)-3):
    f, = struct.unpack('<f', proc1[i:i+4])
    if 1e-6 < abs(f) < 100:
        print('  f32@%d=%.6f (x512=%.2f)' % (i, f, f*512))

d = open(os.path.join(BASE, 'PRV01__072.VLC'), 'rb').read()
print('VLC:', d.decode('ascii', errors='replace').replace('\t',' ')[:220])

# 数据
dt = '<i2' if bits == 16 else '<i4'
DOFF = 0x1000 if bits == 16 else 0x2000   # 探测
def load(path, off):
    raw = np.fromfile(path, dtype=dt, offset=off)
    n = len(raw)//ns
    return raw[:n*ns].reshape(n, ns).T.astype(np.float64)
# 自动找数据偏移: 试几个找非零
for off in (0x400, 0x800, 0x1000, 0x2000):
    t = np.fromfile(SRC, dtype=dt, count=ns*4, offset=off)
    print('偏移0x%x 前8值:' % off, t[:8], '非零=%.2f' % (t!=0).mean())
