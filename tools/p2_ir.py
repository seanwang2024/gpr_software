# -*- coding: utf-8 -*-
"""P_2 脉冲响应反演: h = IFFT(H), H 由互谱估计(垂直方向) → 直接看滤波器结构
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

FS = np.fft.rfft(S, axis=0)
FR = np.fft.rfft(R, axis=0)
# 逐bin最小二乘复增益
H = np.zeros(NS//2+1, complex)
for k in range(NS//2+1):
    x = FS[k]; y = FR[k]
    d = (x.conj()*x).sum()
    H[k] = (x.conj()*y).sum()/d if d > 1e-6 else 0
# 只保留能量可信的前 60 bin, 其余置 0(避免噪声项污染脉冲响应)
Hc = np.concatenate([H[:60], np.zeros(NS//2+1-60)])
Hfull = np.concatenate([Hc, np.conj(Hc[-2:0:-1])])
h = np.fft.ifft(Hfull).real

print('经验脉冲响应 h[0..80] (×1000, 保留|h|>0.005):')
for n in range(0, 90):
    if abs(h[n]) > 0.005:
        print('h[%3d] = %+.4f' % (n, h[n]))
print('h[490..511](负延迟区):')
for n in range(490, 512):
    if abs(h[n]) > 0.005:
        print('h[%3d] = %+.4f  (等效负延迟 %d)' % (n, h[n], n-512))
