#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe16.py — 精确u(r)曲线 + 反演P_S/P_T每点值的等效窗口中心r*(hw网格)
"""
import os
import numpy as np

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

def Emed(r, hw):
    a0, a1 = max(0, r-hw), min(512, r+hw)
    return np.median(np.sqrt((P2[a1]-P2[a0])/(a1-a0)))

def hatA(knots):
    n = len(knots)
    A = np.zeros((512, n)); r = np.arange(512.0)
    A[:, 0] = np.clip((knots[1]-r)/(knots[1]-knots[0]), 0, 1)
    A[:, -1] = np.clip((r-knots[-2])/(knots[-1]-knots[-2]), 0, 1)
    for k in range(1, n-1):
        lo, hi = knots[k-1], knots[k+1]
        A[:, k] = np.clip(np.minimum((r-lo)/(knots[k]-lo), (hi-r)/(hi-knots[k])), 0, 1)
    return A

def extract_med(R, knots):
    G = np.where((np.abs(S) > 2000) & (R != SENT) & (R != 0), R/np.where(S == 0, 1, S), np.nan)
    rows = np.arange(2, 512)
    A = hatA(np.array(knots, float))
    acc = []
    for t in range(0, nt, 5):
        ok = ~np.isnan(G[rows, t])
        if ok.sum() < 50: continue
        sol, *_ = np.linalg.lstsq(A[rows][ok], G[rows, t][ok], rcond=None)
        acc.append(sol)
    return np.median(np.array(acc), axis=0)

print('u(r)=800k/E(r±hw) 精确曲线 (hw=32):')
for r in range(0, 513, 16):
    print('  r=%3d E=%7.0fk u=%.3f   [r±48: E=%7.0fk u=%.3f]  [r±16: E=%7.0fk u=%.3f]' %
          (r, Emed(r, 32)/1e3, 800e3/Emed(r, 32), Emed(r, 48)/1e3, 800e3/Emed(r, 48),
           Emed(r, 16)/1e3, 800e3/Emed(r, 16)))

for name, fn, npts in (('P_S', '1103_010 P_S.DZT', 8), ('P_T', '1103_010 P_T.DZT', 4)):
    R = load(os.path.join(CAL, 'Proc', fn)).astype(np.float64)
    knots = np.round(np.arange(npts)*512.0/(npts-1)).astype(int)
    vals = extract_med(R, knots)
    print('\n[%s] knots=%s vals=%s' % (name, knots.tolist(), ' '.join('%.3f' % v for v in vals)))
    for k in range(npts):
        # 反演: 找 (r, hw, T) 使 800k/E = val → E_req, 列出每个hw下最近的r
        Ereq = 800e3/vals[k]
        row = []
        for hw in (16, 24, 32, 40, 48, 64):
            Es = np.array([Emed(r, hw) for r in range(0, 513, 8)])
            i = np.argmin(np.abs(Es-Ereq))
            row.append('hw%d:r%d(%.2f)' % (hw, i*8, Es[i]/Ereq))
        print('  v%d=%.3f(knot%d) Ereq=%.0fk → %s' % (k, vals[k], knots[k], Ereq/1e3, ' '.join(row)))
