#include "Signature.h"
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>

namespace sig {

// v1 embedded vendor key. In a real release this is replaced at build time from a secret and,
// in v2, becomes an asymmetric key pair (only the PUBLIC half ships). Symmetric here means a
// determined attacker who extracts this key can mint licenses — acceptable for a v1 desktop
// product whose goal is honest-user licensing + tamper detection, not DRM. Documented.
static const QByteArray kVendorKey =
    QByteArrayLiteral("occountant.v1.license+update.signing-key.do-not-reuse");

QByteArray sign(const QByteArray& payload)
{
    return QMessageAuthenticationCode::hash(payload, kVendorKey, QCryptographicHash::Sha256).toHex();
}

bool verify(const QByteArray& payload, const QByteArray& signatureHex)
{
    const QByteArray expected = sign(payload);
    // Length-independent early out, then a constant-time compare so a forger cannot time the
    // first-differing byte.
    if (expected.size() != signatureHex.size()) return false;
    quint8 diff = 0;
    for (int i = 0; i < expected.size(); ++i)
        diff |= static_cast<quint8>(expected[i] ^ signatureHex[i]);
    return diff == 0;
}

QString keyFingerprint()
{
    const QByteArray fp = QCryptographicHash::hash(kVendorKey, QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(fp.left(12));
}

} // namespace sig
