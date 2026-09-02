<?php
/** admin/admins.php —— 管理员管理(账号列表/新增/禁用/重置密码 仅super) + 所有登录者可改自己密码 */
require_once __DIR__ . '/lib.php';
$admin = boot();
$msg = ''; $msgCls = 'msg-ok';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if (!csrf_ok()) exit('CSRF 校验失败, 请刷新页面');
    $act = $_POST['action'] ?? '';

    if ($act === 'selfpwd') {
        $old = (string)($_POST['old'] ?? ''); $new = (string)($_POST['new'] ?? '');
        $st = db()->prepare('SELECT * FROM lic_admins WHERE id=?'); $st->execute([$admin['id']]);
        $me = $st->fetch();
        if (strlen($new) < 8) { $msg = '新密码至少 8 位'; $msgCls = 'msg-err'; }
        elseif (!password_verify($old, $me['password_hash'])) { $msg = '原密码错误'; $msgCls = 'msg-err'; }
        else {
            db()->prepare('UPDATE lic_admins SET password_hash=? WHERE id=?')
                ->execute([password_hash($new, PASSWORD_DEFAULT), $admin['id']]);
            $msg = '密码已修改';
        }
    } elseif ($admin['role'] !== 'super') {
        exit('403 仅超级管理员可管理账号');
    } elseif ($act === 'add') {
        $u = trim((string)($_POST['u'] ?? '')); $p = (string)($_POST['p'] ?? '');
        $role = ($_POST['role'] ?? '') === 'super' ? 'super' : 'admin';
        if (!preg_match('/^[A-Za-z0-9_]{3,32}$/', $u)) { $msg = '用户名需 3-32 位字母/数字/下划线'; $msgCls = 'msg-err'; }
        elseif (strlen($p) < 8) { $msg = '密码至少 8 位'; $msgCls = 'msg-err'; }
        else {
            try {
                db()->prepare('INSERT INTO lic_admins (username, password_hash, role) VALUES (?,?,?)')
                    ->execute([$u, password_hash($p, PASSWORD_DEFAULT), $role]);
                alog('admin_add', '', "user {$u} role {$role} by {$admin['username']}");
                $msg = "已添加管理员 {$u}";
            } catch (PDOException $e) { $msg = '用户名已存在'; $msgCls = 'msg-err'; }
        }
    } elseif ($act === 'toggle') {
        $id = (int)($_POST['id'] ?? 0);
        if ($id === (int)$admin['id']) { $msg = '不能禁用自己'; $msgCls = 'msg-err'; }
        else {
            // 不允许禁用最后一个 super
            $st = db()->prepare("SELECT COUNT(*) c FROM lic_admins WHERE role='super' AND disabled=0 AND id<>?");
            $st->execute([$id]);
            $target = db()->prepare('SELECT * FROM lic_admins WHERE id=?'); $target->execute([$id]);
            $t = $target->fetch();
            if ($t && $t['role'] === 'super' && $st->fetch()['c'] === 0) { $msg = '不能禁用最后一个超级管理员'; $msgCls = 'msg-err'; }
            else {
                db()->prepare('UPDATE lic_admins SET disabled=1-disabled WHERE id=?')->execute([$id]);
                $msg = $t ? ($t['disabled'] ? "已启用 {$t['username']}" : "已禁用 {$t['username']}") : '完成';
            }
        }
    } elseif ($act === 'resetpwd') {
        $id = (int)($_POST['id'] ?? 0); $p = (string)($_POST['p'] ?? '');
        $target = db()->prepare('SELECT * FROM lic_admins WHERE id=?'); $target->execute([$id]);
        $t = $target->fetch();
        if (!$t) { $msg = '账号不存在'; $msgCls = 'msg-err'; }
        elseif (strlen($p) < 8) { $msg = '新密码至少 8 位'; $msgCls = 'msg-err'; }
        else {
            db()->prepare('UPDATE lic_admins SET password_hash=? WHERE id=?')
                ->execute([password_hash($p, PASSWORD_DEFAULT), $id]);
            alog('admin_resetpwd', '', "user {$t['username']} by {$admin['username']}");
            $msg = "已重置 {$t['username']} 的密码";
        }
    }
}

page_head('管理员', $admin, 'admin');
?>
<h2>管理员</h2>
<?php if ($msg): ?><div class="msg <?= $msgCls ?>"><?= h($msg) ?></div><?php endif; ?>

<div class="card" id="self">
  <h2>修改我的密码</h2>
  <form method="post" class="kv">
    <?= csrf_field() ?><input type="hidden" name="action" value="selfpwd">
    <label>原密码</label><input type="password" name="old" required>
    <label>新密码(≥8位)</label><input type="password" name="new" required minlength="8">
    <button class="btn btn-pri" type="submit">修改</button>
  </form>
</div>

<?php if ($admin['role'] === 'super'): ?>
<div class="card">
  <h2>新增管理员</h2>
  <form method="post" class="kv">
    <?= csrf_field() ?><input type="hidden" name="action" value="add">
    <label>用户名</label><input type="text" name="u" required pattern="[A-Za-z0-9_]{3,32}">
    <label>密码(≥8位)</label><input type="text" name="p" required minlength="8">
    <label>角色</label><select name="role"><option value="admin">管理员</option><option value="super">超级管理员</option></select>
    <button class="btn btn-pri" type="submit">添加</button>
  </form>
</div>

<div class="card">
  <h2>账号列表</h2>
  <table><tr><th>ID</th><th>用户名</th><th>角色</th><th>状态</th><th>创建</th><th>最后登录</th><th>操作</th></tr>
  <?php foreach (db()->query('SELECT * FROM lic_admins ORDER BY id')->fetchAll() as $a): ?>
    <tr>
      <td><?= (int)$a['id'] ?></td>
      <td><?= h($a['username']) ?><?= (int)$a['id'] === (int)$admin['id'] ? ' <span class="tag">我</span>' : '' ?></td>
      <td><?= $a['role'] === 'super' ? '<span class="tag tag-ok">超级</span>' : '管理员' ?></td>
      <td><?= $a['disabled'] ? '<span class="tag tag-bad">已禁用</span>' : '<span class="tag tag-ok">正常</span>' ?></td>
      <td class="small mono"><?= h($a['created_at']) ?></td>
      <td class="small mono"><?= h((string)$a['last_login_at']) ?></td>
      <td>
        <?php if ((int)$a['id'] !== (int)$admin['id']): ?>
        <form method="post" style="display:inline">
          <?= csrf_field() ?><input type="hidden" name="action" value="toggle"><input type="hidden" name="id" value="<?= (int)$a['id'] ?>">
          <button class="btn btn-sm" type="submit"><?= $a['disabled'] ? '启用' : '禁用' ?></button></form>
        <?php endif; ?>
        <button class="btn btn-sm" type="button" onclick="var f=document.getElementById('p<?= (int)$a['id'] ?>');f.style.display=f.style.display==='none'?'':'none'">重置密码</button>
      </td>
    </tr>
    <tr id="p<?= (int)$a['id'] ?>" style="display:none;background:#fbfcff"><td colspan="7">
      <form method="post" class="kv" style="margin:6px 0">
        <?= csrf_field() ?><input type="hidden" name="action" value="resetpwd"><input type="hidden" name="id" value="<?= (int)$a['id'] ?>">
        <label>新密码(≥8位)</label><input type="text" name="p" minlength="8" required style="width:180px">
        <button class="btn btn-sm btn-pri" type="submit">重置 <?= h($a['username']) ?> 的密码</button></form></td></tr>
  <?php endforeach; ?>
  </table>
</div>
<?php else: ?>
<div class="card"><p class="small">账号管理仅超级管理员可见。</p></div>
<?php endif; ?>
<?php page_foot(); ?>
