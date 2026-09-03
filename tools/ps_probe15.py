#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe15.py — 可分离模型检验: g_k(t) = c_k · h(t), h = T·smooth_α(1/E_deep窗口)
+ 全模型重建质量
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
PA = np.cumsum(np.concatenate([np.zeros((1, nt)), np.abs(S)], 0), 0)
gk = np.load(os.path.join(CAL, 'ps_gk.npy'))       # P_S 8点
SENT = -(1 << 24)

def E(a0, a1, defn='rms'):
    n = a1-a0
    return np.sqrt((P2[a1]-P2[a0])/n) if defn == 'rms' else (PA[a1]-PA[a0])/n

# (a) SVD主成分 vs 深层能量窗×平滑网格
G = gk - gk.mean(axis=1, keepdims=True)
U, sv, Vt = np.linalg.svd(G, full_matrices=False)
h1 = Vt[0]
print('[SVD] sv占比:', ' '.join('%.3f' % (v/sv.sum()) for v in sv))
best = None
for a0, a1 in ((0, 512), (2, 512), (128, 512), (96, 512), (160, 512), (112, 512), (104, 512), (128, 448), (192, 512), (64, 512)):
    for defn in ('rms', 'mab'):
        u = 1.0/E(a0, a1, defn)
        u = u - u.mean()
        for tau in (1, 2, 4, 6, 8, 10, 14, 20, 28, 40, 60, 100, 200):
            alpha = min(1.0, 1.0/tau)
            v = u.copy() if alpha >= 1 else lfilter([alpha], [1, -(1-alpha)], u)
            c = np.corrcoef(h1, v)[0, 1]
            if best is None or abs(c) > abs(best[0]):
                best = (c, a0, a1, defn, tau)
print('★h1最优: corr=%+.4f rows[%d,%d) %s τ=%d' % best)

# (b) 可分离模型: g_k = c_k·v, v=T·smooth(1/E_deep) — 逐点残差
c, a0, a1, defn, tau = best
alpha = min(1.0, 1.0/tau)
v = lfilter([alpha], [1, -(1-alpha)], 1.0/E(a0, a1, defn))
print('\n[可分离模型] v=smooth(1/E[%d,%d) %s τ=%d):' % (a0, a1, defn, tau))
tot0 = tot1 = 0
for k in range(8):
    ck = np.dot(gk[k], v)/np.dot(v, v)
    r0 = np.linalg.norm(gk[k]-gk[k].mean())/np.linalg.norm(gk[k])
    r1 = np.linalg.norm(gk[k]-ck*v)/np.linalg.norm(gk[k])
    tot0 += r0**2; tot1 += r1**2
    print('  g_%d: 常数模型残差=%.2f%%  h(t)模型残差=%.2f%%  c_k=%.2f' % (k, 100*r0, 100*r1, ck))
print('  总体: 常数%.2f%% → h(t)%.2f%% (改善%.1f×)' % (100*np.sqrt(tot0), 100*np.sqrt(tot1), np.sqrt(tot0/tot1)))

# (c) 全模型重建 P_S: R_model = interp(g_k=c_k·v)·S vs 真实RS
def hatA(knots):
    n = len(knots)
    A = np.zeros((512, n)); r = np.arange(512.0)
    A[:, 0] = np.clip((knots[1]-r)/(knots[1]-knots[0]), 0, 1)
    A[:, -1] = np.clip((r-knots[-2])/(knots[-1]-knots[-2]), 0, 1)
    for k in range(1, n-1):
        lo, hi = knots[k-1], knots[k+1]
        A[:, k] = np.clip(np.minimum((r-lo)/(knots[k]-lo), (hi-r)/(hi-knots[k])), 0, 1)
    return A
knots = np.round(np.arange(8)*512.0/7).astype(int)
gm = np.array([np.dot(gk[k], v)/np.dot(v, v)*v for k in range(8)])   # [8, nt]
RS = load(os.path.join(CAL, 'Proc', '1103_010 P_S.DZT')).astype(np.float64)
Gfield = hatA(knots.astype(float)) @ gm                              # [512, nt]
Rmod = Gfield*S
ok = (RS != SENT) & (np.abs(S) > 500)
d = (Rmod[ok]-RS[ok])
print('\n[全模型重建 vs 真实P_S] 相对MAE=%.3f%%  corr=%.6f  p99|Δ|=%.0f (信号med|x|=%.0f)' %
      (100*np.median(np.abs(d/RS[ok])), np.corrcoef(Rmod[ok], RS[ok])[0, 1],
       np.percentile(np.abs(d), 99), np.median(np.abs(S))))
