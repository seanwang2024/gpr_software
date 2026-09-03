#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe14.py — 数据驱动: 逐点解有效能量窗权重 w_k(r):  1/g_k(t) = Σ_r w_k(r)·S(r,t)^2
正常方程 (S²S²ᵀ)w = S²y, 512x512, 分块累计
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

def extract_gk(R, knots, step=5):
    G = np.where((np.abs(S) > 2000) & (R != SENT) & (R != 0), R/np.where(S == 0, 1, S), np.nan)
    rows = np.arange(2, 512)
    A = hatA(np.array(knots, float))
    out = []
    for t in range(0, nt, step):
        ok = ~np.isnan(G[rows, t])
        if ok.sum() < 50: continue
        sol, *_ = np.linalg.lstsq(A[rows][ok], G[rows, t][ok], rcond=None)
        out.append((t, sol))
    return out

# S² 分块自相关矩阵
SQ = (S**2)
print('累计 S²S²ᵀ ...')
Gmat = np.zeros((512, 512))
for i in range(0, nt, 400):
    j = min(nt, i+400)
    B = SQ[:, i:j]
    Gmat += B @ B.T
Gmat /= nt

for name, fn, npts in (('P_S', '1103_010 P_S.DZT', 8), ('P_T', '1103_010 P_T.DZT', 4)):
    R = load(os.path.join(CAL, 'Proc', fn)).astype(np.float64)
    knots = np.round(np.arange(npts)*512.0/(npts-1)).astype(int)
    gks = extract_gk(R, knots)
    print('\n===== %s =====' % name)
    for k in range(npts):
        ts = np.array([t for t, _ in gks]); ys = np.array([1.0/s[k] for _, s in gks])
        ym = ys.mean()
        Xy = (SQ[:, ts] @ (ys-ym)) / len(ts)
        w = np.linalg.solve(Gmat + 1e-10*np.trace(Gmat)/512*np.eye(512), Xy)
        # 权重形状: 打印非零集中区
        wp = np.maximum(w, 0); wp = wp/wp.sum() if wp.sum() > 0 else wp
        top = np.argsort(wp)[::-1][:200]
        lo, hi = np.min(top), np.max(top)
        cen = float(np.sum(np.arange(512)*wp))
        fit = np.corrcoef(SQ[:, ts].T @ w + ym*0, ys)[0, 1]
        print('  knot%3d(val=%.3f): 权重质心=%.0f  主区间r%d-%d  正权重占比=%.2f  拟合corr=%.4f' %
              (knots[k], np.median([s[k] for _, s in gks]), cen, lo, hi,
               wp.sum()/max(np.abs(w).sum(), 1e-12), fit))
        # 每32行权重和
        print('    权重行和(32行桶):', ' '.join('%.2f' % wp[i:i+32].sum() for i in range(0, 512, 32)))
