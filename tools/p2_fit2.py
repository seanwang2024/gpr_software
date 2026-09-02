# -*- coding: utf-8 -*-
"""P_2 修正版: 正确1极点HP差分形式 y[n]=a*(y[n-1]+x[n]-x[n-1]); 重新网格扫描
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
S = src[:, :64]; R = ref[:, :64]

def alpha(mode, fc):
    x = 2*np.pi*fc/fs
    return {'exp': 1-np.exp(-x), 'lin': x, 'bil': x/(1+x), 'sqrt': np.sqrt(x),
            'sqrtc': np.sqrt(x/(1+x))}[mode]

def lp_all(X, a, y0):
    Y = np.empty_like(X)
    Y[0] = X[0] if y0=='x0' else 0.0
    for n in range(1, X.shape[0]):
        Y[n] = Y[n-1] + a*(X[n]-Y[n-1])
    return Y

def hp_correct(X, a, y0):
    Y = np.empty_like(X)
    Y[0] = a*X[0] if y0=='zero' else X[0]
    for n in range(1, X.shape[0]):
        Y[n] = a*(Y[n-1] + X[n]-X[n-1])
    return Y

def hp_diff(X, a, y0):
    return X - lp_all(X, a, y0)

rows = slice(6, 500)
res = []
for lag in (0, 2):
    Xs = np.vstack([S[lag:], np.zeros((lag, S.shape[1]))]) if lag else S
    for mode in ('exp','lin','bil','sqrt','sqrtc'):
        for hpform in ('correct','diff'):
            for order in ('lp_hp','hp_lp'):
                for y0 in ('x0','zero'):
                    aLP, aHP = alpha(mode, 800e6), alpha(mode, 256e6)
                    if order == 'lp_hp':
                        out = hp_correct(lp_all(Xs, aLP, y0), aHP, y0) if hpform=='correct' \
                              else hp_diff(lp_all(Xs, aLP, y0), aHP, y0)
                    else:
                        mid = hp_correct(Xs, aHP, y0) if hpform=='correct' else hp_diff(Xs, aHP, y0)
                        out = lp_all(mid, aLP, y0)
                    c = np.corrcoef(out[rows].ravel(), R[rows].ravel())[0,1]
                    res.append((c, lag, mode, hpform, order, y0))
res.sort(reverse=True)
for r in res[:10]:
    print('corr=%.6f lag=%d mode=%-5s hp=%-7s order=%-5s y0=%s' % r)
