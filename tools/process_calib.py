#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
process_calib.py — PROCESS 标定工具 (本软件 ↔ RADAN 对齐)
素材: test_input_raw_files/process标定/
  原始 1103_010.DZT (数据偏移0x20000, 512采样, 32bit, 5953数据道+64标签道)
  Proc/P_n.DZT = RADAN 处理结果(数值基准) + DZT头 proc history(typeId+sub+参数)
用法:
  python process_calib.py cases    # 解码全部 P_n 处理记录 + 用例表
  python process_calib.py run P_1  # 用本程序算法跑 P_1 用例并与 RADAN 结果对比
  python process_calib.py run all  # 全部用例
"""
import struct, sys, os, glob
import numpy as np

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CAL = os.path.join(BASE, 'test_input_raw_files', 'process标定')
SRC = os.path.join(CAL, '1103_010.DZT')
PROC = os.path.join(CAL, 'Proc')
NSAMP = 512
DATA_OFF = 0x20000
LABEL_TRACES = 64     # 左 0-63 道为标签, 处理/对比时剔除

# ---------- DZT ----------
def load_dzt(path):
    raw = np.fromfile(path, dtype='<i4', offset=DATA_OFF)
    nt = len(raw) // NSAMP
    return raw[:nt * NSAMP].reshape(nt, NSAMP).T.astype(np.float64)   # [512, nt]

def load_dzt_raw_i32(path):
    raw = np.fromfile(path, dtype='<i4', offset=DATA_OFF)
    nt = len(raw) // NSAMP
    return raw[:nt * NSAMP].reshape(nt, NSAMP).T                      # [512, nt] i32

# ---------- proc history 解码 ----------
def parse_proc(dzt_path):
    """返回 [(typeId, sub, params_bytes), ...] — DZT头 rh_proc@48/rh_nproc@50, 追加式记录"""
    h = open(dzt_path, 'rb').read(1024)
    off, = struct.unpack('<h', h[48:50])
    sz, = struct.unpack('<h', h[50:52])
    if off <= 0: off = 128
    blob = h[off:off + sz]
    recs = []
    i = 0
    while i + 2 <= len(blob):
        tid, sub = blob[i], blob[i + 1]
        # 记录长度未知 → 启发: 已知类型的参数长度表, 未知类型启发切分
        ln = 2 + PARAM_LEN.get(tid, len(blob) - i)
        ln = min(ln, len(blob) - i)
        recs.append((tid, sub, blob[i + 2:i + ln]))
        i += ln
    return recs

# 参数长度(字节) — typeId → len; 未知的由 cases 模式人工补充
PARAM_LEN = {0x4d: 4, 0x5f: 3, 0x1c: 8}

def views(params):
    """参数字节多视图: hex / f32 / u16"""
    out = []
    for i in range(0, len(params) - 3, 4):
        f, = struct.unpack('<f', params[i:i + 4])
        if abs(f) > 1e-6 and abs(f) < 1e9:
            out.append('f32@%d=%g' % (i, f))
    for i in range(0, len(params) - 1, 2):
        u, = struct.unpack('<H', params[i:i + 2])
        if 10 < u < 70000:
            out.append('u16@%d=%d' % (i, u))
    return ' '.join(out) if out else params.hex(' ')

def cmd_cases():
    print('== 原始 DZT proc 记录 ==')
    for tid, sub, p in parse_proc(SRC):
        print('  tid=0x%02x(%d) sub=%d %s' % (tid, tid, sub, views(p)))
    print()
    jpgs = sorted(glob.glob(os.path.join(CAL, '*', '*.JPG')))
    for f in sorted(glob.glob(os.path.join(PROC, '*.DZT'))):
        name = os.path.basename(f)
        mtime = os.path.getmtime(f)
        print('== %s (%s) ==' % (name, __import__('time').strftime('%H:%M:%S', __import__('time').localtime(mtime))))
        for tid, sub, p in parse_proc(f):
            print('  tid=0x%02x(%d) sub=%d  raw[%s]  %s' % (tid, tid, sub, p.hex(' '), views(p)))
    print()
    print('参考JPG时间:')
    for j in jpgs:
        print('  %-40s %s' % (os.path.relpath(j, CAL),
                              __import__('time').strftime('%H:%M:%S', __import__('time').localtime(os.path.getmtime(j)))))

if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'cases'
    if cmd == 'cases':
        cmd_cases()
    else:
        print('未知命令:', cmd)
