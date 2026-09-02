# -*- coding: utf-8 -*-
"""P_2 精修: alpha=sqrt(1/c) 精确系数, 求增益g, 逐行corr分布定位残差结构
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
S = src[:, :256]; R = ref[:, :256]

aHP = np.sqrt(1.0/15.9155)   # 0.25064
aLP = np.sqrt(1.0/5.0857)    # 0.44334

def lp_all(X, a):
    Y = np.empty_like(X); Y[0] = 0.0
    for n in range(1, X.shape[0]):
        Y[n] = Y[n-1] + a*(X[n]-Y[n-1])
    return Y

def hp_df(X, a):
    Y = np.empty_like(X); Y[0] = 0.0
    for n in range(1, X.shape[0]):
        Y[n] = Y[n-1] + a*(X[n]-X[n-1])
    return Y

X = np.vstack([S[2:], np.zeros((2, S.shape[1]))])   # 上移2行
out = hp_df(lp_all(X, aLP), aHP)
rows = slice(6, 500)
o, r = out[rows].ravel(), R[rows].ravel()
g = (o*r).sum() / (o*o).sum()
print('LS增益 g=%.6f, corr=%.6f, MAE(raw)=%.1f' % (g, np.corrcoef(o,r)[0,1], np.abs(g*o-r).mean()))
print('无增益 corr=%.6f MAE=%.1f' % (np.corrcoef(o,r)[0,1], np.abs(o-r).mean()))

# 逐行corr(定位残差行)
rc = [np.corrcoef(out[s,:], R[s,:])[0,1] for s in range(2, 512)]
rc = np.array(rc)
print('行corr: 2-9:', np.round(rc[2:10],3))
print('行corr: 10-99均值=%.4f 100-300=%.4f 300-509=%.4f' % (rc[10:100].mean(), rc[100:300].mean(), rc[300:510].mean()))
print('最差20行:', np.argsort(rc)[:20]+0)

# 残差谱: 每8行段的残差能量占比
resd = (g*out - R)[2:510]
sg = np.abs(np.fft.rfft(resd, axis=0)).mean(axis=1)
print('残差谱前12bin:', np.round(sg[:12],1))
