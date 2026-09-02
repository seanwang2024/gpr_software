# -*- coding: utf-8 -*-
"""P_2 2D系统辨识: H2D(ky,kx) = |S2D*R2D|/|S2D|², 检验可分离性 Hvert(ky)·Hhor(kx)
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')
ref = load(BASE + '/Proc/1103_010 P_2.DZT')
N = min(src.shape[1], ref.shape[1])
S = src[:, :N]; R = ref[:, :N]

FS = np.fft.rfft2(S); FR = np.fft.rfft2(R)
H2 = (FS.conj()*FR) / np.maximum(FS.conj()*FS, 1e-9)
M = np.abs(H2)

print('H2D 幅值矩阵 (行=垂直频率bin 0..12, 列=水平频率bin 0..8):')
print('ky\\kx ' + ''.join('%7d' % k for k in range(9)))
for ky in range(13):
    print('%5d ' % ky + ''.join('%7.3f' % M[ky, kx] for kx in range(9)))

# 可分离性检验: ky=0 行(纯水平) 与 大ky 平均(垂直主导)
print('\n纯水平响应 M[0,kx]:', np.round(M[0,:10],3))
print('纯垂直响应 M[ky,0]:', np.round(M[:16,0],3))
