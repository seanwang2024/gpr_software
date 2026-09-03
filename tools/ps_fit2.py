#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_fit2.py — 完整重建拟合: R = hat插值(T·smooth(1/E_k))·S, 网格(hw,dc,α,域)
输出最优参数 + P_T 交叉验证
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
SENT = -(1 << 24)

def hatA(knots):
    n = len(knots)
    A = np.zeros((512, n)); r = np.arange(512.0)
    A[:, 0]   = np.clip((knots[1]-r)/(knots[1]-knots[0]), 0, 1)
    A[:, -1]  = np.clip((r-knots[-2])/(knots[-1]-knots[-2]), 0, 1)
    for k in range(1, n-1):
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
        sol, *_ = np.linalg.lstsq(A[rows][ok], G[rows, t][ok], rcond=None)
        gk[:, t] = sol
    return gk

def energy(c, hw, defn):
    a0, a1 = max(0, c-hw), min(512, c+hw)
    n = a1-a0
    if n < 4: return None
    if defn == 'rms': return np.sqrt((P2[a1]-P2[a0])/n)
    return (PA[a1]-PA[a0])/n

def recon_eval(gk_true, knots, hw, dc, alpha, defn, domain):
    """g_model = T·smooth(1/E) 或 T/smooth(E) 或 dB域; 返回相对残差(节点增益级别)"""
    A = hatA(knots)
    gm = np.zeros_like(gk_true); Ts = []
    for k in range(len(knots)):
        E = energy(knots[k]+dc, hw, defn)
        if E is None or E.mean() <= 0: return None
        if alpha >= 1.0:
            sm = E
        else:
            sm = np.sqrt(lfilter([alpha], [1, -(1-alpha)], E**2)) if domain == 'E' else \
                 lfilter([alpha], [1, -(1-alpha)], E)
        if domain in ('1/E', 'E'):
            v = 1.0/sm
            T = np.dot(gk_true[k], v)/np.dot(v, v)
            gm[k] = T*v
        elif domain == 'dB':
            ldE = lfilter([alpha], [1, -(1-alpha)], 20*np.log10(E))
            v = 10**(-ldE/20.0)
            T = np.dot(gk_true[k], v)/np.dot(v, v)
            gm[k] = T*v
        Ts.append(T)
    err = np.linalg.norm(gk_true[:, 100:-100]-gm[:, 100:-100])/np.linalg.norm(gk_true[:, 100:-100])
    return err, np.median(Ts)

for name, npts, fn, tc in (('P_S', 8, '1103_010 P_S.DZT', 20), ('P_T', 4, '1103_010 P_T.DZT', 40)):
    R = load(os.path.join(CAL, 'Proc', fn)).astype(np.float64)
    knots = np.round(np.arange(npts)*512.0/(npts-1)).astype(int)
    gk = extract_gk(R, knots)
    print('\n===== %s npts=%d TC=%d knots=%s =====' % (name, npts, tc, knots.tolist()))
    print('  节点增益中位:', ' '.join('%7.3f' % np.median(gk[k]) for k in range(npts)))
    best = None
    for domain in ('1/E', 'E', 'dB'):
        for defn in ('rms', 'mab'):
            for hw in (24, 32, 40, 48, 64):
                for dc in (-16, -8, 0, 8, 16):
                    # α网格: 围绕 1/TC 精细扫
                    for tau in (tc/4, tc/3, tc/2.5, tc/2, tc/1.7, tc/1.5, tc/1.3, tc, tc/0.8, tc/0.6, tc/0.5):
                        alpha = min(1.0, 1.0/tau)
                        r = recon_eval(gk, knots, hw, dc, alpha, defn, domain)
                        if r is None: continue
                        if best is None or r[0] < best[0]:
                            best = (r[0], domain, defn, hw, dc, tau, r[1])
    print('  ★最优: 残差=%.3f%% 域=%s 能量=%s hw=%d dc=%+d τ=%.1f T=%.0f' %
          (100*best[0], best[1], best[2], best[3], best[4], best[5], best[6]))
    # 各节点单独T的残差(允许多T)
    err, T = recon_eval(gk, knots, best[3], best[4], min(1, 1/best[5]), best[2], best[1])
    print('    (单T中位 %.0f)' % T)
