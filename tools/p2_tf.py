# -*- coding: utf-8 -*-
"""P_2 系统辨识: 用 src/ref 全道对估计经验传递函数 H(f) = mean(FFT(ref)/FFT(src))
频率轴: fs=25.6GS/s, bin=50MHz; HP256→bin5.1, LP800→bin16
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
N = min(src.shape[1], ref.shape[1])
S = src[:, :N]; R = ref[:, :N]
fs = NS / 20e-9
binMHz = fs / NS / 1e6   # 50 MHz/bin

FS = np.fft.rfft(S, axis=0)
FR = np.fft.rfft(R, axis=0)
# 平均互谱/自谱 (比直接平均比值更稳)
H = (FS.conj() * FR).sum(axis=1) / (FS.conj() * FS).sum(axis=1)
mag = np.abs(H)
ph = np.angle(H)
print('bin  f(MHz)    |H|      phase(deg)')
for k in range(0, 40):
    print('%3d %7.0f  %.5f  %8.1f' % (k, k*binMHz, mag[k], np.degrees(ph[k])))
