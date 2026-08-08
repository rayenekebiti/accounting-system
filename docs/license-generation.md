# License Generation — Vendor Workflow (Ed25519)

How the vendor generates a customer license, where the keys live, and how a customer activates it
offline. This extends the **existing** `LicenseManager` — it is not a separate licensing system.

> **Security in one line:** paid editions (Personal/Business) require a **vendor Ed25519 signature**
> the shipped app can verify but never forge. The self-issued trial stays symmetric (HMAC), and a
> self-signed token claiming a paid edition is **rejected**.

---

## Keys — where they live

Licenses reuse the vendor **Ed25519** key introduced for update signing (domain-separated so a license
signature can never be confused with an update signature — see `docs/update-signing.md`):

- **Public key** — embedded in every build (`app/Ed25519Signature.cpp`, `kUpdatePublicKey`). The
  shipped app uses it to **verify** licenses. It is safe to ship.
- **Private key (secret seed)** — compiled in **only** under `-DACCT_DEV_SIGNING`
  (`kUpdateSecretSeed`, `#ifdef ACCT_DEV_SIGNING`). It exists **only** on the vendor/dev machine and in
  the generator tool. `tools/release.sh` builds the customer artifact with `-DACCT_DEV_SIGNING=OFF`, so
  **the private key is never in a shipped binary** (verified: the release exe contains the public key
  only). **Never** commit a production private key or distribute the generator.

The current embedded keypair is the **development** keypair (fingerprint shown by the generator). For
public GA, generate a fresh keypair offline, keep the secret in a vault/HSM, and replace
`kUpdatePublicKey` (and the dev secret) — see `docs/update-signing.md` §"Key management".

---

## How to generate a key

The generator is a **developer-only** console tool, built only when `ACCT_DEV_SIGNING=ON`:

```bash
# On the vendor machine (dev build; ACCT_DEV_SIGNING defaults to ON):
cmake -S . -B build -G Ninja
cmake --build build --target license_gen

# Business license, 1 year, two features:
./build/license_gen.exe --name "Acme Ltd" --plan business \
    --features export,priority-support --issued 2026-08-01 --expires 2027-08-01

# Personal, perpetual (no expiry):
./build/license_gen.exe --name "Jane Doe" --plan personal --expires perpetual
```

### Running the tool (avoid STATUS_DLL_NOT_FOUND)

`license_gen.exe` depends on `Qt6Core.dll`, `libcrypto-3-x64.dll`, `libstdc++-6.dll`, and
`libgcc_s_seh-1.dll`. Run it **either** way:

- **From the MSYS2 UCRT64 shell** (or any shell with `C:\msys64\ucrt64\bin` on `PATH`) — the DLLs
  resolve from `ucrt64\bin`. This is the normal dev flow.
- **Self-contained (recommended for portability)** — bundle the tool with its own DLL closure so it
  runs anywhere, no `PATH` setup:
  ```bash
  cmake --build build --target license_gen_dist
  ./build/license_gen_dist/license_gen.exe --name "Acme Ltd" --plan business --expires 2027-01-01
  ```
  `build/license_gen_dist/` is a copyable folder (tool + all required DLLs).

> Running bare `build/license_gen.exe` from a plain shell/`cmd` with neither of the above will fail
> silently with exit code `0xC0000135` (`STATUS_DLL_NOT_FOUND`) — a missing-DLL error, not a bug in the
> tool. Smoke test: `bash tools/license_gen_smoke.sh` (builds the dist and verifies a clean-room run).

Options:

| Option | Meaning | Default |
|---|---|---|
| `--name` | Customer / company name (**required**) | — |
| `--plan` | `personal` \| `business` \| `trial` | `business` |
| `--features` | Comma-separated enabled feature keys | none |
| `--issued` | Issue date `YYYY-MM-DD` | today |
| `--expires` | Expiry `YYYY-MM-DD`, or `perpetual` | `perpetual` |
| `--id` | License id | random UUID |

Output: diagnostics go to **stderr**; the **activation key** (one `OCCLIC-…` line) goes to **stdout**,
so you can copy or pipe it. The key encodes `{plan, name, id, issued, expires, features}` plus a 64-byte
Ed25519 signature.

> The generator prints an empty error if built **without** the private key
> (`-DACCT_DEV_SIGNING=OFF`) — that is the release/customer configuration, which intentionally cannot
> sign.

---

## Token format (v2)

`OCCLIC-` + `base64url(JSON)` + `.` + `signatureHex`, where the JSON is:

```json
{ "v":2, "alg":"ed25519", "ed":2, "to":"Acme Ltd", "id":"<uuid>",
  "iat":1785542400, "exp":1817164799, "feat":["export","priority-support"] }
```

- `alg` = `ed25519` (vendor) or `hs256` (self-signed trial). The verifier picks the scheme from `alg`.
- `ed`: 0=Trial, 1=Personal, 2=Business. `exp`=0 means perpetual.
- Ed25519 tokens are signed over `"occ.lic.v2:" + payload` (domain separation).
- Back-compat: a v1 token (no `alg`) is treated as `hs256`/Trial.

---

## Customer activation workflow (offline)

1. The customer receives the `OCCLIC-…` key (email is fine — it grants only what it says and is not a
   secret that can mint other licenses).
2. In Occountant: **Settings → About → paste the key → Activate**.
3. The app verifies the Ed25519 signature **locally with the embedded public key** — **no server, no
   internet**. On success the edition unlocks and the enabled features are recorded; the customer's
   books are unchanged.
4. Expiry + a 7-day grace window apply exactly as before; a perpetual license never expires.

**What the app rejects** (all offline, all tested in `ACCT_C2TEST`): a modified key, a wrong/mismatched
signature, an expired key (past grace), and — crucially — a **self-signed (HMAC) token claiming a paid
edition** (only the trial may be self-signed).

---

## Security model (why this is trustworthy)

- **Only the vendor can grant paid editions.** Paid licenses need the Ed25519 **private** key, which is
  never shipped. Extracting anything from a customer binary yields only the **public** key.
- **The trial is unchanged and offline-mintable.** It stays HMAC/local-trust; forging a trial only
  extends your own trial (the accepted v1 threat) — and a forged *paid* HMAC token is rejected.
- **Offline-first.** Activation and verification never touch a network.
- **Domain-separated.** A license signature cannot be replayed as an update signature.

## Tests (`ACCT_C2TEST`, 38/0)

Valid vendor key activates (Business/Personal); features carried; customer name recorded; `OCCLIC-`
prefix accepted; **modified key rejected**; **wrong signature rejected**; **expired key rejected**;
**HMAC-paid rejected**; **trial behavior unchanged**; plus the pre-existing expired/grace/tamper/
corrupt-cache/first-run-trial cases (now on Ed25519 for paid).
