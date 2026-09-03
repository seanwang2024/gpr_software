#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe13.py — 数值反演: 每个点值 v_k 反推 E_req=T/v, 在(中心r,半宽hw)全网格找满足窗口
看有效窗口中心的模式 (P_S与P_T对照)
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

def hatA(knots):
    n = len(knots)
    A = np.zeros((512, n)); r = np.arange(512.0)
    A[:, 0] = np.clip((knots[1]-r)/(knots[1]-knots[0]), 0, 1)
    A[:, -1] = np.clip((r-knots[-2])/(knots[-1]-knots[-2]), 0, 1)
    for k in range(1, n-1):
        lo, hi = knots[k-1], knots[k+1]
        A[:, k] = np.clip(np.minimum((r-lo)/(knots[k]-lo), (hi-r)/(hi-knots[k])), 0, 1)
    return A

def extract_medians(R, knots):
    G = np.where((np.abs(S) > 2000) & (R != SENT) & (R != 0), R/np.where(S == 0, 1, S), np.nan)
    rows = np.arange(2, 512)
    A = hatA(np.array(knots, float))
    vals = np.zeros(len(knots))
    for t in range(0, nt, 3):
        pass
    acc = []
    for t in range(0, nt, 3):
        ok = ~np.isnan(G[rows, t])
        if ok.sum() < 50: continue
        sol, *_ = np.linalg.lstsq(A[rows][ok], G[rows, t][ok], rcond=None)
        acc.append(sol)
    return np.median(np.array(acc), axis=0)

# E(r, hw) 道中位能量
def Emed(r, hw):
    a0, a1 = max(0, r-hw), min(512, r+hw)
    return np.median(np.sqrt((P2[a1]-P2[a0])/(a1-a0)))

for name, fn, npts in (('P_S', '1103_010 P_S.DZT', 8), ('P_T', '1103_010 P_T.DZT', 4)):
    R = load(os.path.join(CAL, 'Proc', fn)).astype(np.float64)
    knots = np.round(np.arange(npts)*512.0/(npts-1)).astype(int)
    vals = extract_medians(R, knots)
    print('\n===== %s knots=%s vals=%s =====' % (name, knots.tolist(),
          ' '.join('%.3f' % v for v in vals)))
    for T in (550e3, 650e3, 740e3, 800e3):
        print('  T=%.0f:' % T)
        for k in range(npts):
            Ereq = T/vals[k]
            sols = []
            for hw in (8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 128):
                rs = [r for r in range(0, 513, 4) if abs(Emed(r, hw)-Ereq)/Ereq < 0.02]
                if rs: sols.append('hw%d:r%d-%d' % (hw, min(rs), max(rs)))
            print('    v%d=%.3f Ereq=%.0fk → %s' % (k, vals[k], Ereq/1e3, ' | '.join(sols) if sols else '(无2%内解)'))
