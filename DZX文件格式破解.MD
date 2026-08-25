# DZX 文件格式破解（程序实现视角）

> 2026-08-25 · 依据: 本程序 DZX 读写实现 + `test_input_raw_files/DZX格式反推测试/参数.md`
> （RADAN 对照 + PROJ 控制变量二进制 diff, 27 组样本交叉验证）
> 与 `docs/DZT File Format 4-13-15.pdf` 的差异见 **§9**。操作记录见 **§7/§8**。

---

## 1. 总体结构

DZX = **纯文本 XML**（UTF-8, 行尾 CRLF, 缩进 2 空格）：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<DZX xmlns="www.geophysical.com/DZX/1.02">
  <GlobalProperties>…</GlobalProperties>
  <File><Profile>…WayPt 航点…</Profile></File>
  <LayerGroup>…层位…</LayerGroup> ×N
  <TargetGroup>…目标…</TargetGroup> ×N
  <DataCollection>…采集信息…</DataCollection>
  <BinaryData>…处理记录(二进制转义)…</BinaryData> ×N
</DZX>
```

- 与同名 DZT 配套（`xxx.DZT` ↔ `xxx.DZT` 同目录同名）；**mtime 与 DZT 一致**表示"已同步处理"。
- 本软件自有后缀 `.DX` 与 `.DZX` **字节级相同**（见 `DT和DX格式.md`）。

## 2. GlobalProperties（程序 DZX 写出格式）

```xml
<GlobalProperties>
  <verticalUnit>m</verticalUnit>
  <horizontalUnit>m</horizontalUnit>
  <dielectric>6.0</dielectric>          <!-- 介电常数 εr -->
  <readOnly>0</readOnly>
  <unitsPerMark>0.0000000</unitsPerMark>
  <unitsPerScan>0.0200000</unitsPerScan> <!-- 米/道 = 1/spm -->
</GlobalProperties>
```

## 3. File/Profile/WayPt（航点 = 本程序"编辑标记"）

```xml
<File>
  <Profile>
    <scanRange>0,1732</scanRange>
    <WayPt>
      <scan>141</scan>          <!-- 道号 -->
      <distance>2.8200000</distance>      <!-- 边界点才有 -->
      <localCoords>0,0,0</localCoords>
    </WayPt>
    <WayPt>                     <!-- 用户标记点: 有 mark+name -->
      <scan>141</scan>
      <mark>Combo</mark>
      <name>MARK01</name>       <!-- 本程序命名 MARK%02d, RADAN 显示 -->
    </WayPt>
  </Profile>
</File>
```

- **判别规则（程序实现）**：`<WayPt>` 含 `<name>` = 用户标记（可增删）；只有 scan+distance = 采集边界点（保留不动）。
- 每个 MARK 配一条 `<WayPtNameProperties><name>MARKnn</name>…`。
- 命名规律: `MARK%02d`（1 起）；CSV 导出同规则。

## 4. LayerGroup（层位点, RADAN 原生）

```xml
<LayerGroup>
  <groupName>第1层</groupName>
  <display>1</display>              <!-- 1=显示 -->
  <color>0</color>                  <!-- RADAN 色索引 -->
  <sizePx>4</sizePx>                <!-- 圆点大小 -->
  <outline>1</outline>
  <readOnly>0</readOnly>
  <diameter>0.0000000</diameter>
  <link>1</link>
  <pickType>0</pickType>
  <layerNum>0</layerNum>            <!-- 0..6 = 第1~7层 -->
  <velMethod>0</velMethod>
  <lockVelocity>0</lockVelocity>
  <defaultVelocity>0.1060000</defaultVelocity>
  <LayerWayPt>
    <scanSampChanProp>400,143,0,0</scanSampChanProp>   <!-- scan,samp,chan,prop -->
    <timeAmpDepVel>0.0000000,0.0000000,0.0000000,0.1060000</timeAmpDepVel>
  </LayerWayPt> ×N
</LayerGroup>
```

- 点按 trace 升序；曲线显示 = 点连线 + 彩色圆点。
- 本程序写出时 `defaultVelocity` 固定 0.1060000, 无 timeAmpDepVel 也可被 RADAN 读取。

## 5. TargetGroup（目标解释 = 钢筋/空洞类 + 本程序异常标注）

```xml
<TargetGroup>
  <groupName>钢筋</groupName>
  <display>1</display>
  <color>13</color>
  <sizePx>3</sizePx>
  <outline>1</outline>
  <readOnly>0</readOnly>
  <diameter>0.1200000</diameter>
  <link>1</link>
  <pickType>1</pickType>
  <layerNum>7</layerNum>            <!-- ★ TargetGroup 恒为 7 -->
  <velMethod>0</velMethod>
  <lockVelocity>0</lockVelocity>
  <defaultVelocity>0.1060009</defaultVelocity>
  <TargetWayPt>
    <scanSampChanProp>187,179,0,0</scanSampChanProp>
    <timeAmpDepVel>13.9843750,2202487.0000000,0.7372444,0.1060000</timeAmpDepVel>
    <!--            时间ns    振幅        极性      速度 -->
  </TargetWayPt> ×N
</TargetGroup>
```

**`<defaultVelocity>` 尾 4 位 = 本程序私有编码**（RADAN 视为冗余精度不改不删, 详见 `DZX兼容编码方案.md`）：

```
0.106 [D1形状][D2D3字号][D4魔数9]   D1: 0圆1矩2多边形3文本
```

## 6. InterpGroup（本程序旧版自定义, 已废弃→TargetGroup）

```xml
<InterpGroup>
  <Horizon name="路基顶面" color="#00ffff" width="2" visible="1" dashed="1">
    <Pt><scan>50</scan><samp>200</samp></Pt>…
  </Horizon>
  <Anomaly shape="0" name="异常标注1" remark="" color="#ffff00" font="Microsoft YaHei" fontSize="12">
    <Rect>100,50,160,90</Rect>     <!-- 或 <Pt> 多边形顶点 -->
  </Anomaly>
</InterpGroup>
```

⚠ RADAN 读取时会**删除未知 group** → 已改用 TargetGroup；读取仅作旧文件回退。

## 7. BinaryData —— DZX 处理操作记录（位置与格式）⭐

### 7.1 记录位置
- DZX 内 `<BinaryData>…</BinaryData>` 元素, **每条 = 一步处理**, 出现顺序 = RADAN 处理顺序。
- DZX 经 RADAN 处理后**干净**（0 条 BinaryData, 结果写回 DZT）。

### 7.2 记录格式（blob 头 + 参数区, 全部小端）

| 偏移 | 类型 | 含义 |
|---|---|---|
| 0x00-0x01 | u16 | 记录声明字节数（0x002C/0x0096/0x0048…） |
| 0x02-0x07 | 6B | 固定头 `01 00 00 02 <sz> 00` |
| 0x08 | u8 | **typeId**（处理类型） |
| 0x09 | u8 | 点数/阶数（增益=点数, IIR垂直=8） |
| 0x0A 起 | — | 参数区（随 typeId 不同, 见下表） |

### 7.3 typeId 完整映射（27 组样本交叉验证）

| typeId | 处理 | 参数偏移与格式 | DZXDEMO 实测 |
|---|---|---|---|
| **99** | DC去除（振幅偏移去除） | 无参数（空记录, 18B） | — |
| **77** | 时间零点 | 修整量 f32@0x0A; range f32@0x16 | -5.263 / 105.263 |
| **59** | 增益 | 点数 u8@0x09; dB f32@0x0B 起每 4B | 3 点 [50,60,70] |
| **4** | IIR 垂直带通 | LP u16@0x20; HP u16@0x22; 采样末 u16@0x26 | 310 / 25 / 485 |
| **63** | FIR 垂直低通 | LP u16@0x1E; 采样末 u16@0x24 | PROJ__008=800 |
| **64** | FIR 垂直高通 | HP u16@0x20; 采样末 u16@0x24 | 190 / 485 |
| **63-66** | FIR 垂直族 | 63+形×2+(HP?1:0), 形: 方块=0 三角=1 | |
| **13/14** | IIR 水平（叠加/背景去除） | 截止 f32@0x0A（扫描数） | 14: 16.0 |
| **67-70** | FIR 水平族（平滑/背景去除） | 长度 f32@0x0A（DZX）/ f32@0x1（DZT 内） | 67: 21.0 |
| **95** | 专用背景去除 | 5B: type(全部通过/扫描范围/自适应/无) | |

**typeId 规律**: 垂直 FIR = `63+形×2+(HP?1:0)`, 水平 FIR = `67+形×2+(背景去除?1:0)`。
滤波频率单位: 垂直 = u16 **MHz**; 水平 = f32 **扫描数**。

### 7.4 与直觉相反的两点（PDF 没写清）
1. PROJ 控制实验的"高通/低通"**标签与实际相反**（二进制 diff 证实）: typeId4 @0x20=LP、@0x22=HP; typeId64 @0x20=HP。以 RADAN 对照值为准。
2. typeId 99/77（DC去除/时间零点）是**主机参数记录**, PROCESS 导入时跳过; 时间零点实际用 DZT 头 `rhf_position`(offset 22) 做数据上移。

## 8. DZT 头内操作记录（处理历史）⭐

| 项 | 值 |
|---|---|
| 位置 | DZT 头 **offset 128 起**（rh_nproc 计数 @ offset 50） |
| 写入方式 | **追加式**（非覆写）— 每次处理操作在尾部追加一条记录 |
| 时间零点记录 | typeId 0x4d, sub=0 + f32(rhf_position 原值) |
| 时间零点主机参数 | DZT 头 **offset 0x82**（f32, ns; 导入即可见, 读取不应用） |
| 处理后输出 | rhf_position 归零, 编辑时间更新, proc history 追加 |

## 9. 与 `docs/DZT File Format 4-13-15.pdf` 的差异（程序实测结论）

| 项 | PDF | 程序实测（更清晰/修正） |
|---|---|---|
| 时间零点 | 归入处理链 | = **主机参数**存 DZT 0x82; DZX typeId77 仅记录; 实际位移用 rhf_position@22 |
| DC去除 typeId | — | 99, 空记录 |
| 垂直滤波偏移 | 未细化 | IIR4: LP@0x20/HP@0x22; FIR63: LP@0x1E; FIR64: HP@0x20 |
| proc history | — | 追加式; rh_nproc@50 计数 |
| BinaryData 头 | — | u16 长度 + 6B 固定头 `01 00 00 02 sz 00` + typeId@8 |
| TargetGroup | — | layerNum 恒 7; velocity 尾 4 位冗余可作私有编码 |

## 10. 本程序 DZX 写入规则（文本级手术）

- MarkGroup/TargetGroup/LayerGroup 写入 = 删旧段 → 原位置/DataCollection 前插新段, **其余字节不动**
- 无 BinaryData 时同步 DZX mtime = DZT mtime
- flush 时机 = RADAN 规律: 关闭文件/切换文件/退出程序（平时全内存）
