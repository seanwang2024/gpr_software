<?php
/**
 * api/unbind.php —— 在线解绑(客户端发起)
 * POST {licenseKey, deviceId}: 匹配当前绑定 → 释放(unused, unbind_count+1), Key 可迁移新机
 */
require_once __DIR__ . '/common.php';

list($db, $key, $deviceId) = lic_guard(['licenseKey', 'deviceId']);
lic_throttle($db);

$st = $db->prepare('SELECT * FROM lic_keys WHERE license_key = ?');
$st->execute([$key]);
$row = $st->fetch();

if (!$row) { lic_log($db, 'unbind_fail', $key, $deviceId, 'no key'); sleep(1); lic_err('ERR_NO_KEY', '授权码不存在'); }
if ($row['status'] !== 'active' || strcasecmp($row['device_id'], $deviceId) !== 0) {
    lic_log($db, 'unbind_fail', $key, $deviceId, 'not bound this device'); sleep(1);
    lic_err('ERR_NOT_BOUND', '该授权码未绑定当前电脑, 无需解绑');
}

$db->prepare("UPDATE lic_keys SET status='unused', device_id=NULL, unbind_count=unbind_count+1 WHERE id=?")
    ->execute([$row['id']]);
lic_log($db, 'unbind', $key, $deviceId, 'by client');
lic_ok(['licenseKeyMasked' => lic_mask_key($key)]);
