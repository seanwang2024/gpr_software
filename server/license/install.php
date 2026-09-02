<?php
/**
 * install.php —— 一次性安装: 建表 + 创建 super 管理员(随机密码仅显示一次)
 * 用法: https://www.sxfpga.cn/license/install.php?setup=<LIC_SETUP_SECRET>
 * 重跑保护: ①install.lock 文件 ②已存在管理员即拒绝
 */
require __DIR__ . '/config.php';
header('Content-Type: text/html; charset=utf-8');

$lockFile = __DIR__ . '/install.lock';
$installed = file_exists($lockFile);

function fail($msg) { echo "<h3>❌ $msg</h3><p><a href='javascript:history.back()'>返回</a></p>"; exit; }

try {
    $db = new PDO('mysql:host=' . LIC_DB_HOST . ';dbname=' . LIC_DB_NAME . ';charset=utf8mb4',
        LIC_DB_USER, LIC_DB_PASS, [
            PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        ]);
} catch (PDOException $e) { fail('数据库连接失败: ' . htmlspecialchars($e->getMessage())); }

if ($installed) fail('已安装(install.lock 存在)。如需重装请先经 FTP 删除 install.lock 并清空 lic_* 表。');

$hasAdmin = $db->query("SELECT COUNT(*) c FROM lic_admins")->fetch()['c'] > 0;
if ($hasAdmin) { @file_put_contents($lockFile, date('Y-m-d H:i:s')); fail('已存在管理员, 拒绝重装(已补写 install.lock)。'); }

if (!isset($_GET['setup']) || !hash_equals(LIC_SETUP_SECRET, $_GET['setup'])) fail('安装密钥(setup)错误。');

$sql = <<<SQL
CREATE TABLE IF NOT EXISTS lic_admins (
  id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  username VARCHAR(32) NOT NULL UNIQUE,
  password_hash VARCHAR(255) NOT NULL,
  role VARCHAR(16) NOT NULL DEFAULT 'admin',
  disabled TINYINT NOT NULL DEFAULT 0,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  last_login_at DATETIME NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS lic_keys (
  id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  license_key VARCHAR(24) NOT NULL UNIQUE,
  feature_mask INT NOT NULL DEFAULT 1,
  status VARCHAR(16) NOT NULL DEFAULT 'unused',
  device_id VARCHAR(64) NULL,
  activated_at DATETIME NULL,
  unbind_count INT UNSIGNED NOT NULL DEFAULT 0,
  customer_name VARCHAR(64) DEFAULT '',
  note VARCHAR(255) DEFAULT '',
  created_by INT UNSIGNED NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  INDEX idx_status (status),
  INDEX idx_device (device_id),
  INDEX idx_customer (customer_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS lic_logs (
  id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  license_key VARCHAR(24) NOT NULL DEFAULT '',
  device_id VARCHAR(64) DEFAULT '',
  action VARCHAR(24) NOT NULL,
  ip VARCHAR(45) DEFAULT '',
  detail VARCHAR(255) DEFAULT '',
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_key (license_key),
  INDEX idx_action (action),
  INDEX idx_time (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
SQL;
$db->exec($sql);

// 随机初始密码(仅显示一次)
$alphabet = 'ABCDEFGHJKMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789';
$pwd = '';
for ($i = 0; $i < 12; $i++) $pwd .= $alphabet[random_int(0, strlen($alphabet) - 1)];
$db->prepare('INSERT INTO lic_admins (username, password_hash, role) VALUES (?,?,?)')
    ->execute(['admin', password_hash($pwd, PASSWORD_DEFAULT), 'super']);
@file_put_contents($lockFile, date('Y-m-d H:i:s'));
?>
<!DOCTYPE html><html lang="zh-CN"><head><meta charset="utf-8"><title>授权云安装</title></head>
<body style="font-family:system-ui;max-width:520px;margin:80px auto;line-height:1.9">
<h2>✅ 安装完成</h2>
<p>3 张表(lic_admins / lic_keys / lic_logs)创建成功。</p>
<p style="background:#fff3cd;padding:12px;border-radius:6px">
  初始管理员: <b>admin</b><br>
  初始密码: <b style="font-size:18px;color:#c00"><?= htmlspecialchars($pwd) ?></b><br>
  <span style="color:#c00">⚠ 此密码仅显示这一次, 请立即抄写保存！</span>
</p>
<p>登录地址: <a href="admin/login.php">admin/login.php</a> —— 登录后请在「管理员」页修改密码。</p>
<p style="color:#888">本页已自锁(install.lock), 现在可通过 FTP 删除本文件。</p>
</body></html>
