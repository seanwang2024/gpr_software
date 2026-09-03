#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe10.py — 检验: g_k(t) 在道方向是否分段线性(每隔M道采样后线性插值)
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
gk = np.load(os.path.join(CAL, 'ps_gk.npy'))
RR = load(os.path.join(CAL, 'Proc', '1103_010 P_R.DZT')).astype(np.float64)

# P_R: a(t) 逐道斜率(逐道回归斜率已=纯比值)
a = np.array([np.median(RR[2:, t][np.abs(S[2:, t]) > 1000]/S[2:, t][np.abs(S[2:, t]) > 1000]) for t in range(nt)])
d1 = np.diff(a)
print('[P_R a(t)] 一阶差分 绝对值中位=%.3e  最大=%.3e' % (np.median(np.abs(d1)), np.abs(d1).max()))
d2 = np.diff(a, 2)
print('[P_R] 二阶差分: med|Δ²|=%.3e p99=%.3e max=%.3e (若分段线性, Δ²在节点处尖峰)' %
      (np.median(np.abs(d2)), np.percentile(np.abs(d2), 99), np.abs(d2).max()))
# Δ² 尖峰位置间距
thr = 20*np.median(np.abs(d2))
spikes = np.where(np.abs(d2) > thr)[0]
gaps = np.diff(spikes)
print('  尖峰(>20×med)=%d个, 间距中位=%s' % (len(spikes), np.median(gaps) if len(gaps) > 2 else '-'))
# 打印一段: a(t)与逐点斜率
print('  a(t) t=1000..1030:', ' '.join('%.5f' % v for v in a[1000:1031]))
print('  Δ t=1000..1029:  ', ' '.join('%+.2e' % v for v in d1[1000:1030]))

# P_S: g_3(t) 同检验
for k in (3, 5):
    g = gk[k]
    d1 = np.diff(g); d2 = np.diff(g, 2)
    print('\n[P_S g_%d] med|Δ|=%.3e  med|Δ²|=%.3e p99|Δ²|=%.3e' % (k, np.median(np.abs(d1)), np.median(np.abs(d2)), np.percentile(np.abs(d2), 99)))
    thr = 20*np.median(np.abs(d2))
    spikes = np.where(np.abs(d2) > thr)[0]
    gaps = np.diff(spikes)
    print('  尖峰=%d 间距中位=%s' % (len(spikes), np.median(gaps) if len(gaps) > 2 else '-'))
    print('  g t=2000..2030:', ' '.join('%.4f' % v for v in g[2000:2031]))
    print('  Δ:             ', ' '.join('%+.1e' % v for v in d1[2000:2030]))
