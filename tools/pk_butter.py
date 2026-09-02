# -*- coding: utf-8 -*-
"""闭式窗验证: Butterworth频域带通窗 |H| = [1+(f/fL)^-2n]^-1/2 · [1+(f/fH)^2n]^-1/2
n=极点数, fH/fL=标称截止(由c系数: MHz=fs/2pi/c); 全列FFT零相位滤波
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000
fs = NS / 20e-9

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')
S = src[:, :128]
FS = np.fft.fft(S, axis=0)
freq = np.fft.fftfreq(NS, 1/fs)

CASES = [
    ('P_2', 256e6, 800e6, 1),
    ('P_I', 300e6, 700e6, 1),
    ('P_J', 400e6, 800e6, 4),
    ('P_K', 400e6, 800e6, 12),
]

for name, fH, fL, n in CASES:
    R = load(BASE + '/Proc/1103_010 %s.DZT' % name)[:, :128]
    W = (1.0/np.sqrt(1.0+(np.maximum(freq,1e3)/fL)**(-2*n))) \
      * (1.0/np.sqrt(1.0+(freq/fH)**(2*n)))
    Y = np.fft.ifft(FS * W[:, None], axis=0).real
    c = np.corrcoef(Y[4:500].ravel(), R[4:500].ravel())[0,1]
    # 增益最优
    o, r = Y[4:500].ravel(), R[4:500].ravel()
    g = (o*r).sum()/(o*o).sum()
    mae = np.abs(g*o - r).mean()
    print('%s: Butterworth窗(n=%2d) corr=%.6f  g=%.4f MAE=%.0f' % (name, n, c, g, mae))
