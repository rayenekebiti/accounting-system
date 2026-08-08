# Release Notes Template

Customer-facing release notes. Keep them plain, honest, and useful. Lead with what the customer
gains; be explicit about anything that needs their attention.

```
# Occountant <version> — <YYYY-MM-DD>

<One or two sentences: the headline of this release.>

## New
- <Feature, in plain language — what it lets the customer do.>

## Improved
- <Refinement — clearer wording, faster screen, better layout.>

## Fixed
- <Bug fix, described from the customer's point of view (not the code).>

## Please note
- <Anything requiring attention: a behaviour change, a manual step, a deprecation.>
- <If none: "Your data is unchanged and upgrades automatically. No action needed.">

## Security
- <Any security-relevant change. If none, omit this section.>

---
Channel: <stable | rc | beta | development>
This release preserves all your data. The accounting engine is unchanged and your books remain
byte-for-byte reconstructable from their history.
Verify your install: SHA256SUMS is published alongside the download.
```

## Style rules

- Write for the business owner, not the developer. "Record part-payments against several invoices,"
  not "refactored the allocation VM."
- Never claim a fix you didn't ship or verify.
- If a release changes nothing a customer sees (internal only), say so honestly.
- Every public release links its `SHA256SUMS` and is signed.
