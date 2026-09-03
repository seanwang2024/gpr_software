#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe12.py — P_T(点数4) 节点位置实测: 行剖面 + 多种节点假设LS残差对比
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
RT = load(os.path.join(CAL, 'Proc', '1103_010 P_T.DZT')).astype(np.float64)
_, nt = S.shape
SENT = -(1 << 24)

G = np.where((np.abs(S) > 2000) & (RT != SENT) & (RT != 0), RT/np.where(S == 0, 1, S), np.nan)
prof = np.array([np.nanmedian(G[i:i+16]) for i in range(0, 512, 16)])
print('[P_T] G行剖面(16行块中位):')
print('  ' + ' '.join('%6.2f' % v for v in prof))
print('[P_S] 对照(已知knots 0,73,...512):')
RS = load(os.path.join(CAL, 'Proc', '1103_010 P_S.DZT')).astype(np.float64)
G2 = np.where((np.abs(S) > 2000) & (RS != SENT) & (RS != 0), RS/np.where(S == 0, 1, S), np.nan)
prof2 = np.array([np.nanmedian(G2[i:i+16]) for i in range(0, 512, 16)])
print('  ' + ' '.join('%6.2f' % v for v in prof2))

# 多节点假设LS残差
def hatA(knots):
    n = len(knots)
    A = np.zeros((512, n)); r = np.arange(512.0)
    A[:, 0] = np.clip((knots[1]-r)/(knots[1]-knots[0]), 0, 1)
    A[:, -1] = np.clip((r-knots[-2])/(knots[-1]-knots[-2]), 0, 1)
    for k in range(1, n-1):
        lo, hi = knots[k-1], knots[k+1]
        A[:, k] = np.clip(np.minimum((r-lo)/(knots[k]-lo), (hi-r)/(hi-knots[k])), 0, 1)
    return A

def ls_resid(G, knots):
    rows = np.arange(2, 512)
    A = hatA(np.array(knots, float))
    rr = []
    for t in range(0, nt, 7):
        ok = ~np.isnan(G[rows, t])
        if ok.sum() < 50: continue
        sol, *_ = np.linalg.lstsq(A[rows][ok], G[rows, t][ok], rcond=None)
        pred = A[rows][ok] @ sol
        rr.append(np.nanmedian(np.abs(G[rows, t][ok]-pred)/np.abs(pred)))
    return 100*np.median(rr)

print('\n[LS残差对比]')
for knots in ([0, 171, 341, 512], [64, 192, 320, 448], [0, 128, 256, 384, 512],
              [32, 96, 160, 224, 288, 352, 416, 480], [0, 73, 146, 219, 293, 366, 439, 512]):
    print('  P_T knots %-32s 残差=%.4f%%   P_S 残差=%.4f%%' %
          (str(knots), ls_resid(G, knots), ls_resid(G2, knots)))
