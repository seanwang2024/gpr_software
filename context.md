# 项目上下文 (2026-08-19)

## 项目概述
劳雷GPR(探地雷达)数据处理软件,Qt 6.8.3 + MinGW + OpenCV 4.11.0 static, 当前版本 v1.0.87

## 当前工作: UI重构
按 `specs/软件需求20260817/软件需求20260817/UI/主页-文件头.png`(同源HTML=精确规格)彻底重构

### 已完成(v1.0.87 头部彻底重构):
- **图标系统**: 内嵌 Material Symbols Outlined 矢量字体(设计稿同款) + JetBrains Mono
  - `include/MatIcon.h/.cpp`: MatIcon::icon(name,color,checked,hover,size,fill) 按码点渲染
  - resources/fonts/ 三件套(ttf+codepoints+mono), qrc /fonts 前缀
- **顶栏 TopBar** (`include/TopBar.h/.cpp`, 40px, 替代旧CustomTitleBar已删除):
  劳雷▾品牌下拉(打开/关闭/保存+数据组装/工作路径/格式转换占位) | 5模块标签(互斥,active=蓝字+底2px蓝线)
  | 右上角⚙(关于+检查升级)/?(占位)/◯(占位) | —□×
- **Ribbon 120px**: tabBar隐藏,模块切换由TopBar驱动; 主页4组(组名在底+竖分隔线):
  文件操作 | 图像显示(线扫描/线扫描+波形互斥active=#1e60d5, 波列图独立checkable=m_btnStack)
  | 色彩渲染=两行下拉框样式(挂原30调色板/20变换表菜单) | 数据信息(文件头toggle)
- **文件头右侧350px栏**: 标题栏40px(文件头属性+✕) + 8行两列表格(键白/值等宽mono),
  字段: 文件名/天线频率(型号→MHz表)/采样点数/总道数/时窗/介电常数/采集日期/道间距
  - readDztHeaderInfo()/createHeaderPanel()/setHeaderPanelVisible()/refreshHeaderPanel()
  - 切tab刷新, 无文件收起, toggle后重定位悬浮切换按钮
- **状态栏28px**: 左●就绪 | 右mono"道号:N 深度:X m"(其余3项进tooltip)+进度条
- 删除: 主页简易处理/其他组(功能在数据处理tab)、旧缩放按钮、左侧树形文件头面板
- 配色token: #0048af主色(原#004aae)/#f8f9ff面/#dee9fc悬停/#d9e3f6状态栏/#c3c6d6线

### v1.0.85-1.0.86(已被1.0.87替代):
- 5个菜单标签框架/波列图切换/浅蓝主题雏形

## DZX PROCESS 逆向(已完成)
完整typeId映射已验证(27组文件交叉对照):

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

**typeId规律**: 垂直=63+形×2+(HP?1:0), 水平=67+形×2+(背景去除?1:0)
形状: 方块=0, 三角=1

### DZT proc history 机制
- 处理历史存DZT头 offset 128+ (rh_nproc@50)
- **追加式**(非覆写), 每次操作追加记录
- DZX 处理后干净(0 BinaryData)
- 时间零点主机参数存DZT头 0x82

### DZX BinaryData 偏移表
| 偏移 | 含义 |
|---|---|
| 0x00-0x01 | 记录长度 |
| 0x02-0x07 | 固定头 |
| 0x08 | typeId |
| 0x09 | 点数/阶数 |
| 参数区 | 0x09或0x0A起 |

## 时间零点处理(已实现 v1.0.67+)
- 读 rhf_position(offset 22)
- skip = nsamp×|sigPos|/rhf_range
- 数据上移, 底部补零, 显示 drawRows=nsamp-skip
- B-SCAN只显示有效行(486), 无底部零条
- 波形Y轴保持 0-511 不变
- 处理后DZT: proc history追加 0x4d sub=0 + float(rhf_position原值)
- 输出DZT头: rhf_position归零, 编辑时间更新

## 颜色变换表(v1.0.82-1.0.84)
- 20种灰度映射LUT, 从PNG精确提取
- 内嵌到代码 s_cxLUTData[20][256], 不需要外部bin
- LUT数据来源: specs/颜色变换表.png, bar位置(2, 21+i*16)-(257, 32+i*16)

## 升级重启(已修复 v1.0.69)
- 用 ShellExecuteW 启动批处理(不共享AllocConsole控制台)
- 批处理: 等PID退出→taskkill兜底→copy覆盖→start新版本

## 采样点数(已修复 v1.0.62-1.0.63)
- pixelsPerRow = m_nsamp(从文件头读, 非写死512)
- 256/512等自适应

## 增益(已修复 v1.0.63)
- 增益表gN=m_pixelsPerRow(4处: applyGain/saveProcessedFile/一键×2)
- 手柄Y跨度0..nsamp-1 (CustomChartView::setSampleCount)
- 结束点=nsamp-1(m_gainSampleEndItem)

## 关键文件
- `specs/UI重构-主页-文件头.md` — UI规格
- `specs/RADAN_DZT_DZX生成规律.md` — RADAN文件生成机制
- `test_input_raw_files/DZX格式反推测试/参数.md` — DZX完整解码方案
- `test_input_raw_files/DZX格式反推测试/*.py` — 解码工具
- `resources/ui_ref/*.png` — UI参考图标(56个)
- `specs/color_transform_luts.json` — 颜色变换LUT数据

## 发布流程
1. 改 version.h APP_VERSION
2. cmake --build . --target MyQtApp
3. cp MyQtApp.exe /d/gpr_test/
4. git add -A && git commit && git push
5. curl FTP上传 exe + version.json 到 seanwang.gotoftp5.com/wwwroot/
6. FTP密码: sean2020

## 安装包
installer/MyQtApp.iss → ISCC.exe → D:\gpr_release\MyQtApp_Setup_<ver>.exe
