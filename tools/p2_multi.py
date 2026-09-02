# -*- coding: utf-8 -*-
"""三样本干净H对比: P_2(HP256/LP800/1极) P_I(HP300/LP700/1极) P_J(HP400/LP800/4极)
提取: 裙摆3dB点、平台增益、增益深度缓变 → 推 α(fc)/极点/归一化规律
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

def est_H(S, R, r0=4, r1=505, kmax=26):
    L = r1 - r0
    FS = np.fft.rfft(S[r0:r1], axis=0); FR = np.fft.rfft(R[r0:r1], axis=0)
    H = np.zeros(kmax+1, complex)
    for k in range(kmax+1):
        d = (FS[k].conj()*FS[k]).sum()
        H[k] = (FS[k].conj()*FR[k]).sum()/d if d > 1e-6 else 0
    return H

src = load(BASE + '/1103_010.DZT')
S = src[:, :512]
binMHz = 25.6e9 / (505-4) / 1e6   # ≈51.1 MHz/bin

for name, hp, lp, npole in (('P_2', 256, 800, 1), ('P_I', 300, 700, 1), ('P_J', 400, 800, 4)):
    R = load(BASE + '/Proc/1103_010 %s.DZT' % name)[:, :512]
    H = est_H(S, R)
    M = np.abs(H)
    plat = M[8:12].mean()
    # 3dB点(相对平台): HP侧
    def cross(arr, target, lo, hi):
        for k in range(lo, hi):
            if arr[k] >= target:
                # 线性内插
                f = (target - arr[k-1]) / (arr[k] - arr[k-1]) if arr[k] != arr[k-1] else 0
                return (k-1+f) * binMHz
        return -1
    hp3 = cross(M, plat*0.707, 0, 8)
    print('%s HP%d LP%d %d极 | H:', name, hp, lp, npole)
    print('   |H|=', np.round(M[:22], 3))
    print('   平台=%.3f  HP3dB≈%.0fMHz(标称%d)' % (plat, hp3, hp))
