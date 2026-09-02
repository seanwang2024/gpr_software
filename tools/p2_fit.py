# -*- coding: utf-8 -*-
"""P_2 模型=时间零点上移2行 + IIR LP800/HP256 (行1哨兵/行0原值)
网格: lag{0,2} × α公式 × HP形式 × 级联顺序 × 初始条件, 在行5..500(避边界)评corr
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
    return {'exp': 1-np.exp(-x), 'lin': x, 'bil': x/(1+x), 'bil2': x/(2+x), 'sqrt': np.sqrt(x)}[mode]

def lp_all(X, a, y0):
    Y = np.empty_like(X)
    Y[0] = X[0] if y0=='x0' else 0.0
    for n in range(1, X.shape[0]):
        Y[n] = Y[n-1] + a*(X[n]-Y[n-1])
    return Y

def run(lag, mode, hpform, order, y0):
    aLP, aHP = alpha(mode, 800e6), alpha(mode, 256e6)
    X = S.copy()
    if lag:                      # 先上移 lag 行(底部清零)
        X = np.vstack([X[lag:], np.zeros((lag, X.shape[1]))])
    if order == 'lp_hp':
        X = lp_all(X, aLP, y0)
        X = X - lp_all(X, aHP, y0) if hpform=='diff' else lp_all(X, aHP, y0)*0  # 占位
    return X

# 重写清晰版
def filt(X, mode, hpform, order, y0):
    aLP, aHP = alpha(mode, 800e6), alpha(mode, 256e6)
    def LP(x): return lp_all(x, aLP, y0)
    def HP(x):
        if hpform == 'diff':
            return x - lp_all(x, aHP, y0)
        Y = np.empty_like(x)
        Y[0] = x[0] if y0=='x0' else 0.0
        for n in range(1, x.shape[0]):
            Y[n] = Y[n-1] + aHP*(x[n]-x[n-1])
        return Y
    return HP(LP(X)) if order=='lp_hp' else LP(HP(X))

rows = slice(6, 500)   # 评估区: 避开顶部哨兵/边缘与底部清零
res = []
for lag in (0, 2):
    Xs = np.vstack([S[lag:], np.zeros((lag, S.shape[1]))]) if lag else S
    for mode in ('exp','lin','bil','bil2','sqrt'):
        for hpform in ('diff','df'):
            for order in ('lp_hp','hp_lp'):
                for y0 in ('x0','zero'):
                    out = filt(Xs, mode, hpform, order, y0)
                    c = np.corrcoef(out[rows].ravel(), R[rows].ravel())[0,1]
                    res.append((c, lag, mode, hpform, order, y0))
res.sort(reverse=True)
for r in res[:10]:
    print('corr=%.6f lag=%d mode=%-4s hp=%-4s order=%-5s y0=%s' % r)
