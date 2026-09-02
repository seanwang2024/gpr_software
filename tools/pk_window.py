# -*- coding: utf-8 -*-
"""频域窗假设验证: ref = IFFT(FFT(X) * W), W=测得的干净H(含/不含相位), X=src(全列,行对齐待定)
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')
S = src[:, :64]

def est_H_complex(Sg, R, r0=4, r1=505, kmax=200):
    L = r1 - r0
    FS = np.fft.rfft(Sg[r0:r1], axis=0); FR = np.fft.rfft(R[r0:r1], axis=0)
    H = np.zeros(kmax+1, complex)
    for k in range(kmax+1):
        d = (FS[k].conj()*FS[k]).sum()
        H[k] = (FS[k].conj()*FR[k]).sum()/d if d > 1e-6 else 0
    return H

for name in ('P_K', 'P_J', 'P_2'):
    R = load(BASE + '/Proc/1103_010 %s.DZT' % name)[:, :64]
    Hc = est_H_complex(S, R)
    # 窗: 幅值形状, 相位=0(零相位); 高于kmax的bin=0(带外)
    W = np.zeros(NS, complex)
    mag = np.abs(Hc)
    W[:len(mag)] = mag            # 零相位实窗
    FS = np.fft.fft(X_shift := np.vstack([S[2:], np.zeros((2, 64))]), axis=0)
    Y = np.fft.ifft(FS * W[:, None], axis=0).real
    c0 = np.corrcoef(Y[4:500].ravel(), R[4:500].ravel())[0,1]
    # 也试复数窗(含估计相位)
    W2 = np.zeros(NS, complex)
    W2[:len(Hc)] = Hc
    Y2 = np.fft.ifft(FS * W2[:, None], axis=0).real
    c1 = np.corrcoef(Y2[4:500].ravel(), R[4:500].ravel())[0,1]
    # 不shift版本
    FS0 = np.fft.fft(S, axis=0)
    Y3 = np.fft.ifft(FS0 * W[:, None], axis=0).real
    c2 = np.corrcoef(Y3[4:500].ravel(), R[4:500].ravel())[0,1]
    print('%s: 零相位窗+lag2 corr=%.5f | 复数窗+lag2 corr=%.5f | 零相位窗+无lag corr=%.5f'
          % (name, c0, c1, c2))
