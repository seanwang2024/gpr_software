# -*- coding: utf-8 -*-
"""干净版增益律: 对称|f|窗, K = 实测|H_est| / Butter(f) 的平台均值; 四点拟合
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

def est_H(R, r0=4, r1=505, kmax=60):
    L = r1 - r0
    FS = np.fft.rfft(S[r0:r1], axis=0); FR = np.fft.rfft(R[r0:r1], axis=0)
    H = np.zeros(kmax+1, complex)
    for k in range(kmax+1):
        d = (FS[k].conj()*FS[k]).sum()
        H[k] = (FS[k].conj()*FR[k]).sum()/d if d > 1e-6 else 0
    return H

def butter(f, fH, fL, n):
    f = np.maximum(np.abs(f), 1e3)
    return 1.0/np.sqrt(1.0+(f/fL)**(-2*n)) / np.sqrt(1.0+(f/fH)**(2*n))

CASES = [('P_2', 256e6, 800e6, 1), ('P_I', 300e6, 700e6, 1),
         ('P_J', 400e6, 800e6, 4), ('P_K', 400e6, 800e6, 12)]
print('%-5s %-16s %-8s %-8s' % ('样本', '平台bins K均值', '预测K', '误差'))
Ks = []
for name, fH, fL, n in CASES:
    R = load(BASE + '/Proc/1103_010 %s.DZT' % name)[:, :512]
    Hm = np.abs(est_H(R))
    # 平台: 通带中心±2bin (几何中心 bin)
    kc = int(round(np.sqrt(fH*fL)/ (fs/NS)))
    bins = range(max(2, kc-2), kc+3)
    fk = np.array([k*fs/NS for k in bins])
    K = np.mean(Hm[list(bins)] / butter(fk, fH, fL, n))
    Ks.append(K)
    bw = (fL-fH)/1e6
    pred = 0.244*np.sqrt(bw/100.0)*n**0.21
    print('%-5s (%3.0f,%3.0f,%2d)  K=%.4f   %.4f   %+.1f%%' % (name, fH/1e6, fL/1e6, n, K, pred, 100*(pred/K-1)))
# 重新拟合四点: K = C * bw^a * n^b
import itertools
data = [((fL-fH)/1e6, n, K) for (nm, fH, fL, n), K in zip(CASES, Ks)]
# log-linear LS: lnK = lnC + a·ln(bw) + b·ln(n)
A = np.array([[1, np.log(bw), np.log(n)] for bw, n, K in data])
y = np.log([K for bw, n, K in data])
sol, *_ = np.linalg.lstsq(A, y, rcond=None)
C, a, b = np.exp(sol[0]), sol[1], sol[2]
print('拟合: K = %.5f * bw^%.3f * n^%.3f' % (C, a, b))
for bw, n, K in data:
    p = C*bw**a*n**b
    print('  bw=%3.0f n=%2d  实测%.4f 预测%.4f  %+.1f%%' % (bw, n, K, p, 100*(p/K-1)))
