# -*- coding: utf-8 -*-
"""克西霍夫 v3 物理形状: dt^2 = tau^2 + (gamma*dx/tau)^2 (v_rms∝tau 的正确双曲线)
P_M(宽127)解码 + 三样本(P_5/P_L/P_M)联合gamma拟合
"""
import struct
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

# P_M proc
p = BASE + '/Proc/1103_010 P_M.DZT'
h = open(p, 'rb').read(1024)
off, = struct.unpack('<h', h[48:50]); sz, = struct.unpack('<h', h[50:52])
blob = h[off:off+sz]
print('P_M proc:', blob.hex(' '))
f8, = struct.unpack('<f', blob[8:12])
print('  斜率f32=%.6f ×512=%.2f  sub=宽度=%d' % (f8, f8*512, blob[7]))

src = load(BASE + '/1103_010.DZT')
S = src[:, 200:264]

def kirch_phys(X, gamma, half_ap):
    nr, nc = X.shape
    Y = np.zeros((nr, nc))
    for tau in range(1, nr):
        for x0 in range(nc):
            x_lo = max(0, x0-half_ap); x_hi = min(nc-1, x0+half_ap)
            xs = np.arange(x_lo, x_hi+1)
            dt = np.sqrt(tau*tau + (gamma*(xs-x0)/tau)**2)
            i0 = np.clip(dt.astype(int), 0, nr-2)
            fr = dt - i0
            v = X[i0, xs]*(1-fr) + X[i0+1, xs]*fr
            Y[tau, x0] = v.sum()
    return Y

rows = slice(2, 500)
CASES = [('P_5', 31), ('P_L', 31), ('P_M', 63)]   # (名, 半孔径=宽度/2)
REFS = {n: load(BASE + '/Proc/1103_010 %s.DZT' % n)[:, 200:264] for n, _ in CASES}

best = []
for gamma in (3.0, 5.0, 7.0, 9.0, 12.0, 16.0, 22.0, 30.0):
    cs = []
    for n, ap in CASES:
        Y = kirch_phys(S, gamma, ap)
        cs.append(np.corrcoef(Y[rows].ravel(), REFS[n][rows].ravel())[0,1])
    print('gamma=%5.1f  P_5=%.4f P_L=%.4f P_M=%.4f' % (gamma, *cs))
    best.append((np.mean(cs), gamma, cs))
best.sort(reverse=True)
g0 = best[0][1]
for g in np.arange(g0*0.6, g0*1.45, g0*0.08):
    cs = []
    for n, ap in CASES:
        Y = kirch_phys(S, g, ap)
        cs.append(np.corrcoef(Y[rows].ravel(), REFS[n][rows].ravel())[0,1])
    print('细: gamma=%6.2f  P_5=%.4f P_L=%.4f P_M=%.4f' % (g, *cs))
