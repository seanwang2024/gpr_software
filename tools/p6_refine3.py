# -*- coding: utf-8 -*-
"""P_6 精修3: lag扫描 × {线性自相关, 循环自相关} × 头部处理
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
OP = 31

def acorr_linear(x, L):
    n = len(x)
    return np.array([np.dot(x[:n-k], x[k:]) / n for k in range(L + 1)])

def acorr_circ(x, L):
    n = len(x)
    F = np.fft.rfft(x)
    ps = (F*F.conj()).real
    full = np.fft.irfft(ps, n)
    return full[:L+1] / n

def run(lag, acorr, whiten, head):
    def one(x):
        L = lag + OP
        r = acorr(x, L)
        r = r.copy(); r[0] *= (1.0 + whiten)
        A = np.empty((OP, OP))
        for i in range(OP):
            for j in range(OP):
                A[i, j] = r[abs(i - j)]
        b = np.array([r[lag + j] for j in range(OP)])
        try:
            f = np.linalg.solve(A, b)
        except np.linalg.LinAlgError:
            f = np.zeros(OP)
        n = len(x); e = np.zeros(n)
        for k in range(n):
            s = 0.0
            for i in range(OP):
                if k - lag - i >= 0:
                    s += f[i] * x[k - lag - i]
            e[k] = x[k] - s
        if head == 'zero':
            e[:lag + OP] = 0.0
        elif head == 'pass':
            e[:lag] = x[:lag]
        return e
    Y = np.column_stack([one(S[:, c]) for c in range(S.shape[1])])
    return np.corrcoef(Y[2:500].ravel(), R[2:500].ravel())[0,1]

print('lag  线性      循环     (循环+头置零)')
for lag in (1, 2, 3, 4, 5, 6, 8):
    c1 = run(lag, acorr_linear, 0.10, 'keep')
    c2 = run(lag, acorr_circ, 0.10, 'keep')
    c3 = run(lag, acorr_circ, 0.10, 'zero')
    print('%2d   %.5f  %.5f  %.5f' % (lag, c1, c2, c3))
