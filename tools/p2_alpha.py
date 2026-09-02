# -*- coding: utf-8 -*-
"""P_2 自由α时域拟合: 模型=shift2+LP(aL)+HP(x-LP(aH)) 族, 网格细化 aL/aH 直接最小残差
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
S = src[:, :96]; R = ref[:, :96]
X = np.vstack([S[2:], np.zeros((2, S.shape[1]))])

def lp(Xv, a):
    Y = np.empty_like(Xv); Y[0] = 0.0
    for n in range(1, Xv.shape[0]):
        Y[n] = Y[n-1] + a*(Xv[n]-Y[n-1])
    return Y

rows = (4, 500)
Rv = R[rows[0]:rows[1]].ravel()
best = []
for aL in np.arange(0.05, 0.50, 0.01):
    XL = lp(X, aL)
    for aH in np.arange(0.03, 0.40, 0.01):
        out = XL - lp(XL, aH)
        c = np.corrcoef(out[rows[0]:rows[1]].ravel(), Rv)[0,1]
        best.append((c, aL, aH))
best.sort(reverse=True)
print('粗网格 top5:', [(round(c,5), round(aL,3), round(aH,3)) for c,aL,aH in best[:5]])
c0, aL0, aH0 = best[0]
# 细化
fine = []
for aL in np.arange(aL0-0.012, aL0+0.012, 0.0015):
    XL = lp(X, aL)
    for aH in np.arange(aH0-0.012, aH0+0.012, 0.0015):
        out = XL - lp(XL, aH)
        c = np.corrcoef(out[rows[0]:rows[1]].ravel(), Rv)[0,1]
        fine.append((c, aL, aH))
fine.sort(reverse=True)
print('细网格 top5:', [(round(c,5), round(aL,4), round(aH,4)) for c,aL,aH in fine[:5]])
c, aL, aH = fine[0]
out = lp(X, aL) - lp(lp(X, aL), aH)
d = out[4:500]-R[4:500]
print('最优 corr=%.6f  MAE=%.1f  (对应公式检查: exp(aL)fc=%.0fMHz exp(aH)fc=%.0fMHz)'
      % (c, np.abs(d).mean(), -np.log(1-aL)*25.6e9/2/np.pi/1e6, -np.log(1-aH)*25.6e9/2/np.pi/1e6))
