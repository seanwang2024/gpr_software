# -*- coding: utf-8 -*-
"""P_5 克西霍夫起步: proc记录全解码 + 哨兵检查 + 基线corr + 头部关键字段"""
import struct
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

for name in ('P_5',):
    p = BASE + '/Proc/1103_010 %s.DZT' % name
    h = open(p, 'rb').read(1024)
    off, = struct.unpack('<h', h[48:50]); sz, = struct.unpack('<h', h[50:52])
    blob = h[off:off+sz]
    print(name, 'proc区 sz=%d:' % sz, blob.hex(' '))
    # 逐字节尝试 f32
    for i in range(len(blob) - 3):
        f, = struct.unpack('<f', blob[i:i+4])
        if 1e-3 < abs(f) < 100 and abs(f*1000 - round(f*1000)) < 1e-6:
            pass  # 太多, 只打印头几条对齐位置
    # 已知启发: 记录1条 = 4d00(e8be32c0) + 24 3f ... 先按 4d 6B 跳过
    rest = blob[6:]
    print('  残余(去4d记录):', rest.hex(' '))
    print('  残余长度', len(rest))
    for i in range(0, len(rest) - 3):
        f, = struct.unpack('<f', rest[i:i+4])
        if 0.001 < abs(f) < 200:
            print('    f32@%d = %.6f' % (i, f))
    # 头部关键: rh_nsamp@20? rhf_range@26(4B), position@22
    rng, = struct.unpack('<f', h[26:30]); pos, = struct.unpack('<f', h[22:26])
    npt, = struct.unpack('<h', h[20:22])
    print('  头: range=%.3f position=%.4f nsamp=%d' % (rng, pos, npt))
    # 哨兵检查
    R = load(p)
    print('  行0前3:', R[0, :3], '行1前3:', R[1, :3], '哨兵数:', int((R[1] == -16777216).sum()))
    # 基线
    S = load(BASE + '/1103_010.DZT')
    print('  基线corr(P_5 vs 原始) = %.4f' % np.corrcoef(S[2:500, :64].ravel(), R[2:500, :64].ravel())[0,1])
