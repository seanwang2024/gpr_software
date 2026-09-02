# -*- coding: utf-8 -*-
"""时域核解剖: W_est(测得窗) IFFT -> RADAN 实际卷积核; 看支撑/形状/归一化规律
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000
fs = NS / 20e-9

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')
S = src[:, :512]

def est_H(Sg, R, r0=4, r1=505, kmax=250):
    L = r1 - r0
    FS = np.fft.rfft(Sg[r0:r1], axis=0); FR = np.fft.rfft(R[r0:r1], axis=0)
    H = np.zeros(kmax+1, complex)
    for k in range(kmax+1):
        d = (FS[k].conj()*FS[k]).sum()
        H[k] = (FS[k].conj()*FR[k]).sum()/d if d > 1e-6 else 0
    return H

for name, fH, fL, n in (('P_2', 256e6, 800e6, 1), ('P_I', 300e6, 700e6, 1),
                        ('P_J', 400e6, 800e6, 4), ('P_K', 400e6, 800e6, 12)):
    R = load(BASE + '/Proc/1103_010 %s.DZT' % name)[:, :512]
    Hc = est_H(S, R)
    W = np.zeros(NS, complex)
    W[:len(Hc)] = Hc
    h = np.fft.ifft(W).real
    # 核翻转(因果性对称无所谓), 打印主瓣
    peak = np.abs(h).max()
    support = np.where(np.abs(h) > peak*0.02)[0]
    print('%s(n=%2d) 核峰=%.4f 主瓣行0-8:' % (name, n, peak), np.round(h[:9]/peak, 3))
    print('   支撑(>2%%峰): 行%d..%d (长%d)  核和=%.4f' % (
        support.min(), support.max(), len(support), h.sum()))
