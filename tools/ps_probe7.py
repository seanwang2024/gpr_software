#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe7.py — (a)gk场SVD可分离性 (b)P_R wiggle 的压缩来源(MA窗口) (c)h(t)驱动量
"""
import os
import numpy as np

CAL = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   'test_input_raw_files', 'process标定')
S  = np.load(os.path.join(CAL, 'raw.npy'))
NSAMP, OFF = 512, 0x20000
def load(path):
    a = np.fromfile(path, dtype='<i4', offset=OFF)
    nt = len(a) // NSAMP
    return a[:nt*NSAMP].reshape(nt, NSAMP).T
RR = load(os.path.join(CAL, 'Proc', '1103_010 P_R.DZT')).astype(np.float64)
gk = np.load(os.path.join(CAL, 'ps_gk.npy'))
_, nt = S.shape

# ---------- (a) SVD ----------
G = gk.copy()
Gm = G - G.mean(axis=1, keepdims=True)
U, sv, Vt = np.linalg.svd(Gm, full_matrices=False)
print('[SVD gk] 奇异值占比:', ' '.join('%.4f' % (v/sv.sum()) for v in sv))
print('  U[:,0] (深度形状):', ' '.join('%+.3f' % v for v in U[:, 0]))
h = Vt[0] * sv[0]                                    # 第一成分沿道
print('  h(t) std=%.2f%%' % (100*h.std()/np.abs(h).mean()))

# ---------- (b) P_R wiggle vs 平均窗宽 ----------
rat = np.array([np.median(RR[2:, t][np.abs(S[2:, t]) > 1000] / S[2:, t][np.abs(S[2:, t]) > 1000]) for t in range(nt)])
rms_tr = np.sqrt(np.mean(S[2:]**2, axis=0))
inv = 1.0/rms_tr
def ma(x, w):
    k = np.ones(w)/w
    y = np.convolve(x, k, 'same')
    y[:w//2] = x[:w//2]; y[-(w//2):] = x[-(w//2):]
    return y
print('\n[P_R] rat(t) std=%.3f%%; corr/幅度 vs MA-w(1/RMS):' % (100*rat.std()/rat.mean()))
for w in (1, 20, 50, 100, 200, 500, 1000, 2000, 4000, 5953):
    x = ma(inv, min(w, nt))
    c = np.corrcoef(rat, x)[0, 1]
    # rat对x回归斜率 → 被解释幅度
    sl = np.polyfit(x, rat, 1)[0]
    expl = abs(sl)*x.std()
    print('  w=%4d: corr=%+.4f  解释幅度=%.3f%%  (残差std=%.3f%%)' %
          (w, c, 100*expl/rat.mean(), 100*(rat-np.polyval(np.polyfit(x, rat, 1), x)).std()/rat.mean()))

# 因果exp平滑版
def smooth_fwd(x, alpha):
    y = np.empty_like(x); acc = x[0]
    for i in range(len(x)):
        acc = alpha*x[i]+(1-alpha)*acc; y[i]=acc
    return y
print('  [因果exp] τ=20: corr=%+.4f  τ=100: corr=%+.4f  τ=500: %+.4f' % (
    np.corrcoef(rat, smooth_fwd(inv, 1/20.))[0,1],
    np.corrcoef(rat, smooth_fwd(inv, 1/100.))[0,1],
    np.corrcoef(rat, smooth_fwd(inv, 1/500.))[0,1]))

# 分位散点: rat是否被压缩(非线性)
print('\n[P_R] 分位散点 (1/RMS 十分位 vs rat):')
q = np.quantile(inv, np.linspace(0, 1, 11))
for i in range(10):
    m = (inv >= q[i]) & (inv <= q[i+1])
    print('   inv/RMS分位%d: mean_inv=%.3e  rat=%.5f (std=%.5f)' %
          (i, inv[m].mean(), rat[m].mean(), rat[m].std()))

# ---------- (c) h(t)与什么相关 ----------
P2 = np.cumsum(np.concatenate([np.zeros((1, nt)), S**2], 0), 0)
print('\n[h(t)] corr vs 各行带1/E(rms), 原始与MA-100:')
for (a0, a1) in ((2, 64), (64, 128), (128, 256), (256, 384), (384, 512), (2, 512)):
    E = np.sqrt((P2[a1]-P2[a0])/(a1-a0))
    print('  rows%3d-%3d: raw=%+.3f  ma100=%+.3f' % (a0, a1-1,
          np.corrcoef(h, 1/E)[0,1], np.corrcoef(h, ma(1/E, 100))[0,1]))
