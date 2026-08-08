# -*- coding: utf-8 -*-
import struct
base = 'D:/gpr_software/test_input_raw_files/测试样本/测试样本/DZXDEMO/Proc/'
files = [('DZXDEMO__002 P_2.DZT',(45,310,190,3130)),
         ('DZXDEMO__003 P_1.DZT',(40,320,180,1300)),
         ('DZXDEMO__004 P_1.DZT',(40,330,170,1300))]
for fn,mhz in files:
    with open(base+fn,'rb') as f: h=f.read(1024)
    rng=struct.unpack_from('<f',h,26)[0]
    pos=struct.unpack_from('<f',h,22)[0]
    nsamp=struct.unpack_from('<H',h,4)[0]
    po=struct.unpack_from('<H',h,48)[0]; npr=struct.unpack_from('<H',h,50)[0]
    hist=h[po:po+npr] if po>0 and npr>0 else b''
    # find IIR(04) record floats and sample-range(65) end
    iir_f2=iir_f8=None; sr_end=None
    cur=0
    while cur<len(hist):
        t=hist[cur]
        if t==0x04 and cur+12<=len(hist):
            iir_f2=struct.unpack_from('<f',hist,cur+2)[0]; iir_f8=struct.unpack_from('<f',hist,cur+8)[0]; cur+=12
        elif t==0x65 and cur+5<=len(hist):
            sr_end=struct.unpack_from('<H',hist,cur+3)[0]; cur+=5
        elif t==0x3b and cur+3<=len(hist): cur+=3+hist[cur+1]*4
        elif t in (0x40,0x3f,0x43) and cur+5<=len(hist): cur+=5
        elif t==0x4d and cur+6<=len(hist): cur+=6
        elif t==0x63 and cur+4<=len(hist): cur+=4
        elif t==0x0e and cur+6<=len(hist): cur+=6
        else: cur+=1
    dt=rng/nsamp if nsamp else 0
    print('=== %s ==='%fn)
    print('  header: range=%.3fns pos=%.3f nsamp=%d  dt=%.5fns  fs=%.1fMHz'%(rng,pos,nsamp,dt,1000.0/dt if dt else 0))
    print('  sample-range end=%s'%sr_end)
    print('  RADAN: IIR HP%d LP%d  FIR HP%d LP%d'%mhz)
    print('  IIR coeff: f@2=%.4f f@8=%.4f'%(iir_f2,iir_f8))
    if iir_f8: print('  K from HP = f@8/HP = %.5f ; from LP = f@2/LP = %.5f'%(iir_f8/mhz[0], iir_f2/mhz[1]))
    print('  1/K_HP=%.3f  1/K_LP=%.3f'%(mhz[0]/iir_f8 if iir_f8 else 0, mhz[1]/iir_f2 if iir_f2 else 0))
    print()
