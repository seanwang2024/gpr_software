#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe17.py — 精确求解: v_k ?= T/E(knot_k±w/2) — 网格(T, w)最小化全部点失配
+ 备选: 非对称窗(段左/段右/中点)
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
PA = np.cumsum(np.concatenate([np.zeros((1, nt)), np.abs(S)], 0), 0)
SENT = -(1 << 24)

def Emed(a0, a1, defn='rms'):
    a0, a1 = max(0, a0), min(512, a1)
    n = a1-a0
    if defn == 'rms': return np.median(np.sqrt((P2[a1]-P2[a0])/n))
    return np.median((PA[a1]-PA[a0])/n)

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

def solve(name, fn, npts):
    R = load(os.path.join(CAL, 'Proc', fn)).astype(np.float64)
    knots = np.round(np.arange(npts)*512.0/(npts-1)).astype(int)
    vals = extract_med(R, knots)
    print('\n[%s] knots=%s\n  vals=%s' % (name, knots.tolist(), ' '.join('%.3f' % v for v in vals)))
    # 网格: T × w(窗总宽), 窗=knot±w/2钳位; 失配=中位|v_k·E/T −1|
    best = None
    for defn in ('rms', 'mab'):
        for w in range(16, 160, 4):
            Es = np.array([Emed(knots[k]-w//2, knots[k]+w//2+w%2, defn) for k in range(npts)])
            for T in np.arange(400e3, 1200e3, 5e3):
                m = np.median(np.abs(vals*Es/T - 1))
                if best is None or m < best[0]:
                    best = (m, defn, w, T, Es)
    m, defn, w, T, Es = best
    print('  ★对称窗: 失配=%.3f%% %s w=%d T=%.0f' % (100*m, defn, w, T))
    print('    各点: ' + ' '.join('%.2f%%' % (100*abs(v*e/T-1)) for v, e in zip(vals, Es)))
    # 非对称窗: 窗=[knot_a, knot+b]网格独立优化每点(a,b)∈小网格, 但T共享: 先给定最优T再反解
    # 改为: 每点独立最优E=T·v/v → 报告等效窗位置
    for k in range(npts):
        Ereq = T/vals[k]
        cands = []
        for a0 in range(0, 512, 8):
            for w2 in range(16, 200, 8):
                e = Emed(a0, a0+w2, defn)
                if e > 0 and abs(e/Ereq-1) < 0.03:
                    cands.append((a0, a0+w2))
        if cands:
            lo = min(c[0] for c in cands); hi = max(c[1] for c in cands)
            print('    v%d(knot%d): 等效窗候选%d个 范围[%d,%d]' % (k, knots[k], len(cands), lo, hi))
    return vals, knots, T, defn

solve('P_S', '1103_010 P_S.DZT', 8)
solve('P_T', '1103_010 P_T.DZT', 4)
