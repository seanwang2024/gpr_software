# -*- coding: utf-8 -*-
"""P_6 反褶积精修: 白化形式/预测方向/Toeplitz约定/输出形式
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')
ref = load(BASE + '/Proc/1103_010 P_6.DZT')
S = src[:, 200:264]; R = ref[:, 200:264]
OP, LAG, WH = 31, 5, 0.10

def decon_trace(x, whiten_form, pred_dir, biased):
    n = len(x)
    L = LAG + OP
    if biased:
        r = np.array([np.dot(x[:n-k], x[k:]) / n for k in range(L + 1)])
    else:
        r = np.array([np.dot(x[:n-k], x[k:]) / max(1, n-k) for k in range(L + 1)])
    r0 = r[0]
    if whiten_form == 'mul':      r[0] = r0 * (1 + WH)
    elif whiten_form == 'add':    r[0] = r0 + WH * r0
    elif whiten_form == 'lam':    r[0] = r0 * (1 + WH)       # same as mul
    elif whiten_form == 'trace':  r[0] = r0 + WH * (r[1] if len(r) > 1 else r0)
    A = np.empty((OP, OP))
    for i in range(OP):
        for j in range(OP):
            A[i, j] = r[abs(i - j)]
    b = np.array([r[LAG + j] for j in range(OP)])
    try:
        f = np.linalg.solve(A, b)
    except np.linalg.LinAlgError:
        f = np.zeros(OP)
    # 预测误差: e[k] = x[k] - sum f[i]*x[k-LAG-i](前向) 或 x[k+LAG+i](后向)
    e = np.zeros(n)
    if pred_dir == 'fwd':
        for k in range(n):
            s = 0.0
            for i in range(OP):
                if k - LAG - i >= 0:
                    s += f[i] * x[k - LAG - i]
            e[k] = x[k] - s
    else:  # 'sym'/'bwd': 用 x[k+LAG+i]
        for k in range(n):
            s = 0.0
            for i in range(OP):
                if k + LAG + i < n:
                    s += f[i] * x[k + LAG + i]
            e[k] = x[k] - s
    return e

rows = slice(2, 500)
res = []
for whiten_form in ('mul', 'trace'):
    for pred_dir in ('fwd', 'bwd'):
        for biased in (False, True):
            Y = np.column_stack([decon_trace(S[:, c], whiten_form, pred_dir, biased)
                                 for c in range(S.shape[1])])
            c0 = np.corrcoef(Y[rows].ravel(), R[rows].ravel())[0,1]
            # 最优增益
            o, rr = Y[rows].ravel(), R[rows].ravel()
            g = (o*rr).sum()/(o*o).sum()
            res.append((c0, whiten_form, pred_dir, biased, g))
res.sort(reverse=True)
for r in res[:8]:
    print('corr=%.5f whiten=%-5s dir=%-3s biased=%d g=%.3f' % r)
