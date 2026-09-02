<?php
/**
 * admin/lib.php —— 管理后台公共库: DB/会话/登录态/CSRF/页面骨架
 * PHP 7.4 兼容(无 PHP8 语法)。
 */
require_once __DIR__ . '/../config.php';

function db() {
    static $pdo = null;
    if ($pdo === null) {
        $pdo = new PDO('mysql:host=' . LIC_DB_HOST . ';dbname=' . LIC_DB_NAME . ';charset=utf8mb4',
            LIC_DB_USER, LIC_DB_PASS, [
                PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
                PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
            ]);
    }
    return $pdo;
}

function is_https() {
    if (isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== '' && $_SERVER['HTTPS'] !== 'off')
        return true;
    // 反向代理终结TLS时(本主机即如此), 后端只能看 X-Forwarded-Proto
    if (isset($_SERVER['HTTP_X_FORWARDED_PROTO'])
        && strpos($_SERVER['HTTP_X_FORWARDED_PROTO'], 'https') === 0)
        return true;
    return isset($_SERVER['SERVER_PORT']) && $_SERVER['SERVER_PORT'] == 443;
}

function force_https() {
    if (!is_https() && isset($_SERVER['HTTP_HOST'], $_SERVER['REQUEST_URI'])) {
        header('Location: https://' . $_SERVER['HTTP_HOST'] . $_SERVER['REQUEST_URI'], true, 301);
        exit;
    }
}

function sess_start() {
    if (session_status() === PHP_SESSION_ACTIVE) return;
    ini_set('session.use_strict_id', '1');
    session_set_cookie_params([
        'httponly' => true,
        'samesite' => 'Lax',
        'secure'   => is_https(),
        'lifetime' => 0,
    ]);
    session_name('LICSESS');
    session_start();
    if (!isset($_SESSION['csrf'])) $_SESSION['csrf'] = bin2hex(random_bytes(16));
}

/** 登录态引导: 未登录跳 login.php; 返回当前管理员行 */
function boot($superOnly = false) {
    force_https();
    sess_start();
    if (empty($_SESSION['aid'])) { header('Location: login.php'); exit; }
    $st = db()->prepare('SELECT * FROM lic_admins WHERE id=? AND disabled=0');
    $st->execute([$_SESSION['aid']]);
    $a = $st->fetch();
    if (!$a) { session_destroy(); header('Location: login.php'); exit; }
    // 30 分钟无操作过期
    if (isset($_SESSION['t']) && time() - $_SESSION['t'] > 1800) {
        session_destroy(); header('Location: login.php?expire=1'); exit;
    }
    $_SESSION['t'] = time();
    if ($superOnly && $a['role'] !== 'super') { echo '403 无权限(仅超级管理员)'; exit; }
    return $a;
}

function csrf_field() { return '<input type="hidden" name="csrf" value="' . $_SESSION['csrf'] . '">'; }
function csrf_ok()    { return isset($_POST['csrf']) && hash_equals($_SESSION['csrf'], (string)$_POST['csrf']); }

function h($s) { return htmlspecialchars((string)$s, ENT_QUOTES, 'UTF-8'); }

function client_ip() {
    return isset($_SERVER['HTTP_X_FORWARDED_FOR']) && $_SERVER['HTTP_X_FORWARDED_FOR'] !== ''
        ? trim(explode(',', $_SERVER['HTTP_X_FORWARDED_FOR'])[0])
        : (isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '');
}

function alog($action, $key = '', $detail = '') {
    try {
        db()->prepare('INSERT INTO lic_logs (license_key, action, ip, detail) VALUES (?,?,?,?)')
            ->execute([$key, $action, client_ip(), mb_substr($detail, 0, 250)]);
    } catch (PDOException $e) { /* 忽略 */ }
}

/** 生成不冲突的授权码 DT-XXXXX-XXXXX-XXXXX (字母表去除 0/O/1/I) */
function gen_key() {
    $alphabet = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
    $st = db()->prepare('SELECT id FROM lic_keys WHERE license_key=?');
    for ($try = 0; $try < 20; $try++) {
        $groups = [];
        for ($g = 0; $g < 3; $g++) {
            $s = '';
            for ($i = 0; $i < 5; $i++) $s .= $alphabet[random_int(0, strlen($alphabet) - 1)];
            $groups[] = $s;
        }
        $key = 'DT-' . implode('-', $groups);
        $st->execute([$key]);
        if (!$st->fetch()) return $key;
    }
    exit('key generation failed');
}

function status_label($s) {
    if ($s === 'active')  return '<span class="tag tag-ok">已激活</span>';
    if ($s === 'revoked') return '<span class="tag tag-bad">已作废</span>';
    return '<span class="tag">未使用</span>';
}
function feat_label($mask) {
    $f = [];
    if ($mask & 1) $f[] = 'AI分析';
    return $f ? implode('+', $f) : '无';
}

function page_head($title, $admin, $active) {
    $nav = [
        'dash'  => ['index.php', '概览'],
        'lic'   => ['licenses.php', '授权码'],
        'logs'  => ['logs.php', '日志'],
        'admin' => ['admins.php', '管理员'],
    ];
    $navHtml = '';
    foreach ($nav as $k => $it) {
        if ($k === 'admin' && $admin['role'] !== 'super') continue;
        $cls = $k === $active ? ' class="on"' : '';
        $navHtml .= "<a href=\"{$it[0]}\"$cls>{$it[1]}</a>";
    }
    $selfChange = '<a href="admins.php#self">改密码</a>';
    echo <<<HTML
<!DOCTYPE html><html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="robots" content="noindex,nofollow">
<title>$title · 地听授权云</title>
<style>
*{box-sizing:border-box} body{font:14px/1.6 system-ui,"Microsoft YaHei",sans-serif;margin:0;background:#f2f4f8;color:#222}
.hd{background:#0f2b52;color:#fff;display:flex;align-items:center;padding:0 20px;height:52px}
.hd b{font-size:16px;margin-right:28px}.hd a{color:#c9d8ef;text-decoration:none;margin-right:18px;padding:0 2px;line-height:50px;display:inline-block}
.hd a.on{color:#fff;border-bottom:2px solid #4d8dff}.hd .sp{flex:1}.hd .usr{font-size:12px;color:#9db6dc}
.wrap{max-width:1200px;margin:20px auto;padding:0 16px}
.card{background:#fff;border-radius:8px;padding:16px 20px;margin-bottom:16px;box-shadow:0 1px 3px rgba(0,0,0,.06)}
h2{font-size:17px;margin:0 0 14px}.h22{font-size:15px;margin:18px 0 10px;color:#444}
table{border-collapse:collapse;width:100%}th,td{padding:8px 10px;border-bottom:1px solid #eef1f6;text-align:left;font-size:13px}
th{background:#f7f9fc;color:#555;font-weight:600;white-space:nowrap}
tr:hover td{background:#fafcff}
.tag{display:inline-block;padding:1px 10px;border-radius:10px;background:#e8ecf3;color:#555;font-size:12px}
.tag-ok{background:#e2f5e7;color:#177a37}.tag-bad{background:#fdeaea;color:#b02626}
.btn{display:inline-block;padding:6px 16px;border-radius:6px;border:1px solid #c9d3e2;background:#fff;color:#235;cursor:pointer;font-size:13px;text-decoration:none}
.btn:hover{background:#f0f4fb}.btn-pri{background:#0f5fd6;border-color:#0f5fd6;color:#fff}.btn-pri:hover{background:#0d54bd}
.btn-sm{padding:2px 10px;font-size:12px}.btn-danger{color:#b02626;border-color:#e3bcbc}
input[type=text],input[type=password],input[type=number],select{padding:6px 10px;border:1px solid #ccd5e2;border-radius:6px;font-size:13px}
input:focus,select:focus{outline:none;border-color:#0f5fd6}
.mono{font-family:Consolas,monospace}
.kv{display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin-bottom:10px}
.kv label{color:#555;font-size:13px}
.msg{padding:10px 14px;border-radius:6px;margin-bottom:14px}.msg-ok{background:#e2f5e7;color:#177a37}.msg-err{background:#fdeaea;color:#b02626}
.stat{display:flex;gap:16px;flex-wrap:wrap}.stat .s{flex:1;min-width:150px;background:#f7f9fc;border-radius:8px;padding:14px 18px}
.stat .s b{font-size:26px;display:block}.stat .s span{color:#667;font-size:13px}
.pager{margin-top:12px;text-align:center;font-size:13px}.pager a{margin:0 6px;text-decoration:none;color:#0f5fd6}
.small{font-size:12px;color:#889}.genlist{background:#f4f8ff;border:1px dashed #9db6dc;border-radius:6px;padding:10px 14px;font-family:monospace;line-height:1.9}
</style></head><body>
<div class="hd"><b>🔑 地听授权云</b>{$navHtml}<span class="sp"></span>$selfChange<span class="usr">&nbsp;{$admin['username']} · <a href="logout.php" style="color:#ffd7d7;margin:0 0 0 10px">退出</a></span></div>
<div class="wrap">
HTML;
}

function page_foot() { echo '</div></body></html>'; }
