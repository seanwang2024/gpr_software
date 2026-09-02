<?php
/** probe.php —— 部署能力探针(验证后立即删除): PHP版本/openssl/pdo_mysql/写权限 */
header('Content-Type: text/plain; charset=utf-8');
echo 'php=' . PHP_VERSION . "\n";
echo 'openssl=' . (extension_loaded('openssl') ? 'YES' : 'NO') . "\n";
echo 'pdo_mysql=' . (extension_loaded('pdo_mysql') ? 'YES' : 'NO') . "\n";
echo 'json=' . (extension_loaded('json') ? 'YES' : 'NO') . "\n";
echo 'mbstring=' . (extension_loaded('mbstring') ? 'YES' : 'NO') . "\n";
echo 'writable=' . (is_writable(__DIR__) ? 'YES' : 'NO') . "\n";
echo 'https=' . (isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off' ? 'YES' : 'NO') . "\n";
echo 'sig_test=' . (function_exists('openssl_sign') ? 'YES' : 'NO') . "\n";
