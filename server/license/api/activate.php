<?php
/**
 * api/activate.php —— 在线激活
 * POST {licenseKey, deviceId} + Header X-Client-Key
 * 语义: unused→绑定激活 | active+同机→幂等重发凭证 | active+异机→ERR_BOUND_OTHER | revoked→ERR_REVOKED
 */
require_once __DIR__ . '/common.php';

list($db, $key, $deviceId) = lic_guard(['licenseKey', 'deviceId']);
lic_throttle($db);

$db->beginTransaction();
try {
    $st = $db->prepare('SELECT * FROM lic_keys WHERE license_key = ? FOR UPDATE');
    $st->execute([$key]);
    $row = $st->fetch();

    if (!$row) {
        $db->rollBack(); lic_log($db, 'activate_fail', $key, $deviceId, 'no key'); sleep(1);
        lic_err('ERR_NO_KEY', '授权码不存在, 请核对输入(注意区分数字0与字母O)');
    }
    if ($row['status'] === 'revoked') {
        $db->rollBack(); lic_log($db, 'activate_fail', $key, $deviceId, 'revoked'); sleep(1);
        lic_err('ERR_REVOKED', '该授权码已被作废, 请联系客服');
    }
    if ($row['status'] === 'active' && strcasecmp($row['device_id'], $deviceId) !== 0) {
        $db->rollBack(); lic_log($db, 'activate_fail', $key, $deviceId, 'bound other: ' . $row['device_id']); sleep(1);
        lic_err('ERR_BOUND_OTHER', '该授权码已绑定其他电脑(1个授权码仅可绑定1台), 如需迁移请在原电脑解绑或联系客服解绑');
    }

    $reissue = ($row['status'] === 'active');
    if (!$reissue) {
        $db->prepare("UPDATE lic_keys SET status='active', device_id=?, activated_at=NOW() WHERE id=?")
            ->execute([$deviceId, $row['id']]);
    }
    $db->commit();
} catch (PDOException $e) {
    if ($db->inTransaction()) $db->rollBack();
    lic_err('ERR_DB', '服务暂时不可用, 请稍后重试');
}

lic_log($db, 'activate', $key, $deviceId, $reissue ? 'reissue' : 'bind');
lic_ok([
    'licenseKeyMasked' => lic_mask_key($key),
    'deviceId'         => $deviceId,
    'featureMask'      => (int)$row['feature_mask'],
    'isPermanent'      => 1,
    'credential'       => lic_credential($key, $deviceId, (int)$row['feature_mask']),
]);
