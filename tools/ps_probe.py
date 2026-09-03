#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe.py — 自动增益(typeId 0x1a) P_S(点数8/整体增益2.0/水平时常20) 结构探针
目标: 重建立数事实 → 比值图 G=R/S 的 深度分段性 / 水平平滑性 / 钳位 / 常数来源
"""
import os, struct
import numpy as np

CAL = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   'test_input_raw_files', 'process标定')
SRC = os.path.join(CAL, '1103_010.DZT')
NSAMP, OFF = 512, 0x20000

def load(path):
    a = np.fromfile(path, dtype='<i4', offset=OFF)
    nt = len(a) // NSAMP
    return a[:nt*NSAMP].reshape(nt, NSAMP).T          # [512, nt] int32 view

S = load(SRC)
RS = load(os.path.join(CAL, 'Proc', '1103_010 P_S.DZT'))
RR = load(os.path.join(CAL, 'Proc', '1103_010 P_R.DZT'))
print('raw %s  P_S %s  P_R %s' % (S.shape, RS.shape, RR.shape))
nt = min(S.shape[1], RS.shape[1], RR.shape[1])
S, RS, RR = S[:, :nt].astype(np.float64), RS[:, :nt].astype(np.float64), RR[:, :nt].astype(np.float64)

# ---- 0. 哨兵行检查 ----
for nm, R in (('P_S', RS), ('P_R', RR)):
    for r in range(3):
        vals, cnt = np.unique(R[r], return_counts=True)
        print('%s row%d uniq=%d top: %s' % (nm, r, len(vals),
              ' '.join('%d:x%d' % (v, c) for v, c in sorted(zip(vals, cnt), key=lambda t: -t[1])[:3])))

# ---- 1. P_R 常数复核 + 钳位 ----
m = np.abs(S) > 100
g_R = RR[m] / S[m]
print('\n[P_R] ratio med=%.6f  p1=%.6f p99=%.6f  min=%.4f max=%.4f' %
      (np.median(g_R), *np.percentile(g_R, [1, 99]), g_R.min(), g_R.max()))
print('[P_R] raw max=%d  0.71784*rawmax=%.0f  RR max=%d  2^24=%d' %
      (S.max(), 0.71784*S.max(), RR.max(), 1 << 24))
over = (0.71784*S) > (1 << 24)
print('[P_R] 超2^24点数=%d, RR[over]是否全=2^24: %s' % (over.sum(), bool(np.all(RR[over] == (1 << 24)))))
under = (-0.71784*S) < -(1 << 24)
print('[P_R] 低于-2^24点数=%d, 全=-2^24: %s' % (under.sum(), bool(np.all(RR[under] == -(1 << 24)))))

# ---- 2. 常数来源: target = 0.71784 × 各种能量 ----
print('\n[常数来源] cand target = 0.71784*E:')
for nm, E in (('全局RMS', np.sqrt(np.mean(S**2))), ('全局mean|x|', np.mean(np.abs(S))),
              ('全道RMS之和/道数', np.mean(np.sqrt(np.mean(S**2, axis=0)))),
              ('道RMS的mean', np.mean(np.sqrt(np.mean(S**2, axis=1)))),
              ('max|S|/2', np.abs(S).max()/2)):
    print('  %-18s E=%.2f → target=%.2f' % (nm, E, 0.71784*E))

# ---- 3. P_S 比值图 ----
SENT = -(1 << 24)
mask = (np.abs(S) > 200) & (RS != SENT) & (RS != 0)
G = np.where(mask, RS / np.where(S == 0, 1, S), np.nan)
print('\n[P_S] 有效比值点=%d/%d (%.1f%%)' % (mask.sum(), mask.size, 100*mask.sum()/mask.size))
print('[P_S] G: med=%.4f p5=%.4f p95=%.4f min=%.4f max=%.4f' %
      (np.nanmedian(G), *np.nanpercentile(G, [5, 95]), np.nanmin(G), np.nanmax(G)))
print('[P_S] RS max=%d (>2^24=%s) RS min=%d' % (RS.max(), RS.max() > (1 << 24), RS.min()))
big = RS > (1 << 24)
print('[P_S] 超2^24点数=%d, 未钳位(max=%d)' % (big.sum(), RS[big].max() if big.any() else -1))

# 行剖面(每64行一段的段内中位)
print('\n[P_S] 行剖面(64行/段中位增益):')
for seg in range(8):
    rows = slice(seg*64, (seg+1)*64)
    gm = np.nanmedian(G[rows])
    ss = np.median(np.abs(S[rows]))
    print('  段%d rows%3d-%3d  G_med=%9.3f  |S|_med=%8.1f  G×|S|=%10.1f' %
          (seg, seg*64, seg*64+63, gm, ss, gm*ss))

# 段内细剖(前128行, 每16行)
print('\n[P_S] 行细剖(每16行, 0-192):')
for r0 in range(0, 192, 16):
    print('  rows%3d-%3d G_med=%9.3f' % (r0, r0+15, np.nanmedian(G[r0:r0+16])))

# 水平: 固定行带, G沿trace变化有多快 (相邻trace差)
print('\n[P_S] 水平平滑性(相邻道G中位差 vs 道间原始能量差):')
for seg in range(8):
    rows = slice(seg*64, (seg+1)*64)
    g_tr = np.nanmedian(G[rows], axis=0)               # [nt] 每道段增益
    e_tr = np.sqrt(np.nanmean(np.where(mask[rows], S[rows]**2, np.nan), axis=0))
    dg = np.nanmedian(np.abs(np.diff(g_tr)))
    de = np.nanmedian(np.abs(np.diff(e_tr))) / max(np.nanmedian(e_tr), 1)
    print('  段%d: |ΔG|med=%.4f (Gmed=%.2f, %.2f%%)  |ΔE|/E=%.2f%%' %
          (seg, dg, np.nanmedian(g_tr), 100*dg/max(np.nanmedian(g_tr), 1e-9), 100*de))

# ---- 4. 保存比值图供后续拟合 ----
np.save(os.path.join(CAL, 'ps_ratio.npy'), np.where(mask, G, np.nan))
np.save(os.path.join(CAL, 'raw.npy'), S)
print('\nsaved ps_ratio.npy / raw.npy')
