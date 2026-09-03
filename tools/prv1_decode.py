# -*- coding: utf-8 -*-
"""PRV01__072 P_1 偏移标定: 头解码 + 深部能量评估 + 理论k模型"""
"""PRV01__072 P_1 偏移标定: 头解码 + 深部能量评估 + 理论k模型"""
import struct
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'

def header(path):
    h = open(path, 'rb').read(1024)
    nsamp, = struct.unpack('<h', h[10:12])       # rh_nsamp@10
    bits = h[12]                                  # rh_bits
    rng, = struct.unpack('<f', h[26:30])          # rhf_range
    off, = struct.unpack('<h', h[48:50]); sz, = struct.unpack('<h', h[50:52])
    return nsamp, bits, rng, h[off:off+sz]

for p in ('PRV01__072.DZT', 'PRV01__072 P_1.DZT'):
    ns, bits, rng, proc = header(BASE + '/' + p)
    print('%s: nsamp=%d bits=%d range=%.3fns proc=%s' % (p, ns, bits, rng, proc.hex(' ')))

# vlc
d = open(BASE + '/PRV01__072.VLC', 'rb').read()
print('VLC:', d.decode('ascii', errors='replace').replace('\t', ' ')[:200])

# 数据加载(bits 决定)与深部能量
NS = ns if ns > 0 else 512
def load(p, dtype):
    raw = np.fromfile(p, dtype=dtype, offset=2048 if False else 0)
    return raw
# 先探测数据偏移: 用 bits=16 → '<i2'; 假定数据从 rh_proc 后或 0x800
h = open(BASE + '/PRV01__072.DZT', 'rb').read()
print('数据偏移探测: rh_proc@48-52:', h[48:52].hex(' '), ' 头1024末尾:', h[1016:1024].hex(' '))
