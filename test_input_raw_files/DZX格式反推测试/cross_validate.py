# -*- coding: utf-8 -*-
"""DZX (process definition) vs Proc DZT (processing history) cross-validation.
For each DZXDEMO__NNN: decode DZX BinaryData (what to apply) and
the corresponding Proc DZT proc-history (what was applied), compare."""
import struct, re, os, math
import xml.etree.ElementTree as ET

base = 'D:/gpr_software/test_input_raw_files/测试样本/测试样本/DZXDEMO'
proc = base + '/Proc'

def uu_decode(text):
    text = (text.replace("&amp;","&").replace("&lt;","<").replace("&gt;",">")
                 .replace("&quot;",'"').replace("&apos;","'"))
    text = re.sub(r"[\r\n\t ]","",text)
    out = bytearray(); pos = 0
    while pos < len(text):
        n = (ord(text[pos])-32) & 0x3F; pos += 1
        if n == 0: break
        nc = ((n+2)//3)*4
        for i in range(0,nc,4):
            v = [(ord(text[pos+i+j])-32)&0x3F if pos+i+j<len(text) else 0 for j in range(4)]
            b = (v[0]<<18)|(v[1]<<12)|(v[2]<<6)|v[3]
            out += bytes([(b>>16)&0xFF,(b>>8)&0xFF,b&0xFF])
        pos += nc
    d = struct.unpack_from('<H',out,0)[0] if len(out)>=2 else len(out)
    return bytes(out[:d]) if 0<d<=len(out) else bytes(out)

def U16(b,off): return struct.unpack_from('<H',b,off)[0] if off+2<=len(b) else 0
def FL(b,off):  return struct.unpack_from('<f',b,off)[0]  if off+4<=len(b) else 0.0

# ---- DZX BinaryData decode ----
def decode_dzx(dzx_path):
    procs = []
    root = ET.parse(dzx_path).getroot()
    for el in root.iter():
        if el.tag.split('}')[-1] != 'BinaryData': continue
        blob = uu_decode(el.text or '')
        if len(blob) < 9: continue
        tid = blob[8]
        desc = {'tid': tid, 'raw_len': len(blob)}
        if tid == 99:
            desc['name'] = '时间零点(主机参数)'; desc['skip'] = True
        elif tid == 77:
            desc['name'] = 'DC去除'; desc['val'] = FL(blob,0x0A)
        elif tid == 59:
            npts = blob[9] if len(blob)>9 else 0
            gains = [FL(blob,0x0B+i*4) for i in range(npts) if 0x0B+i*4+4<=len(blob)]
            desc['name'] = '增益'; desc['npts'] = npts; desc['gains'] = gains
        elif tid == 4:
            desc['name'] = 'IIR垂直'; desc['lp_u16'] = U16(blob,0x20); desc['hp_u16'] = U16(blob,0x22)
        elif tid == 64:
            desc['name'] = 'FIR垂直HP'; desc['hp_u16'] = U16(blob,0x20)
        elif tid == 63:
            desc['name'] = 'FIR垂直LP'; desc['lp_u16'] = U16(blob,0x1E)
        elif tid == 14:
            desc['name'] = 'IIR水平'; desc['val'] = FL(blob,0x0A)
        elif tid == 13:
            desc['name'] = 'IIR水平(13)'; desc['val'] = FL(blob,0x0A)
        elif tid == 67:
            desc['name'] = 'FIR水平平滑'; desc['val'] = FL(blob,0x09)
        else:
            desc['name'] = 'typeId %d'%tid
        procs.append(desc)
    return procs

# ---- DZT proc history decode ----
def decode_dzt_hist(dzt_path):
    with open(dzt_path,'rb') as f: h = f.read(1024)
    rng = FL(h,26); nsamp = U16(h,4); fs = 1000.0*nsamp/rng if rng>0 else 0
    po = U16(h,48); npr = U16(h,50)
    steps = []
    if not(po>0 and npr>0 and po+npr<=len(h)): return steps, fs, nsamp
    hist = h[po:po+npr]; cur = 0
    while cur < len(hist):
        t = hist[cur]
        if t==0x4d and cur+6<=len(hist):
            sub=hist[cur+1]; v=FL(hist,cur+2)
            steps.append({'name':'时间零点' if sub==0 else 'DC去除','val':v})
            cur+=6
        elif t==0x3b and cur+3<=len(hist):
            n=hist[cur+1]; g=[FL(hist,cur+3+i*4) for i in range(n) if cur+3+i*4+4<=len(hist)]
            steps.append({'name':'增益','npts':n,'gains':g}); cur+=3+n*4
        elif t==0x04 and cur+12<=len(hist):
            c2=FL(hist,cur+2); c8=FL(hist,cur+8)
            hp=fs/(2*math.pi*c2) if c2>0 else 0; lp=fs/(2*math.pi*c8) if c8>0 else 0
            steps.append({'name':'IIR垂直','hp':round(hp),'lp':round(lp),'c2':round(c2,2),'c8':round(c8,2)})
            cur+=12
        elif t==0x40 and cur+5<=len(hist):
            c=FL(hist,cur+1); hp=1.17*fs/c if c>0 else 0; cur+=5
            lp_str=''
            if cur<len(hist) and hist[cur]==0x3f and cur+5<=len(hist):
                lc=FL(hist,cur+1); lp=round(0.6434*fs) if lc>=0.99 else (round(1.17*fs/lc) if lc>0.01 else 0); lp_str=' LP %d'%lp; cur+=5
            steps.append({'name':'FIR垂直','hp':round(hp),'extra':lp_str})
        elif t in (0x0e,0x0d) and cur+6<=len(hist):
            steps.append({'name':'IIR水平','val':FL(hist,cur+2)}); cur+=6
        elif t==0x43 and cur+5<=len(hist):
            steps.append({'name':'FIR水平平滑','val':FL(hist,cur+1)}); cur+=5
        elif t==0x65 and cur+5<=len(hist): cur+=5
        elif t==0x63 and cur+4<=len(hist): cur+=4
        else: cur+=1
    return steps, fs, nsamp

# ---- Cross-validate all ----
dzx_files = sorted(f for f in os.listdir(base) if f.endswith('.DZX'))
print('='*80)
print('DZX <-> Proc DZT cross-validate')
print('='*80)
for dzxf in dzx_files:
    num = dzxf.replace('DZXDEMO__','').replace('.DZX','')
    # Find matching Proc DZT (P_1)
    pdzt = None
    for pf in os.listdir(proc):
        if pf.startswith('DZXDEMO__%s P_1.DZT'%num):
            pdzt = pf; break
    print('\n--- DZXDEMO__%s ---'%num)
    dzx_procs = decode_dzx(os.path.join(base,dzxf))
    print('  DZX BinaryData (%d proc):'%len(dzx_procs))
    for p in dzx_procs:
        if p.get('skip'): print('    %s (跳过)'%p['name']); continue
        d = p.get('name','?')
        if 'val' in p: print('    %s: %.2f'%(d,p['val']))
        elif 'gains' in p: print('    %s: %d点 %s dB'%(d,p.get('npts',0),' / '.join('%.1f'%g for g in p['gains'])))
        elif 'hp_u16' in p and 'lp_u16' in p: print('    %s: HP(u16)=%d LP(u16)=%d MHz'%(d,p['hp_u16'],p['lp_u16']))
        elif 'hp_u16' in p: print('    %s: HP(u16)=%d MHz'%(d,p['hp_u16']))
        elif 'lp_u16' in p: print('    %s: LP(u16)=%d MHz'%(d,p['lp_u16']))
        else: print('    %s'%d)

    if pdzt:
        steps, fs, nsamp = decode_dzt_hist(os.path.join(proc,pdzt))
        print('  Proc DZT %s (nsamp=%d fs=%.0fMHz, %d steps):'%(pdzt,nsamp,fs,len(steps)))
        for s in steps:
            n=s['name']
            if 'val' in s: print('    %s: %.2f'%(n,s['val']))
            elif 'gains' in s: print('    %s: %d点 %s dB'%(n,s.get('npts',0),' / '.join('%.1f'%g for g in s['gains'])))
            elif 'hp' in s and 'lp' in s: print('    %s: HP=%d LP=%d MHz (coeff %.1f/%.1f)'%(n,s['hp'],s['lp'],s.get('c2',0),s.get('c8',0)))
            elif 'hp' in s: print('    %s: HP=%d MHz%s'%(n,s['hp'],s.get('extra','')))
            else: print('    %s'%n)
    else:
        print('  (无匹配 Proc DZT)')
