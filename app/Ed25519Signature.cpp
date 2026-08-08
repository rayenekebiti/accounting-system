// Ed25519Signature.cpp — asymmetric detached signing for the UPDATE path (see Signature.h).
//
// Uses OpenSSL's vetted Ed25519 (EVP_PKEY_ED25519) rather than a hand-vendored primitive: the
// correctness of update verification is a security boundary, and a transcription bug in an ECC
// implementation could either break every update or silently fail to actually verify. OpenSSL's
// implementation is RFC 8032-conformant and continuously audited; only the (small) wiring here is
// ours, and it is fully exercised by the c2test round-trip + tamper + wrong-key checks.
//
// KEYS. The public key is always compiled in (verification-only). The secret SEED is compiled in
// only under ACCT_DEV_SIGNING, so a release artifact (ACCT_DEV_SIGNING=OFF) can verify but never
// sign. The keys below are the DEVELOPMENT keypair used by CI/gates and controlled pilots. For a
// public GA release: generate a fresh keypair OFFLINE, keep the secret in a vault/HSM (never in the
// repo), and replace kUpdatePublicKey with the production public key (or inject it at build time).

#include "Signature.h"

#include <QCryptographicHash>
#include <openssl/evp.h>

namespace sig {
namespace {

// Ed25519 raw public key (32 bytes) — DEV keypair. Verification uses only this.
const unsigned char kUpdatePublicKey[32] = {
    0xdc,0xf6,0x40,0x31,0x55,0xd6,0xa5,0x0f, 0xcf,0x8a,0x02,0x37,0x4c,0x2b,0x04,0x2c,
    0x0e,0x5d,0x20,0x6d,0xd6,0x0d,0xdc,0x5e, 0x5a,0x69,0xdb,0x61,0xbe,0x53,0xd9,0xde
};

#ifdef ACCT_DEV_SIGNING
// Ed25519 raw private SEED (32 bytes) — DEV keypair. Present only in dev/CI/vendor-signing builds.
const unsigned char kUpdateSecretSeed[32] = {
    0xa4,0x3c,0xb8,0xde,0xa9,0xd2,0x38,0x5f, 0x3d,0x58,0xf2,0x5f,0x27,0x7e,0x84,0x76,
    0xc3,0x04,0xa1,0x87,0x66,0x2c,0x1b,0x06, 0x8a,0xe5,0xba,0x58,0x5d,0xfe,0xcf,0xb6
};
#endif

const unsigned char* asU8(const QByteArray& b)
{
    // constData() is never null for a default-constructed QByteArray; for an empty payload OpenSSL
    // reads 0 bytes, which is well-defined.
    return reinterpret_cast<const unsigned char*>(b.constData());
}

} // namespace

QByteArray signDetached(const QByteArray& payload)
{
#ifdef ACCT_DEV_SIGNING
    EVP_PKEY* sk = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, kUpdateSecretSeed, 32);
    if (!sk) return {};

    QByteArray out;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx && EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, sk) == 1) {
        unsigned char sig[64];
        size_t len = sizeof(sig);
        if (EVP_DigestSign(ctx, sig, &len, asU8(payload), static_cast<size_t>(payload.size())) == 1
            && len == 64)
            out = QByteArray(reinterpret_cast<char*>(sig), 64).toHex();
    }
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(sk);
    return out;
#else
    (void)payload;
    return {};   // release build: the secret is not compiled in — cannot sign.
#endif
}

bool verifyDetached(const QByteArray& payload, const QByteArray& signatureHex)
{
    const QByteArray sig = QByteArray::fromHex(signatureHex);
    if (sig.size() != 64) return false;   // Ed25519 signatures are exactly 64 bytes.

    EVP_PKEY* pk = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, kUpdatePublicKey, 32);
    if (!pk) return false;

    bool ok = false;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx && EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pk) == 1) {
        ok = EVP_DigestVerify(ctx, asU8(sig), 64,
                              asU8(payload), static_cast<size_t>(payload.size())) == 1;
    }
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pk);
    return ok;
}

QString updateKeyFingerprint()
{
    const QByteArray fp = QCryptographicHash::hash(
        QByteArray(reinterpret_cast<const char*>(kUpdatePublicKey), 32),
        QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(fp.left(12));
}

} // namespace sig
