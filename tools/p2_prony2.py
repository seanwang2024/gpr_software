# -*- coding: utf-8 -*-
"""P_2 干净辨识 v2: 行4..505内部块(避开哨兵/行0/底部清零) + 实系数Prony
P_1 同流程验证(已知FIR答案: HP=x-MA(85), LP=MA(14)级联)
"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

def est_H_clean(S, R, r0, r1, kmax):
    L = r1 - r0
    FS = np.fft.rfft(S[r0:r1], axis=0); FR = np.fft.rfft(R[r0:r1], axis=0)
    H = np.zeros(kmax+1, complex)
    for k in range(kmax+1):
        d = (FS[k].conj()*FS[k]).sum()
        H[k] = (FS[k].conj()*FR[k]).sum()/d if d > 1e-6 else 0
    return H, L

def prony_real(H, kuse, L, M, N):
    """实系数频域LS: Re/Im 分开。z=e^{-2πik/L}"""
    rows, rhs = [], []
    for k in kuse:
        z = np.exp(-2j*np.pi*k/L)
        cz = [np.real(z**i) for i in range(M+1)] + [np.real(-H[k]*z**j) for j in range(1, N+1)]
        sz = [np.imag(z**i) for i in range(M+1)] + [np.imag(-H[k]*z**j) for j in range(1, N+1)]
        rows.append(cz); rhs.append(np.real(H[k]))
        rows.append(sz); rhs.append(np.imag(H[k]))
    sol, *_ = np.linalg.lstsq(np.array(rows), np.array(rhs), rcond=None)
    b = sol[:M+1]; a = np.concatenate([[1.0], sol[M+1:]])
    err = []
    for k in kuse:
        z = np.exp(-2j*np.pi*k/L)
        num = sum(b[i]*z**(-i) for i in range(M+1))
        den = sum(a[j]*z**(-j) for j in range(N+1))
        err.append(abs(num/den - H[k]))
    return b, a, np.array(err)

src = load(BASE + '/1103_010.DZT')
S = src[:, :512]

# ---- P_1 验证 ----
ref1 = load(BASE + '/Proc/1103_010 P_1.DZT'); R1 = ref1[:, :512]
H1, L = est_H_clean(S, R1, 4, 505, 24)
print('P_1 干净H 前12bin:', np.round(np.abs(H1[:12]), 3))
b1, a1, e1 = prony_real(H1, range(1, 20), L, 10, 6)
print('P_1 Prony MAE=%.4f' % np.abs(e1).mean())
print('  b =', np.round(b1, 4))
print('  a =', np.round(a1, 4))

# ---- P_2 ----
ref2 = load(BASE + '/Proc/1103_010 P_2.DZT'); R2 = ref2[:, :512]
H2, L = est_H_clean(S, R2, 4, 505, 24)
print('P_2 干净H 前12bin:', np.round(np.abs(H2[:12]), 3))
for M, N in ((4, 4), (6, 6), (8, 8)):
    b, a, e = prony_real(H2, range(1, 20), L, M, N)
    print('P_2 M=%d N=%d MAE=%.4f' % (M, N, np.abs(e).mean()))
    print('  b =', np.round(b, 4))
    print('  a =', np.round(a, 4))
