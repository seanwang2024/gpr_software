<?php
/**
 * api/verify.php —— 授权状态校验(客户端巡检; 未来业务云二校验同用此口)
 * POST {licenseKey, deviceId} → {code:0, valid, featureMask}
 * 仅失败时写日志(防巡检刷表), 网络异常由客户端静默处理。
 */
require_once __DIR__ . '/common.php';

list($db, $key, $deviceId) = lic_guard(['licenseKey', 'deviceId']);

$st = $db->prepare('SELECT * FROM lic_keys WHERE license_key = ?');
$st->execute([$key]);
$row = $st->fetch();

$valid = $row && $row['status'] === 'active' && strcasecmp($row['device_id'], $deviceId) === 0;
if (!$valid) lic_log($db, 'verify_fail', $key, $deviceId, $row ? $row['status'] : 'no key');

lic_ok([
    'valid'       => $valid ? 1 : 0,
    'featureMask' => $valid ? (int)$row['feature_mask'] : 0,
]);
