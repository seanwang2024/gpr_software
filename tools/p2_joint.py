# -*- coding: utf-8 -*-
"""双样本联合拟合: P_2+P_I 同时最优的 α=k/c^p 及实现形式; P_J 验证极点级联
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
X = np.vstack([S96[2:], np.zeros((2, 96))])       # lag2

CASES = [  # (name, cHP, cLP, npole)
    ('P_2', 15.9155, 5.0857, 1),
    ('P_I', 13.5812, 5.8205, 1),
    ('P_J', 10.1859, 5.0930, 4),
]
REFS = {n: load(BASE + '/Proc/1103_010 %s.DZT' % n)[:, :96] for n, *_ in CASES}

def lp(Xv, a):
    Y = np.empty_like(Xv); Y[0] = 0.0
    for n in range(1, Xv.shape[0]):
        Y[n] = Y[n-1] + a*(Xv[n]-Y[n-1])
    return Y

def fwdbwd(Xv, a):
    return lp(lp(Xv[::-1], a)[::-1], a)

def bandpass(Xv, cHP, cLP, k, p, npole, bidir):
    aL = k / (cLP ** p)
    aH = k / (cHP ** p)
    out = Xv
    for _ in range(npole):
        if bidir: out = fwdbwd(out, aL)
        else:     out = lp(out, aL)
    for _ in range(npole):
        if bidir: out = out - fwdbwd(out, aH)
        else:     out = out - lp(out, aH)
    return out

rows = slice(4, 500)
print('%-6s %-10s %-22s %-22s %-22s' % ('(k,p)', '形式', 'P_2 corr', 'P_I corr', 'P_J corr(级联4)'))
best = []
for p in (0.5, 1.0, 1.5):
    for k in ([0.25, 0.5, 1.0] if p != 1.0 else [0.03, 0.06, 0.1, 0.15]):
        for bidir in (False, True):
            cs = []
            for name, cH, cL, np_ in CASES:
                out = bandpass(X, cH, cL, k, p, np_, bidir)
                cs.append(np.corrcoef(out[rows].ravel(), REFS[name][rows].ravel())[0,1])
            best.append((sum(cs[:2])/2, min(cs[:2]), k, p, bidir, cs))
best.sort(key=lambda t: -t[1])
for b in best[:8]:
    print('k=%-5s p=%-3s %-8s  P_2=%.5f P_I=%.5f P_J=%.5f' % (
        b[2], b[3], '双向' if b[4] else '单向', b[5][0], b[5][1], b[5][2]))
