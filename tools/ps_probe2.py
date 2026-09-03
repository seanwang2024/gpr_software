#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe2.py — 自动增益第2探针: (a)P_R逐道比值 vs 道能量 (b)P_S单道G列的折线节点结构
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

S  = load(os.path.join(CAL, '1103_010.DZT')).astype(np.float64)
RS = load(os.path.join(CAL, 'Proc', '1103_010 P_S.DZT')).astype(np.float64)
RR = load(os.path.join(CAL, 'Proc', '1103_010 P_R.DZT')).astype(np.float64)
nt = S.shape[1]

# ---------- (a) P_R: 逐道比值 vs 道能量 ----------
body = slice(2, 512)                                  # 排 row0保留/row1哨兵
rat_tr = np.array([np.median(RR[body, t][np.abs(S[body, t]) > 1000] /
                             S[body, t][np.abs(S[body, t]) > 1000]) for t in range(nt)])
rms_tr = np.sqrt(np.mean(S[body] ** 2, axis=0))
mabs_tr = np.mean(np.abs(S[body]), axis=0)
inv = 1.0 / rms_tr
print('[P_R] 逐道比值: med=%.6f std=%.6f (%.2f%%)' % (np.median(rat_tr), rat_tr.std(), 100*rat_tr.std()/rat_tr.mean()))
for nm, x in (('1/道RMS', inv), ('1/道mean|x|', 1.0/mabs_tr)):
    c = np.corrcoef(rat_tr, x)[0, 1]
    k = np.median(rat_tr / x)
    print('  corr(比值,%s)=%.6f  比值/%s 中位=%.2f (残差std=%.4f)' %
          (nm, c, nm, k, (rat_tr/x/k).std()))

# ---------- (b) P_S: 单道 G 列结构 ----------
SENT = -(1 << 24)
def gcol(t):
    s, r = S[:, t], RS[:, t]
    ok = (np.abs(s) > 1000) & (r != SENT)
    g = np.full(512, np.nan)
    g[ok] = r[ok] / s[ok]
    return g

for t in (100, 3000):
    g = gcol(t)
    print('\n[P_S] trace%d G列(每8行中位):' % t)
    prof = np.array([np.nanmedian(g[i:i+8]) for i in range(0, 512, 8)])
    print('  ' + ' '.join('%6.2f' % v for v in prof))
    # 节点检测: 相邻8行块的差分符号变化
    print('  差分: ' + ' '.join('%+5.2f' % v for v in np.diff(prof)))

# 折线节点残差检验: 若在 r_k=k*512/7 处折线, 段内二阶差分≈0, 节点处大
print('\n[P_S] 全体道平均G行剖面(1行粒度, rows 0-160 & 500-511):')
Gm = np.load(os.path.join(CAL, 'ps_ratio.npy'))
prof1 = np.nanmedian(Gm[:, ::50], axis=1)             # 抽样道再取行中位
print('  rows0-40:   ' + ' '.join('%5.2f' % prof1[i] for i in range(0, 41, 4)))
print('  rows40-120: ' + ' '.join('%5.2f' % prof1[i] for i in range(40, 121, 8)))
print('  rows120-200:' + ' '.join('%5.2f' % prof1[i] for i in range(120, 201, 8)))
print('  尾部: ' + ' '.join('%5.2f' % prof1[i] for i in range(492, 512, 4)))

# 8点等距节点假设: 段内线性度检验 (二阶差分能量)
knots = np.round(np.arange(8) * 512 / 7).astype(int)
print('\n[节点假设] r_k = k*512/7 =', knots.tolist())
seg2nd = []
for k in range(7):
    a, b = knots[k], knots[k+1]
    seg = prof1[a:b]
    if len(seg) >= 3:
        seg2nd.append(np.mean(np.diff(seg, 2) ** 2) ** .5)
print('  段内二阶差分RMS:', ' '.join('%.3f' % v for v in seg2nd))
print('  节点处一阶差分跳变 |Δ斜率|:',
      ' '.join('%.3f' % abs((prof1[knots[k]+2]-prof1[knots[k]-2]) - (prof1[knots[k]-2]-prof1[knots[k]-6])) for k in range(1, 7)))

# ---------- (c) P_S 水平平滑: 固定行带的G(t)与原始能量1/E(t) 的滤波关系 ----------
rows = slice(290, 295)                                # 近节点292处
g_t = np.nanmedian(Gm[rows], axis=1) if Gm.shape[0] == 512 else None
# 直接重算: 每道该行带的中位G
g_t = np.array([np.nanmedian(Gm[rows, t]) for t in range(nt)])
e_t = np.sqrt(np.mean(S[rows] ** 2, axis=0))
raw = 1.0 / e_t
print('\n[P_S] 水平: rows290-294  corr(G_t, 1/E_t)=%.4f' % np.corrcoef(g_t, raw)[0, 1])
for tau in (20,):
    for alpha_mode, alpha in (('1/tau', 1.0/tau), ('1-exp(-1/tau)', 1-np.exp(-1.0/tau))):
        y = np.empty(nt); acc = raw[0]
        for t in range(nt):
            acc = alpha*raw[t] + (1-alpha)*acc
            y[t] = acc
        c = np.corrcoef(g_t, y)[0, 1]
        k = np.median(g_t / y)
        print('  τ=%d α=%s(%.4f): corr=%.4f  G/(平滑1/E)=%.3f 残差std=%.3f%%' %
              (tau, alpha_mode, alpha, c, k, 100*(g_t/y/k).std() if k > 0 else -1))
