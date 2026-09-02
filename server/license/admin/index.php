<?php
/** admin/index.php —— 概览: 统计 + 7日激活 + 最近日志 */
require_once __DIR__ . '/lib.php';
$admin = boot();
page_head('概览', $admin, 'dash');

$c = db()->query("SELECT
    COUNT(*) total,
    SUM(status='unused') s_unused,
    SUM(status='active') s_active,
    SUM(status='revoked') s_revoked,
    SUM(unbind_count) s_unbind
    FROM lic_keys")->fetch();

$trend = db()->query("SELECT DATE(activated_at) d, COUNT(*) c FROM lic_keys
    WHERE activated_at > DATE_SUB(NOW(), INTERVAL 7 DAY) GROUP BY DATE(activated_at) ORDER BY d")->fetchAll();
$byDay = [];
foreach ($trend as $t) $byDay[$t['d']] = (int)$t['c'];
$maxC = max(1, max($byDay ?: [1]));

$logs = db()->query('SELECT * FROM lic_logs ORDER BY id DESC LIMIT 12')->fetchAll();
$actName = [
    'gen'=>'生成','activate'=>'激活','activate_fail'=>'激活失败','reissue'=>'重发凭证',
    'unbind'=>'解绑','unbind_fail'=>'解绑失败','unbind_admin'=>'后台解绑','revoke'=>'作废','restore'=>'恢复',
    'del'=>'删除','login_fail'=>'登录失败','verify_fail'=>'校验失败',
];
?>
<h2>概览</h2>
<div class="card"><div class="stat">
  <div class="s"><b><?= (int)$c['total'] ?></b><span>授权码总数</span></div>
  <div class="s"><b><?= (int)$c['s_unused'] ?></b><span>未使用</span></div>
  <div class="s"><b style="color:#177a37"><?= (int)$c['s_active'] ?></b><span>已激活</span></div>
  <div class="s"><b style="color:#b02626"><?= (int)$c['s_revoked'] ?></b><span>已作废</span></div>
  <div class="s"><b><?= (int)$c['s_unbind'] ?></b><span>累计解绑次数</span></div>
</div></div>

<div class="card">
  <h2>近 7 日激活</h2>
  <?php if (!$byDay): ?>
    <p class="small">暂无激活记录</p>
  <?php else: foreach ($byDay as $d => $n): ?>
    <div class="kv"><span class="mono small" style="width:90px"><?= h($d) ?></span>
      <div style="flex:1;background:#eef3fb;border-radius:4px;height:16px">
        <div style="width:<?= round($n / $maxC * 100) ?>%;height:16px;background:#4d8dff;border-radius:4px"></div>
      </div><b style="width:30px;text-align:right"><?= $n ?></b></div>
  <?php endforeach; endif; ?>
</div>

<div class="card">
  <h2>最近操作 <a class="btn btn-sm" href="logs.php" style="float:right">全部日志 →</a></h2>
  <table><tr><th>时间</th><th>动作</th><th>授权码</th><th>设备</th><th>IP</th><th>详情</th></tr>
  <?php foreach ($logs as $l): ?>
    <tr><td class="mono small"><?= h($l['created_at']) ?></td>
        <td><?= h($actName[$l['action']] ?? $l['action']) ?></td>
        <td class="mono small"><?= h($l['license_key']) ?></td>
        <td class="mono small"><?= h(mb_substr($l['device_id'], 0, 12)) ?></td>
        <td class="mono small"><?= h($l['ip']) ?></td>
        <td class="small"><?= h($l['detail']) ?></td></tr>
  <?php endforeach; ?></table>
</div>
<?php page_foot(); ?>
