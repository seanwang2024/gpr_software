#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe6.py — 无模型实测: 输出RS分段(knot对齐)能量结构 → 平衡量定义
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
RS = load(os.path.join(CAL, 'Proc', '1103_010 P_S.DZT')).astype(np.float64)
gk = np.load(os.path.join(CAL, 'ps_gk.npy'))
_, nt = S.shape
knots = np.round(np.arange(8) * 512.0 / 7).astype(int)

# 输出分段能量(每道): rms / meanabs, 段=knot对齐73行
print('[输出RS分段能量] 每段沿道的中位(±std%):')
for k in range(7):
    a0, a1 = knots[k], knots[k+1]
    rms_o  = np.sqrt(np.mean(RS[a0:a1]**2, axis=0))
    mabs_o = np.mean(np.abs(RS[a0:a1]), axis=0)
    rms_i  = np.sqrt(np.mean(S[a0:a1]**2, axis=0))
    print('  段%d rows%3d-%3d  outRMS=%9.1f(%.1f%%)  outMABS=%9.1f(%.1f%%)  inRMS=%9.1f  g_k×inRMS=%9.1f'
          % (k, a0, a1-1, np.median(rms_o), 100*rms_o.std()/rms_o.mean(),
             np.median(mabs_o), 100*mabs_o.std()/mabs_o.mean(),
             np.median(rms_i), np.median(gk[k])*np.median(rms_i)))

# 关键: g_k(t)·E_k(t) 的沿道稳定性(段能量×节点增益) — 各窗口定义
P2 = np.cumsum(np.concatenate([np.zeros((1, nt)), S**2], 0), 0)
PA = np.cumsum(np.concatenate([np.zeros((1, nt)), np.abs(S)], 0), 0)
print('\n[g_k(t)·E(t) 沿道 std%%]  窗口=段[k,k+73)  各能量定义:')
for k in range(7):
    a0, a1 = knots[k], knots[k+1]
    E_rms  = np.sqrt((P2[a1]-P2[a0])/(a1-a0))
    E_mabs = (PA[a1]-PA[a0])/(a1-a0)
    pr = gk[k]*E_rms; pm = gk[k]*E_mabs
    print('  k=%d: rms窗 std=%.2f%%  mabs窗 std=%.2f%%   (g_k med=%.3f)' %
          (k, 100*pr.std()/pr.mean(), 100*pm.std()/pm.mean(), np.median(gk[k])))

# 若g_k与段能量反相关不完美 → 平衡量可能是"段输出能量恒定": 检验out段能量沿道std
# (上表outRMS std%就是) — 若1-2%则平衡目标=该段RMS恒定
# 再看: 整道输出RMS沿道
rms_all_o = np.sqrt(np.mean(RS[2:]**2, axis=0))
print('\n[整道输出RMS] med=%.1f std=%.2f%%' % (np.median(rms_all_o), 100*rms_all_o.std()/rms_all_o.mean()))

# P_R对照: 输出=0.72×S, 整道RMS
RR = load(os.path.join(CAL, 'Proc', '1103_010 P_R.DZT')).astype(np.float64)
rms_all_r = np.sqrt(np.mean(RR[2:]**2, axis=0))
rms_all_s = np.sqrt(np.mean(S[2:]**2, axis=0))
print('[P_R] 整道outRMS med=%.1f std=%.2f%% | 原始整道RMS std=%.2f%% | out/in 比 med=%.6f std=%.2f%%'
      % (np.median(rms_all_r), 100*rms_all_r.std()/rms_all_r.mean(),
         100*rms_all_s.std()/rms_all_s.mean(),
         np.median(rms_all_r/rms_all_s), 100*(rms_all_r/rms_all_s).std()/(rms_all_r/rms_all_s).mean()))

# P_R: 逐道比值 vs 1/整道RMS 的精细回归(是否含截距/平滑)
rat = rms_all_r / rms_all_s
inv = 1.0 / rms_all_s
print('[P_R] corr(rat, 1/整道RMS)=%.5f' % np.corrcoef(rat, inv)[0, 1])
# 平滑后
def smooth_fwd(x, alpha):
    y = np.empty_like(x); acc = x[0]
    for i in range(len(x)):
        acc = alpha*x[i]+(1-alpha)*acc; y[i]=acc
    return y
smE = smooth_fwd(rms_all_s, 1/20.)
print('[P_R] corr(rat, 1/smooth20(整道RMS))=%.5f' % np.corrcoef(rat, 1/smE)[0, 1])
smE2 = smooth_fwd(1.0/rms_all_s, 1/20.)
print('[P_R] corr(rat, smooth20(1/RMS))=%.5f' % np.corrcoef(rat, smE2)[0, 1])
