# -*- coding: utf-8 -*-
"""P_O: vlc速度场(拾取点插值)克西霍夫 — 字段语义网格
vlc组: (a, b, c, d, e) = (11.282,251,7.449,7.449,0.636)(13.960,311,61.447,0,0.606)...
候选: 时间列∈{a,b}, 速度列∈{c,d}(cm/ns或mm/ns), 单位标定
v(t)分段线性 → v_rms(τ) → dt²=τ²+(2Δx·dx_m/v_rms)² 求和
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000
RNG_NS = 20.0

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')
RO = load(BASE + '/Proc/1103_010 P_O.DZT')
S = src[:, 200:264]; R = RO[:, 200:264]

# vlc 拾取点(去掉前导0,0,0,5,0; 第1组c=d=7.449 视为cm对, 第2组74.487为mm对→同一拾取点)
PTS_CM = [(11.282, 7.449), (13.960, 6.1447), (16.326, 4.2738), (18.070, 2.4595)]
# (组b字段, 组c/d均值换cm/ns): 74.487mm=7.4487cm, 61.447mm=6.1447cm...

def vrms_of(tcol, pts_ns_cm):
    """按拾取点(时间ns, 速度cm/ns)分段线性 → v_rms(τ)样本表(单位cm/ns)"""
    ts = np.array([0.0] + [p[0] for p in pts_ns_cm] + [RNG_NS*2])
    vs = np.array([pts_ns_cm[0][1]] + [p[1] for p in pts_ns_cm] + [pts_ns_cm[-1][1]])
    tt = np.arange(NS) * RNG_NS / NS
    v = np.interp(tt, ts, vs)                      # v(t) m? cm/ns
    # v_rms(τ) = sqrt(mean(v(t)^2, 0..τ))
    c2 = np.cumsum(v*v) / np.arange(1, NS+1)
    return np.sqrt(c2)                             # cm/ns per τ样本

def kirch_vt(X, dx_m, vr, half_ap):
    """dt^2 = tau^2 + (2*dx_m*100/(vr_cm*tau_dt))²  — 全换样本单位"""
    nr, nc = X.shape
    Y = np.zeros((nr, nc))
    dtns = RNG_NS / NS
    for tau in range(1, nr):
        v_cm = vr[tau]                              # cm/ns
        k = 2.0 * dx_m * 100.0 / max(v_cm * dtns, 1e-9)   # 横向偏移(样本)每道
        for x0 in range(nc):
            lo, hi = max(0, x0-half_ap), min(nc-1, x0+half_ap)
            xs = np.arange(lo, hi+1)
            dt = np.sqrt(tau*tau + (k*(xs-x0))**2)
            i0 = np.clip(dt.astype(int), 0, nr-2); fr = dt - i0
            Y[tau, x0] = (X[i0, xs]*(1-fr) + X[i0+1, xs]*fr).sum()
    return Y

rows = slice(2, 500)
res = []
# 字段语义: 时间用a列; 速度候选: 原样cm / ×0.1(mm→cm) / ×10; 首尾外推
for scl in (1.0, 0.1, 10.0):
    pts = [(t, v*scl) for t, v in PTS_CM]
    vr = vrms_of('a', pts)
    for dx in (0.01, 0.02, 0.05, 0.1, 0.2, 0.5):
        Y = kirch_vt(S, dx, vr, 31)
        c = np.corrcoef(Y[rows].ravel(), R[rows].ravel())[0,1]
        res.append((c, scl, dx))
res.sort(reverse=True)
for r in res[:10]:
    print('corr=%.5f 速度换算×%.1f 道间距=%.2fm' % r)
