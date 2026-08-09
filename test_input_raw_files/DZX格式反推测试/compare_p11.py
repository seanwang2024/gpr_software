# -*- coding: utf-8 -*-
import struct, hashlib

def FL(b,o): return struct.unpack_from('<f',b,o)[0] if o+4<=len(b) else 0.0
def U16(b,o): return struct.unpack_from('<H',b,o)[0] if o+2<=len(b) else 0

def dump(fn):
    with open(fn,'rb') as f: h=f.read(1024)
    po=U16(h,48); npr=U16(h,50)
    print('  procOff=%d npr=%d' % (po,npr))
    if po>0 and npr>0 and po+npr<=len(h):
        hist=h[po:po+npr]
        print('  hex:',' '.join('%02x'%b for b in hist))
        cur=0; n=0
        while cur<len(hist):
            t=hist[cur]; n+=1
            if t==0x4d and cur+6<=len(hist): print('  #%d 0x4d sub=%d val=%.3f'%(n,hist[cur+1],FL(hist,cur+2))); cur+=6
            elif t==0x3b and cur+3<=len(hist):
                np=hist[cur+1]; g=['%.1f'%FL(hist,cur+3+i*4) for i in range(np) if cur+3+i*4+4<=len(hist)]
                print('  #%d 0x3b gain %dpt %s'%(n,np,'/'.join(g))); cur+=3+np*4
            elif t==0x04 and cur+12<=len(hist): print('  #%d 0x04 IIRv f2=%.2f f8=%.2f'%(n,FL(hist,cur+2),FL(hist,cur+8))); cur+=12
            elif t==0x65 and cur+5<=len(hist): print('  #%d 0x65 range %d-%d'%(n,U16(hist,cur+1),U16(hist,cur+3))); cur+=5
            elif t==0x40 and cur+5<=len(hist): print('  #%d 0x40 FIRHP %.2f'%(n,FL(hist,cur+1))); cur+=5
            elif t==0x3f and cur+5<=len(hist): print('  #%d 0x3f FIRLP %.3f'%(n,FL(hist,cur+1))); cur+=5
            elif t in (0x0e,0x0d) and cur+6<=len(hist): print('  #%d 0x%02x IIRh %.1f'%(n,t,FL(hist,cur+2))); cur+=6
            elif t==0x43 and cur+5<=len(hist): print('  #%d 0x43 FIRhs %.1f'%(n,FL(hist,cur+1))); cur+=5
            elif t==0x63 and cur+4<=len(hist): print('  #%d 0x63 marker'%n); cur+=4
            else: print('  #%d 0x%02x ??'%(n,t)); cur+=1
    return h

base='D:/gpr_software/test_input_raw_files/测试样本/测试样本/DZXDEMO/Proc/'
print('=== P_1 ===')
h1=dump(base+'DZXDEMO__002 P_1.DZT')
print()
print('=== P_11 (P_1 + gain only) ===')
h11=dump(base+'DZXDEMO__002 P_11.DZT')
print()
with open(base+'DZXDEMO__002 P_1.DZT','rb') as f: f.seek(0x20000); d1=f.read(256)
with open(base+'DZXDEMO__002 P_11.DZT','rb') as f: f.seek(0x20000); d11=f.read(256)
print('data@0x20000 identical:', d1==d11)
print()
print('header bytes that differ (0-128):')
for i in range(min(len(h1),len(h11))):
    if h1[i]!=h11[i]:
        print('  off %d (0x%02x): P_1=0x%02x(%d)  P_11=0x%02x(%d)' % (i,i,h1[i],h1[i],h11[i],h11[i]))
