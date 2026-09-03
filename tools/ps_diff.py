#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""ps_diff.py — 同进程并排执行'0.29%版'与'11%版'代码, 数组级对比"""
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
def hatA(kf):
    n = len(kf); A = np.zeros((512, n)); r = np.arange(512.0)
    A[:,0] = np.clip((kf[1]-r)/(kf[1]-kf[0]),0,1); A[:,-1] = np.clip((r-kf[-2])/(kf[-1]-kf[-2]),0,1)
    for k in range(1,n-1):
        lo,hi = kf[k-1],kf[k+1]
        A[:,k] = np.clip(np.minimum((r-lo)/(kf[k]-lo),(hi-r)/(hi-kf[k])),0,1)
    return A
def extract(R, kf):
    G = np.where((np.abs(S)>2000)&(R!=SENT)&(R!=0), R/np.where(S==0,1,S), np.nan)
    rows = np.arange(2,512); A = hatA(kf); gk = np.zeros((len(kf), nt))
    for t in range(nt):
        ok = ~np.isnan(G[rows,t])
        sol,*_ = np.linalg.lstsq(A[rows][ok],G[rows,t][ok],rcond=None)
        gk[:,t] = sol
    return gk

npts = 8
R = load(os.path.join(CAL,'Proc','1103_010 P_S.DZT')).astype(np.float64)
kf = np.arange(npts)*512.0/(npts-1); sp = 512.0/(npts-1)
gk = extract(R, kf)
Eref = np.sqrt((P2[512]-P2[2])/510)
Eks = []
for k in range(npts):
    a0,a1 = max(0,int(round(kf[k]-sp/2))), min(512,int(round(kf[k]+sp/2)))
    Eks.append(np.sqrt((P2[a1]-P2[a0])/(a1-a0)))

# ===== 代码块A(0.29%版原文) =====
tau = 20.0
al = 1.0/tau
gmA = np.zeros_like(gk); TsA = []
for k in range(npts):
    v = lfilter([al],[1,-(1-al)],Eref/Eks[k])
    T = np.dot(gk[k][100:],v[100:])/np.dot(v[100:],v[100:])
    gmA[k] = T*v; TsA.append(T)
errA = np.linalg.norm(gk[:,100:-100]-gmA[:,100:-100])/np.linalg.norm(gk[:,100:-100])
print('块A(原文): err=%.4f%%' % (100*errA))

# ===== 代码块B(11%版等价式) =====
al2 = 1.0/20.0
errsB = []
for k in range(npts):
    v2 = lfilter([al2],[1-(1-al2)],Eref/Eks[k])
    C2 = np.dot(gk[k][100:-100],v2[100:-100])/np.dot(v2[100:-100],v2[100:-100])
    errsB.append(np.linalg.norm(gk[k][100:-100]-C2*v2[100:-100])/np.linalg.norm(gk[k][100:-100]))
print('块B: err=%.4f%%' % (100*float(np.sqrt(np.mean([e*e for e in errsB])))))

# 数组指纹
print('gk[5][100:105]:', gk[5][100:105])
print('v[5][100:105]: ', v[100:105])
print('Eks[5] med=%.1f  Eref med=%.1f  kf=%s sp=%.4f' % (np.median(Eks[5]), np.median(Eref), kf[:3], sp))
print('al=%.6f  al2=%.6f' % (al, al2))
