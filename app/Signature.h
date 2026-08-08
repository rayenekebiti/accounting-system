#ifndef APP_SIGNATURE_H
#define APP_SIGNATURE_H

#include <QByteArray>
#include <QString>

// Detached-signature primitives for the licensing + update layers. Nothing here is on the
// accounting path. Two DISTINCT trust models — do not conflate them (see docs/update-signing.md):
//
//   • LICENSING (sign/verify): HMAC-SHA256 with an embedded vendor key. This is a SYMMETRIC,
//     LOCAL-TRUST scheme — the app mints its own trial token locally, so it must hold the key.
//     A user who extracts it can only extend their own trial. Acceptable v1 posture; not a
//     boundary against a determined attacker. Intentionally kept symmetric.
//
//   • UPDATES (signDetached/verifyDetached): ASYMMETRIC Ed25519. The shipped app embeds only the
//     PUBLIC key and can VERIFY an update but cannot forge one. The SECRET key is compiled in only
//     under -DACCT_DEV_SIGNING (dev/CI/gates + the offline vendor signing tool); a release artifact
//     built with ACCT_DEV_SIGNING=OFF contains no secret, so a forged update payload is rejected
//     even by someone who fully reverse-engineers the binary. This is the real trust boundary for
//     update delivery.
namespace sig {

// ── Licensing (symmetric HMAC — local trial mint) ──
// Hex-encoded HMAC over `payload`. Used by the license mint (local) + a few test fixtures.
QByteArray sign(const QByteArray& payload);
// Constant-time verification of an HMAC signature produced by sign(). false = tampered.
bool verify(const QByteArray& payload, const QByteArray& signatureHex);
// Short fingerprint of the HMAC key (diagnostics / key-rotation visibility).
QString keyFingerprint();

// ── Updates (asymmetric Ed25519 — vendor signs, app only verifies) ──
// Hex-encoded Ed25519 detached signature (64 bytes → 128 hex). Available only when the secret is
// compiled in (ACCT_DEV_SIGNING); returns empty otherwise — a release binary CANNOT sign.
QByteArray signDetached(const QByteArray& payload);
// Verify an Ed25519 detached signature against the embedded update PUBLIC key. false = forged/
// tampered/wrong-key/malformed. Always available (verification needs only the public key).
bool verifyDetached(const QByteArray& payload, const QByteArray& signatureHex);
// Short fingerprint of the embedded update PUBLIC key (SHA-256, first 12 hex).
QString updateKeyFingerprint();

} // namespace sig

#endif // APP_SIGNATURE_H
