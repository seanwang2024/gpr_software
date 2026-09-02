# 项目上下文 (2026-09-02)

## 项目概述
地听(探地雷达)数据处理软件(v1.0.148起品牌: 劳雷→地听, MyQtApp.exe→depro.exe, LOGO=resources/diting_logo),
Qt 6.8.3 + MinGW + OpenCV 4.11.0 static, **当前版本 v1.0.168**(已发布FTP并验证).
主题系统: include/Theme.h 8个inline token(科技蓝#0048af / 岩土橙#ff2000, 默认橙), QSettings("Diting","depro"), TopBar⚙主题设计子菜单切换.

## 已完成模块(全部)
- **主页**(v1.0.87-97): TopBar+Ribbon 4组+文件头右栏350px+状态栏28px+色彩叠加(RADAN规律: 调色板[变换表(灰度)])
- **编辑**(v1.0.98-107): 编辑标记(MarkGroup DZX)+数据块(多矩形框+防重叠+保留唯一+裁剪)+横向缩放; 雷达缩略图蓝框上顶下底
- **数据解译**(v1.0.108-131): 追踪+异常标注+TargetGroup DZX兼容+数据导出CSV(RADAN 31列UTF-16LE)+图像导出(renderExportImage)
- **AI分析**(v1.0.13x-147, 第5标签): AI检测(框选aiRects/统计/病害列表)+AI报告(无检测时提示"请先进行AI检测"); ribbon组名"AI分析", 默认AI检测选中蓝底
- **品牌栏下拉**(v1.0.14x-15x): 打开/关闭/保存 + 数据组装(BDT)+工作路径+格式转换(DZT/DZX↔DT/DX) 三对话框已实现

## 数据解译要点(v1.0.108-131)
按 specs/软件需求20260817/.../数据解译-追踪异常.html:
- **Ribbon 3组**: 层位/目标追踪(自动magic_button/手动edit互斥) | 异常标注工具(圆/矩/多边形/文本四选一) | 解译成果导出(数据导出/图像导出)
- **右侧320px面板**: 层位列表(默认2层: 路基顶面#00ffff虚线/基底原土层#ff00ff实线) | 追踪控制 | 异常标注列表
- **异常列表交互**: ListRowWidget几何hit-test — 单击任意位置=切换选中, 双击=改名(不靠itemClicked不响应区)
- **TargetGroup持久化(v1.0.131)**: 异常写同名DZX原生`<TargetGroup>`(layerNum=7同RADAN钢筋/空洞);
  `<defaultVelocity>0.106[D1形状][D2D3字号][D4魔数9]`编码私有信息; 几何=TargetWayPt(圆=圆心+半径点/矩文本=TL+BR/多边形=N顶点);
  RADAN原生组导入(≥3点→多边形, ptRaw振幅往返保真); 旧InterpGroup仅读取回退; 编码详见 **DZX兼容编码方案.md**; flush=关闭/切换/退出时写
- **数据导出**: exportInterpCsv() 31列RADAN CSV UTF-16LE, 默认同名可改, 导出后QDesktopServices本机关联程序打开
- 数据解译页交互在 imageClicked lambda 内分发(仅 ribbonTab idx=3 拦截)

## PROCESS 标定(本软件↔RADAN, v1.0.150-160, 详见 **处理process文档.md**)
素材 `test_input_raw_files/process标定/`(原始1103_010.DZT 5953道×512采样 + Proc/P_n.DZT数值基准), 工具 `tools/process_calib.py`.
**已标定算法**(写进 MainWindow.cpp, 与RADAN逐字节/逐点对齐):
| 处理 | 算法 | 对齐度 | 版本 |
|---|---|---|---|
| 背景去除-全部 | 全局行均值扣除(整道均值, 非滑动窗); 窗口spin上限99999, ≥2×道数即全局 | 逐字节100% | v1.0.153 |
| FIR垂直高通 | x−MA(N), **N=round(2/3·fs/fc)**, 中心窗+边缘钳位 | 8位MAE=0.64灰阶 | v1.0.157 |
| FIR垂直低通 | MA(N), **N=round(0.443·fs/fc)**(−3dB=fc) | 8位MAE=0.19, 85%逐点相等 | v1.0.157 |
| FIR带通 | 上两者级联 | MAE=0.655, corr=0.986 | v1.0.157 |
| 自适应增益(2点Normal) | 指数dB线性斜率=20·log10(首尾1/4窗去DC均值绝对值比) + 逐道L1^0.3补偿 | MAE≈2灰阶, corr=0.975 | v1.0.158-159 |
| 时间零点-自动选峰 | 平均扫描前P%行取|峰|→pos_ns=p·range/N, 数据上移round(pos_ns)行 | 99.6%逐字节 | v1.0.160 |
**时间零点显示规则(v1.0.162-166, RADAN规律已闭环)**:
- 顶部死区: row0保留原值+row1=-2^24哨兵(黑线); 有效行(峰行−移位量起)竖向拉伸满高(图高不变=拉伸)
- 标尺 RANGE = range−|零点位置|(1位小数, 实测20−2.00=18); updateRulers()刷新
- 信号位置(ns)=2ns整倍数(ceil, 显示-2.00); **偏移量=位置−峰时间永远正值**(补偿整倍数剩下的值), 存待处理proc记录`4d 00 f32(正偏移)`
- 恢复(v1.0.167): 必须清零 zeroApplied/skipRows/topDead/posNs/offsetNs + updateRulers() 回满量程
**性能**: FIR前缀和maClamp O(1)内部(旧O(N)每样3M×85≈260M次>10s→现<1s); 样本访问memcpy
**待标定**: IIR(P_2, 200→256量化未复现) | 噪音带去除(P_4, 本程序未实现) | 反褶积(P_5) | 克西霍夫(P_6) | 希尔伯特能量/相位/频率(P_7/P_8/P_9/P_A, 频率RADAN自身报错) | P_E(0x1a sub1未识别)
**已证伪**: "所有P_n先应用头挂起时间零点" — P_3与原始零滞后corr=1.0, 头`4d 00`记录处理后保留未执行

## 一键处理(v1.0.168)
- 隧道衬砌标准流程5步(零点/背景/带通/增益/自适应增益), 状态栏分步进度条(stepBegin/stepTick: 步名+步内百分比, 每512道刷新, 完成100%后1s隐藏)
- 增益条目=自适应增益(AGC点数:2, 与RADAN对齐); 独立"增益(AGC)"条目已移除
- 开始/应用/恢复三按钮逻辑; 恢复走closeCurrentTab同链路复位

## DZX/DZT 格式逆向(历史成果)
完整typeId映射(27组文件交叉对照):

| typeId | 含义 | 格式 |
|---|---|---|
| 99 | DC去除(振幅偏移去除) | 无参数(空记录) |
| 77 | 时间零点 | f@0x0A=偏移量ns |
| 59 | 增益 | npts@9, dB@0x0B |
| 4 | IIR垂直 | coeff@2/@8→MHz=fs/(2π×c) + 阶数@1/@7 |
| 63-66 | FIR垂直(方块/三角×LP/HP) | coeff@1→MHz=1.17×fs/c |
| 13/14 | IIR水平(叠加/背景去除) | f@0x0A |
| 67-70 | FIR水平(方块/三角×平滑/背景去除) | f@0x09(DZX)/f@1(DZT) |
| 95 | 专用背景去除 | 5B: type(全部通过/扫描范围/自适应/无) |

typeId规律: 垂直=63+形×2+(HP?1:0), 水平=67+形×2+(背景去除?1:0); 形状: 方块=0三角=1

### DZT proc history 机制
- 处理历史存DZT头 rh_proc@0x30(=128)+rh_nproc@0x32; **追加式**, 每条 `typeId(1B)+sub(1B)+变长参数`
- 本轮实测: `40 00 f32 f32`(FIR) `04 01 f32`(IIR, 15.9155=50/π) `5f 00 00 00 00`(背景全部) `54 01`(噪音带) `24 3f …f32(4.0)`(反褶积) `1e 1f 05 …f32(10.0)`(克西霍夫) `1c 00/01/02 +f32`(希尔伯特) `4d 04 4B(10%→60)`(自动选峰)
- DZX处理后干净(0 BinaryData); 时间零点主机参数存DZT头0x82

### DZX BinaryData 偏移表
0x00-0x01记录长度 | 0x02-0x07固定头 | 0x08 typeId | 0x09点数/阶数 | 参数区0x09或0x0A起

## 工程陷阱(踩过的坑, 勿再踩)
- **QStringLiteral宏内不能拼接变量**: `" + Theme::pri + "`在宏内=编译错误, 必须拆成 `QStringLiteral("…") + Theme::pri + …`; 批量修复工具 tools/fix_qsl.py
- **QSS裸声明级联**: 父容器`background/border`裸声明会覆盖所有子控件(无视选择器特异性), 容器样式必须用限定选择器或去掉裸声明
- **closeTab sender()依赖**: lambda直调时sender()=nullptr静默返回 — 三入口架构 closeCurrentTab()/closeTabInGroup()/closeTab()
- **表格半构建期crash**: itemChanged刷新合计时读item(r,c)为nullptr — 建行期blockSignals+null守卫
- **QString格式双%**: `%1%%`显示错, 需转义
- **渲染自检**: 环境变量 GPR_SELF_RENDER/GPR_EXPORT_RENDER/GPR_AI_RENDER 离屏QWidget::grab()→PNG→PIL像素分析定位UI bug
- 升级重启(v1.0.69): ShellExecuteW启批处理(不共享AllocConsole), 批处理等PID→taskkill兜底→copy→start

## 历史修复(摘要)
- 采样点数自适应: pixelsPerRow=m_nsamp(非写死512), 256/512等(v1.0.62)
- 增益表gN=m_pixelsPerRow, 手柄Y跨度0..nsamp-1(v1.0.63)
- 颜色变换表20种LUT内嵌 s_cxLUTData[20][256], 源specs/颜色变换表.png(v1.0.82-84)
- 时间零点手动: skip=nsamp×|sigPos|/range, 数据上移底部补零(v1.0.67)

## 待做
- **PROCESS标定剩余**: IIR(P_2) / 噪音带去除(P_4未实现) / 反褶积(P_5) / 克西霍夫(P_6) / 希尔伯特(P_7-9,P_A) / P_E(0x1a)
- AI分析细化(检测统计/病害列表已通)
- 数据导出振幅/时间戳字段(占位)
- 编辑数据块用户回测; MarkGroup/TargetGroup RADAN实测(RADAN读/存后仍在)

## 关键文件
- **处理process文档.md** — PROCESS标定总表(op对照/用例corr/RADAN行为规律/未对齐清单)
- DZX兼容编码方案.md / DZT文件破解.MD / DZX文件破解.MD / BDT格式.md / DT和DX格式.md — 格式文档
- tools/process_calib.py(标定harness) / tools/fix_qsl.py(QStringLiteral修复)
- specs/软件需求20260817/软件需求20260817/UI/*.html — 各模块交互UI规格
- specs/RADAN_DZT_DZX生成规律.md; specs/color_transform_luts.json; specs/UI重构-主页-文件头.md
- test_input_raw_files/process标定/ — 标定素材; resources/ui_ref/*.png — UI参考(56个)
- include/Theme.h(主题token) / include/MatIcon.h(矢量图标, Material Symbols+JetBrains Mono字体resources/fonts/)

## 发布流程
1. 改 include/version.h APP_VERSION + installer/depro.iss MyAppVersion及OutputBaseFilename
2. cmake --build . --target depro (产物 depro.exe)
3. cp depro.exe /d/gpr_test/ (先删旧exe)
4. git add -A && git commit -m "[vX.Y.Z] …" && git push
5. curl FTP上传 depro.exe + version.json(downloadUrl/fileName=depro.exe) 到 seanwang.gotoftp5.com/wwwroot/
   (FTP用户seanwang 密码sean2020, 2026-09-02已验证登录230/上传/大小一致)
6. 安装包: installer/depro.iss → ISCC.exe → D:\gpr_release\depro_Setup_<ver>.exe (需先确保D:\gpr_test为最新Release部署)
