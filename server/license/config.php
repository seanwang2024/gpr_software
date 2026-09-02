<?php
/**
 * 授权云配置（模板）—— 真实私钥绝不入库！
 * 部署流程: 本地用 secrets/ 中密钥替换私钥占位值后经 FTP 上传到 wwwroot/license/config.php
 * 私钥以 PHP 变量形态存放于 .php 文件内, 浏览器直接访问只会执行空输出, 无法下载源码。
 */

define('LIC_DB_HOST', 'localhost');
define('LIC_DB_NAME', 'seanwang');
define('LIC_DB_USER', 'seanwang');
define('LIC_DB_PASS', 'sean2020');

// RSA 私钥(PEM, 多行原样) —— 部署时替换为真实私钥(见 secrets/ 与 tools/deploy.md)
define('LIC_RSA_PRIVATE_PEM', "__PRIVATE_KEY_PLACEHOLDER__");

// 客户端接口弱防刷头 X-Client-Key 的值(编译进 exe, 仅防爬虫批量刷)
define('LIC_CLIENT_KEY', '5193f860d4777d6504cbf3bcf8b5ab23');

// install.php 一次性安装密钥(安装完成后作废)
define('LIC_SETUP_SECRET', 'ad11e44446a05774615a69d61f46fe29');

date_default_timezone_set('Asia/Shanghai');
