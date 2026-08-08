# -*- coding: utf-8 -*-
import struct, math
base='D:/gpr_software/test_input_raw_files/测试样本/测试样本/DZXDEMO/Proc/'
for fn,radan in [('DZXDEMO__002 P_2.DZT',(45,310,190,3130)),
                 ('DZXDEMO__003 P_1.DZT',(40,320,180,1300)),
                 ('DZXDEMO__004 P_1.DZT',(40,330,170,1300))]:
    with open(base+fn,'rb') as f: h=f.read(1024)
    rng=struct.unpack_from('<f',h,26)[0]; nsamp=struct.unpack_from('<H',h,4)[0]
    fs=1000.0*nsamp/rng
    po=struct.unpack_from('<H',h,48)[0]; npr=struct.unpack_from('<H',h,50)[0]
    hist=h[po:po+npr]
    cur=0; out={}
    while cur<len(hist):
        t=hist[cur]
        if t==0x04 and cur+12<=len(hist):
            out['IIR_HP']=fs/(2*math.pi*struct.unpack_from('<f',hist,cur+2)[0])
            out['IIR_LP']=fs/(2*math.pi*struct.unpack_from('<f',hist,cur+8)[0]); cur+=12
        elif t==0x40 and cur+5<=len(hist):
            out['FIR_HP']=1.165*fs/struct.unpack_from('<f',hist,cur+1)[0]; cur+=5
        elif t==0x3f and cur+5<=len(hist):
            c=struct.unpack_from('<f',hist,cur+1)[0]
            out['FIR_LP']=0.6434*fs if c>=0.99 else 1.165*fs/c; cur+=5
        elif t==0x65 and cur+5<=len(hist): cur+=5
        elif t==0x4d and cur+6<=len(hist): cur+=6
        elif t==0x63 and cur+4<=len(hist): cur+=4
        elif t==0x3b and cur+3<=len(hist): cur+=3+hist[cur+1]*4
        elif t==0x0e and cur+6<=len(hist): cur+=6
        elif t==0x43 and cur+5<=len(hist): cur+=5
        else: cur+=1
    print('%s (fs=%.0f)'%(fn,fs))
    print('  IIR HP: %.0f (RADAN %d)   LP: %.0f (RADAN %d)'%(out.get('IIR_HP',0),radan[0],out.get('IIR_LP',0),radan[1]))
    print('  FIR HP: %.0f (RADAN %d)   LP: %.0f (RADAN %d)'%(out.get('FIR_HP',0),radan[2],out.get('FIR_LP',0),radan[3]))
