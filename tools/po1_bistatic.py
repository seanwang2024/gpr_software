# -*- coding: utf-8 -*-
"""P_O1(收发距0.2m) — 双偏移距双曲线模型:
t(dx) = 0.5*[sqrt(tau^2+(2(dx-s/2)/v)^2) + sqrt(tau^2+(2(dx+s/2)/v)^2)]
"""
import struct
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000
RNG_NS = 20.0
DX = 0.10          # 道间距(扫描/单位100mm)

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

# P_O1 proc + DZX bistaticSep
p = BASE + '/Proc/1103_010 P_O1.DZT'
h = open(p, 'rb').read(1024)
off, = struct.unpack('<h', h[48:50]); sz, = struct.unpack('<h', h[50:52])
blob = h[off:off+sz]
print('P_O1 proc:', blob.hex(' '))
import re
dzx = open(BASE + '/Proc/1103_010 P_O1.DZX', encoding='utf-8', errors='replace').read()
m = re.search(r'<bistaticSep>([^<]+)', dzx)
print('DZX bistaticSep =', m.group(1) if m else 'N/A')

src = load(BASE + '/1103_010.DZT')
R1 = load(p); RO = load(BASE + '/Proc/1103_010 P_O.DZT')
S = src[:, 200:264]
print('corr P_O1 vs P_O = %.4f (变化幅度)' % np.corrcoef(R1[2:500,200:264].ravel(), RO[2:500,200:264].ravel())[0,1])

# vlc速度
PTS = [(11.282, 7.449), (13.960, 6.1447), (16.326, 4.2738), (18.070, 2.4595)]
ts = np.array([0.0]+[p[0] for p in PTS]+[40.0]); vs = np.array([PTS[0][1]]+[p[1] for p in PTS]+[PTS[-1][1]])
tt = np.arange(NS)*RNG_NS/NS
v = np.interp(tt, ts, vs)
vr = np.sqrt(np.cumsum(v*v)/np.arange(1, NS+1))   # cm/ns

def kirch_bista(X, s_m, half_ap, dx_m=DX):
    nr, nc = X.shape
    Y = np.zeros((nr, nc))
    dtns = RNG_NS/NS
    for tau in range(1, nr):
        v_cm = vr[tau]
        for x0 in range(nc):
            lo, hi = max(0, x0-half_ap), min(nc-1, x0+half_ap)
            xs = np.arange(lo, hi+1)
            dxm = (xs-x0)*dx_m
            k = 2.0/(v_cm/100.0)/dtns             # (2d/v) 样本换算系数
            t1 = np.sqrt(tau*tau + (k*(dxm - s_m/2))**2)
            t2 = np.sqrt(tau*tau + (k*(dxm + s_m/2))**2)
            dt = 0.5*(t1+t2)
            i0 = np.clip(dt.astype(int), 0, nr-2); fr = dt-i0
            Y[tau, x0] = (X[i0, xs]*(1-fr) + X[i0+1, xs]*fr).sum()
    return Y

rows = slice(2, 500)
R1s = R1[:, 200:264]
for s in (0.0, 0.1, 0.2, 0.3):
    Y = kirch_bista(S, s, 31)
    c = np.corrcoef(Y[rows].ravel(), R1s[rows].ravel())[0,1]
    print('收发距 s=%.1fm 模型 vs P_O1 corr=%.5f' % (s, c))
# P_O 复核(s=0 应≈0.904)
Y0 = kirch_bista(S, 0.0, 31)
print('s=0 vs P_O corr=%.5f (复核)' % np.corrcoef(Y0[rows].ravel(), RO[rows and slice(2,500), 200:264][2:500].ravel())[0,1])
