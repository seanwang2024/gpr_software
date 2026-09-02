# -*- coding: utf-8 -*-
"""检验行错位: ref行r ↔ src行r+k 的最佳k序列"""
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')[:, :64]
for name in ('P_2', 'P_J'):
    R = load(BASE + '/Proc/1103_010 %s.DZT' % name)[:, :64]
    print(name + ' ref行r 到 src最佳匹配行(±8窗口偏移):')
    offs = []
    for r in range(4, 64, 4):
        b = R[r] - R[r].mean()
        bestv, bestk = -2, None
        for k in range(-8, 9):
            if not (0 <= r + k < 512):
                continue
            s = src[r + k] - src[r + k].mean()
            c = np.corrcoef(b, s)[0, 1]
            if c > bestv:
                bestv, bestk = c, k
        offs.append(bestk)
        print('  行%3d: 偏移%+d corr=%.3f' % (r, bestk, bestv))
    print('  偏移序列:', offs)
