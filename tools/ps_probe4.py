#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe4.py — 自动增益第4探针: 反滤波恢复 u_k(t)=T/E_k, 再网格搜索能量定义
模型: g_k(t) = smooth(u_k)(t), u_k = T/E_k(行窗口能量×定义)
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
gk = np.load(os.path.join(CAL, 'ps_gk.npy'))          # [8, nt] LS提取节点增益
nt = S.shape[1]
knots = np.round(np.arange(8) * 512.0 / 7).astype(int)

def inv_exp_fwd(g, alpha):
    """u(t) = (g(t) - (1-α)g(t-1))/α  — 因果指数平滑的逆"""
    if alpha >= 1.0:
        return g.copy()
    g0 = g[0]
    u = np.empty_like(g)
    u[0] = g0
    u[1:] = (g[1:] - (1-alpha)*g[:-1]) / alpha
    return u

# 能量定义: 行窗口 [c-hw, c+hw) 钳位; def: rms/meanabs/max
def energy(c, hw, defn='rms'):
    a0, a1 = max(0, c-hw), min(512, c+hw)
    seg = S[a0:a1]
    if defn == 'rms':     return np.sqrt(np.mean(seg**2, axis=0))
    if defn == 'meanabs': return np.mean(np.abs(seg), axis=0)
    if defn == 'max':     return np.max(np.abs(seg), axis=0)

print('[knot1 强反射节点] 网格: α × (窗口中心偏移, 半宽, 能量定义) → argmax corr(u_α, 1/E)')
best = None
for alpha in (1.0, 0.4, 0.2, 0.1, 0.067, 0.05, 0.033, 0.02, 0.01):
    u = inv_exp_fwd(gk[1], alpha)
    u = u[50:-50]                                     # 去边缘
    for defn in ('rms', 'meanabs'):
        for hw in (8, 16, 24, 32, 48, 64):
            for dc in (-32, -16, 0, 16, 32):
                c = knots[1] + dc
                E = energy(c, hw, defn)[50:-50]
                c_val = np.corrcoef(u, 1.0/E)[0, 1]
                if best is None or abs(c_val) > abs(best[0]):
                    best = (c_val, alpha, defn, hw, dc)
    print('  α=%.3f → 当前最佳 corr=%+.4f' % (alpha, best[0]))
print('  ★ knot1 最佳: corr=%+.4f α=%.3f %s hw=%d dc=%+d' % best)

# 全部节点统一网格(固定中心=knot, 扫α×hw×defn)
print('\n[各节点最佳] (中心=knot_k):')
for k in range(8):
    bk = None
    for alpha in (1.0, 0.2, 0.1, 0.05, 0.033, 0.02, 0.01):
        u = inv_exp_fwd(gk[k], alpha)[50:-50]
        for defn in ('rms', 'meanabs'):
            for hw in (8, 16, 24, 32, 48, 64):
                E = energy(knots[k], hw, defn)[50:-50]
                v = np.corrcoef(u, 1.0/E)[0, 1]
                if bk is None or abs(v) > abs(bk[0]):
                    bk = (v, alpha, defn, hw)
    print('  g%d(knot%d): corr=%+.4f α=%.3f %s hw=%d' % (k, knots[k], *bk))

# dB域模型: g_dB = smooth(-E_dB)
print('\n[dB域] u_dB = invfilter(20log10 g_k); corr(u_dB, -20log10 E):')
for k in (1, 3):
    gdB = 20*np.log10(np.abs(gk[k]) + 1e-9)
    for alpha in (1.0, 0.1, 0.05, 0.02):
        u = inv_exp_fwd(gdB, alpha)[50:-50]
        for hw in (16, 32, 64):
            EdB = -20*np.log10(energy(knots[k], hw, 'rms'))[50:-50]
            print('  k=%d α=%.2f hw=%2d: corr=%+.4f' % (k, alpha, hw, np.corrcoef(u, EdB)[0, 1]))

# 交叉验证: g_k之间的相对结构 — g1/g3 是否与 E3/E1 相关(消T)
print('\n[消T检验] corr(g_j/g_k, E_k/E_j)  (若g=T/E则应为+1):')
for (j, k) in ((1, 3), (1, 5), (3, 5), (0, 3)):
    r = gk[j] / gk[k]
    for hw in (16, 32, 64):
        Ej, Ek = energy(knots[j], hw), energy(knots[k], hw)
        print('  g%d/g%d hw=%d: corr=%+.4f' % (j, k, hw, np.corrcoef(r, Ek/Ej)[0, 1]))
