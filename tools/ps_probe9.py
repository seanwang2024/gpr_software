#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_probe9.py — 肉眼+滞后扫描: g_k(t)到底响应什么能量
"""
import os
import numpy as np

CAL = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   'test_input_raw_files', 'process标定')
S = np.load(os.path.join(CAL, 'raw.npy'))
gk = np.load(os.path.join(CAL, 'ps_gk.npy'))
_, nt = S.shape
knots = np.round(np.arange(8)*512.0/7).astype(int)
P2 = np.cumsum(np.concatenate([np.zeros((1, nt)), S**2], 0), 0)

def E(a0, a1):
    return np.sqrt((P2[a1]-P2[a0])/(a1-a0))

# (a) 滞后互相关: corr(g_k(t), 1/E(t-L)) L∈[-80,80]
for k, hw in ((3, 32), (2, 32), (5, 32)):
    a0, a1 = max(0, knots[k]-hw), min(512, knots[k]+hw)
    u = 1.0/E(a0, a1)
    Ls = range(-80, 81, 4)
    cc = []
    for L in Ls:
        if L >= 0: cc.append(np.corrcoef(gk[k][L:], u[:nt-L])[0, 1])
        else:      cc.append(np.corrcoef(gk[k][:nt+L], u[-L:])[0, 1])
    cc = np.array(cc)
    i = np.argmax(np.abs(cc))
    print('[g%d 滞后谱] 最佳 L=%+d corr=%+.3f | L=-80:%+.3f L=0:%+.3f L=+80:%+.3f' %
          (k, list(Ls)[i], cc[i], cc[0], cc[list(Ls).index(0)], cc[-1]))

# (b) 短窗口细看: traces 2000-2120, g_3 vs 各窗口1/E
print('\n[细看 g_3 & g_1, traces 2000-2020]')
a3 = 1.0/E(knots[3]-32, knots[3]+32); a1e = 1.0/E(knots[1]-32, knots[1]+32)
print('  t     g_0    g_1    g_2    g_3    g_4    g_5    g_6    g_7  | 1/E3(x1e6) 1/E1(x1e6) 全道1/RMS(x1e6)')
rms_tr = np.sqrt((P2[512]-P2[2])/510)
for t in range(2000, 2021):
    print('  %4d %s | %8.3f %8.3f %8.3f' % (t, ' '.join('%6.3f' % gk[k, t] for k in range(8)),
          a3[t]*1e6, a1e[t]*1e6, (1/rms_tr[t])*1e6))

# (c) g_k(t) 与 全道1/RMS 相关 (P_R同款检验)
print('\n[g_k vs 全道1/RMS] (P_R: corr=0.867 弹性3%):')
for k in range(8):
    print('  g_%d: corr=%+.4f' % (k, np.corrcoef(gk[k], 1/rms_tr)[0, 1]))

# (d) 段能量剖面: 全体中位 E(rows band) — 看能量沿深度分布 vs g_k中位
print('\n[深度能量剖面 vs 增益节点] (16行带):')
prof = [np.median(E(i, i+16)) for i in range(0, 512, 16)]
print('  E:', ' '.join('%6.0f' % v for v in prof))
print('  g:', ' '.join('%6.2f' % np.median(gk[k]) for k in range(8)), '(knots 0,73,146,219,293,366,439,512)')
prod = [np.median(gk[k]) * np.median(E(max(0,knots[k]-32), min(512,knots[k]+32))) for k in range(8)]
print('  g_k·E_knot±32:', ' '.join('%8.0f' % v for v in prod))
