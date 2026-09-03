# 项目上下文 (2026-09-03)

## 项目概述
地听(探地雷达)数据处理软件(v1.0.148起品牌: 劳雷→地听, MyQtApp.exe→depro.exe, LOGO=resources/diting_logo),
Qt 6.8.3 + MinGW + OpenCV 4.11.0 static, **当前版本 v1.0.186**(已发布FTP+安装包并验证).
主题系统: include/Theme.h 8个inline token(科技蓝#0048af / 岩土橙#ff2000, 默认橙), QSettings("Diting","depro"), TopBar⚙主题设计子菜单切换.
构建双轨: CMakeLists.txt(主) + MyQtApp.pro(Qt Creator, 需与CMake同步增删源文件).

## 已完成模块(全部)
- **主页**(v1.0.87-97): TopBar+Ribbon 4组+文件头右栏350px+状态栏28px+色彩叠加(RADAN规律: 调色板[变换表(灰度)])
- **编辑**(v1.0.98-107): 编辑标记(MarkGroup DZX)+数据块(多矩形框+防重叠+保留唯一+裁剪)+横向缩放; 雷达缩略图蓝框上顶下底
- **数据解译**(v1.0.108-131): 追踪+异常标注+TargetGroup DZX兼容+数据导出CSV(RADAN 31列UTF-16LE)+图像导出(renderExportImage)
- **AI分析**(v1.0.13x-147, 第5标签): AI检测(框选aiRects/统计/病害列表)+AI报告; **v1.0.169起未激活锁定, 入口拦截弹授权窗**
- **品牌栏下拉**(v1.0.14x-15x): 打开/关闭/保存 + 数据组装(BDT)+工作路径+格式转换(DZT/DZX↔DT/DX) 三对话框已实现
- **授权License**(v1.0.169): 见下节

## 授权系统(v1.0.169, specs/license管理需求.md)
- **服务端** `server/license/`(已部署 www.sxfpga.cn/license/): PHP7.4+MySQL, api/{activate,unbind,verify}.php, admin/(登录/批量生成/解绑/作废/恢复/CSV/日志/管理员), install.php建表lic_admins/lic_keys/lic_logs
  管理后台 admin/Diting2026(super); Key格式DT-XXXXX-XXXXX-XXXXX(32字符集去0O1I); credential=RSA-SHA256签名payload的base64, PHP openssl_sign PKCS#1v1.5
- **客户端** include/License.h + src/License.cpp: 验签用**Windows CryptoAPI**(CryptStringToBinaryA→CryptDecodeObjectEx→CryptImportPublicKeyInfo→CryptVerifySignature, **签名字节序需反转**), CMake链crypt32/advapi32
  deviceId=SHA256(MachineGuid|卷序列号)64hex; 凭证存QSettings license/credential; 巡检24h周期, 网络失败静默放行(离线靠本地凭证)
- **门控**: 仅锁AI分析(其余全可用); TopBar🔑图标(showLicenseDialog); 三处拦截: moduleChanged idx==4 / AI检测+AI报告按钮 / runAiDetection+showAiReportDialog入口
- 自检: GPR_LICENSE_SELFTEST=1 内置测试向量验签
- 接口文档: **业务云接入-授权云接口文档.md**(X-Client-Key/X-Server-Secret双通道, verify支持业务云二校验)
- 私钥只存服务器config.php, 仓库模板占位; server/license/secrets/已gitignore

## 文件关联(v1.0.171-174)
- .DT显示地听图标(specs/地听文件LOGO.jpg→installer/diting_logo.ico), .DX无图标; 双击.DT打开depro.exe(注册表+openFileFromShell)
- .DT可拖入APP(dragEnterEvent与dropEvent都要接受.dt, 只改一处=无声失败)
- Win11兼容加固v1.0.171(DPI/缺DLL兜底), 待用户实测

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

## PROCESS 标定(本软件↔RADAN, v1.0.150-185, 详见 **处理process文档.md**)
素材 `test_input_raw_files/process标定/`(原始1103_010.DZT 5953道×512采样 + Proc/P_n.DZT数值基准 + PRV01_72等外场数据), 工具 `tools/process_calib.py`.
**已标定并C++落地**(MainWindow.cpp, 与RADAN逐字节/逐点对齐):
| 处理 | 算法 | 对齐度 | 版本 |
|---|---|---|---|
| 背景去除-全部 | 全局行均值扣除; 窗口spin上限99999, ≥2×道数即全局 | 逐字节100% | v1.0.153 |
| FIR垂直高通 | x−MA(N), **N=round(2/3·fs/fc)**, 中心窗+边缘钳位 | 8位MAE=0.64灰阶 | v1.0.157 |
| FIR垂直低通 | MA(N), **N=round(0.443·fs/fc)**(−3dB=fc) | 8位MAE=0.19, 85%逐点相等 | v1.0.157 |
| FIR带通 | 上两者级联 | MAE=0.655, corr=0.986 | v1.0.157 |
| 自适应增益(2点Normal) | 指数dB线性斜率=20·log10(首尾1/4窗去DC均值绝对值比) + 逐道L1^0.3补偿 | MAE≈2灰阶, corr=0.975 | v1.0.158-159 |
| 时间零点-自动选峰 | 平均扫描前P%行取\|峰\|→posNs, **skip=qRound(posNs)直接ns数(非样本换算!)**, 数据上移skip行 | 99.6%逐字节 | v1.0.160/184 |
| IIR垂直(P_J/P_K) | **频域Butterworth窗零相位**(FFT→\|H\|→IFFT): n=极点=sub字节, α窗公式, 增益K=0.244·√(bw/100)·n^0.21; proc写`04 sub f32(c=fs/2πfc)` | 深层corr>0.99 | v1.0.178 |
| 克西霍夫偏移(P_L-PO/PRV01) | 双曲线sIn=√(sOut²+(kΔx)²), **k=2·DX·100/(v_cm·dt_ns)**, DX=0.01m(扫描/单位100=100道/m); 速度=proc斜率×512(slope=0.566标志时取vlc文件); 线性插值孔径求和/nAp×Gain; row0保留+row1哨兵 | 深层corr 0.99+ | v1.0.179 |
| 反褶积(P_6) | 逐道Wiener预测误差: 有偏自相关r[k]=Σx·x/N, 白化r[0]×(1+0.10), 31×31 Toeplitz高斯消元, e[k]=x[k]−Σf[i]x[k−lag−i] | 已落地 | v1.0.179 |
**用户豁免(不再标)**: 噪音带去除(P_4) | 希尔伯特能量/相位/频率(P_7/P_8/P_9/P_A)
**自动增益(typeId 0x1a)已完全破解+落地(v1.0.186, §9)**:
- proc `1a <点数> 00 f32@3(整体增益) f32@7(水平时常)` 11B非对齐 + DZT头0x86镜像
- 算法: 节点k·nsamp/(npts-1)等距, 能量窗=**帽子支集**(相邻节点中点间), **g_k(t)=C·因果指数平滑(E_全道/E_帽窗)**,
  α=1−exp(−1/TC), 深度增益域线性插值, C=0.721+0.26775·整体增益(经验线性非dB), row0保留+row1哨兵, 不钳位
- 端到端: P_S 0.999995(C++实测) / P_T 0.998 / P_R(点数1≈常数0.722) 0.99997
- C++: applyRadanAutoGain() 增益面板"增益类型=自动"(点数/整体增益/水平时长三参数), 自检GPR_AGC_TEST=1
- 待释: C(og)线性外推og>2待第三样本; P_R逐道wiggle 0.58%幅度来源
**时间零点显示规则(v1.0.162-166)**:
- 顶部死区: row0保留原值+row1=-2^24哨兵(黑线); 有效行(峰行−移位量起)竖向拉伸满高(图高不变=拉伸)
- 标尺 RANGE = range−|零点位置|(1位小数); updateRulers()刷新
- 信号位置(ns)=2ns整倍数(ceil); 偏移量=位置−峰时间永远正值, 存proc `4d 00 f32(正偏移)`
- 恢复: 清零 zeroApplied/skipRows/topDead/posNs/offsetNs + updateRulers()
**已证伪**: "所有P_n先应用头挂起时间零点" — P_3与原始零滞后corr=1.0, 头`4d 00`记录处理后保留未执行
**性能**: FIR前缀和maClamp O(1)内部(旧O(N)>10s→现<1s); 样本访问memcpy

## 数据处理ribbon重构(v1.0.180-185, 按specs/.../数据处理-增益.html+PNG)
- **一键处理** | **自定义处理**(一大组内3小组, 灰色竖线区隔): [时间零点|偏移] | [滤波|距离归一化|背景清除] | [增益|高级滤波]; 处理范围组不显示
- 互斥QButtonGroup(m_btnOneClick+7按钮), 默认一键处理选中; 切换=closeAllProcDialogs()关闭上一个设置窗+隐藏左面板
- 选中样式=浅橙底+橙边框; makeProcBtn工厂(icon+下方文字); 小组竖线=QFrame NoFrame+WA_StyledBackground+setFixedSize(1,64)(sizeHint高0坑)
- 图标与一键处理水平齐平: g1btns AlignTop + contentsMargins(0,10,0,0)(v1.0.185下移10px)
- 设置窗维持原9个对话框(m_zeroDlg/m_gainDlg/...), 不用右侧参数面板

## 一键处理(v1.0.168)
- 隧道衬砌标准流程5步(零点/背景/带通/增益/自适应增益), 状态栏分步进度条(stepBegin/stepTick, 每512道刷新, 完成100%后1s隐藏)
- 增益条目=自适应增益(AGC点数:2); 开始/应用/恢复三按钮; 恢复走closeCurrentTab同链路复位

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
| 0x1a(26) | **自动增益** | 点数@1, f32@3(整体增益), f32@7(水平时常), 非对齐 |

typeId规律: 垂直=63+形×2+(HP?1:0), 水平=67+形×2+(背景去除?1:0); 形状: 方块=0三角=1

### DZT proc history 机制
- 处理历史存DZT头 rh_proc@0x30(=128)+rh_nproc@0x32; **追加式**, 每条 `typeId(1B)+sub(1B)+变长参数`
- 本轮实测: `40 00 f32 f32`(FIR) `04 01 f32`(IIR) `5f 00 00 00 00`(背景全部) `54 01`(噪音带) `24 3f …f32`(反褶积`1e <op> <lag> f32(白化)`) `1e 1f 05 …f32`(克西霍夫`24 <宽度> f32(斜率) 1b0100 f32(增益)`) `1c 00/01/02 +f32`(希尔伯特) `4d 04 4B`(自动选峰) `1a <点数> <1B> f32 f32`(自动增益)
- DZX处理后干净(0 BinaryData); 时间零点主机参数存DZT头0x82

### DZX BinaryData 偏移表
0x00-0x01记录长度 | 0x02-0x07固定头 | 0x08 typeId | 0x09点数/阶数 | 参数区0x09或0x0A起

## 工程陷阱(踩过的坑, 勿再踩)
- **QStringLiteral宏内不能拼接变量**: 必须拆成 `QStringLiteral("…") + Theme::pri + …`; 批量修复工具 tools/fix_qsl.py
- **QSS裸声明级联**: 父容器`background/border`裸声明覆盖所有子控件, 容器样式必须限定选择器
- **closeTab sender()依赖**: lambda直调时sender()=nullptr — 三入口架构
- **表格半构建期crash**: itemChanged读item为nullptr — 建行期blockSignals+null守卫
- **QString格式双%**: `%1%%`需转义
- **Qt Creator链接错undefined reference**: MyQtApp.pro漏源文件(License.cpp) — 新增文件必须CMake+pro双同步
- **QFrame竖线QSS不渲染**: NoFrame+WA_StyledBackground+setFixedSize(1,64)(sizeHint高度0)
- **拖拽只改dropEvent**: dragEnterEvent也要同步接受.dt, 否则无声失败
- **一键处理预设切换crash**: 移除条目后m_ocAgc悬空 — null守卫再setChecked
- **时间零点skip单位**: qRound(posNs×nsamp/range)=50错, RADAN=qRound(posNs)=2(直接ns数)
- **行std分析被哨兵污染**: row1=-2^24哨兵会抬高std统计, 分析前先剔哨兵行
- **中文路径python脚本**: 首行`# -*- coding: utf-8 -*-`或glob/os.listdir导航
- **渲染自检**: GPR_SELF_RENDER/GPR_EXPORT_RENDER/GPR_AI_RENDER/GPR_TAB_RENDER/GPR_ZERO_TEST/GPR_LICENSE_SELFTEST 离屏渲染/算法自测
- 升级重启(v1.0.69): ShellExecuteW启批处理, 批处理等PID→taskkill兜底→copy→start
- 构建产物qmake_build/qtc_build勿入库(已gitignore+git rm --cached)

## 历史修复(摘要)
- 采样点数自适应: pixelsPerRow=m_nsamp(v1.0.62); 增益表gN=m_pixelsPerRow(v1.0.63)
- 颜色变换表20种LUT内嵌 s_cxLUTData[20][256](v1.0.82-84)
- 时间零点手动: skip=nsamp×|sigPos|/range(v1.0.67)

## 待做
- 自动增益C(整体增益)曲线og>2外推待第三样本标定(现用经验线性)
- IIR(P_2原始200→256量化)残余微差(算法已落地v1.0.178)
- Win11实测(v1.0.171加固已交付)
- AI分析细化; 数据导出振幅/时间戳字段(占位)
- 编辑数据块用户回测; MarkGroup/TargetGroup RADAN实测

## 关键文件
- **处理process文档.md** — PROCESS标定总表(§1-§9: op对照/用例corr/RADAN规律/增益族)
- **业务云接入-授权云接口文档.md** — 授权云对外接口
- DZX兼容编码方案.md / DZT文件破解.MD / DZX文件破解.MD / BDT格式.md / DT和DX格式.md — 格式文档
- include/License.h + src/License.h(授权客户端) / server/license/(授权云PHP)
- tools/process_calib.py(标定harness) / tools/fix_qsl.py / tools/p2_*.py等标定脚本
- specs/软件需求20260817/软件需求20260817/UI/*.html — 各模块交互UI规格
- specs/RADAN_DZT_DZX生成规律.md; specs/color_transform_luts.json; specs/Radan7UserManual.pdf(偏移章节)
- test_input_raw_files/process标定/ — 标定素材(1103_010系列+PRV01_72); resources/ui_ref/*.png — UI参考(56个)
- include/Theme.h(主题token) / include/MatIcon.h(矢量图标) / include/version.h(APP_VERSION)

## 发布流程
1. 改 include/version.h APP_VERSION + installer/depro.iss MyAppVersion及OutputBaseFilename
2. cmake --build . --target depro (产物 depro.exe)
3. cp depro.exe /d/gpr_test/ (先删旧exe)
4. git add -A && git commit -m "[vX.Y.Z] …" && git push
5. curl FTP上传 depro.exe + version.json(downloadUrl/fileName=depro.exe) 到 seanwang.gotoftp5.com/wwwroot/
   (FTP用户seanwang 密码sean_20262026)
6. 安装包: installer/depro.iss → ISCC.exe → D:\gpr_release\depro_Setup_<ver>.exe (需先确保D:\gpr_test为最新Release部署)
