# -*- coding: utf-8 -*-
"""P_2 残差解剖: 最好单向模型(exp, lag2)的残差按行统计 → 找时变增益/结构
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')
ref = load(BASE + '/Proc/1103_010 P_2.DZT')
S = src[:, :512]; R = ref[:, :512]
fs = NS / 20e-9

def alpha(fc): return 1-np.exp(-2*np.pi*fc/fs)
aL, aH = alpha(800e6), alpha(256e6)

def lp(Xv, a):
    Y = np.empty_like(Xv); Y[0] = 0.0
    for n in range(1, Xv.shape[0]):
        Y[n] = Y[n-1] + a*(Xv[n]-Y[n-1])
    return Y

X = np.vstack([S[2:], np.zeros((2, S.shape[1]))])
out = lp(X, aL); out = out - lp(out, aH)

# 逐行: ref能量 vs model能量 → 行增益曲线 g[row]
num = (out*R).sum(axis=1); den = (out*out).sum(axis=1)
g = np.where(den > 1e-6, num/den, np.nan)
print('行增益 g[row] (每32行):')
for r0 in range(0, 512, 32):
    seg = g[r0:r0+32]
    print('  行%3d-%3d: g均值=%.4f  ref/std=%.0f  模型std=%.0f' % (
        r0, r0+31, np.nanmean(seg), R[r0:r0+32].std(), out[r0:r0+32].std()))

# 每行归一后的行内corr(检验形状是否行行都对)
rc = []
for s in range(2, 512):
    a = out[s]; b_ = R[s]
    if a.std() > 1 and b_.std() > 1:
        rc.append(np.corrcoef(a, b_)[0,1])
rc = np.array(rc)
print('行内corr: 均值=%.4f min=%.3f @行%d  <0.9的行数=%d/%d' % (rc.mean(), rc.min(), rc.argmin()+2, (rc<0.9).sum(), len(rc)))

# 残差白性: 全体残差 lag1 自相关
res = (out - R)[4:500]
f = res.ravel()
print('残差 lag0/lag1/lag2 自相关: %.3f %.3f %.3f' % (1.0, np.corrcoef(f[:-1], f[1:])[0,1], np.corrcoef(f[:-2], f[2:])[0,1]))
