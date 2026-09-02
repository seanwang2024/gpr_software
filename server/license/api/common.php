<?php
/**
 * api/common.php —— 授权 API 公共: 配置/DB/输入校验/限速/签名/JSON 响应
 * 所有接口仅接受 POST JSON + Header X-Client-Key(弱防刷)。
 */
require_once __DIR__ . '/../config.php';

function lic_db() {
    static $db = null;
    if ($db === null) {
        $db = new PDO('mysql:host=' . LIC_DB_HOST . ';dbname=' . LIC_DB_NAME . ';charset=utf8mb4',
            LIC_DB_USER, LIC_DB_PASS, [
                PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
                PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
            ]);
    }
    return $db;
}

function lic_json($arr) {
    header('Content-Type: application/json; charset=utf-8');
    echo json_encode($arr, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    exit;
}
function lic_ok($extra = [])   { lic_json(array_merge(['code' => 0, 'msg' => 'ok'], $extra)); }
function lic_err($err, $msg)   { lic_json(['code' => 1, 'err' => $err, 'msg' => $msg]); }

/** 读取 POST JSON body 为数组 */
function lic_input() {
    $raw = file_get_contents('php://input');
    $j = json_decode($raw, true);
    return is_array($j) ? $j : [];
}

/** License Key 规范化: 去空白/连字符转大写后按 DT-XXXXX-XXXXX-XXXXX 重组 */
function lic_norm_key($k) {
    $k = strtoupper(preg_replace('/[^A-Za-z0-9]/', '', (string)$k));
    if (strlen($k) === 17 && substr($k, 0, 2) === 'DT') {
        return 'DT-' . substr($k, 2, 5) . '-' . substr($k, 7, 5) . '-' . substr($k, 12, 5);
    }
    return $k; // 长度不符 → 查库必然 miss
}

function lic_client_ip() {
    return isset($_SERVER['HTTP_X_FORWARDED_FOR']) && $_SERVER['HTTP_X_FORWARDED_FOR'] !== ''
        ? trim(explode(',', $_SERVER['HTTP_X_FORWARDED_FOR'])[0])
        : (isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '');
}

function lic_log($db, $action, $key = '', $deviceId = '', $detail = '') {
    try {
        $db->prepare('INSERT INTO lic_logs (license_key, device_id, action, ip, detail) VALUES (?,?,?,?,?)')
            ->execute([$key, $deviceId, $action, lic_client_ip(), mb_substr($detail, 0, 250)]);
    } catch (PDOException $e) { /* 日志失败不阻断主流程 */ }
}

/** 接口入口统一检查: 方法/Header/字段, 返回 [db, licenseKey, deviceId] 或直接 lic_err 退出 */
function lic_guard($fields) {
    if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') lic_err('ERR_METHOD', '仅接受 POST');
    $ck = $_SERVER['HTTP_X_CLIENT_KEY'] ?? '';
    if ($ck === '' || !hash_equals(LIC_CLIENT_KEY, $ck)) lic_err('ERR_CLIENT', '客户端标识无效');
    $in = lic_input();
    $vals = [];
    foreach ($fields as $f) {
        $v = isset($in[$f]) ? trim((string)$in[$f]) : '';
        if ($v === '') lic_err('ERR_PARAM', '参数缺失: ' . $f);
        $vals[] = $v;
    }
    if (count($fields) !== 2) lic_err('ERR_PARAM', '参数过多');
    list($key, $deviceId) = $vals;
    $key = lic_norm_key($key);
    if (!preg_match('/^DT-[A-Z2-9]{5}-[A-Z2-9]{5}-[A-Z2-9]{5}$/', $key))
        lic_err('ERR_KEY_FORMAT', '授权码格式不正确, 应为 DT-XXXXX-XXXXX-XXXXX');
    if (!preg_match('/^[0-9a-fA-F]{32,64}$/', $deviceId))
        lic_err('ERR_DEVICE_FORMAT', '设备标识格式不正确');
    return [lic_db(), $key, strtolower($deviceId)];
}

/** 失败限速: 同 IP 15 分钟内失败 ≥10 次 → 拒绝 */
function lic_throttle($db) {
    $st = $db->prepare("SELECT COUNT(*) c FROM lic_logs WHERE ip=? AND created_at > DATE_SUB(NOW(), INTERVAL 15 MINUTE)
                        AND action IN ('activate_fail','unbind_fail','login_fail')");
    $st->execute([lic_client_ip()]);
    if ($st->fetch()['c'] >= 10) {
        sleep(2);
        lic_err('ERR_THROTTLE', '尝试过于频繁, 请15分钟后再试');
    }
}

/** 组装 payload 并用 RSA-SHA256 签名 → 返回整体 base64 的凭证串 */
function lic_credential($licenseKey, $deviceId, $featureMask) {
    $payload = "DTLIC1|{$licenseKey}|{$deviceId}|{$featureMask}|1";
    $sig = '';
    if (!openssl_sign($payload, $sig, LIC_RSA_PRIVATE_PEM, OPENSSL_ALGO_SHA256)) {
        lic_err('ERR_SIGN', '签名失败');
    }
    $json = json_encode(['payload' => $payload, 'sig' => base64_encode($sig)],
        JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    return base64_encode($json);
}

/** 脱敏显示: DT-AB12C-*****-***** */
function lic_mask_key($key) {
    return substr($key, 0, 8) . '-*****-*****';
}
