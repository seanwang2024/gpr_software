#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_fit3.py — 最终模型全重建: 窗=帽子支集, g_k=T·smooth_α(1/E_k), α网格
P_S(TC=20) / P_T(TC=40) 交叉验证
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
SENT = -(1 << 24)

def hatA(knots_f):
    n = len(knots_f)
    A = np.zeros((512, n)); r = np.arange(512.0)
    A[:, 0] = np.clip((knots_f[1]-r)/(knots_f[1]-knots_f[0]), 0, 1)
    A[:, -1] = np.clip((r-knots_f[-2])/(knots_f[-1]-knots_f[-2]), 0, 1)
    for k in range(1, n-1):
        lo, hi = knots_f[k-1], knots_f[k+1]
        A[:, k] = np.clip(np.minimum((r-lo)/(knots_f[k]-lo), (hi-r)/(hi-knots_f[k])), 0, 1)
    return A

def extract_gk(R, knots_f):
    G = np.where((np.abs(S) > 2000) & (R != SENT) & (R != 0), R/np.where(S == 0, 1, S), np.nan)
    rows = np.arange(2, 512)
    A = hatA(knots_f)
    gk = np.zeros((len(knots_f), nt))
    for t in range(nt):
        ok = ~np.isnan(G[rows, t])
        sol, *_ = np.linalg.lstsq(A[rows][ok], G[rows, t][ok], rcond=None)
        gk[:, t] = sol
    return gk

def Es(npts):
    """帽子支集窗能量 E_k(t)"""
    knots_f = np.arange(npts)*512.0/(npts-1)
    sp = 512.0/(npts-1)
    out = []
    for k in range(npts):
        a0, a1 = int(round(knots_f[k]-sp/2)), int(round(knots_f[k]+sp/2))
        a0, a1 = max(0, a0), min(512, a1)
        out.append(np.sqrt((P2[a1]-P2[a0])/(a1-a0)))
    return out

def run(name, fn, npts, tc):
    R = load(os.path.join(CAL, 'Proc', fn)).astype(np.float64)
    knots_f = np.arange(npts)*512.0/(npts-1)
    gk = extract_gk(R, knots_f)
    Ek = Es(npts)
    A = hatA(knots_f)
    print('\n===== %s (npts=%d TC=%d) =====' % (name, npts, tc))
    best = None
    for domain in ('1/E', 'E', 'dB'):
        for tau in np.arange(2, 80, 1.0):
            alpha = min(1.0, 1.0/tau)
            gm = np.zeros_like(gk)
            for k in range(npts):
                if domain == '1/E':
                    v = lfilter([alpha], [1, -(1-alpha)], 1.0/Ek[k])
                elif domain == 'E':
                    v = 1.0/np.sqrt(lfilter([alpha], [1, -(1-alpha)], Ek[k]**2))
                else:
                    ldE = lfilter([alpha], [1, -(1-alpha)], 20*np.log10(Ek[k]))
                    v = 10**(-ldE/20.0)
                T = np.dot(gk[k][100:], v[100:])/np.dot(v[100:], v[100:])
                gm[k] = T*v
            err = np.linalg.norm(gk[:, 100:-100]-gm[:, 100:-100])/np.linalg.norm(gk[:, 100:-100])
            if best is None or err < best[0]:
                best = (err, domain, tau)
    err, domain, tau = best
    print('  ★ 残差=%.3f%%  域=%s  τ*=%.1f (TC=%d, τ*/TC=%.2f)' % (100*err, domain, tau, tc, tau/tc))
    # 用最优配置重建并评估最终输出
    alpha = min(1.0, 1.0/tau)
    gm = np.zeros_like(gk); Ts = []
    for k in range(npts):
        if domain == '1/E':
            v = lfilter([alpha], [1, -(1-alpha)], 1.0/Ek[k])
        elif domain == 'E':
            v = 1.0/np.sqrt(lfilter([alpha], [1, -(1-alpha)], Ek[k]**2))
        else:
            ldE = lfilter([alpha], [1, -(1-alpha)], 20*np.log10(Ek[k]))
            v = 10**(-ldE/20.0)
        T = np.dot(gk[k][100:], v[100:])/np.dot(v[100:], v[100:])
        gm[k] = T*v; Ts.append(T)
    print('  T_k:', ' '.join('%.0f' % t for t in Ts))
    Gfield = A @ gm
    Rmod = Gfield*S
    ok = (R != SENT) & (np.abs(S) > 500)
    d = Rmod[ok]-R[ok]
    print('  最终重建: 相对MAE=%.3f%%  corr=%.6f  p99|Δ|/med|x|=%.2f' %
          (100*np.median(np.abs(d/R[ok])), np.corrcoef(Rmod[ok], R[ok])[0, 1],
           np.percentile(np.abs(d), 99)/np.median(np.abs(S[ok]))))

run('P_S', '1103_010 P_S.DZT', 8, 20)
run('P_T', '1103_010 P_T.DZT', 4, 40)
