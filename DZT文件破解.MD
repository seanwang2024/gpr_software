# DZT 文件格式破解（程序实现视角 + 官方 PDF 对照）

> 2026-08-25 · 依据: 本程序 `loadDZTFile/readDztHeaderInfo/saveProcessedFile` 实现 +
> `docs/DZT File Format 4-13-15.pdf`（GSSI 官方, 2015-04-13）+
> `test_input_raw_files/DZX格式反推测试/参数.md`（RADAN 实测）
> ⭐ 操作记录（处理历史）见 §5。

---

## 1. 总体结构

```
DZT = [文件头 1024B 起] + [数据区 (逐扫描)]
      头实际长度 = rh_data 偏移(见 §2); 本程序样本实测 0x20000 (128KB)
数据区 = 扫描(道) × rh_nchan 通道; 每扫描 = rh_nsamp 个采样
采样字长 = rh_bits (8/16/32 bit); 本程序按 32bit 小端有符号读取
```

常量（PDF §B）: `MINHEADSIZE=1024, PARAREASIZE=128, RGPS=40B×2(GPS区@944), INFOAREASIZE=816`。

## 2. 文件头字段全表（偏移 | 类型 | 字段 | 含义 | 程序用法）

| 偏移 | 类型 | 字段 | 含义 | 本程序用法 |
|---|---|---|---|---|
| 0x00 | i16 | rh_tag | 0x00ff=新头, 0xfnff=旧 | — |
| 0x02 | i16 | rh_data | 数据偏移; **<1024 时 = 1024×rh_data** | m_dataOffset（实测 0x20000） |
| 0x04 | i16 | rh_nsamp | 每扫描采样数 | ★ m_pixelsPerRow（256/512 自适应） |
| 0x06 | i16 | rh_bits | 数据字长 8/16/32 | 按 32bit 读 |
| 0x08 | i16 | rh_zero | SIR-30=重复/采样; 8bit 数据=0x80, 16bit=0x8000 | — |
| 0x0A | f32 | rhf_sps | 扫描/秒 | — |
| 0x0E | f32 | rhf_spm | **扫描/米** | ★ markerSpacingM() 里程换算 |
| 0x12 | f32 | rhf_mpm | 米/标记 | — |
| 0x16 | f32 | rhf_position | **位置零点 (ns)** | ★ 时间零点上移量 |
| 0x1A | f32 | rhf_range | **时窗 (ns)** | ★ 深度/滤波换算 |
| 0x1E | i16 | rh_npass | 2-D 文件通道数 | — |
| 0x20 | 4B | rhb_cdt | 创建日期 (tagRFDate) | rdDate(32) 显示 |
| 0x24 | 4B | rhb_mdt | **最后修改日期** | ★ 处理后回写当前时间 |
| 0x28 | i16 | rh_rgain | range gain 偏移 | — |
| 0x2A | i16 | rh_nrgain | range gain 大小 | — |
| 0x2C | i16 | rh_text | 文本偏移 | — |
| 0x2E | i16 | rh_ntext | 文本大小 | — |
| 0x30 | i16 | rh_proc | **处理历史偏移** | ★ 默认 128 |
| 0x32 | i16 | rh_nproc | **处理历史大小** | ★ 追加后回写 (§5) |
| 0x34 | i16 | rh_nchan | 通道数 | — |
| 0x36 | f32 | rhf_epsr | **平均介电常数** | ★ 深度=c·t/(2√εr) |
| 0x3A | f32 | rhf_top | 位置(米) | — |
| 0x3E | f32 | rhf_depth | 范围(米) | — |
| 0x42 | 8B | rh_coordX | X 坐标(fstart+fend) | — |
| 0x4A | f32 | rhf_servo_level | 增益伺服 | — |
| 0x4E | 3B | reserved | 保留 | — |
| 0x51 | u8 | rh_accomp | 天线配置组件 | — |
| **0x52** | i16 | rh_sconfig (PDF) | 设置配置号 | **实测=时间零点主机参数 f32 @0x82** ⚠ 见 §6 |
| 0x54 | i16 | rh_spp | 每遍扫描数 | — |
| 0x56 | i16 | rh_linenum | 线号 | — |
| 0x58 | 8B | rh_coordY | Y 坐标 | — |
| 0x60 | u8 | rh_lineorder:4 / rh_slicetype:4 | 位域 | — |
| 0x61 | char | rh_dtype | 数据类型 | — |
| 0x62 | char[14] | rh_antname | **天线名** | 天线频率映射 (98) |
| 0x70 | u8 | rh_pass0TX:4 / rh_pass1TX:4 | 发射掩码 | — |
| 0x71 | u8 | rh_version:3 / rh_system:5 | 版本/系统 | — |
| 0x72 | char[12] | rh_name | **初始文件名** | 显示 (114, 截 \0) |
| 0x7E | i16 | rh_chksum | 头校验和 | — |
| 0x80 | 816B | variable | 可变数据区 | — |
| 0x3B0 | 80B | rh_RGPS[2] | GPS 记录(GGA) | — |

（偏移按十六进制; PDF 十进制 82=0x52、98=0x62、114=0x72、128=0x80、944=0x3B0）

## 3. tagRFDate 位域（4 字节, 小端打包）

```
sec2:5 (秒/2, 0-29) | min:6 | hour:5 | day:5 | month:4 | year:7 (年-1980)
```
程序 rdDate(32/36) 解码为 "Mon,DD YYYY,HH:MM:SS"；值为 0 显示 "00:00:00"。
写回（mdt@36）按同位域打包当前时间。

## 4. 数据区

- 组织: **扫描(道)主序** — 第 t 道第 s 采样位于 `dataOffset + (t×nsamp + s)×字长`
- 多通道: 逐通道交错
- 本程序样本: 32bit 小端有符号（i32, 采样值直接参与 LUT/调色板索引 `val/65536+128`）

## 5. ⭐ 操作记录（处理历史）— 位置与字段

| 项 | 值 |
|---|---|
| 偏移指针 | **rh_proc @0x30**（默认 128 = PARAREASIZE 之后） |
| 大小计数 | **rh_nproc @0x32**（字节数, 非条数） |
| 写入方式 | **追加式** — 新记录写在 `rh_proc + rh_nproc` 尾部, 然后回写 rh_nproc |
| 记录格式 | `typeId(1B) + sub(1B) + 参数(变长)` |

**已实证的记录**（时间零点, 本程序 saveProcessedFile 写出）：

```
0x4D 0x00 <f32 原rhf_position>
 │    │     └ 偏移量 ns (如 -5.263)
 │    └ sub=0 = 时间零点
 └ typeId=77(0x4d) — 与 DZX BinaryData typeId 空间一致
```

RADAN 处理后 DZT 的 proc history 同为追加式（非覆写）；配套 DZX 变干净（0 条 BinaryData，处理结果落 DZT）。typeId 含义见 `DZX文件格式破解.MD §7.3`。

## 6. 与官方 PDF 的差异 / 实测修正 ⚠

| 项 | PDF 说法 | 程序实测（RADAN 行为） |
|---|---|---|
| **@0x82** | rh_sconfig (i16, 设置配置号) | **时间零点主机参数 f32**（DZXDEMO: 3.966ns, "导入即可见, 读取不应用"） |
| 时间零点处理 | 未说明链路 | DZX typeId77 仅是记录; 实际数据上移用 **rhf_position@0x16**: `skip = nsamp×|position|/range`, 上移后底部补零 |
| rh_data | <1024 时偏移=1024×rh_data | 同（实测 rh_data=128 → 0x20000 头） |
| rh_nproc | 处理历史大小(字节) | 同; 且**追加式**, 每次 操作 +N 字节 |
| mtime 同步 | — | DZX 无 BinaryData 时 DZX mtime 同步 = DZT mtime（"已处理"标记） |

## 7. 本程序输出 DZT 的头修改清单（saveProcessedFile）

1. `rhf_position@0x16 = 0`（时间零点已处理）
2. proc history 追加 `0x4D 00 <f32 原position>`（§5 格式）, `rh_nproc@0x32` 回写
3. `rhb_mdt@0x24` = 当前时间（tagRFDate 打包）
4. 裁剪文件另做 `patchDztHeaderForTab` 头一致性补丁（道数等）
5. 输出路径 `proc/<原名>_P_NN.DZT`, 完成后作为新 tab 打开

## 8. 程序视角关键字段速查

```
读: nsamp@4  spm@14  position@22  range@26  cdt@32  mdt@36
    nproc@50  nchan@52  epsr@54  antname@98  name@114  数据偏移@2
写: position@22=0  proc追加@rh_proc+rh_nproc  nproc@50  mdt@36
```
