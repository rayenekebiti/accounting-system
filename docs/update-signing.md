# Update Signing — Asymmetric Ed25519 (Phase C9)

This documents the update-payload signing scheme and closes ship-checklist **WARNING #6**
("upgrade update-payload signing to v2 Ed25519"). Nothing here is on the accounting path.

## Two distinct trust models — do not conflate them

`app/Signature.h` exposes two families of primitives for two genuinely different problems:

| | **Licensing** | **Updates** |
|---|---|---|
| API | `sig::sign` / `sig::verify` | `sig::signDetached` / `sig::verifyDetached` |
| Scheme | HMAC-SHA256 (symmetric) | **Ed25519 (asymmetric)** |
| Who signs | the **app itself**, locally | the **vendor**, offline |
| Key in shipped binary | the shared HMAC key | only the **public** key |
| Threat model | local-trust: a user who extracts the key can only extend **their own** trial | real boundary: nobody without the vendor **secret** can forge an update the app will accept |
| Why not asymmetric | the app must mint its own offline trial, so it must hold a key | verification needs only the public half |

The licensing scheme is intentionally symmetric and is **not** a defence against a determined
attacker — it deters casual tampering with an offline trial. This is unchanged from v1 and
acceptable for the product's goals. The **update** scheme is the security boundary, and it is now
asymmetric.

## How update signing works now

- **Verification (shipped app):** `sig::verifyDetached(payload, sigHex)` verifies a 64-byte Ed25519
  detached signature over the downloaded payload **bytes** against the embedded **public** key
  (`app/Ed25519Signature.cpp`). `UpdateManager` calls it before staging a download and again before
  applying a staged bundle at startup. A tampered/truncated/forged payload — or a signature made
  with any other key — is rejected and never staged/applied. The DB is untouched.
- **Signing (vendor, offline):** the vendor holds the **secret** key and produces the signature with
  the `ACCT_SIGN` helper on a signing-enabled build:
  ```
  ACCT_SIGN=Occountant-setup.bin ./AccountingQuick.exe   # prints the hex Ed25519 signature
  ```
  Drop the printed hex into `manifest.json`'s `sig` field.
- **Crypto provider:** OpenSSL `EVP_PKEY_ED25519` (RFC 8032-conformant, continuously audited) — we
  deliberately did **not** hand-vendor an elliptic-curve primitive, because a transcription bug in
  update verification is a security hole. Only the (small) wiring is ours, and it is covered by the
  `c2test` round-trip / payload-tamper / signature-tamper / malformed-length / wrong-algorithm
  checks. This adds a runtime dependency on `libcrypto-3-x64.dll`, bundled by `tools/deploy-deps.sh`
  and proven present by `tools/cleanroom.ps1`.

## Keeping the secret out of shipped binaries

The secret seed and the signing code live behind `#ifdef ACCT_DEV_SIGNING`:

- **Dev/CI/gates** build with `-DACCT_DEV_SIGNING=ON` (the CMake default) — signing works, so the
  fixtures in `c2test`/`accept` and the `ACCT_SIGN` tool can produce signatures.
- **Release artifacts** build with `-DACCT_DEV_SIGNING=OFF` (`tools/release.sh` passes this). The
  secret is **not compiled in**; `signDetached()` returns empty. The binary can verify updates but
  cannot forge them.

Verified: the dev exe contains both keys; the `ACCT_DEV_SIGNING=OFF` exe contains the **public key
only** — the secret bytes are absent (byte-scan of the linked executable).

## Key management for public GA (still required before wide release)

The keypair currently embedded is a **development** keypair, used by CI/gates and controlled pilots.
Before a public GA with hosted updates:

1. Generate a **production** Ed25519 keypair **offline** (e.g. `openssl genpkey -algorithm ed25519`).
2. Keep the secret in a vault/HSM — **never** commit it to the repo.
3. Replace `kUpdatePublicKey` in `app/Ed25519Signature.cpp` with the production public key (or inject
   it at build time), and rotate the fingerprint surfaced by `sig::updateKeyFingerprint()`.
4. Sign release payloads from an offline signing build/host that holds the production secret.
5. Stand up HTTPS manifest/payload hosting and point the updater's source at it (ship-checklist
   WARNING #2 — still open, and is what makes the asymmetric boundary matter in practice).

Until hosted updates go live, the pilot delivers updates by hand (or not at all), so the dev keypair
is sufficient and the boundary above is fully in force for any update the app is asked to apply.
