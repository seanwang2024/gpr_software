# -*- coding: utf-8 -*-
"""P_L/P_5 克西霍夫 v2: v(z)=slope*z (slope=proc f32), v_rms=slope*tau/sqrt(3)
t = tau*sqrt(1+(beta*dx/tau)^2), 扫beta×权重×孔径×lag; P_L标定→P_5验证
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')
RL = load(BASE + '/Proc/1103_010 P_L.DZT')
R5 = load(BASE + '/Proc/1103_010 P_5.DZT')
S = src[:, 200:264]

def kirch_vz(X, beta, weight, half_ap, slope_rel):
    """slope_rel: v_rms(τ)=slope*τ*rel  (rel=1/sqrt3 等)"""
    nr, nc = X.shape
    Y = np.zeros((nr, nc))
    for tau in range(1, nr):
        for x0 in range(nc):
            x_lo = max(0, x0-half_ap); x_hi = min(nc-1, x0+half_ap)
            xs = np.arange(x_lo, x_hi+1)
            dt = tau*np.sqrt(1.0 + (beta*(xs-x0)/tau)**2)
            ti = dt  # 非整数 → 线性插值
            i0 = np.clip(ti.astype(int), 0, nr-2)
            fr = ti - i0
            v = X[i0, xs]*(1-fr) + X[i0+1, xs]*fr
            r = tau/np.maximum(dt, 1e-9)
            w = {'one':1.0,'taut':r,'sqrt':np.sqrt(r),'t32':r**1.5,'sq':r*r}[weight]
            Y[tau, x0] = (v*w).sum()
    return Y

rows = slice(2, 500)
RLs = RL[:, 200:264]; R5s = R5[:, 200:264]
res = []
for beta in (0.05, 0.1, 0.2, 0.3, 0.5, 0.8, 1.2, 2.0):
    for weight in ('one', 'taut', 'sqrt', 't32'):
        Y = kirch_vz(S, beta, weight, 31, 1/np.sqrt(3))
        c = np.corrcoef(Y[rows].ravel(), RLs[rows].ravel())[0,1]
        res.append((c, beta, weight))
res.sort(reverse=True)
for r in res[:8]:
    print('corr=%.5f beta=%.2f w=%s' % r)
