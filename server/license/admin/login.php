<?php
/** admin/login.php —— 管理员登录(失败计数限速 + bcrypt) */
require_once __DIR__ . '/lib.php';
force_https();
sess_start();

$err = '';
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $u = trim((string)($_POST['u'] ?? ''));
    $p = (string)($_POST['p'] ?? '');

    // IP 限速: 15 分钟内失败 ≥10 次锁定
    $st = db()->prepare("SELECT COUNT(*) c FROM lic_logs WHERE ip=? AND action='login_fail'
                         AND created_at > DATE_SUB(NOW(), INTERVAL 15 MINUTE)");
    $st->execute([client_ip()]);
    if ($st->fetch()['c'] >= 10) {
        $err = '失败次数过多, 请15分钟后再试';
    } elseif ($u === '' || $p === '') {
        $err = '请输入用户名和密码';
    } else {
        $st = db()->prepare('SELECT * FROM lic_admins WHERE username=?');
        $st->execute([$u]);
        $a = $st->fetch();
        if ($a && !$a['disabled'] && password_verify($p, $a['password_hash'])) {
            session_regenerate_id(true);
            $_SESSION['aid'] = (int)$a['id'];
            $_SESSION['t'] = time();
            db()->prepare('UPDATE lic_admins SET last_login_at=NOW() WHERE id=?')->execute([$a['id']]);
            header('Location: index.php');
            exit;
        }
        alog('login_fail', '', 'user: ' . mb_substr($u, 0, 30));
        usleep(500000); // 0.5s 拖慢爆破
        $err = '用户名或密码错误';
    }
}
if (isset($_GET['expire'])) $err = '会话已过期, 请重新登录';
?>
<!DOCTYPE html><html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><meta name="robots" content="noindex,nofollow">
<title>登录 · 地听授权云</title>
<style>
body{font:14px/1.6 system-ui,"Microsoft YaHei",sans-serif;margin:0;background:#0f2b52;height:100vh;display:flex;align-items:center;justify-content:center}
.box{background:#fff;border-radius:10px;padding:34px 38px;width:340px;box-shadow:0 8px 30px rgba(0,0,0,.35)}
h2{margin:0 0 6px;text-align:center}.sub{text-align:center;color:#889;font-size:12px;margin-bottom:20px}
input{width:100%;padding:9px 12px;border:1px solid #ccd5e2;border-radius:6px;font-size:14px;margin-bottom:14px;box-sizing:border-box}
input:focus{outline:none;border-color:#0f5fd6}
button{width:100%;padding:10px;border:0;border-radius:6px;background:#0f5fd6;color:#fff;font-size:15px;cursor:pointer}
button:hover{background:#0d54bd}
.err{background:#fdeaea;color:#b02626;padding:8px 12px;border-radius:6px;margin-bottom:14px;font-size:13px}
</style></head><body>
<div class="box">
  <h2>🔑 地听授权云</h2><div class="sub">License 管理后台</div>
  <?php if ($err) echo '<div class="err">' . h($err) . '</div>'; ?>
  <form method="post" autocomplete="off">
    <input type="text" name="u" placeholder="用户名" autofocus>
    <input type="password" name="p" placeholder="密码">
    <button type="submit">登 录</button>
  </form>
</div></body></html>
