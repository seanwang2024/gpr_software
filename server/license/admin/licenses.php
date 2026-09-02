<?php
/**
 * admin/licenses.php —— 授权码管理核心页
 * 批量生成 | 搜索(授权码/客户/状态) | 后台解绑/作废/恢复/删除 | 备注 | 分页
 */
require_once __DIR__ . '/lib.php';
$admin = boot();
$msg = ''; $msgCls = 'msg-ok';
$generated = [];

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if (!csrf_ok()) { exit('CSRF 校验失败, 请刷新页面'); }
    $act = $_POST['action'] ?? '';
    $id = (int)($_POST['id'] ?? 0);
    $st = db()->prepare('SELECT * FROM lic_keys WHERE id=?');
    $st->execute([$id]);
    $row = $st->fetch();

    if ($act === 'gen') {
        $n = max(1, min(200, (int)($_POST['count'] ?? 1)));
        $customer = mb_substr(trim($_POST['customer'] ?? ''), 0, 60);
        $note = mb_substr(trim($_POST['note'] ?? ''), 0, 200);
        $mask = isset($_POST['f_ai']) ? 1 : 0;
        $ins = db()->prepare('INSERT INTO lic_keys (license_key, feature_mask, customer_name, note, created_by) VALUES (?,?,?,?,?)');
        for ($i = 0; $i < $n; $i++) {
            $k = gen_key();
            $ins->execute([$k, $mask, $customer, $note, (int)$admin['id']]);
            $generated[] = $k;
        }
        alog('gen', $generated[0] . ($n > 1 ? " …共{$n}个" : ''), "by {$admin['username']}, customer={$customer}, mask={$mask}");
        $msg = "成功生成 {$n} 个授权码" . ($customer ? "(客户: {$customer})" : '') . ' —— 请立即复制保存';
    } elseif (!$row) {
        $msg = '记录不存在'; $msgCls = 'msg-err';
    } elseif ($act === 'unbind_admin') {
        if ($row['status'] !== 'active') { $msg = '该授权码当前未激活绑定'; $msgCls = 'msg-err'; }
        else {
            db()->prepare("UPDATE lic_keys SET status='unused', device_id=NULL, unbind_count=unbind_count+1 WHERE id=?")->execute([$id]);
            alog('unbind_admin', $row['license_key'], "by {$admin['username']}, old device: {$row['device_id']}");
            $msg = "已解绑 {$row['license_key']} (客户可重新激活)";
        }
    } elseif ($act === 'revoke') {
        db()->prepare("UPDATE lic_keys SET status='revoked' WHERE id=?")->execute([$id]);
        alog('revoke', $row['license_key'], "by {$admin['username']}");
        $msg = "已作废 {$row['license_key']} (已激活客户端将在联网巡检后锁定)";
    } elseif ($act === 'restore') {
        $new = $row['device_id'] ? 'active' : 'unused';
        db()->prepare('UPDATE lic_keys SET status=? WHERE id=?')->execute([$new, $id]);
        alog('restore', $row['license_key'], "by {$admin['username']} → {$new}");
        $msg = "已恢复 {$row['license_key']} → " . ($new === 'active' ? '已激活(原绑定有效)' : '未使用');
    } elseif ($act === 'del') {
        db()->prepare('DELETE FROM lic_keys WHERE id=?')->execute([$id]);
        alog('del', $row['license_key'], "by {$admin['username']}, status={$row['status']}, device={$row['device_id']}");
        $msg = "已删除 {$row['license_key']}(日志保留审计)";
    } elseif ($act === 'note') {
        db()->prepare('UPDATE lic_keys SET customer_name=?, note=? WHERE id=?')->execute([
            mb_substr(trim($_POST['customer'] ?? ''), 0, 60), mb_substr(trim($_POST['note'] ?? ''), 0, 200), $id]);
        $msg = "已更新备注 #{$id}";
    }
}

// ---- 列表筛选 ----
$q = trim((string)($_GET['q'] ?? ''));
$fs = (string)($_GET['status'] ?? '');
$page = max(1, (int)($_GET['page'] ?? 1));
$per = 20;
$where = []; $args = [];
if ($q !== '') { $where[] = '(license_key LIKE ? OR customer_name LIKE ? OR device_id LIKE ?)'; $args[] = "%$q%"; $args[] = "%$q%"; $args[] = "%$q%"; }
if (in_array($fs, ['unused', 'active', 'revoked'], true)) { $where[] = 'status=?'; $args[] = $fs; }
$w = $where ? (' WHERE ' . implode(' AND ', $where)) : '';

$total = db()->prepare("SELECT COUNT(*) c FROM lic_keys$w");
$total->execute($args); $total = (int)$total->fetch()['c'];
$pages = max(1, (int)ceil($total / $per));
$page = min($page, $pages);
$st = db()->prepare("SELECT * FROM lic_keys$w ORDER BY id DESC LIMIT $per OFFSET " . (($page - 1) * $per));
$st->execute($args);
$rows = $st->fetchAll();

$qs = http_build_query(['q' => $q, 'status' => $fs]);
page_head('授权码', $admin, 'lic');
?>
<h2>授权码管理</h2>
<?php if ($msg): ?><div class="msg <?= $msgCls ?>"><?= h($msg) ?></div><?php endif; ?>

<?php if ($generated): ?>
<div class="card">
  <h2>新生成的授权码 <span class="small">(仅此一次集中显示, 已可到列表搜索)</span></h2>
  <div class="genlist"><?= implode('<br>', array_map('h', $generated)) ?></div>
</div>
<?php endif; ?>

<div class="card">
  <h2>批量生成</h2>
  <form method="post" class="kv" onsubmit="return confirm('确认生成 ' + this.count.value + ' 个授权码?')">
    <?= csrf_field() ?><input type="hidden" name="action" value="gen">
    <label>数量</label><input type="number" name="count" value="10" min="1" max="200" style="width:70px">
    <label>客户名称</label><input type="text" name="customer" placeholder="选填, 如: XX隧道项目部" style="width:200px">
    <label>功能</label><label><input type="checkbox" name="f_ai" checked> AI分析模块</label>
    <input type="text" name="note" placeholder="备注(选填)" style="width:220px">
    <button class="btn btn-pri" type="submit">生成</button>
  </form>
</div>

<div class="card">
  <form method="get" class="kv">
    <input type="text" name="q" value="<?= h($q) ?>" placeholder="搜索: 授权码 / 客户 / 设备ID" style="width:260px">
    <select name="status">
      <option value="">全部状态</option>
      <option value="unused" <?= $fs === 'unused' ? 'selected' : '' ?>>未使用</option>
      <option value="active" <?= $fs === 'active' ? 'selected' : '' ?>>已激活</option>
      <option value="revoked" <?= $fs === 'revoked' ? 'selected' : '' ?>>已作废</option>
    </select>
    <button class="btn btn-pri" type="submit">搜索</button>
    <span class="sp" style="flex:1"></span>
    <a class="btn" href="export.php?<?= $qs ?>">导出 CSV</a>
  </form>

  <table>
    <tr><th>ID</th><th>授权码</th><th>客户</th><th>功能</th><th>状态</th><th>绑定设备</th><th>激活时间</th><th>解绑</th><th>备注</th><th style="width:230px">操作</th></tr>
    <?php if (!$rows): ?><tr><td colspan="10" style="text-align:center;color:#889;padding:24px">无记录</td></tr><?php endif; ?>
    <?php foreach ($rows as $r): ?>
    <tr>
      <td><?= (int)$r['id'] ?></td>
      <td class="mono"><?= h($r['license_key']) ?></td>
      <td><?= h($r['customer_name']) ?></td>
      <td class="small"><?= feat_label((int)$r['feature_mask']) ?></td>
      <td><?= status_label($r['status']) ?></td>
      <td class="mono small" title="<?= h($r['device_id']) ?>"><?= h(mb_substr((string)$r['device_id'], 0, 12)) ?></td>
      <td class="small mono"><?= h((string)$r['activated_at']) ?></td>
      <td class="small" style="text-align:center"><?= (int)$r['unbind_count'] ?></td>
      <td class="small"><?= h($r['note']) ?></td>
      <td>
        <?php if ($r['status'] === 'active'): ?>
        <form method="post" style="display:inline" onsubmit="return confirm('解绑 <?= h($r['license_key']) ?>? 客户可重新激活到新电脑。')">
          <?= csrf_field() ?><input type="hidden" name="action" value="unbind_admin"><input type="hidden" name="id" value="<?= (int)$r['id'] ?>">
          <button class="btn btn-sm" type="submit">解绑</button></form>
        <?php endif; ?>
        <?php if ($r['status'] !== 'revoked'): ?>
        <form method="post" style="display:inline" onsubmit="return confirm('作废 <?= h($r['license_key']) ?>? 已激活客户端联网巡检后将锁定!')">
          <?= csrf_field() ?><input type="hidden" name="action" value="revoke"><input type="hidden" name="id" value="<?= (int)$r['id'] ?>">
          <button class="btn btn-sm btn-danger" type="submit">作废</button></form>
        <?php else: ?>
        <form method="post" style="display:inline"><?= csrf_field() ?><input type="hidden" name="action" value="restore"><input type="hidden" name="id" value="<?= (int)$r['id'] ?>">
          <button class="btn btn-sm" type="submit">恢复</button></form>
        <?php endif; ?>
        <form method="post" style="display:inline" onsubmit="return confirm('删除 <?= h($r['license_key']) ?>? 不可恢复(日志保留)!')">
          <?= csrf_field() ?><input type="hidden" name="action" value="del"><input type="hidden" name="id" value="<?= (int)$r['id'] ?>">
          <button class="btn btn-sm btn-danger" type="submit">删</button></form>
        <button class="btn btn-sm" type="button" onclick="var f=document.getElementById('n<?= (int)$r['id'] ?>');f.style.display=f.style.display==='none'?'':'none'">备注</button>
      </td>
    </tr>
    <tr id="n<?= (int)$r['id'] ?>" style="display:none;background:#fbfcff">
      <td colspan="10"><form method="post" class="kv" style="margin:6px 0">
        <?= csrf_field() ?><input type="hidden" name="action" value="note"><input type="hidden" name="id" value="<?= (int)$r['id'] ?>">
        <label>客户</label><input type="text" name="customer" value="<?= h($r['customer_name']) ?>" style="width:180px">
        <label>备注</label><input type="text" name="note" value="<?= h($r['note']) ?>" style="width:320px">
        <button class="btn btn-sm btn-pri" type="submit">保存</button></form></td>
    </tr>
    <?php endforeach; ?>
  </table>

  <div class="pager">共 <?= $total ?> 条 · 第 <?= $page ?>/<?= $pages ?> 页
    <?php if ($page > 1): ?><a href="?<?= $qs ?>&page=1">« 首页</a><a href="?<?= $qs ?>&page=<?= $page - 1 ?>">‹ 上页</a><?php endif; ?>
    <?php if ($page < $pages): ?><a href="?<?= $qs ?>&page=<?= $page + 1 ?>">下页 ›</a><a href="?<?= $qs ?>&page=<?= $pages ?>">末页 »</a><?php endif; ?>
  </div>
</div>
<?php page_foot(); ?>
