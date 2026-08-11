# -*- coding: utf-8 -*-
"""Compare P_1 vs P_11 DZT and DZX to find RADAN's generation rules."""
import struct, hashlib, os

base = 'D:/gpr_software/test_input_raw_files/测试样本/测试样本/DZXDEMO/Proc/'

def FL(b,o): return struct.unpack_from('<f',b,o)[0] if o+4<=len(b) else 0.0
def U16(b,o): return struct.unpack_from('<H',b,o)[0] if o+2<=len(b) else 0

# ===================== DZT comparison =====================
print('='*70)
print('DZT comparison: P_1 vs P_11')
print('='*70)

with open(base+'DZXDEMO__002 P_1.DZT','rb') as f: d1 = f.read()
with open(base+'DZXDEMO__002 P_11.DZT','rb') as f: d11 = f.read()

# Header diff (first 256 bytes)
print('\n--- Header bytes that differ (0-256) ---')
for i in range(min(256, len(d1), len(d11))):
    if d1[i] != d11[i]:
        # Try to interpret
        desc = ''
        if i == 22: desc = ' (rhf_position/signalPos)'
        elif i == 36: desc = ' (rhb_mdt edit time byte0)'
        elif i == 50: desc = ' (rh_nproc byte0)'
        elif i == 51: desc = ' (rh_nproc byte1)'
        elif i >= 128: desc = ' (proc history area)'
        print('  off %3d (0x%02x): P_1=0x%02x(%3d)  P_11=0x%02x(%3d)%s' % (i,i,d1[i],d1[i],d11[i],d11[i],desc))

# Proc history
po1 = U16(d1,48); npr1 = U16(d1,50)
po11 = U16(d11,48); npr11 = U16(d11,50)
print('\n--- Proc history ---')
print('  P_1:  procOff=%d npr=%d' % (po1, npr1))
print('  P_11: procOff=%d npr=%d' % (po11, npr11))
if npr11 > npr1:
    extra = d11[po11+npr1 : po11+npr11]
    print('  P_11 extra bytes (%d): %s' % (len(extra), ' '.join('%02x'%b for b in extra)))
    # Parse extra records
    cur = 0
    while cur < len(extra):
        t = extra[cur]
        if t == 0x1b and cur+7<=len(extra):
            print('    typeId 0x1b(27) 增益调整: u16@1=%d float@3=%.2f' % (U16(extra,cur+1), FL(extra,cur+3)))
            cur += 7
        elif t == 0x4d and cur+6<=len(extra):
            sub = extra[cur+1]
            print('    typeId 0x4d(77) sub=%d val=%.3f' % (sub, FL(extra,cur+2)))
            cur += 6
        elif t == 0x04 and cur+12<=len(extra):
            print('    typeId 0x04(4) IIRv: f@2=%.2f f@8=%.2f b1=%d b7=%d' % (FL(extra,cur+2),FL(extra,cur+8),extra[1],extra[7]))
            cur += 12
        elif t == 0x65 and cur+5<=len(extra):
            print('    typeId 0x65(101) range: %d-%d' % (U16(extra,cur+1),U16(extra,cur+3)))
            cur += 5
        elif t in (0x40,0x3f,0x42,0x41,0x43,0x44,0x45,0x46) and cur+5<=len(extra):
            print('    typeId 0x%02x(%d): f@1=%.2f' % (t,t,FL(extra,cur+1)))
            cur += 5
        elif t in (0x0e,0x0d) and cur+6<=len(extra):
            print('    typeId 0x%02x(%d) IIRh: f@2=%.1f' % (t,t,FL(extra,cur+2)))
            cur += 6
        elif t == 0x63 and cur+4<=len(extra):
            print('    typeId 0x63(99) marker')
            cur += 4
        elif t == 0x5f and cur+5<=len(extra):
            print('    typeId 0x5f(95) 背景去除: type=%d' % extra[1])
            cur += 5
        else:
            print('    typeId 0x%02x(%d) unknown, skip 1' % (t,t))
            cur += 1

# Signal position
sig1 = FL(d1,22); sig11 = FL(d11,22)
print('\n--- Signal position (offset 22) ---')
print('  P_1:  %.4f' % sig1)
print('  P_11: %.4f' % sig11)

# Data comparison
data1 = d1[0x20000:]
data11 = d11[0x20000:]
print('\n--- Data area ---')
print('  P_1  data size: %d' % len(data1))
print('  P_11 data size: %d' % len(data11))
print('  Data MD5 P_1:  %s' % hashlib.md5(data1).hexdigest()[:16])
print('  Data MD5 P_11: %s' % hashlib.md5(data11).hexdigest()[:16])
print('  Data identical: %s' % (data1 == data11))

# ===================== DZX comparison =====================
print('\n' + '='*70)
print('DZX comparison: P_1.DZX vs P_11.DZX')
print('='*70)

with open(base+'DZXDEMO__002 P_1.DZX','r') as f: x1 = f.read()
with open(base+'DZXDEMO__002 P_11.DZX','r') as f: x11 = f.read()

print('\n  P_1.DZX  size: %d' % len(x1))
print('  P_11.DZX size: %d' % len(x11))
print('  DZX identical: %s' % (x1 == x11))

# Line-by-line diff
import difflib
lines1 = x1.splitlines()
lines11 = x11.splitlines()
diff = list(difflib.unified_diff(lines1, lines11, 'P_1.DZX', 'P_11.DZX', lineterm=''))
print('\n--- DZX diff ---')
for line in diff[:80]:
    print('  %s' % line)
