# -*- coding: utf-8 -*-
"""P_Q: 新vlc + 收发距40道(4m!) + 增益8 — 解码与基础分析"""
import struct
import numpy as np

BASE = r'D:/gpr_software/test_input_raw_files/process标定'
NS, OFF = 512, 0x20000

# 1) 新VLC
d = open(BASE + '/1103_010.VLC', 'rb').read()
print('VLC(%dB):' % len(d), d.decode('ascii', errors='replace').replace('\t', ' ')[:300])

# 2) P_Q proc
p = BASE + '/Proc/1103_010 P_Q.DZT'
h = open(p, 'rb').read(1024)
off, = struct.unpack('<h', h[48:50]); sz, = struct.unpack('<h', h[50:52])
blob = h[off:off+sz]
print('\nP_Q proc(%d):' % sz, blob.hex(' '))
i = 6
while i + 1 < len(blob):
    tid, sub = blob[i], blob[i+1]
    rec = blob[i:]
    f32s = []
    for j in range(2, len(rec) - 3):
        f, = struct.unpack('<f', rec[j:j+4])
        if 1e-6 < abs(f) < 1000:
            f32s.append((j, f))
    print('  记录@%d: type=0x%02x sub=%d  候选f32: %s' % (
        i, tid, sub, ['@%d=%.5f' % (a, b) for a, b in f32s[:6]]))
    i += 13   # 24记录长13B(经验)

def load(p):
    raw = np.fromfile(p, dtype='<i4', offset=OFF)
    nt = len(raw) // NS
    return raw[:nt*NS].reshape(nt, NS).T.astype(np.float64)

src = load(BASE + '/1103_010.DZT')
RQ = load(p)
print('哨兵:', int((RQ[1] == -16777216).sum()))
RO = load(BASE + '/Proc/1103_010 P_O.DZT')
S = src[:, 200:264]
print('corr P_Q vs 原始 = %.4f | vs P_O = %.4f' % (
    np.corrcoef(S[2:500].ravel(), RQ[2:500, 200:264].ravel())[0,1],
    np.corrcoef(RO[2:500, 200:264].ravel(), RQ[2:500, 200:264].ravel())[0,1]))
# 横向位移检测: P_Q行 vs 原始行最佳横向滞后(收发距40道→应见大位移)
for row in (60, 120, 240):
    b = RQ[row, 100:164] - RQ[row, 100:164].mean()
    best = (0, 0)
    for sh in range(-60, 61):
        a = src[row, 100+sh:164+sh] if 0 <= 100+sh and 164+sh <= 5953 else None
        if a is None: continue
        a = a - a.mean()
        c = np.corrcoef(b, a)[0,1]
        if c > best[0]: best = (c, sh)
    print('行%3d: 最佳横向位移=%+d道 corr=%.3f' % (row, best[1], best[0]))
