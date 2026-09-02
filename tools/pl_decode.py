# -*- coding: utf-8 -*-
"""P_L(偏移 v=0) vs P_5(偏移 v=14.99) 对照分析"""
import struct
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

# proc
p = BASE + '/Proc/1103_010 P_L.DZT'
h = open(p, 'rb').read(1024)
off, = struct.unpack('<h', h[48:50]); sz, = struct.unpack('<h', h[50:52])
print('P_L proc:', h[off:off+sz].hex(' '))
b = h[off:off+sz]
for i in range(6, len(b) - 3):
    f, = struct.unpack('<f', b[i:i+4])
    if 1e-6 < abs(f) < 200:
        print('  f32@%d = %.6f' % (i, f))

src = load(BASE + '/1103_010.DZT')
R = load(p)
P5 = load(BASE + '/Proc/1103_010 P_5.DZT')
S = src[:, 200:264]; RL = R[:, 200:264]; R5 = P5[:, 200:264]

print('哨兵数:', int((R[1] == -16777216).sum()))
print('corr: P_L vs 原始 = %.4f | P_L vs P_5 = %.4f' % (
    np.corrcoef(S[2:500].ravel(), RL[2:500].ravel())[0,1],
    np.corrcoef(RL[2:500].ravel(), R5[2:500].ravel())[0,1]))
# 增益: LS
g = (S[2:500]*RL[2:500]).sum()/(S[2:500]*S[2:500]).sum()
print('P_L/src LS增益 = %.4f (备注增益4)' % g)
# 逐道corr(P_L vs src): 若v=0偏移=纯增益, 每道corr≈1
cs = [np.corrcoef(S[2:500, c], RL[2:500, c])[0,1] for c in range(20, 44)]
print('逐道corr(P_L,src) 均值=%.4f min=%.3f' % (np.mean(cs), np.min(cs)))
# 谱比
FS = np.abs(np.fft.rfft(S[4:505], axis=0)).mean(axis=1); FL = np.abs(np.fft.rfft(RL[4:505], axis=0)).mean(axis=1)
print('谱比 P_L/src 前12bin:', np.round((FL[:12]+1e-9)/(FS[:12]+1e-9), 3))
# 行std对比
print('行std P_L(每64行):', np.round([RL[i:i+64].std()/1e4 for i in range(0, 512, 64)], 1))
print('行std src(每64行):', np.round([S[i:i+64].std()/1e4 for i in range(0, 512, 64)], 1))
