# -*- coding: utf-8 -*-
"""P_2 零相位双向滤波假设: filtfilt 式(前向+后向), 每段1极点
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
fs = NS / 20e-9
S = src[:, :96]; R = ref[:, :96]
X0 = np.vstack([S[2:], np.zeros((2, S.shape[1]))])

def alpha(mode, fc):
    x = 2*np.pi*fc/fs
    return {'exp': 1-np.exp(-x), 'lin': x, 'bil': x/(1+x), 'sqrt': np.sqrt(x)}[mode]

def lp(Xv, a, y0=0.0):
    Y = np.empty_like(Xv); Y[0] = y0
    for n in range(1, Xv.shape[0]):
        Y[n] = Y[n-1] + a*(Xv[n]-Y[n-1])
    return Y

def fwdbwd(Xv, a):
    return lp(lp(Xv[::-1], a)[::-1], a)      # 双向LP(零相位)

rows = slice(4, 500)
res = []
for mode in ('exp','lin','bil','sqrt'):
    aL, aH = alpha(mode, 800e6), alpha(mode, 256e6)
    L2 = fwdbwd(X0, aL)                       # 双向LP800
    H2 = X0 - fwdbwd(X0, aH)                  # 双向HP256 = x − 双向LP
    for name, out in (
        ('LP2→HP2', L2 - fwdbwd(L2, aH)),
        ('HP2→LP2', H2 - fwdbwd(H2, aL) * 0 + fwdbwd(H2, aL) - fwdbwd(H2, aL) + fwdbwd(H2, aL)),  # placeholder
    ):
        pass
    # 清晰版
    out1 = L2 - fwdbwd(L2, aH)                # 先LP双向 再(x−LP双向)
    mid = X0 - fwdbwd(X0, aH)
    out2 = fwdbwd(mid, aL)
    # 单向HP+双向LP / 双向HP+单向LP
    out3 = lp(L2, aH) * 0 + (L2 - lp(L2, aH))
    c1 = np.corrcoef(out1[rows].ravel(), R[rows].ravel())[0,1]
    c2 = np.corrcoef(out2[rows].ravel(), R[rows].ravel())[0,1]
    c3 = np.corrcoef(out3[rows].ravel(), R[rows].ravel())[0,1]
    res += [(c1, mode, '双LP→双HP'), (c2, mode, '双HP→双LP'), (c3, mode, '双LP→单HP')]
res.sort(reverse=True)
for r in res[:8]:
    print('corr=%.6f mode=%-4s %s' % r)
