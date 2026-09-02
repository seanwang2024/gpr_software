# -*- coding: utf-8 -*-
"""各样本独立β细扫(固定β双曲线): β* vs slope → 判常速物理 β∝1/v
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')
S = src[:, 200:264]

def kirch(X, beta, half_ap):
    nr, nc = X.shape
    Y = np.zeros((nr, nc))
    for tau in range(1, nr):
        for x0 in range(nc):
            x_lo = max(0, x0-half_ap); x_hi = min(nc-1, x0+half_ap)
            xs = np.arange(x_lo, x_hi+1)
            dt = np.sqrt(tau*tau + (beta*(xs-x0))**2)
            i0 = np.clip(dt.astype(int), 0, nr-2)
            fr = dt - i0
            Y[tau, x0] = (X[i0, xs]*(1-fr) + X[i0+1, xs]*fr).sum()
    return Y

rows = slice(2, 500)
CASES = [('P_5', 31, 0.029277), ('P_L', 31, 0.056600), ('P_M', 63, 0.056600)]
for n, ap, slope in CASES:
    R = load(BASE + '/Proc/1103_010 %s.DZT' % n)[:, 200:264]
    best = (0, 0)
    for beta in np.arange(4.0, 26.1, 1.0):
        Y = kirch(S, beta, ap)
        c = np.corrcoef(Y[rows].ravel(), R[rows].ravel())[0,1]
        if c > best[0]: best = (c, beta)
    print('%s(斜率%.4f): 最优β=%.1f corr=%.5f  (slope×β=%.3f)' % (
        n, slope, best[1], best[0], slope*best[1]))
