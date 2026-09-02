<?php
/** admin/logs.php —— 操作日志(激活/解绑/失败/后台操作) */
require_once __DIR__ . '/lib.php';
$admin = boot();

$act = (string)($_GET['action'] ?? '');
$q = trim((string)($_GET['q'] ?? ''));
$page = max(1, (int)($_GET['page'] ?? 1));
$per = 50;
$where = []; $args = [];
$acts = ['activate', 'activate_fail', 'unbind', 'unbind_fail', 'unbind_admin', 'revoke', 'restore',
         'del', 'gen', 'login_fail', 'verify_fail', 'admin_add', 'admin_resetpwd'];
if (in_array($act, $acts, true)) { $where[] = 'action=?'; $args[] = $act; }
if ($q !== '') { $where[] = '(license_key LIKE ? OR device_id LIKE ? OR ip LIKE ? OR detail LIKE ?)';
    $args[] = "%$q%"; $args[] = "%$q%"; $args[] = "%$q%"; $args[] = "%$q%"; }
$w = $where ? (' WHERE ' . implode(' AND ', $where)) : '';

$total = db()->prepare("SELECT COUNT(*) c FROM lic_logs$w");
$total->execute($args); $total = (int)$total->fetch()['c'];
$pages = max(1, (int)ceil($total / $per));
$page = min($page, $pages);
$st = db()->prepare("SELECT * FROM lic_logs$w ORDER BY id DESC LIMIT $per OFFSET " . (($page - 1) * $per));
$st->execute($args);

$actName = [
    'gen'=>'生成','activate'=>'激活','activate_fail'=>'激活失败','unbind'=>'解绑','unbind_fail'=>'解绑失败',
    'unbind_admin'=>'后台解绑','revoke'=>'作废','restore'=>'恢复','del'=>'删除','login_fail'=>'登录失败',
    'verify_fail'=>'校验失败','admin_add'=>'加管理员','admin_resetpwd'=>'重置密码',
];
$qs = http_build_query(['action' => $act, 'q' => $q]);
page_head('日志', $admin, 'logs');
?>
<h2>操作日志</h2>
<div class="card">
  <form method="get" class="kv">
    <select name="action">
      <option value="">全部动作</option>
      <?php foreach ($acts as $a): ?>
        <option value="<?= $a ?>" <?= $act === $a ? 'selected' : '' ?>><?= $actName[$a] ?></option>
      <?php endforeach; ?>
    </select>
    <input type="text" name="q" value="<?= h($q) ?>" placeholder="搜索: 授权码/设备/IP/详情" style="width:240px">
    <button class="btn btn-pri" type="submit">筛选</button>
  </form>

  <table><tr><th>ID</th><th>时间</th><th>动作</th><th>授权码</th><th>设备</th><th>IP</th><th>详情</th></tr>
  <?php foreach ($st->fetchAll() as $l): ?>
    <tr>
      <td class="small"><?= (int)$l['id'] ?></td>
      <td class="mono small"><?= h($l['created_at']) ?></td>
      <td><span class="tag"><?= h($actName[$l['action']] ?? $l['action']) ?></span></td>
      <td class="mono small"><?= h($l['license_key']) ?></td>
      <td class="mono small" title="<?= h($l['device_id']) ?>"><?= h(mb_substr((string)$l['device_id'], 0, 12)) ?></td>
      <td class="mono small"><?= h($l['ip']) ?></td>
      <td class="small"><?= h($l['detail']) ?></td>
    </tr>
  <?php endforeach; ?>
  </table>

  <div class="pager">共 <?= $total ?> 条 · 第 <?= $page ?>/<?= $pages ?> 页
    <?php if ($page > 1): ?><a href="?<?= $qs ?>&page=<?= $page - 1 ?>">‹ 上页</a><?php endif; ?>
    <?php if ($page < $pages): ?><a href="?<?= $qs ?>&page=<?= $page + 1 ?>">下页 ›</a><?php endif; ?>
  </div>
</div>
<?php page_foot(); ?>
