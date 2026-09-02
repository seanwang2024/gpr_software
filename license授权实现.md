# License 授权实现(v1.0.169)

按 `specs/license管理需求.md` 实现的无账号单机永久授权体系。
**第一版: 仅在线激活/在线解绑/在线巡检, 无离线激活。**

## 架构
```
depro.exe(内置RSA公钥) ──HTTPS──> www.sxfpga.cn/license/api/*.php ──> MySQL seanwang(lic_*表)
管理员浏览器 ──HTTPS──> www.sxfpga.cn/license/admin/ (登录+批量生成+解绑/作废+日志+CSV)
```

## 一、服务端(仓库 server/license/, 部署到 wwwroot/license/)

| 文件 | 作用 |
|---|---|
| config.php | DB/RSA私钥/X-Client-Key(**仓库为模板, 真实私钥已注入版在 secrets/config.deploy.php, 不入库**) |
| install.php | 一次性建表+初始管理员(随机密码仅显示一次; 自锁 install.lock; ?setup=密钥) |
| api/activate.php | 激活: unused→绑定 / 同机幂等重发凭证 / 异机拒绝 / 作废拒绝 |
| api/unbind.php | 客户端解绑(释放可迁移, unbind_count+1) |
| api/verify.php | 巡检校验(客户端启动+24h; 失败才写日志防刷表) |
| admin/*.php | 管理后台(见下) |

### 管理后台功能(https://www.sxfpga.cn/license/admin/)
- **概览**: 总数/未用/已激活/已作废/累计解绑 + 7日激活条形图 + 最近操作
- **授权码**: 批量生成(1-200个/次, 客户名+功能勾选[bit0 AI分析]+备注) | 搜索(码/客户/设备/状态) | 行操作: 解绑(客服处理硬件迁移)/作废(联网即锁)/恢复/删除 | 备注/客户名编辑 | CSV导出(UTF-8 BOM)
- **日志**: 全动作流水(生成/激活/激活失败/解绑/作废/登录失败/校验失败), 按动作/关键字筛选
- **管理员**: super可增删启停账号+重置密码; 所有人可改自己密码; bcrypt+CSRF+IP失败限速(15min≥10次锁)
- 登录: 会话30min无操作过期, 强制HTTPS, httponly+SameSite=Lax

### 安全设计(spec 对应)
- RSA私钥仅在 config.php 的 PHP 变量内(.php 直访不出源码); **测试/上线两套密钥, 上线必轮换**
- PDO 预处理防注入; license_key 唯一索引保 1Key=1机
- API 失败限速: 同IP 15min 内 activate_fail/unbind_fail/login_fail ≥10 → 拒绝
- X-Client-Key 弱防刷(编译进exe, spec三类密钥之第3类, 强度低属预期)
- spec三类密钥: ①RSA对(云私钥/exe公钥) ②服务间Secret(暂无业务云, 预留) ③X-Client-Key

## 二、客户端(depro.exe)

| 文件 | 内容 |
|---|---|
| include/License.h / src/License.cpp | 机器指纹/凭证存取/CryptoAPI验签/isUnlocked/自检 |
| version.h | APP_LICENSE_API + APP_LICENSE_CLIENT_KEY |
| MainWindow.cpp | showLicenseDialog(两态)/licensePatrol(启动+24h)/requireLicense门控 |
| TopBar.cpp | 右上角🔑按钮 → showLicenseDialog |

- **机器指纹**: SHA256(注册表 MachineGuid | 系统盘卷序列号) → 64hex; 显示前16位4-4-4-4分组。换机/重装系统=新ID(需客服后台解绑旧机)
- **验签**: Windows 原生 CryptoAPI(CryptStringToBinary→CryptDecodeObjectEx→CryptImportPublicKeyInfo→CryptVerifySignature, PKCS#1 v1.5 SHA-256, **签名需反转字节序**), 与服务端 openssl_sign 逐字节对应; **无 OpenSSL DLL 依赖**(TLS走schannel)
- **凭证**: 服务端返回 base64(JSON{payload,sig}); payload="DTLIC1|授权码|设备ID|功能位|永久"; 本地 QSettings("Diting","depro") license/credential; 激活时**先本地验签再落盘**
- **解锁判定**(每次实时计算): 凭证解析✓ + RSA验签✓ + payload设备ID==本机 → featureMask bit0 → AI分析解锁
- **门控**(spec: 按钮可见不置灰, 点击弹授权窗): ①TopBar AI分析标签拦截+回拨 ②ribbon AI检测/AI报告按钮 ③AI面板"运行AI检测" ④runAiDetection/showAiReportDialog 函数入口
- **巡检**: 启动+每24h POST verify; valid=0(后台作废)→清凭证+状态栏"授权已失效"; **网络失败静默放行**(断网靠本地凭证, spec §4.3)
- **授权窗**: 未激活态=锁定提示+设备ID(复制)+授权码输入(自动大写)+在线激活; 已激活态=✅永久授权+脱敏码/设备ID/类型/已解锁+解绑授权(确认框)+导出授权信息(txt)
- **About**: 授权状态行(绿"已激活(永久)·脱敏码"/红"未激活") + 【授权管理…】按钮
- **自检**: `GPR_LICENSE_SELFTEST=1 ./depro.exe`(退出码0=过); `GPR_LICENSE_RENDER=1` 自动开授权窗截图 license_dialog.png(两态随本地凭证); `GPR_LIC_DEBUG=1` 验签逐步GLE

## 三、API 契约
请求: `POST https://www.sxfpga.cn/license/api/<action>.php`, Content-Type: application/json, Header `X-Client-Key: <值见version.h>`, body `{"licenseKey":"DT-...","deviceId":"<64hex>"}`
- 成功 activate: `{"code":0,"licenseKeyMasked":"DT-XXXXX-…","deviceId":..,"featureMask":1,"isPermanent":1,"credential":"<base64>"}`
- verify: `{"code":0,"valid":1|0,"featureMask":..}`
- 失败: `{"code":1,"err":"ERR_*","msg":"中文提示"}`; 错误码: ERR_NO_KEY/ERR_REVOKED/ERR_BOUND_OTHER/ERR_NOT_BOUND/ERR_KEY_FORMAT/ERR_CLIENT/ERR_THROTTLE/ERR_NETWORK
- 授权码格式: `DT-XXXXX-XXXXX-XXXXX`(字母表去0/O/1/I), 输入自动大写、去空格

## 四、部署(✅ 已于 2026-09-02 完成并全矩阵验证)
- 上传: config.deploy.php→config.php + install.php + api/ + admin/ (完整命令见 server/license/tools/deploy.md)
- 安装: `install.php?setup=<secrets/setup_secret.txt>` 一次性建表+初始管理员(密码在 **secrets/admin_password.txt**, 不入库)
- 管理后台: **https://www.sxfpga.cn/license/admin/login.php** (admin)
- 已验证: API 8分支矩阵(激活/校验/异机拒/幂等/解绑拒/解绑/迁移/旧机失效) + 服务端凭证经客户端公钥验签OK + 真实Key端到端激活(本机指纹) + 作废→启动巡检自动清凭证锁定
- 环境: PHP 7.4.33 + openssl + pdo_mysql; **反代终结TLS**(xf_proto=https, 后端port=80) — is_https() 已兼容

## 五、运维手册
- **客户报"激活码绑了别的电脑"**: 后台授权码页搜码→解绑→客户重新激活
- **客户换电脑/重装系统**: 同上(设备指纹变了, 客服后台解绑旧绑定即可, 无需新码)
- **客户丢码**: 后台按客户名搜, 电话核对后告知
- **退款/违规**: 作废(已激活机器联网巡检后自动锁; 离线机器下次联网才生效——spec 短板③)
- **上线前必做**: ①轮换RSA密钥对(生成→公钥进License.h重编译→私钥进config.php重上传) ②install.php+probe.php 删除 ③管理员初始密码改掉 ④lic_keys表定时备份(核心资产, spec §7.5)
