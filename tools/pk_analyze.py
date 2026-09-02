# -*- coding: utf-8 -*-
"""P_K(LP800/HP400/12极) 对照 P_J(同截止/4极): 解码proc + 干净H + 极高频异常 + 行偏移
"""
import numpy as np
import struct

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

# 1) proc 解码
h = open(BASE + '/Proc/1103_010 P_K.DZT', 'rb').read(1024)
off, = struct.unpack('<h', h[48:50]); sz, = struct.unpack('<h', h[50:52])
print('P_K proc:', h[off:off+sz].hex(' '))
b = h[off:off+sz]
for i in (8, 14):
    f, = struct.unpack('<f', b[i:i+4])
    print('  f32@%d = %.4f -> %.0f MHz' % (i, f, 25.6e9/2/np.pi/f/1e6))

src = load(BASE + '/1103_010.DZT')
S = src[:, :512]

def est_H(R, r0=4, r1=505, kmax=26):
    L = r1 - r0
    FS = np.fft.rfft(S[r0:r1], axis=0); FR = np.fft.rfft(R[r0:r1], axis=0)
    H = np.zeros(kmax+1, complex)
    for k in range(kmax+1):
        d = (FS[k].conj()*FS[k]).sum()
        H[k] = (FS[k].conj()*FR[k]).sum()/d if d > 1e-6 else 0
    return H

for name in ('P_J', 'P_K'):
    R = load(BASE + '/Proc/1103_010 %s.DZT' % name)[:, :512]
    H = est_H(R)
    print('\n%s 干净H前20bin:' % name, np.round(np.abs(H[:20]), 3))
    FS = np.abs(np.fft.rfft(S[4:505], axis=0)); FR = np.abs(np.fft.rfft(R[4:505], axis=0))
    hi = FR[150:250].sum()/FS[150:250].sum()
    print('  极高频(7.6-12G)能量比 = %.3f' % hi)
    # 行偏移
    R64 = R[:, :64]; S64 = S[:, :64]
    offs = []
    for r in range(16, 48, 4):
        bb = R64[r] - R64[r].mean()
        bv, bk = -2, None
        for k in range(-10, 11):
            if 0 <= r+k < 512:
                s = S64[r+k] - S64[r+k].mean()
                c = np.corrcoef(bb, s)[0,1]
                if c > bv: bv, bk = c, k
        offs.append(bk)
    print('  行偏移序列(16..44):', offs)
