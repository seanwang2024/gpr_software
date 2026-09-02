# -*- coding: utf-8 -*-
"""P_2 第三轮: ①Butterworth 2极点LP/HP(RBJ biquad) ②水平(道间)IIR ③方向判定
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
fs = NS / 20e-9
S = src[:, :64]; R = ref[:, :64]

# ---- ① 方向判定: 水平谱(道间)能量对比 ----
def spatial_spec(X):
    return np.abs(np.fft.rfft(X - X.mean(axis=1, keepdims=True), axis=1)).mean(axis=0)
ss, sr = spatial_spec(S), spatial_spec(R)
print('① 水平谱比值(ref/src) 前8bin:', np.round(sr[:8]/np.maximum(ss[:8],1), 3))

# ---- ② RBJ biquad (垂直, 2极点) ----
def biquad_lp(X, fc, Q=0.7071):
    w0 = 2*np.pi*fc/fs
    al = np.sin(w0)/(2*Q)
    b0, b1, b2 = (1-np.cos(w0))/2, 1-np.cos(w0), (1-np.cos(w0))/2
    a0, a1, a2 = 1+al, -2*np.cos(w0), 1-al
    b0, b1, b2, a1, a2 = b0/a0, b1/a0, b2/a0, a1/a0, a2/a0
    Y = np.zeros_like(X); x1 = np.zeros(X.shape[1]); x2 = np.zeros(X.shape[1])
    y1 = np.zeros(X.shape[1]); y2 = np.zeros(X.shape[1])
    for n in range(X.shape[0]):
        xn = X[n]
        yn = b0*xn + b1*x1 + b2*x2 - a1*y1 - a2*y2
        Y[n] = yn; x2, x1 = x1, xn; y2, y1 = y1, yn
    return Y

def biquad_hp(X, fc, Q=0.7071):
    w0 = 2*np.pi*fc/fs
    al = np.sin(w0)/(2*Q)
    b0, b1, b2 = (1+np.cos(w0))/2, -(1+np.cos(w0)), (1+np.cos(w0))/2
    a0, a1, a2 = 1+al, -2*np.cos(w0), 1-al
    b0, b1, b2, a1, a2 = b0/a0, b1/a0, b2/a0, a1/a0, a2/a0
    Y = np.zeros_like(X); x1 = np.zeros(X.shape[1]); x2 = np.zeros(X.shape[1])
    y1 = np.zeros(X.shape[1]); y2 = np.zeros(X.shape[1])
    for n in range(X.shape[0]):
        xn = X[n]
        yn = b0*xn + b1*x1 + b2*x2 - a1*y1 - a2*y2
        Y[n] = yn; x2, x1 = x1, xn; y2, y1 = y1, yn
    return Y

rows = slice(6, 500)
for lag in (0, 2):
    X = np.vstack([S[lag:], np.zeros((lag, S.shape[1]))]) if lag else S
    for Q in (0.7071, 0.5, 1.0):
        out = biquad_hp(biquad_lp(X, 800e6, Q), 256e6, Q)
        c = np.corrcoef(out[rows].ravel(), R[rows].ravel())[0,1]
        print('② lag=%d 2极点Butterworth Q=%.3f corr=%.6f' % (lag, Q, c))
    # 2极点 LP+1极点HP 等混合
    out = biquad_lp(X, 800e6) - biquad_lp(biquad_lp(X, 256e6), 256e6)
    print('② lag=%d 2极HP(x-2LP) + 2极LP corr=%.6f' % (lag, np.corrcoef(out[rows].ravel(), R[rows].ravel())[0,1]))
