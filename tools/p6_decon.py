# -*- coding: utf-8 -*-
"""P_6 预测反褶积: proc解码 + Wiener预测误差滤波模型网格
proc期望: 1e 1f 05 f32(10.0) = typeId30 sub=31(算子长) u8=5(滞后) f32=10(白化%)
"""
import struct
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

# 1) proc + 哨兵
p = BASE + '/Proc/1103_010 P_6.DZT'
h = open(p, 'rb').read(1024)
off, = struct.unpack('<h', h[48:50]); sz, = struct.unpack('<h', h[50:52])
blob = h[off:off+sz]
print('P_6 proc:', blob.hex(' '))
R = load(p)
print('行1哨兵数:', int((R[1] == -16777216).sum()), ' 行0前3:', R[0, :3])

src = load(BASE + '/1103_010.DZT')
S = src[:, 200:264]; Rr = R[:, 200:264]
print('基线corr = %.4f' % np.corrcoef(S[2:500].ravel(), Rr[2:500].ravel())[0,1])

# 2) 预测反褶积模型
def decon(X, opLen, lag, whiten, lag_in, err_form, acorr_norm):
    """err_form: 'conv'=预测误差滤波器全长卷积(lag+opLen); 'resid'=逐点残差"""
    nr, nc = X.shape
    Y = np.zeros_like(X)
    for c in range(nc):
        x = X[:, c].astype(np.float64)
        n = len(x)
        # 自相关 0..lag+opLen
        L = lag + opLen
        r = np.array([np.dot(x[:n-k], x[k:]) / max(1, n-k) if acorr_norm == 'short'
                      else np.dot(x[:n-k], x[k:]) / n for k in range(L + 1)])
        r0w = r[0] * (1.0 + whiten)          # 白化: r0*(1+w)
        # Toeplitz: 求 f (opLen) 使 Σf[i]r[|i-j|] = r[lag+j]
        A = np.zeros((opLen, opLen))
        for i in range(opLen):
            for j in range(opLen):
                A[i, j] = r[abs(i - j)]
        b = np.array([r[lag + j] for j in range(opLen)])
        if r0w != r[0]:
            A[range(opLen), range(opLen)] += r0w - r[0]
        try:
            f = np.linalg.solve(A, b)
        except np.linalg.LinAlgError:
            f = np.zeros(opLen)
        # 预测误差滤波器: e[n] = x[n] - Σ f[i] x[n-lag-i]
        pe = np.zeros(lag + opLen); pe[0] = 1.0
        for i in range(opLen):
            pe[lag + i] -= f[i]
        Y[:, c] = np.convolve(x, pe)[:n] if err_form == 'conv' else _resid(x, f, lag)
    return Y

def _resid(x, f, lag):
    n = len(x); e = np.zeros(n); opLen = len(f)
    for k in range(n):
        s = 0.0
        for i in range(opLen):
            if k - lag - i >= 0:
                s += f[i] * x[k - lag - i]
        e[k] = x[k] - s
    return e

rows = slice(2, 500)
res = []
for lag_in in (0, 2):
    X = np.vstack([S[lag_in:], np.zeros((lag_in, S.shape[1]))]) if lag_in else S
    for whiten in (0.10,):
        for acorr_norm in ('short', 'full'):
            for err_form in ('conv', 'resid'):
                Y = decon(X, 31, 5, whiten, lag_in, err_form, acorr_norm)
                c = np.corrcoef(Y[rows].ravel(), Rr[rows].ravel())[0,1]
                res.append((c, lag_in, whiten, acorr_norm, err_form))
res.sort(reverse=True)
for r in res:
    print('corr=%.5f lag=%d whiten=%.2f acorr=%s err=%s' % r)
