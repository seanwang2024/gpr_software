# -*- coding: utf-8 -*-
"""P_2 频域Prony: H(z)=(b0+..+bM z^-M)/(1+a1z^-1+..+aN z^-N) 线性最小二乘拟合
先对 P_1(FIR已知)做方法学验证, 再拟合 P_2
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

def est_H(S, R, kmax):
    FS = np.fft.rfft(S, axis=0); FR = np.fft.rfft(R, axis=0)
    H = np.zeros(kmax+1, complex)
    for k in range(kmax+1):
        d = (FS[k].conj()*FS[k]).sum()
        H[k] = (FS[k].conj()*FR[k]).sum()/d if d > 1e-6 else 0
    return H

def prony(H, kuse, M, N):
    """H·(1+Σa_j z^-j) = Σb_i z^-i  → 线性LS"""
    rows, rhs = [], []
    for k in kuse:
        z = np.exp(-2j*np.pi*k/NS)
        # 未知: b0..bM, a1..aN
        r = np.concatenate([z**np.arange(0, M+1), -H[k]*z**np.arange(1, N+1)])
        rows.append(r); rhs.append(H[k])
    A = np.array(rows); y = np.array(rhs)
    sol, *_ = np.linalg.lstsq(A, y, rcond=None)
    b = sol[:M+1]; a = np.concatenate([[1.0], sol[M+1:]])
    # 拟合质量
    err = []
    for k in kuse:
        z = np.exp(-2j*np.pi*k/NS)
        num = sum(b[i]*z**(-i) for i in range(M+1))
        den = sum(a[j]*z**(-j) for j in range(N+1))
        err.append(abs((num/den)/H[k]-1) if abs(H[k])>1e-9 else 0)
    return b, a, np.array(err)

src = load(BASE + '/1103_010.DZT')

# ---- 方法学验证: P_1 (FIR HP200+LP800 已知) ----
ref1 = load(BASE + '/Proc/1103_010 P_1.DZT')
S = src[:, :512]; R1 = ref1[:, :512]
H1 = est_H(S, R1, 30)
print('P_1 经验H 前12bin:', np.round(np.abs(H1[:12]), 3))
b1, a1, e1 = prony(H1, range(2, 25), 8, 4)
print('P_1 Prony MAE=%.4f  分母极点(应≈1, FIR无极点):' % np.abs(e1).mean(), np.round(a1, 4))

# ---- P_2 ----
ref2 = load(BASE + '/Proc/1103_010 P_2.DZT')
R2 = ref2[:, :512]
H2 = est_H(S, R2, 30)
for M, N in ((4,4),(6,6),(8,4),(4,8)):
    b, a, e = prony(H2, range(1, 25), M, N)
    print('P_2 Prony M=%d N=%d MAE=%.4f' % (M, N, np.abs(e).mean()))
    print('   b =', np.round(b, 4))
    print('   a =', np.round(a, 4))
