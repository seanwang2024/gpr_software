#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe19.py — FIR滤波器最小二乘辨识: g_k ≈ (h ⊛ u_k)/scale, u_k=1/E_k(帽窗)
解正常方程得 h[0..L], 打印形状; P_S(TC=20)/P_T(TC=40)对照
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

def hatA(knots_f):
    n = len(knots_f)
    A = np.zeros((512, n)); r = np.arange(512.0)
    A[:, 0] = np.clip((knots_f[1]-r)/(knots_f[1]-knots_f[0]), 0, 1)
    A[:, -1] = np.clip((r-knots_f[-2])/(knots_f[-1]-knots_f[-2]), 0, 1)
    for k in range(1, n-1):
        lo, hi = knots_f[k-1], knots_f[k+1]
        A[:, k] = np.clip(np.minimum((r-lo)/(knots_f[k]-lo), (hi-r)/(hi-knots_f[k])), 0, 1)
    return A

def extract_gk(R, knots_f):
    G = np.where((np.abs(S) > 2000) & (R != SENT) & (R != 0), R/np.where(S == 0, 1, S), np.nan)
    rows = np.arange(2, 512)
    A = hatA(knots_f)
    gk = np.zeros((len(knots_f), nt))
    for t in range(nt):
        ok = ~np.isnan(G[rows, t])
        sol, *_ = np.linalg.lstsq(A[rows][ok], G[rows, t][ok], rcond=None)
        gk[:, t] = sol
    return gk

def Es(npts):
    knots_f = np.arange(npts)*512.0/(npts-1)
    sp = 512.0/(npts-1)
    out = []
    for k in range(npts):
        a0, a1 = int(round(knots_f[k]-sp/2)), int(round(knots_f[k]+sp/2))
        a0, a1 = max(0, a0), min(512, a1)
        out.append(np.sqrt((P2[a1]-P2[a0])/(a1-a0)))
    return out

def identify(name, fn, npts, L=100):
    R = load(os.path.join(CAL, 'Proc', fn)).astype(np.float64)
    knots_f = np.arange(npts)*512.0/(npts-1)
    gk = extract_gk(R, knots_f)
    Ek = Es(npts)
    # 回归: g_k(t) = Σ_j h[j]·u_k(t-j), u=1/E; 去均值, 忽略前L道
    Rm = np.zeros((L, L)); rv = np.zeros(L)
    for k in range(npts):
        u = 1.0/Ek[k]; u = u - u.mean()
        g = gk[k] - gk[k].mean()
        for a in range(L):
            # rv[a] += Σ g(t)u(t-a)
            rv[a] += np.dot(g[L:], u[L-a:nt-a] if a > 0 else u[L:])
            for b in range(L):
                Rm[a, b] += np.dot(u[L-a:nt-a] if a > 0 else u[L:],
                                   u[L-b:nt-b] if b > 0 else u[L:])
    h = np.linalg.solve(Rm + 1e-9*np.trace(Rm)/L*np.eye(L), rv)
    # 拟合度
    tot = 0; res = 0
    for k in range(npts):
        u = 1.0/Ek[k]; u = u - u.mean(); g = gk[k] - gk[k].mean()
        pred = np.convolve(u, h)[:nt]
        tot += np.dot(g[L:], g[L:]); res += np.dot(g[L:]-pred[L:], g[L:]-pred[L:])
    print('\n[%s] FIR(L=%d) 辨识: 解释方差=%.2f%%' % (name, L, 100*(1-res/tot)))
    print('  h[0..60]:')
    print('   ', ' '.join('%+.3f' % h[i] for i in range(0, 60, 4)))
    print('  h sum=%.3f  h[0]=%.3f argmax=%d' % (h.sum(), h[0], np.argmax(h)))
    # 质心与等效exp
    hp = np.maximum(h, 0); hp = hp/hp.sum()
    cen = np.sum(np.arange(L)*hp)
    print('  质心=%.1f  等效exp α=1/%.1f' % (cen, cen))
    return h

h20 = identify('P_S TC=20', '1103_010 P_S.DZT', 8)
h40 = identify('P_T TC=40', '1103_010 P_T.DZT', 4)
print('\n[比值] h40/h20 质心比=%.2f (TC比=2.0)' %
      (np.sum(np.arange(100)*np.maximum(h40, 0))/max(np.sum(np.arange(100)*np.maximum(h20, 0)), 1e-9)))
