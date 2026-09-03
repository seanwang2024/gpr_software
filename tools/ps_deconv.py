#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_deconv.py — 非参数滤波器辨识: g_k = h ⊛ u_k (u=T/E_knot±32)
H(f)=Σ cross(g,u)/Σ auto(u), IFFT→脉冲响应h(τ) — 看真实滤波形状
"""
import os
import numpy as np
from scipy.signal import lfilter

CAL = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   'test_input_raw_files', 'process标定')
NSAMP, OFF = 512, 0x20000
def load(path):
    a = np.fromfile(path, dtype='<i4', offset=OFF)
    nt = len(a) // NSAMP
    return a[:nt*NSAMP].reshape(nt, NSAMP).T
S = load(os.path.join(CAL, '1103_010.DZT')).astype(np.float64)
_, nt = S.shape
P2 = np.cumsum(np.concatenate([np.zeros((1, nt)), S**2], 0), 0)
PA = np.cumsum(np.concatenate([np.zeros((1, nt)), np.abs(S)], 0), 0)
gk = np.load(os.path.join(CAL, 'ps_gk.npy'))
knots = np.round(np.arange(8)*512.0/7).astype(int)

def E(c, hw, defn):
    a0, a1 = max(0, min(c-hw, 512)), min(512, max(c+hw, 0))
    if a1 - a0 < 4: return None
    n = a1-a0
    return np.sqrt((P2[a1]-P2[a0])/n) if defn == 'rms' else (PA[a1]-PA[a0])/n

def detrend(x, w=501):
    k = np.ones(min(w, len(x)))/min(w, len(x))
    return x - np.convolve(x, k, 'same')

LO, HI = 300, nt-300
for defn in ('rms', 'meanabs'):
    print('\n===== 能量定义: %s (窗=knot±32) =====' % defn)
    num = None; den = None
    for k in (2, 3, 4, 5, 6):
        e = E(knots[k], 32, defn)
        if e is None: continue
        g = detrend(gk[k][LO:HI]); u = detrend(1.0/e[LO:HI])
        G = np.fft.rfft(g); U = np.fft.rfft(u)
        num = G*np.conj(U) if num is None else num + G*np.conj(U)
        den = U*np.conj(U) if den is None else den + U*np.conj(U)
    H = num/(den + 1e-12*den.max())
    h = np.fft.irfft(H, n=HI-LO)
    # 脉冲响应居中显示
    n = len(h); half = 80
    hh = np.roll(h, n//2)                       # 零点居中
    idx = np.arange(n)//2 - ... if False else None
    print('  Σh(总直流增益)=%.4f' % h.sum())
    print('  脉冲响应 h[τ] (τ=-40..+60, ×归一):')
    seg = np.roll(h, len(h)//2)[len(h)//2-40 : len(h)//2+61]
    m = np.abs(seg).max()
    for i in range(0, len(seg), 5):
        print('    τ=%+4d: %+8.4f %s' % (i-40, seg[i]/m, '#'*int(40*abs(seg[i])/m)))

# 对照: 单指数因果 α=1/20 的脉冲响应
print('\n[对照] 因果exp α=1/20: h[0..20] =', ' '.join('%.3f' % ((1/20)*(1-1/20)**i) for i in range(10)))
print('[对照] 集中核(τ=20, 单边): 峰在τ=0, 和=1, 质心≈%d' % (sum(i*(1/20)*(1-1/20)**i for i in range(200))))
