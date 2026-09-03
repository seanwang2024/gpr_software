#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe8.py — 逐道回归 R = a·S + b: 检验DC去除模型 (b ?= -a·mean(S))
"""
import os
import numpy as np

CAL = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   'test_input_raw_files', 'process标定')
S = np.load(os.path.join(CAL, 'raw.npy'))
NSAMP, OFF = 512, 0x20000
def load(path):
    a = np.fromfile(path, dtype='<i4', offset=OFF)
    nt = len(a) // NSAMP
    return a[:nt*NSAMP].reshape(nt, NSAMP).T
RR = load(os.path.join(CAL, 'Proc', '1103_010 P_R.DZT')).astype(np.float64)
_, nt = S.shape

rows = slice(2, 512)
X = S[rows]; Y = RR[rows]
# 逐道: 用全部行(不过滤, 大小信号都在, LS自然权重)
a_tr = np.empty(nt); b_tr = np.empty(nt)
for t in range(nt):
    x, y = X[:, t], Y[:, t]
    a_tr[t], b_tr[t] = np.polyfit(x, y, 1)
mean_tr = X.mean(axis=0)
print('[P_R] 逐道回归: a med=%.6f std=%.4f%% | b med=%.1f std=%.1f' %
      (np.median(a_tr), 100*a_tr.std()/a_tr.mean(), np.median(b_tr), b_tr.std()))
print('      corr(b, -a·mean)=%.5f   b/(-a·mean) med=%.4f' %
      (np.corrcoef(b_tr, -a_tr*mean_tr)[0,1], np.median(b_tr/(-a_tr*mean_tr))))
# 无DC假设的纯比值 wiggle 对比
print('      a的std=%.4f%%  vs 纯比值wiggle 0.58%%' % (100*a_tr.std()/a_tr.mean()))
# 残差: R - (a·S + b) 相对幅度
resid = np.empty(nt)
for t in range(0, nt, 500):
    x, y = X[:, t], Y[:, t]
    resid[t] = np.std(y - (a_tr[t]*x + b_tr[t])) / np.std(y)
print('      残差std/信号std(抽样)=%.5f' % np.median(resid[::500]))
# a(t)与1/RMS关系(消DC后还剩多少)
rms_tr = np.sqrt(np.mean(X**2, axis=0))
print('      corr(a, 1/RMS)=%.4f (原ratio版=0.867)' % np.corrcoef(a_tr, 1/rms_tr)[0,1])

# ---------- P_S: 逐道逐段回归 ----------
RS = load(os.path.join(CAL, 'Proc', '1103_010 P_S.DZT')).astype(np.float64)
knots = np.round(np.arange(8)*512.0/7).astype(int)
print('\n[P_S] 逐段回归 R = g·S + b (段≈knot区间): 报 g(与LS提取gk比对) 与 b:')
gk = np.load(os.path.join(CAL, 'ps_gk.npy'))
for k in (2, 3, 5):
    a0, a1 = knots[k], knots[k+1] if k < 7 else 512
    a1 = min(a1, 512)
    Xk, Yk = S[a0:a1], RS[a0:a1]
    for t in (100, 3000):
        a_, b_ = np.polyfit(Xk[:, t], Yk[:, t], 1)
        print('  段%d trace%d: g=%.4f (gk=%.4f)  b=%.1f  mean(S)=%.1f  b/-g·mean=%.3f' %
              (k, t, a_, gk[k, t], b_, Xk[:, t].mean(), b_/(-a_*Xk[:, t].mean())))
