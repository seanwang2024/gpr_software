/**
 * License.cpp —— 授权客户端实现
 * 验签: Windows 原生 CryptoAPI(CryptImportPublicKeyInfo + CryptVerifySignature, PKCS#1 v1.5 SHA-256)
 *       与服务端 PHP openssl_sign(..., OPENSSL_ALGO_SHA256) 逐字节对应。
 * 注意: CryptoAPI 签名字节序为小端, 需将标准大端签名反转后传入。
 */
#include "License.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include <cstdio>
#include <windows.h>
#include <wincrypt.h>

namespace License {

// ---------------- 公钥(测试密钥对, 上线轮换) ----------------
QString publicKeyPem()
{
    return QStringLiteral(
        "-----BEGIN PUBLIC KEY-----\n"
        "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA1Vcgbtn6YvU0PUBOzoIm\n"
        "ATfftzRzla5HOQZ2CXxkXpFoqyQs5oLdElmJaaCS/i8tSwXwfgu6337sKxgnwiJa\n"
        "7/GDkXMs4lDIlsVpzX85u+LN5XDVsBNkj/g1IY4SDuFJ8BSEn+Cx/AwregPHBmmo\n"
        "v/doSMYr6XzU+geFS55TnCk1LWI2ILo2upVyC2E1rqtV314UyDHKslcBLs1AKNy3\n"
        "QQ3U+QiRnOJprSFgOgt7iwmWvFk009z2pIpD5XxYwjCDWbPEjtcB+EVuHQTlnfqT\n"
        "nCebRKhL3FYgSoyq9EHwh7Qr3//iWZkVNao21Px0BizS/XJICJCdYq1IsNx10IKk\n"
        "kQIDAQAB\n"
        "-----END PUBLIC KEY-----\n");
}

// ---------------- 机器指纹 ----------------
QString deviceId()
{
    static QString cached;
    if (!cached.isEmpty())
        return cached;
    // 1) 注册表 MachineGuid (HKLM\SOFTWARE\Microsoft\Cryptography, 64位进程无重定向)
    QSettings reg(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography"),
                  QSettings::NativeFormat);
    const QString guid = reg.value(QStringLiteral("MachineGuid")).toString();
    // 2) 系统盘卷序列号(重装系统会变, 物理换盘会变)
    DWORD serial = 0;
    const QString sysDrive = qEnvironmentVariable("SystemDrive");
    std::wstring root = sysDrive.isEmpty() ? L"C:\\" : (sysDrive.toStdWString() + L"\\");
    GetVolumeInformationW(root.c_str(), nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);
    const QByteArray src = (guid + QLatin1Char('|') + QString::number(static_cast<uint>(serial))).toUtf8();
    cached = QString::fromLatin1(QCryptographicHash::hash(src, QCryptographicHash::Sha256).toHex());
    return cached;
}

QString deviceIdDisplay()
{
    const QString d = deviceId().left(16);
    return d.left(4) + QLatin1Char('-') + d.mid(4, 4) + QLatin1Char('-')
         + d.mid(8, 4) + QLatin1Char('-') + d.mid(12, 4);
}

QString deviceIdFullGrouped()
{
    const QString d = deviceId();
    QString out;
    for (int i = 0; i < d.size(); i += 8) {
        if (i) out += QLatin1Char('-');
        out += d.mid(i, 8);
    }
    return out;
}

// ---------------- 凭证存取 ----------------
static QString settingsKey() { return QStringLiteral("license/credential"); }

QString loadStored()
{
    QSettings st(QStringLiteral("Diting"), QStringLiteral("depro"));
    return st.value(settingsKey()).toString();
}

void saveStored(const QString &credentialB64)
{
    QSettings st(QStringLiteral("Diting"), QStringLiteral("depro"));
    st.setValue(settingsKey(), credentialB64);
}

void clearStored()
{
    QSettings st(QStringLiteral("Diting"), QStringLiteral("depro"));
    st.remove(settingsKey());
}

// ---------------- 序列化 ----------------
QString serialize(const Credential &c)
{
    QJsonObject o;
    o[QStringLiteral("payload")] = c.payload;
    o[QStringLiteral("sig")] = c.sigB64;
    return QString::fromLatin1(QJsonDocument(o).toJson(QJsonDocument::Compact).toBase64());
}

bool parse(const QString &stored, Credential *out)
{
    if (stored.isEmpty())
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromBase64(stored.toLatin1()));
    if (!doc.isObject())
        return false;
    const QJsonObject o = doc.object();
    Credential c;
    c.payload = o[QStringLiteral("payload")].toString();
    c.sigB64 = o[QStringLiteral("sig")].toString();
    if (c.payload.isEmpty() || c.sigB64.isEmpty())
        return false;
    // payload: DTLIC1|licenseKey|deviceId|featureMask|isPermanent
    const QStringList parts = c.payload.split(QLatin1Char('|'));
    if (parts.size() != 5 || parts[0] != QStringLiteral("DTLIC1"))
        return false;
    c.licenseKey = parts[1];
    c.credDeviceId = parts[2];
    c.featureMask = parts[3].toInt();
    c.isPermanent = parts[4] == QLatin1String("1");
    if (out)
        *out = c;
    return true;
}

// ---------------- RSA-SHA256 验签 (CryptoAPI) ----------------
bool verifySignature(const QByteArray &payload, const QByteArray &sigDer)
{
    if (payload.isEmpty() || sigDer.isEmpty())
        return false;

    // PEM 公钥 → DER SubjectPublicKeyInfo
    const QByteArray pem = publicKeyPem().toLatin1();
    DWORD derLen = 0;
    if (!CryptStringToBinaryA(pem.constData(), pem.size(), CRYPT_STRING_BASE64HEADER,
                              nullptr, &derLen, nullptr, nullptr))
        return false;
    QByteArray der(int(derLen), Qt::Uninitialized);
    if (!CryptStringToBinaryA(pem.constData(), pem.size(), CRYPT_STRING_BASE64HEADER,
                              reinterpret_cast<BYTE *>(der.data()), &derLen, nullptr, nullptr))
        return false;

    bool ok = false;
    HCRYPTPROV prov = 0;
    HCRYPTKEY key = 0;
    HCRYPTHASH hash = 0;
    LPVOID info = nullptr;
    auto step = [](const char *name, BOOL r) {
        if (!r)
            fprintf(stderr, "  verifySignature: %s failed GLE=%lu\n", name, GetLastError());
        return r;
    };
    do {
        if (!step("CryptAcquireContextW",
                  CryptAcquireContextW(&prov, nullptr, MS_ENH_RSA_AES_PROV_W, PROV_RSA_AES,
                                       CRYPT_VERIFYCONTEXT)))
            break;
        // DER → CERT_PUBLIC_KEY_INFO → 导入公钥
        DWORD infoLen = 0;
        if (!step("CryptDecodeObjectEx",
                  CryptDecodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
                                      reinterpret_cast<const BYTE *>(der.constData()), derLen,
                                      CRYPT_DECODE_ALLOC_FLAG, nullptr, &info, &infoLen)))
            break;
        if (!step("CryptImportPublicKeyInfo",
                  CryptImportPublicKeyInfo(prov, X509_ASN_ENCODING,
                                           static_cast<PCERT_PUBLIC_KEY_INFO>(info), &key)))
            break;
        if (!step("CryptCreateHash", CryptCreateHash(prov, CALG_SHA_256, 0, 0, &hash)))
            break;
        if (!step("CryptHashData",
                  CryptHashData(hash, reinterpret_cast<const BYTE *>(payload.constData()),
                                DWORD(payload.size()), 0)))
            break;
        // CryptoAPI 要求小端签名: 反转大端 DER 签名(与标准大端相反)
        QByteArray sigRev(sigDer.size(), Qt::Uninitialized);
        for (int i = 0; i < sigDer.size(); ++i)
            sigRev[i] = sigDer.at(sigDer.size() - 1 - i);
        ok = step("CryptVerifySignature",
                  CryptVerifySignature(hash,
                                       reinterpret_cast<const BYTE *>(sigRev.constData()),
                                       DWORD(sigRev.size()), key, nullptr, 0));
    } while (false);

    if (hash) CryptDestroyHash(hash);
    if (key) CryptDestroyKey(key);
    if (prov) CryptReleaseContext(prov, 0);
    if (info) LocalFree(info);
    return ok;
}

// ---------------- 综合校验 ----------------
bool checkCredential(const QString &stored, Credential *out)
{
    Credential c;
    if (!parse(stored, &c))
        return false;
    const QByteArray sigDer = QByteArray::fromBase64(c.sigB64.toLatin1());
    // 验签 + 设备绑定 双条件(spec §3)
    if (!verifySignature(c.payload.toUtf8(), sigDer))
        return false;
    if (c.credDeviceId.compare(deviceId(), Qt::CaseInsensitive) != 0)
        return false;
    c.valid = true;
    if (out)
        *out = c;
    return true;
}

bool isUnlocked(int featureBit)
{
    Credential c;
    if (!checkCredential(loadStored(), &c))
        return false;
    return (c.featureMask & (1 << featureBit)) != 0;
}

QString maskKey(const QString &licenseKey)
{
    if (licenseKey.size() < 8)
        return QStringLiteral("DT-***-***-***");
    return licenseKey.left(8) + QStringLiteral("-*****-*****");
}

// ---------------- 自测 ----------------
bool selfTest()
{
    // 向量: openssl dgst -sha256 -sign 测试私钥 对固定 payload 的签名
    const QByteArray payload =
        "DTLIC1|DT-TEST1-TEST1-TEST1|0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef|1|1";
    const char *sigB64 =
        "pUvRVsmqSdlQJJ9UE4tJC7+BKQuYveohDBKw1rk4G8buoZQK2rzR9OLBwRzpiMDyJDbKpm/CkkITGqPDyDIhamWt3mY6tTk2yH7njKg9tMZPf/iOwvcFTHWPZ02jUFx65iZb5eNj85jgB0i6AehuR51FLUR0Hq/vfBrFPyf7jX0h4zr8a11AlB9dc0+Vi7XsH2UNhFTfBl1i2k5AZroQzbKloy6Hmm6f4dmCycksYEj/6oNU6kbCpPMjKo5RWKgT71cZTIiVM5R3SHzUXN7Qy1U56tLbtvplpgjXg6Owf64XNMk2IsRSY70b2p/Vx1fgzVKt43iMz5qIOq3EUvbF7w==";

    // 1) 验签通路
    const bool sigOk = verifySignature(payload, QByteArray::fromBase64(sigB64));
    // 2) 篡改检测: 改1字节必须失败
    QByteArray bad = payload;
    bad[bad.size() / 2] = bad.at(bad.size() / 2) == 'X' ? 'Y' : 'X';
    const bool tamperDetected = !verifySignature(bad, QByteArray::fromBase64(sigB64));
    // 3) 凭证序列化/解析往返
    Credential c;
    c.payload = QString::fromLatin1(payload);
    c.sigB64 = QLatin1String(sigB64);
    const QString ser = serialize(c);
    Credential back;
    const bool roundTrip = parse(ser, &back)
        && back.licenseKey == QStringLiteral("DT-TEST1-TEST1-TEST1")
        && back.featureMask == 1 && back.isPermanent;
    // 4) 机器指纹
    const QString dev = deviceId();

    fprintf(stderr, "License::selfTest sig=%s tamper=%s roundtrip=%s deviceId=%s(display %s)\n",
            sigOk ? "OK" : "FAIL", tamperDetected ? "OK" : "FAIL",
            roundTrip ? "OK" : "FAIL",
            qPrintable(dev), qPrintable(deviceIdDisplay()));
    return sigOk && tamperDetected && roundTrip && dev.size() == 64;
}

} // namespace License
