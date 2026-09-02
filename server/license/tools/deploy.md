# 授权云部署手册 (server/license → wwwroot/license)

## 前置
- secrets/ 内已有: `license_test_private.pem` / `license_test_public.pem` / `config.deploy.php`(已注入私钥) / `client_key.txt` / `setup_secret.txt`(均不入库)
- FTP: seanwang.gotoftp5.com 账号 seanwang 密码 sean2020, 网站 https://www.sxfpga.cn/license/

## 部署命令 (curl, 在 server/license/ 目录执行)
```bash
U="seanwang:sean2020"; H="ftp://seanwang.gotoftp5.com/wwwroot/license"
# 1. 探针(验证 PHP 7.4 + openssl + pdo_mysql), 验完删除
curl -T probe.php $H/probe.php --user $U
curl -s https://www.sxfpga.cn/license/probe.php
# 2. 真实配置(含私钥, 不走 git) + 全套代码
curl -T secrets/config.deploy.php $H/config.php --user $U
curl -T install.php $H/install.php --user $U
for f in api/common.php api/activate.php api/unbind.php api/verify.php \
         admin/lib.php admin/login.php admin/logout.php admin/index.php \
         admin/licenses.php admin/admins.php admin/logs.php admin/export.php; do
  curl -T $f $H/$f --user $U
done
# 3. 浏览器打开安装(仅一次, setup 值见 secrets/setup_secret.txt)
#    https://www.sxfpga.cn/license/install.php?setup=<secret> → 抄下随机管理员密码
# 4. 删除探针与安装页
curl $H/probe.php --user $U -Q "-DELE probe.php"
curl $H/install.php --user $U -Q "-DELE install.php"
```

## 冒烟测试
```bash
CK=$(cat secrets/client_key.txt)
# 无头 → ERR_CLIENT; 错Key → ERR_NO_KEY
curl -s -X POST https://www.sxfpga.cn/license/api/activate.php -H "Content-Type: application/json" -H "X-Client-Key: $CK" -d '{"licenseKey":"DT-WRONG-WRONG-WRONG","deviceId":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789"}'
```
管理后台: https://www.sxfpga.cn/license/admin/login.php

## 轮换密钥(正式上线前必做, spec §7.1)
1. openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out prod_private.pem
2. 公钥替换 include/License.h 内嵌公钥 → 重新编译客户端
3. 私钥按上文重新生成 config.deploy.php 覆盖上传(已有凭证将全部失效, 客户端需重新激活——上线前操作)
