#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
ps_final.py — 自动增益最终验证(4样本全通过) — ⚠因果平滑初值必须=u(0)
   scipy lfilter 是零状态(y[0]=α·x[0]), 瞬态毁掉前数百道 → 曾把0.9999误判成0.96!
结果(余弦/Pearson/MAE):
   P_R(1点/0/0)    0.99997  / 0.99997  / 0.38%
   P_S(8点/2.0/20) 0.9999946/ 0.9999918/ 0.13%
   P_T(4点/2.0/40) 0.9999991/ 0.9999989/ 0.10%
   P_U(16点/2.0/20)0.99996  / 0.99991  / 0.16%
"""
import os
import numpy as np

CAL = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   'test_input_raw_files', 'process标定')
NSAMP = 512
def load(path):
    a = np.fromfile(path, dtype='<i4', offset=0x20000)
    nt = len(a) // NSAMP
    return a[:nt*NSAMP].reshape(nt, NSAMP).T.astype(np.float64)

S = load(os.path.join(CAL, '1103_010.DZT'))
_, nt = S.shape
P2 = np.cumsum(np.concatenate([np.zeros((1, nt)), S**2], 0), 0)
Eref = np.sqrt((P2[510]-P2[1])/509)              # C++: refLo=1, refHi=nsamp-2

def causal(x, al):
    """因果指数平滑, 初值x[0] — RADAN语义(lfilter零状态会毁瞬态!)"""
    y = np.empty_like(x); y[0] = x[0]
    for t in range(1, len(x)):
        y[t] = al*x[t] + (1-al)*y[t-1]
    return y

def agc(npts, tc, C):
    kf = np.arange(npts)*512.0/max(npts-1, 1); sp = 512.0/max(npts-1, 1)
    wins = [(0, 512)] if npts == 1 else \
           [(max(0, int(round(kf[k]-sp/2))), min(512, int(round(kf[k]+sp/2)))) for k in range(npts)]
    al = 1.0 if tc <= 0 else (1-np.exp(-1.0/tc))
    out = [C*causal(Eref/np.sqrt((P2[a1]-P2[a0])/(a1-a0)), al) for a0, a1 in wins]
    return np.array(out), kf

def interp(kf, gm):
    G = np.zeros((512, gm.shape[1]))
    if len(kf) == 1:
        G[:] = gm[0]
        return G
    for s in range(512):
        k = min(int(s/(kf[1]-kf[0])), len(kf)-2)
        f = min(1.0, max(0.0, (s-kf[k])/(kf[k+1]-kf[k])))
        G[s] = gm[k]*(1-f)+gm[k+1]*f
    return G

if __name__ == '__main__':
    for name, npts, tc, C in (('P_R', 1, 0, 0.721), ('P_S', 8, 20, 1.2565),
                              ('P_T', 4, 40, 1.2565), ('P_U', 16, 20, 1.2565)):
        R = load(os.path.join(CAL, 'Proc', '1103_010 %s.DZT' % name))
        gm, kf = agc(npts, tc, C)
        G = interp(kf, gm)
        Rmod = np.rint(G*S).astype(np.int64)
        Rmod[0] = R[0]; Rmod[1] = -(1 << 24)
        a = Rmod[2:].ravel().astype(np.float64); b = R[2:].ravel().astype(np.float64)
        cos = np.dot(a, b)/np.sqrt(np.dot(a, a)*np.dot(b, b))
        pear = np.corrcoef(a, b)[0, 1]
        v = b != 0
        mae = 100*np.median(np.abs((a-b)[v]/b[v]))
        print('%s(npts=%2d TC=%2d): 余弦=%.7f Pearson=%.7f MAE=%.4f%%' % (name, npts, tc, cos, pear, mae))
