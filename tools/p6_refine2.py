# -*- coding: utf-8 -*-
"""P_6 反褶积精修2: whiten扫描 × {逐道f, 全局平均f} × 去均值
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
OP, LAG = 31, 5

def acorr(x, L):
    n = len(x)
    return np.array([np.dot(x[:n-k], x[k:]) / n for k in range(L + 1)])

def solve_f(r, whiten):
    r = r.copy()
    r[0] *= (1.0 + whiten)
    A = np.empty((OP, OP))
    for i in range(OP):
        for j in range(OP):
            A[i, j] = r[abs(i - j)]
    b = np.array([r[LAG + j] for j in range(OP)])
    try:
        return np.linalg.solve(A, b)
    except np.linalg.LinAlgError:
        return np.zeros(OP)

def apply_pe(x, f):
    n = len(x); e = np.zeros(n)
    for k in range(n):
        s = 0.0
        for i in range(OP):
            if k - LAG - i >= 0:
                s += f[i] * x[k - LAG - i]
        e[k] = x[k] - s
    return e

rows = slice(2, 500)
print('%-8s %-8s %-8s %s' % ('whiten', '逐道f', '全局f', '(去均值版)'))
for whiten in (0.001, 0.01, 0.05, 0.1, 0.2, 0.5, 1.0):
    # 逐道
    Y1 = np.column_stack([apply_pe(S[:, c], solve_f(acorr(S[:, c], LAG+OP), whiten))
                          for c in range(S.shape[1])])
    c1 = np.corrcoef(Y1[rows].ravel(), R[rows].ravel())[0,1]
    # 全局平均自相关 → 单一 f
    rall = np.mean([acorr(S[:, c], LAG+OP) for c in range(S.shape[1])], axis=0)
    f = solve_f(rall, whiten)
    Y2 = np.column_stack([apply_pe(S[:, c], f) for c in range(S.shape[1])])
    c2 = np.corrcoef(Y2[rows].ravel(), R[rows].ravel())[0,1]
    # 逐道去均值再逐道f
    Sm = S - S.mean(axis=0, keepdims=True)
    Y3 = np.column_stack([apply_pe(Sm[:, c], solve_f(acorr(Sm[:, c], LAG+OP), whiten))
                          for c in range(Sm.shape[1])])
    c3 = np.corrcoef(Y3[rows].ravel(), R[rows].ravel())[0,1]
    print('%-8.3f %-14.5f %-14.5f %.5f' % (whiten, c1, c2, c3))
