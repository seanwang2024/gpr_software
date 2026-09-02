#pragma once
/**
 * License.h —— 软件授权(单机永久 License)客户端模块
 * 机器指纹 + RSA-SHA256 凭证验签(Windows 原生 CryptoAPI, 零外部依赖)。
 * 凭证由授权云签发: base64(JSON{payload:"DTLIC1|key|deviceId|featureMask|perm", sig})
 * 设计文档: specs/license管理需求.md
 */
#include <QString>
#include <QByteArray>

namespace License {

// ---- 功能位 ----
enum Feature {
    FeatureAiAnalysis = 0,   // bit0: AI分析模块
};

// RSA 公钥(PEM) —— 测试密钥对; 正式上线前必须轮换(见 server/license/tools/deploy.md)
QString publicKeyPem();

// ---- 机器指纹 ----
// SHA256( MachineGuid | 系统盘卷序列号 ) → 64位hex; 换机/重装系统=新ID(spec 短板②)
QString deviceId();
QString deviceIdDisplay();          // 显示用: 前16位 4-4-4-4 分组
QString deviceIdFullGrouped();      // 复制用: 64位 8组×8

// ---- 凭证 ----
struct Credential {
    QString payload;       // DTLIC1|licenseKey|deviceId|featureMask|isPermanent
    QString sigB64;        // base64(RSA-SHA256(payload))
    QString licenseKey;    // ← payload 解析
    QString credDeviceId;
    int     featureMask = 0;
    bool    isPermanent = false;
    bool    valid = false; // 验签通过且绑定本机
};

QString serialize(const Credential &c);          // → base64(JSON) 服务端同格式
bool parse(const QString &stored, Credential *out);  // base64(JSON) → struct(仅解析不验签)

// 存取: QSettings("Diting","depro") 键 license/credential
QString loadStored();
void saveStored(const QString &credentialB64);
void clearStored();

// RSA-SHA256 验签(CryptoAPI, PKCS#1 v1.5) —— 与服务端 openssl_sign 对应
bool verifySignature(const QByteArray &payload, const QByteArray &sigDer);

// 综合校验: 解析+验签+payload.deviceId==本机 → out->valid=true
bool checkCredential(const QString &stored, Credential *out = nullptr);

// 高级功能解锁判定(凭证有效且含对应功能位)
bool isUnlocked(int featureBit = FeatureAiAnalysis);

// 授权码脱敏 DT-AB12C-*****-***** (与服务器规则一致)
QString maskKey(const QString &licenseKey);

// 内置向量自测(GPR_LICENSE_SELFTEST=1 时 main 调用): 验签通路+deviceId 输出
bool selfTest();

} // namespace License
