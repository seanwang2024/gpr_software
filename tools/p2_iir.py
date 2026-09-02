# -*- coding: utf-8 -*-
"""P_2 IIR带通逆向: 1极点 LP(800MHz)+HP(256MHz) 级联, 对标 Proc/1103_010 P_2.DZT
fs = 512采样/20ns = 25.6 GS/s
记录: typeId4 sub1 c=15.9155(=fs/(2pi*256M)), typeId3 sub1 c=5.0857(=fs/(2pi*800M))
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)   # [512, nt]

src = load(BASE + '/1103_010.DZT')
ref = load(BASE + '/Proc/1103_010 P_2.DZT')
print('shape', src.shape, ref.shape)
fs = NS / 20e-9

def alpha(mode, fc):
    x = 2*np.pi*fc/fs
    return {'exp': 1-np.exp(-x), 'lin': x, 'bil': x/(1+x), 'bil2': x/(2+x)}[mode]

def lp(x, a, y0='x0'):
    y = np.empty_like(x)
    y[0] = x[0] if y0=='x0' else 0.0
    for n in range(1, len(x)):
        y[n] = y[n-1] + a*(x[n]-y[n-1])
    return y

def hp_diff(x, a, y0='x0'):        # HP = x - LP(x)
    return x - lp(x, a, y0)

def hp_df(x, a, y0='x0'):          # HP = a*(y[n-1] + x[n] - x[n-1])
    y = np.empty_like(x)
    y[0] = x[0] if y0=='x0' else 0.0
    for n in range(1, len(x)):
        y[n] = y[n-1] + a*(x[n]-x[n-1])
    return y

def score(ours):
    d = ours - ref
    corr = np.corrcoef(ours.ravel(), ref.ravel())[0,1]
    return 'corr=%.6f MAE=%.1f max=%d' % (corr, np.abs(d).mean(), np.abs(d).max())

# 逐道太慢 → 先用前64道做算法识别
S = src[:, :64]; R = ref[:, :64]

results = []
for mode in ('exp','lin','bil','bil2'):
    for hpform in ('diff','df'):
        for order in ('lp_hp','hp_lp'):
            for y0 in ('x0','zero'):
                aLP = alpha(mode, 800e6); aHP = alpha(mode, 256e6)
                if order == 'lp_hp':
                    out = lp(S, aLP, y0); out = (hp_diff if hpform=='diff' else hp_df)(out, aHP, y0)
                else:
                    out = (hp_diff if hpform=='diff' else hp_df)(S, aHP, y0); out = lp(out, aLP, y0)
                c = np.corrcoef(out.ravel(), R.ravel())[0,1]
                results.append((c, mode, hpform, order, y0, aLP, aHP))

results.sort(reverse=True)
for r in results[:8]:
    print('corr=%.6f  mode=%-4s hp=%-4s order=%-5s y0=%-4s aLP=%.6f aHP=%.6f' % r)
print('--- baseline(原始 vs P_2):', score(S))
