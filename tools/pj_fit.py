# -*- coding: utf-8 -*-
"""P_J(HP400/LP800/4极) 单向级联细扫: LPx4 + HP(x-LP)x4, alpha=k/c^p
若P_J达到0.99+ → 反推公式 → 用P_2/P_I验证
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')
S96 = src[:, :96]
X = np.vstack([S96[2:], np.zeros((2, 96))])      # lag2 (哨兵证据)
RJ = load(BASE + '/Proc/1103_010 P_J.DZT')[:, :96]

cHP, cLP, NP = 10.1859, 5.0930, 4

def lp(Xv, a):
    Y = np.empty_like(Xv); Y[0] = 0.0
    for n in range(1, Xv.shape[0]):
        Y[n] = Y[n-1] + a*(Xv[n]-Y[n-1])
    return Y

def model(aL, aH, npole=NP):
    out = X
    for _ in range(npole):
        out = lp(out, aL)
    for _ in range(npole):
        out = out - lp(out, aH)
    return out

rows = slice(4, 500)
res = []
for p in (0.4, 0.5, 0.6, 0.7, 1.0):
    for k in np.arange(0.05, 1.05, 0.05):
        aL, aH = k/cLP**p, k/cHP**p
        out = model(aL, aH)
        c = np.corrcoef(out[rows].ravel(), RJ[rows].ravel())[0,1]
        res.append((c, k, p))
res.sort(reverse=True)
print('P_J 单向4极 粗扫 top6:')
for r in res[:6]:
    print('  corr=%.5f k=%.2f p=%.1f  (aL=%.4f aH=%.4f)' % (r[0], r[1], r[2], r[1]/cLP**r[2], r[1]/cHP**r[2]))
c0, k0, p0 = res[0]
fine = []
for p in np.arange(p0-0.1, p0+0.1, 0.02):
    for k in np.arange(max(0.01, k0-0.08), k0+0.08, 0.01):
        aL, aH = k/cLP**p, k/cHP**p
        out = model(aL, aH)
        fine.append((np.corrcoef(out[rows].ravel(), RJ[rows].ravel())[0,1], k, p))
fine.sort(reverse=True)
print('细扫 top5:')
for r in fine[:5]:
    print('  corr=%.5f k=%.3f p=%.3f' % r)
