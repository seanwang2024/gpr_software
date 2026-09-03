#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_fit.py — 自动增益统一定量拟合 (P_S 8点/τ20, P_T 4点/τ40, P_R 1点/τ0)
模型: R(r,t) = G(r,t)·S(r,t);  G = 节点线性插值;  g_k(t) = T·smooth_α(1/E_k(t))
      E_k = 行窗口能量(knot±hw), smooth=因果指数(lfilter)
网格: hw × 中心偏移dc × α × 能量定义, 目标=模型残差
"""
import os, sys
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
SENT = -(1 << 24)
P2 = np.cumsum(np.concatenate([np.zeros((1, nt)), S**2], 0), 0)
PA = np.cumsum(np.concatenate([np.zeros((1, nt)), np.abs(S)], 0), 0)

def hatA(knots):
    A = np.zeros((512, len(knots))); r = np.arange(512.0)
    for k in range(len(knots)):
        if k == 0:
            A[:, 0] = np.clip((knots[1]-r)/(knots[1]-knots[0]), 0, 1)
        elif k == len(knots)-1:
            A[:, k] = np.clip((r-knots[-2])/(knots[-1]-knots[-2]), 0, 1)
        else:
            lo, hi = knots[k-1], knots[k+1]
            A[:, k] = np.clip(np.minimum((r-lo)/(knots[k]-lo), (hi-r)/(hi-knots[k])), 0, 1)
    return A

def extract_gk(R, knots):
    G = np.where((np.abs(S) > 2000) & (R != SENT) & (R != 0), R/np.where(S == 0, 1, S), np.nan)
    rows = np.arange(2, 512)
    A = hatA(knots)
    gk = np.zeros((len(knots), nt)); resid = []
    for t in range(nt):
        ok = ~np.isnan(G[rows, t])
        if ok.sum() < 50: gk[:, t] = np.nan; continue
        sol, *_ = np.linalg.lstsq(A[rows][ok], G[rows, t][ok], rcond=None)
        gk[:, t] = sol
        pred = A[rows][ok] @ sol
        resid.append(np.nanmedian(np.abs(G[rows, t][ok]-pred)/np.abs(pred)))
    return gk, 100*np.median(resid)

def energy(c, hw, defn):
    a0, a1 = max(0, c-hw), min(512, c+hw)
    n = a1-a0
    if defn == 'rms':     return np.sqrt((P2[a1]-P2[a0])/n)
    if defn == 'meanabs': return (PA[a1]-PA[a0])/n

def fit_knot(g, c0, hws, dcs, alphas, defns):
    """g(t) = T·smooth_α(1/E(c0+dc, hw)); 返回最佳(残差, hw, dc, α, defn, T)"""
    best = None
    for defn in defns:
        for hw in hws:
            for dc in dcs:
                E = energy(c0+dc, hw, defn)
                if E.mean() <= 0: continue
                u = 1.0/E
                for alpha in alphas:
                    if alpha >= 1.0: v = u.copy()
                    else: v = lfilter([alpha], [1.0, -(1-alpha)], u)
                    T = np.dot(g, v)/np.dot(v, v)
                    r = np.linalg.norm(g - T*v)/np.linalg.norm(g)
                    if best is None or r < best[0]:
                        best = (r, hw, dc, alpha, defn, T)
    return best

CASES = [
    ('P_S', 8, '1103_010 P_S.DZT'),
    ('P_T', 4, '1103_010 P_T.DZT'),
]
for name, npts, fn in CASES:
    R = load(os.path.join(CAL, 'Proc', fn)).astype(np.float64)
    knots = np.round(np.arange(npts)*512.0/(npts-1)).astype(int)
    gk, res = extract_gk(R, knots)
    print('\n==== %s (npts=%d, knots=%s) LS残差=%.4f%% ====' % (name, npts, knots.tolist(), res))
    print('  节点增益中位:', ' '.join('%7.3f' % np.nanmedian(gk[k]) for k in range(npts)))
    hws = (16, 24, 32, 40, 48, 56, 64, 85)
    dcs = (-32, -24, -16, -8, 0, 8, 16, 24, 32)
    alphas = [1.0] + [1.0/x for x in (2, 4, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 100, 128, 160, 200, 300)]
    for k in range(npts):
        c0 = knots[k]
        r, hw, dc, alpha, defn, T = fit_knot(gk[k], c0, hws, dcs, alphas, ('rms', 'meanabs'))
        print('  knot%3d: 残差=%.3f%%  hw=%d dc=%+d α=%.4f(τ=%.1f) %-7s T=%.0f' %
              (c0, 100*r, hw, dc, alpha, 1/alpha, defn, T))
