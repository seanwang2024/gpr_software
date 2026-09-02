# -*- coding: utf-8 -*-
"""P_5 时深压缩假设: P_5 ≈ resample(src, 压缩比s) (+lag), 扫描s
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')
ref = load(BASE + '/Proc/1103_010 P_5.DZT')
S = src[:, 200:264]; R = ref[:, 200:264]

def resample(X, s):
    nr, nc = X.shape
    idx = np.arange(nr) * s
    i0 = np.clip(idx.astype(int), 0, nr - 2)
    fr = idx - i0
    return X[i0, :]*(1-fr)[:, None] + X[i0+1, :]*fr[:, None]

rows = slice(2, 480)
res = []
for s in np.arange(0.75, 1.0, 0.01):
    Y = resample(S, s)
    c = np.corrcoef(Y[rows].ravel(), R[rows].ravel())[0,1]
    res.append((c, s))
res.sort(reverse=True)
for c, s in res[:6]:
    print('压缩比 s=%.2f corr=%.5f' % (s, c))
# 逐行滞后精细测量(用最优s的残差): 直接测每64行段的最优滞后
best_s = res[0][1]
for r0 in range(8, 460, 64):
    seg_r = R[r0:r0+64, 10]
    bl, bc = 0, -2
    for k in range(-40, 41):
        if 0 <= r0+k and r0+k+64 <= 512:
            seg_s = S[r0+k:r0+k+64, 10]
            cval = np.corrcoef(seg_r, seg_s)[0,1] if seg_r.std()>0 and seg_s.std()>0 else -2
            if cval > bc: bc, bl = cval, k
    print('  行段%3d-%3d: 最优滞后=%+d corr=%.3f → 局部压缩后位置=%.1f' % (r0, r0+63, bl, bc, (r0+32)*(1-best_s)))
