<?php
/** admin/export.php —— 授权码 CSV 导出(UTF-8 BOM, Excel 直接打开不乱码); 过滤条件与列表一致 */
require_once __DIR__ . '/lib.php';
$admin = boot();

$q = trim((string)($_GET['q'] ?? ''));
$fs = (string)($_GET['status'] ?? '');
$where = []; $args = [];
if ($q !== '') { $where[] = '(license_key LIKE ? OR customer_name LIKE ? OR device_id LIKE ?)';
    $args[] = "%$q%"; $args[] = "%$q%"; $args[] = "%$q%"; }
if (in_array($fs, ['unused', 'active', 'revoked'], true)) { $where[] = 'status=?'; $args[] = $fs; }
$w = $where ? (' WHERE ' . implode(' AND ', $where)) : '';

$st = db()->prepare("SELECT * FROM lic_keys$w ORDER BY id");
$st->execute($args);

header('Content-Type: text/csv; charset=utf-8');
header('Content-Disposition: attachment; filename="licenses_' . date('Ymd_His') . '.csv"');
$out = fopen('php://output', 'w');
fwrite($out, "\xEF\xBB\xBF"); // BOM
fputcsv($out, ['ID', '授权码', '状态', '客户名称', '功能', '绑定设备ID', '激活时间', '解绑次数', '备注', '生成时间']);
$stMap = ['unused' => '未使用', 'active' => '已激活', 'revoked' => '已作废'];
while ($r = $st->fetch()) {
    fputcsv($out, [
        $r['id'], $r['license_key'], $stMap[$r['status']] ?? $r['status'], $r['customer_name'],
        feat_label((int)$r['feature_mask']), $r['device_id'], $r['activated_at'],
        $r['unbind_count'], $r['note'], $r['created_at'],
    ]);
}
fclose($out);
