#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe3.py — 自动增益第3探针: LS提取每道8节点增益 g_k(t) → 反推能量窗口/水平滤波/T
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
SENT = -(1 << 24)

# ---------- 帽函数设计阵: 节点 0,73,146,219,293,366,439,512 ----------
knots = np.round(np.arange(8) * 512.0 / 7).astype(int)
A = np.zeros((512, 8))
r = np.arange(512)
for k in range(8):
    if k == 0:
        A[:, 0] = (r <= knots[1]) * (1 - (r - 0) / (knots[1] - 0.0)) * 0 + (r <= knots[1]) * (knots[1] - r) / (knots[1] - 0.0)
        A[r > knots[1], 0] = 0
    elif k == 7:
        A[:, 7] = (r >= knots[6]) * (r - knots[6]) / (512.0 - knots[6])
    else:
        lo, hi = knots[k-1], knots[k+1]
        seg = (r >= lo) & (r <= knots[k])
        A[seg, k] = (r[seg] - lo) / (knots[k] - lo + 0.0)
        seg = (r > knots[k]) & (r <= hi)
        A[seg, k] = (hi - r[seg]) / (hi - knots[k] + 0.0)

# 每道LS拟合 G列 (rows 2..511, |S|>2000 且非哨兵)
G = np.where((np.abs(S) > 2000) & (RS != SENT) & (RS != 0), RS / np.where(S == 0, 1, S), np.nan)
rows = np.arange(2, 512)
gk = np.zeros((8, nt))
resid_rel = []
for t in range(nt):
    ok = ~np.isnan(G[rows, t])
    if ok.sum() < 50:
        gk[:, t] = np.nan; continue
    sol, *_ = np.linalg.lstsq(A[rows][ok], G[rows, t][ok], rcond=None)
    gk[:, t] = sol
    pred = A[rows][ok] @ sol
    resid_rel.append(np.nanmedian(np.abs(G[rows, t][ok] - pred) / np.abs(pred)))
print('节点:', knots.tolist())
print('LS残差(中位|ΔG/G|)=%.4f%%  → 8节点线性插值模型成立' % (100*np.median(resid_rel)))
print('节点增益中位 g_k:', ' '.join('%7.3f' % np.nanmedian(gk[k]) for k in range(8)))
print('节点增益逐道std%%:', ' '.fnames if False else ' '.join('%5.2f' % (100*np.nanstd(gk[k])/np.nanmean(gk[k])) for k in range(8)))

# ---------- 能量窗口反推: g_k(t) vs 1/E(w) ----------
print('\n[corr(g_k, 1/E_w) 网格] 行窗口半宽 hw (中心knot k, 钳位0..511):')
for hw in (8, 16, 24, 32, 36, 48, 64, 96, 128):
    cors = []
    for k in range(8):
        a0, a1 = max(0, knots[k]-hw), min(512, knots[k]+hw)
        E = np.sqrt(np.mean(S[a0:a1] ** 2, axis=0))
        cors.append(np.corrcoef(gk[k], 1.0/E)[0, 1])
    print('  hw=%3d: ' % hw + ' '.join('%+.3f' % c for c in cors))

# ---------- 水平滤波形状: g_k(t) 自相关长度 ----------
print('\n[g_k(t) 自相关] (去趋势后):')
for k in (1, 3, 5):
    x = gk[k] - np.convolve(gk[k], np.ones(201)/201, 'same')
    x = x / x.std()
    ac = [np.corrcoef(x[:-L], x[L:])[0, 1] for L in (1, 2, 5, 10, 20, 40, 80)]
    print('  g%d: ρ(1,2,5,10,20,40,80) = %s' % (k, ' '.join('%+.3f' % v for v in ac)))

# ---------- 水平滤波方向/类型网格: 目标=corr(g_k(t), smooth(1/E_w)) ----------
def smooth_fwd(x, alpha):                       # 因果指数
    y = np.empty_like(x); acc = x[0]
    for i in range(len(x)):
        acc = alpha*x[i] + (1-alpha)*acc; y[i] = acc
    return y
def smooth_bwd(x, alpha):
    return smooth_fwd(x[::-1], alpha)[::-1]
def smooth_ma(x, n):
    k = np.ones(n)/n
    return np.convolve(x, k, 'same')

print('\n[滤波形状网格] k=3 (knot219), hw=36:')
k = 3; hw = 36
a0, a1 = max(0, knots[k]-hw), min(512, knots[k]+hw)
u = 1.0/np.sqrt(np.mean(S[a0:a1]**2, axis=0))
for alpha in (1.0, 0.5, 0.2, 0.1, 0.05, 0.025, 0.01):
    row = 'α=%.3f: fwd=%+.3f bwd=%+.3f' % (alpha,
        np.corrcoef(gk[k], smooth_fwd(u, alpha))[0,1],
        np.corrcoef(gk[k], smooth_bwd(u, alpha))[0,1])
    if alpha < 1:
        row += ' ma%d=%+.3f' % (round(2*(1-alpha)/alpha), np.corrcoef(gk[k], smooth_ma(u, min(nt, round(2*(1-alpha)/alpha))))[0,1])
    print('  ' + row)

# ---------- P_R 结构: rat_tr 是否离散电平 / 平滑 ----------
rat_tr = np.array([np.median(RR[2:512, t][np.abs(S[2:512, t]) > 1000] / S[2:512, t][np.abs(S[2:512, t]) > 1000]) for t in range(nt)])
x = rat_tr - rat_tr.mean()
print('\n[P_R] rat_tr: std=%.5f  自相关ρ(1,5,20)=%s' %
      (rat_tr.std(), ' '.join('%+.3f' % np.corrcoef(x[:-L], x[L:])[0, 1] for L in (1, 5, 20))))
print('[P_R] 直方图(20桶)min/max: %.5f %.5f' % (rat_tr.min(), rat_tr.max()))
h, e = np.histogram(rat_tr, bins=20)
print('   ', ' '.join('%d' % v for v in h))
np.save(os.path.join(CAL, 'ps_gk.npy'), gk)
