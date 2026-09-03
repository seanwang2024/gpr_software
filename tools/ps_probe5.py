#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe5.py — 自动增益决定性拟合: 网格 (α × 行窗口start/width × 能量定义)
判据: u=invExp(g_k,α) 与 T/E 同构 → std(u·E)/mean(u·E) 最小
"""
import os
import numpy as np

CAL = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   'test_input_raw_files', 'process标定')

S = np.load(os.path.join(CAL, 'raw.npy'))             # [512, nt] float64
gk = np.load(os.path.join(CAL, 'ps_gk.npy'))          # [8, nt]
_, nt = S.shape

# ---------- 能量库: (start, width, def) ----------
WIDTHS = (16, 24, 32, 40, 48, 64, 73, 80, 96, 128, 192, 256, 384, 512)
DEFS = ('rms', 'meanabs', 'max')
# 预算行累加供快速窗口能量
P2 = np.cumsum(np.concatenate([np.zeros((1, nt)), S**2], 0), 0)     # [513, nt]
PA = np.cumsum(np.concatenate([np.zeros((1, nt)), np.abs(S)], 0), 0)

def energy(s, w, defn):
    e = P2[s+w] - P2[s]
    if defn == 'rms':     return np.sqrt(e / w)
    if defn == 'meanabs':
        a = (PA[s+w] - PA[s]) / w
        return a
    a0, a1 = s, s+w
    return np.max(np.abs(S[a0:a1]), axis=0)

def inv_exp(g, alpha):
    u = np.empty_like(g); u[0] = g[0]
    u[1:] = (g[1:] - (1-alpha)*g[:-1]) / alpha
    return u

def scan(k, alphas):
    g = gk[k]
    us = {a: inv_exp(g, a) for a in alphas}
    best = []
    for w in WIDTHS:
        for s in range(0, 512-w+1, 4):
            for defn in DEFS:
                E = energy(s, w, defn)
                if E.mean() <= 0: continue
                for a in alphas:
                    p = us[a] * E
                    m, sd = p.mean(), p.std()
                    if m > 0:
                        best.append((sd/m, a, defn, s, w))
    best.sort(key=lambda t: t[0])
    return best

alphas = (1.0, 0.3, 0.15, 0.1, 0.067, 0.05, 0.0488, 0.04, 0.033, 0.025, 0.02, 0.015, 0.01)
for k in (2, 3, 1):
    print('[knot%d] top8 (std(u·E)/mean, α, def, start, width):' % k)
    for row in scan(k, alphas)[:8]:
        print('   %.4f  α=%.4f %-7s s=%3d w=%3d' % row)
    print()
