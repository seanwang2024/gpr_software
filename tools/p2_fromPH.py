# -*- coding: utf-8 -*-
"""P_2 = IIR(P_H)? 用RADAN自己的P_H全列做输入
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

ph  = load(BASE + '/Proc/1103_010 P_H.DZT')
ref = load(BASE + '/Proc/1103_010 P_2.DZT')
fs = NS / 20e-9
R = ref[:, :96]
X = ph[:, :96]

def alpha(mode, fc):
    x = 2*np.pi*fc/fs
    return {'exp': 1-np.exp(-x), 'bil': x/(1+x)}[mode]

def lp(Xv, a, y0=0.0):
    Y = np.empty_like(Xv); Y[0] = y0
    for n in range(1, Xv.shape[0]):
        Y[n] = Y[n-1] + a*(Xv[n]-Y[n-1])
    return Y

def fwdbwd(Xv, a):
    return lp(lp(Xv[::-1], a)[::-1], a)

res = []
for mode in ('exp', 'bil'):
    aL, aH = alpha(mode, 800e6), alpha(mode, 256e6)
    L = lp(X, aL)
    H = X - lp(X, aH)
    cand = {
        '单LP→单HP': L - lp(L, aH),
        '单HP→单LP': lp(H, aL),
        '双LP→双HP': fwdbwd(X, aL) - fwdbwd(fwdbwd(X, aL), aH),
    }
    for name, out in cand.items():
        c = np.corrcoef(out[4:500].ravel(), R[4:500].ravel())[0,1]
        res.append((c, mode, name))
res.sort(reverse=True)
for r in res[:6]:
    print('corr=%.6f mode=%s %s' % r)
print('基线 corr(P_H vs P_2) = %.6f' % np.corrcoef(X[4:500].ravel(), R[4:500].ravel())[0,1])
