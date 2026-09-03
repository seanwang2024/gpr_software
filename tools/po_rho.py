# -*- coding: utf-8 -*-
"""Kirchhoff精化(手册三要素): 预计算双曲线采样, 网格扫 斜坡因子p × Rho滤波q(修正相位) × 平均
P_O主标定, P_M/P_L复核
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000
RNG_NS = 20.0
DX = 0.10

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')
S = src[:, 200:264]
REFS = {n: load(BASE + '/Proc/1103_010 %s.DZT' % n)[:, 200:264] for n in ('P_O', 'P_M', 'P_L')}

# vlc速度(v_rms)
PTS = [(11.282, 74.487), (13.960, 61.447), (16.326, 42.738), (18.070, 24.595)]
ts = np.array([0.0] + [p[0] for p in PTS] + [40.0])
vs = np.array([PTS[0][1]] + [p[1] for p in PTS] + [PTS[-1][1]])
tt = np.arange(NS) * RNG_NS / NS
v = np.interp(tt, ts, vs)
vr = np.sqrt(np.cumsum(v*v) / np.arange(1, NS+1))

# ---- 预计算: 每个(tau,x0)的采样值val与dt ----
HALF_AP = 31
nr, nc = S.shape
VALS = [[None]*nc for _ in range(nr)]
DT = [[None]*nc for _ in range(nr)]
dtns = RNG_NS / NS
for tau in range(1, nr):
    k = 2.0 * DX * 100.0 / max(vr[tau] * dtns, 1e-9)
    for x0 in range(nc):
        lo, hi = max(0, x0-HALF_AP), min(nc-1, x0+HALF_AP)
        xs = np.arange(lo, hi+1)
        dt = np.sqrt(tau*tau + (k*(xs-x0))**2)
        i0 = np.clip(dt.astype(int), 0, nr-2); fr = dt - i0
        VALS[tau][x0] = S[i0, xs]*(1-fr) + S[i0+1, xs]*fr
        DT[tau][x0] = dt

freq = np.fft.rfftfreq(nr)
def rho_filter(Y, q):
    """(iw)^q 修正版: |w|^q e^{i q (pi/2) sign(w)}"""
    if q == 0: return Y
    F = np.fft.rfft(Y, axis=0)
    W = np.zeros(len(freq), complex)
    W[1:] = freq[1:]**q * np.exp(1j * q * np.pi/2)     # 正频率: 相位+q·90°
    W[0] = 0
    Wm = np.conj(W[1:])[::-1]                            # 负频率共轭对称
    full = np.concatenate([W, Wm])
    return np.fft.ifft(np.fft.fft(Y, axis=0)*full[:nr][:, None], axis=0).real

def build(p, avg):
    Y = np.zeros((nr, nc))
    for tau in range(1, nr):
        for x0 in range(nc):
            dt = DT[tau][x0]; val = VALS[tau][x0]
            w = (tau/np.maximum(dt, 1e-9))**p if p else 1.0
            Y[tau, x0] = (val*w).mean() if avg else (val*w).sum()
    return Y

rows = slice(2, 500)
res = []
for p in (0.0, 0.5, 1.0, 1.5):
    Y0 = build(p, True)
    for q in (0.0, 0.25, 0.5, 1.0):
        Y = rho_filter(Y0, q)
        cO = np.corrcoef(Y[rows].ravel(), REFS['P_O'][rows].ravel())[0,1]
        cM = np.corrcoef(Y[rows].ravel(), REFS['P_M'][rows].ravel())[0,1]
        res.append((cO, cM, p, q))
res.sort(reverse=True)
for r in res[:10]:
    print('P_O=%.5f P_M=%.5f  p=%.1f q=%.2f' % r)
