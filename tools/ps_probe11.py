#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe11.py — P_R(点数1) 能量窗口定位: 找 std(a(t)·E(win,t))≈0 的行窗
若存在 → a=T/E_win 无平滑, 窗口=单点节点窗
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
RR = load(os.path.join(CAL, 'Proc', '1103_010 P_R.DZT')).astype(np.float64)
_, nt = S.shape
P2 = np.cumsum(np.concatenate([np.zeros((1, nt)), S**2], 0), 0)
PA = np.cumsum(np.concatenate([np.zeros((1, nt)), np.abs(S)], 0), 0)

a = np.array([np.median(RR[2:, t][np.abs(S[2:, t]) > 1000]/S[2:, t][np.abs(S[2:, t]) > 1000]) for t in range(nt)])
print('a(t): med=%.6f std=%.4f%%' % (np.median(a), 100*a.std()/a.mean()))

best = []
for w in (16, 32, 64, 96, 128, 192, 256, 384, 512):
    for s in range(0, 512-w+1, 4):
        n = w
        Er = np.sqrt((P2[s+w]-P2[s])/n)
        Em = (PA[s+w]-PA[s])/n
        for defn, E in (('rms', Er), ('mab', Em)):
            p = a*E
            best.append((p.std()/p.mean(), defn, s, w, p.mean()))
best.sort()
print('top12 (std(a·E)/mean, def, start, width, T=mean):')
for r in best[:12]:
    print('  %.4f%%  %-3s s=%3d w=%3d  T=%.0f' % (100*r[0], r[1], r[2], r[3], r[4]))

# 对照: 全道窗的数值
Er = np.sqrt((P2[512]-P2[2])/510)
print('全道(rows2-511): std(a·E)=%.2f%%  T=%.0f  E_std=%.2f%%' %
      (100*(a*Er).std()/(a*Er).mean(), (a*Er).mean(), 100*Er.std()/Er.mean()))
