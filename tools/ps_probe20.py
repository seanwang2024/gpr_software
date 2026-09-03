#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe20b.py — P_U(16点/2.0/τ20) 干净验证
"""
import os
import numpy as np
from scipy.signal import lfilter

CAL = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   'test_input_raw_files', 'process标定')
NSAMP, OFF = 512, 0x20000
def load(path):
    a = np.fromfile(path, dtype='<i4', offset=OFF)
    nt = len(a) // NSAMP
    return a[:nt*NSAMP].reshape(nt, NSAMP).T
S = load(os.path.join(CAL, '1103_010.DZT')).astype(np.float64)
_, nt = S.shape
P2 = np.cumsum(np.concatenate([np.zeros((1, nt)), S**2], 0), 0)
SENT = -(1 << 24)

def hatA(kf):
    n = len(kf); A = np.zeros((512, n)); r = np.arange(512.0)
    A[:, 0] = np.clip((kf[1]-r)/(kf[1]-kf[0]), 0, 1)
    A[:, -1] = np.clip((r-kf[-2])/(kf[-1]-kf[-2]), 0, 1)
    for k in range(1, n-1):
        lo, hi = kf[k-1], kf[k+1]
        A[:, k] = np.clip(np.minimum((r-lo)/(kf[k]-lo), (hi-r)/(hi-kf[k])), 0, 1)
    return A

R = load(os.path.join(CAL, 'Proc', '1103_010 P_U.DZT')).astype(np.float64)
npts, tc = 16, 20
kf = np.arange(npts)*512.0/(npts-1)
sp = 512.0/(npts-1)

# LS提取(抽样道, 收集到列表)
G = np.where((np.abs(S) > 2000) & (R != SENT) & (R != 0), R/np.where(S == 0, 1, S), np.nan)
rows = np.arange(2, 512)
A = hatA(kf)
ts, sols = [], []
for t in range(0, nt, 3):
    ok = ~np.isnan(G[rows, t])
    if ok.sum() < 50: continue
    sol, *_ = np.linalg.lstsq(A[rows][ok], G[rows, t][ok], rcond=None)
    ts.append(t); sols.append(sol)
ts = np.array(ts); gk = np.array(sols).T                  # [16, nSample]
print('LS抽样%d道: 节点增益中位=' % len(ts), ' '.join('%6.3f' % np.median(gk[k]) for k in range(npts)))

# 模型B: C=1.2565固定, τ网格
Eref = np.sqrt((P2[510]-P2[1])/509)
wins = [(max(0, int(round(kf[k]-sp/2))), min(512, int(round(kf[k]+sp/2)))) for k in range(npts)]
best = None
m = ts > 100
for tau in np.arange(8, 48, 0.5):
    al = 1.0 - np.exp(-1.0/tau)
    errs = []
    for k in range(npts):
        a0, a1 = wins[k]
        E = np.sqrt((P2[a1]-P2[a0])/(a1-a0))
        v = (1.2565*lfilter([al], [1-(1-al)], Eref/E))[ts]
        errs.append(np.linalg.norm(gk[k][m]-v[m])/np.linalg.norm(gk[k][m]))
    e = float(np.sqrt(np.mean([x*x for x in errs])))
    if best is None or e < best[0]: best = (e, tau)
print('模型B(τ网格): 最优τ=%.1f (TC=20) 残差=%.3f%%' % (best[1], 100*best[0]))
# C拟合(τ=20)
al = 1.0 - np.exp(-1.0/20.0)
Cs = []
for k in range(npts):
    a0, a1 = wins[k]
    E = np.sqrt((P2[a1]-P2[a0])/(a1-a0))
    v = lfilter([al], [1-(1-al)], Eref/E)[ts]
    Cs.append(np.dot(gk[k][m], v[m])/np.dot(v[m], v[m]))
print('C拟合(τ=20): med=%.5f 各点=' % np.median(Cs), ' '.join('%.3f' % c for c in Cs))

# 端到端(τ=20, C=1.2565)
gm = np.zeros((npts, nt))
for k in range(npts):
    a0, a1 = wins[k]
    E = np.sqrt((P2[a1]-P2[a0])/(a1-a0))
    gm[k] = 1.2565*lfilter([al], [1-(1-al)], Eref/E)
Rmod = (A @ gm) * S
Rmod[0] = R[0]; Rmod[1] = -(1 << 24)
d = Rmod[2:] - R[2:]
valid = R[2:] != 0
print('端到端: 相对MAE=%.4f%%  corr=%.7f' %
      (100*np.median(np.abs(d[valid]/R[2:][valid])), np.corrcoef(Rmod[2:].ravel(), R[2:].ravel())[0, 1]))
# 逐节点深度误差剖面(诊断: 哪些深度差)
errrow = np.nanmedian(np.where(valid, np.abs(d)/np.maximum(np.abs(R[2:]), 1), np.nan), axis=1)
print('行误差中位(每64行):', ' '.join('%.2f%%' % (100*np.nanmedian(errrow[i:i+64])) for i in range(0, 512, 64)))
