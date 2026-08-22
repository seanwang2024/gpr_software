# DZX 兼容编码方案（异常标注 ↔ RADAN TargetGroup）

> 版本: v1.0.131 (2026-08-22)
> 目的: 异常标注(圆/矩/闭合多边形/文本)持久化到 DZX, RADAN 读取/保存均不丢失。

## 1. 背景

- 旧方案写自定义 `<InterpGroup>`: RADAN 每次读该 DZX 会**删除未知 group**, 异常数据丢失。
- 解剖 `PRV01__072 P_1.DZX` 发现 RADAN "目标解释"对象(钢筋/空洞)即 `<TargetGroup>`:
  - `<layerNum>` 固定 **7** (LayerGroup 为 0-6, TargetGroup 全部 7)
  - `<pickType>` 1, `<diameter>` 0.1200000, 点元素 `<TargetWayPt>`
  - `<defaultVelocity>` 固定 **7 位小数**(如 0.1060000 / 0.1070000)
- RADAN 把 velocity 当浮点读取, 末 4 位精度(0.0001 m/ns 级)对其为**冗余信息**, 不修改 → 可作私有存储空间。

## 2. 编码: defaultVelocity 尾 4 位

```
<defaultVelocity>0.106 D1 D2D3 D4</defaultVelocity>
                         │  │   └─ D4: 魔数=9 (本软件写入标记)
                         │  └───── D2D3: 字号 00-99 (所有形状均存默认12, 非文本形状读取时忽略)
                         └──────── D1: 形状 0圆 1矩 2闭合多边形 3文本
```

| 异常类型 | velocity 值 | 编码 |
|---|---|---|
| 圆 | 0.1060129 | 0+12+9 |
| 矩形 | 0.1061129 | 1+12+9 |
| 闭合多边形 | 0.1062129 | 2+12+9 |
| 文本 12px | 0.1063129 | 3+12+9 |
| 文本 16px | 0.1063169 | 3+16+9 |

- 速度基准 0.106 m/ns(混凝土), 编码后偏差 < 0.0005 m/ns (<0.5%), RADAN 深度计算影响可忽略。
- **D4≠9 → RADAN 原生组**(如钢筋 0.1060000 / 空洞 0.1070000)。

## 3. 几何映射 (TargetWayPt)

`<TargetWayPt><scanSampChanProp>scan,samp,0,0</scanSampChanProp>` (整数, 取整):

| 形状 | 点数 | 含义 | 还原 |
|---|---|---|---|
| 圆 | 2 | 圆心, 圆心+半径点(x向) | 外接框 = (cx−r, cy−r, 2r, 2r) |
| 矩形 | 2 | TL, BR | rect = TL∪BR |
| 文本 | 2 | 文本框 TL, BR | rect + D2D3 字号 |
| 多边形 | N≥3 | 顶点顺序 | poly |

每点带 `<timeAmpDepVel>0,0,0,VEL</timeAmpDepVel>`(VEL=同组 velocity 值, 对齐 RADAN 原生格式)。

## 4. 字段对照 (异常标注 ↔ TargetGroup)

| 本程序 | TargetGroup 字段 | 说明 |
|---|---|---|
| name | `<groupName>` | 默认"异常标注N"; 用户改名(如"钢筋")原样存, RADAN 直接显示 |
| shape | velocity D1 | 见编码 |
| fontSize | velocity D2D3 | 仅文本 |
| 几何 | TargetWayPt 点列 | 见上表 |
| color | `<color>` 0-3 | RADAN 16色索引(圆黄/矩红/多边绿/文本蓝), 仅 RADAN 显示用; 本程序读回按形状默认色 |
| (无) | display/sizePx/outline/readOnly/diameter/link/pickType/layerNum/velMethod/lockVelocity | 按 RADAN 样例固定: 1/3/1/0/0.12/1/**1**/**7**/0/0 |
| remark | — | 无原生字段、编码位不足 → 会话内保留不持久化(CSV 导出含) |

## 5. 读写规则

**读** (`readDzxTargets`):
1. 遍历全部 `<TargetGroup>`; D4=9 → 本软件异常, 按编码+点列还原。
2. D4≠9 → RADAN 原生目标: ≥3 点 → 导入为闭合多边形; 1-2 点 → 首点处默认尺寸圆(60×40); 名称 = groupName。
3. 旧版 `<InterpGroup>` 回退: 仅当无任何 TargetGroup 异常时读入(下次保存自动迁移为 TargetGroup)。

**写** (`writeDzxTargets`, 文本级手术):
1. 删除 DZX 内全部 `<TargetGroup>...</TargetGroup>` 区段(含前导缩进与换行)。
2. 每个已赋形异常(shape≥0, 多边形≥3点, 框形≥1×1)生成一个 TargetGroup 块。
3. 插入到 `<DataCollection>` 前(无则 `</DZX>` 前); 其余字节原样保留。
4. 无 `<BinaryData>` 时同步 DZX mtime = DZT mtime(与 MarkGroup/LayerGroup 一致)。

**flush 时机**(RADAN 规律): 仅关闭文件/切换文件/退出程序时写入, 平时全部在内存。

## 6. 往返验证链

```
本程序 ──写──▶ DZX(TargetGroup+velocity编码) ──读──▶ RADAN 显示"钢筋"类目标
  ▲                                                    │ RADAN 保存(不删TargetGroup,
  └────────── readDzxTargets 还原 ◀── velocity 尾4位存活 ◀┘ 不改冗余位)
```

RADAN 原生文件(钢筋/空洞) → 本程序导入为多边形/圆 → 写回仍为 TargetGroup, RADAN 可继续编辑。

## 7. 示例: 一个"钢筋"圆异常 (圆心 100,50 半径30)

```xml
  <TargetGroup>
    <groupName>钢筋</groupName>
    <display>1</display>
    <color>0</color>
    <sizePx>3</sizePx>
    <outline>1</outline>
    <readOnly>0</readOnly>
    <diameter>0.1200000</diameter>
    <link>1</link>
    <pickType>1</pickType>
    <layerNum>7</layerNum>
    <velMethod>0</velMethod>
    <lockVelocity>0</lockVelocity>
    <defaultVelocity>0.1060129</defaultVelocity>
    <TargetWayPt>
      <scanSampChanProp>100,50,0,0</scanSampChanProp>
      <timeAmpDepVel>0.0000000,0.0000000,0.0000000,0.1060129</timeAmpDepVel>
    </TargetWayPt>
    <TargetWayPt>
      <scanSampChanProp>130,50,0,0</scanSampChanProp>
      <timeAmpDepVel>0.0000000,0.0000000,0.0000000,0.1060129</timeAmpDepVel>
    </TargetWayPt>
  </TargetGroup>
```

另: RADAN 原生导入的目标(钢筋散点→多边形), 各点原始 `<timeAmpDepVel>`(含振幅)在写回时**原样保留**(点数不变时), 往返不丢振幅数据。
