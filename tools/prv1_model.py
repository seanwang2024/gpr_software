# -*- coding: utf-8 -*-
"""PRV01__072 P_1 全链标定: 哨兵/深部能量/理论k=3.50模型/深段评估"""
import os
import struct
import numpy as np

root = [f for f in os.listdir(r'D:/gpr_software/test_input_raw_files') if 'process' in f][0]
BASE = os.path.join(r'D:/gpr_software/test_input_raw_files', root)
NS, DOFF, RNG = 512, 0x20000, 40.0
DX, V_CM = 0.02, 14.64

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=DOFF)
    n = len(raw)//NS
    return raw[:n*NS].reshape(n, NS).T.astype(np.float64)

src = load(os.path.join(BASE, 'PRV01__072.DZT'))
ref = load(os.path.join(BASE, 'Proc', 'PRV01__072 P_1.DZT'))
print('道数: %d / %d' % (src.shape[1], ref.shape[1]))
print('哨兵(P_1行1=-2^24):', int((ref[1] == -16777216).sum()), ' 行0前3:', ref[0, :3])

# 深部能量曲线(回答"能否满足深层")
print('行std曲线(每64行) src :', np.round([src[i:i+64].std()/1e4 for i in range(0,512,64)],1))
print('行std曲线(每64行) P_1:', np.round([ref[i:i+64].std()/1e4 for i in range(0,512,64)],1))

dtns = RNG/NS
k_theory = 2.0*DX*100.0/(V_CM*dtns)
print('理论k=%.3f (DX=0.02 v=14.64cm/ns range40ns)' % k_theory)

# 模型(64道子集, 避开边缘; 但道数~1733, 取中段)
T0 = src.shape[1]//2 - 32
S = src[:, T0:T0+64]; R = ref[:, T0:T0+64]
nr, nc = S.shape
def kirch(k, ap=31):
    Y = np.zeros((nr, nc))
    for tau in range(1, nr):
        for x0 in range(nc):
            lo, hi = max(0,x0-ap), min(nc-1,x0+ap)
            xs = np.arange(lo, hi+1)
            dt = np.sqrt(tau*tau + (k*(xs-x0))**2)
            i0 = np.clip(dt.astype(int), 0, nr-2); fr = dt-i0
            Y[tau,x0] = (S[i0,xs]*(1-fr)+S[i0+1,xs]*fr).mean()
    return Y
rows = slice(2, 500)
for k in (2.5, 3.0, 3.5, 4.0, 5.0):
    Y = kirch(k)
    print('k=%4.1f corr=%.5f' % (k, np.corrcoef(Y[rows].ravel(), R[rows].ravel())[0,1]))
# 理论k的深段逐段
Y = kirch(k_theory)
print('理论k逐段corr:')
for t0 in range(32, 480, 64):
    print('  段%3d: %.4f' % (t0, np.corrcoef(Y[t0:t0+64].ravel(), R[t0:t0+64].ravel())[0,1]))
