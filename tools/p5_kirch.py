# -*- coding: utf-8 -*-
"""P_5 克西霍夫模型网格: y[x0,tau] = sum_x w(tau,t) * in[x,t], t=sqrt(tau^2+((x-x0)/K)^2)
子集64道(200-263, 远离边缘), 评估区行2-500
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')
ref = load(BASE + '/Proc/1103_010 P_5.DZT')

T0, T1 = 200, 264          # 测试道窗(避开边缘, 孔径63→x0±31 内道取 231-232 才完整..放宽用整窗评估内部)
S = src[:, T0-40:T1+40]    # 输入带孔径余量
R = ref[:, T0:T1]

def kirch(X, K, weight, half_ap, lag):
    if lag:
        X = np.vstack([X[lag:], np.zeros((lag, X.shape[1]))])
    nr, nc = X.shape
    Y = np.zeros((nr, nc))
    for tau in range(nr):
        # 预计算每个 x0 的双曲线
        for x0 in range(nc):
            x_lo = max(0, x0 - half_ap); x_hi = min(nc - 1, x0 + half_ap)
            xs = np.arange(x_lo, x_hi + 1)
            dt = np.sqrt(tau**2 + ((xs - x0) / K) ** 2)
            ti = np.clip(np.round(dt).astype(int), 0, nr - 1)
            w = np.ones_like(dt)
            if weight == 'taut':
                w = np.where(dt > 0, tau / np.maximum(dt, 1e-9), 1.0)
            elif weight == 'sqrt':
                w = np.where(dt > 0, np.sqrt(tau / np.maximum(dt, 1e-9)), 1.0)
            elif weight == 't32':
                w = np.where(dt > 0, (tau / np.maximum(dt, 1e-9)) ** 1.5, 1.0)
            Y[tau, x0] = (X[ti, xs] * w).sum()
    return Y

rows = slice(2, 500)
res = []
for lag in (0, 2):
    for K in (16, 24, 32, 48, 64, 96):
        for weight in ('one', 'taut', 'sqrt', 't32'):
            for ap in (31,):
                Y = kirch(S, K, weight, ap, lag)
                c = np.corrcoef(Y[rows, 40:-40].ravel(), R[rows].ravel())[0,1]
                res.append((c, K, weight, ap, lag))
res.sort(reverse=True)
for r in res[:10]:
    print('corr=%.5f K=%2d w=%-5s 半孔=%d lag=%d' % r)
