# Pilot Feedback Form — Occountant (C11)

A short form the pilot user fills in (or the operator fills in with them). Keep it light — the point
is honest signal, not a survey wall. One submission per notable moment or at each check-in. Findings
flow into `docs/customer-feedback.md` (class A–E) and `docs/pilot-discovery-log.md`.

---

```
Support ID:      OCC-____-____            Date: __________
Business type:   (freelancer | consultant | small shop | repair | service)
Occountant vers: __________               Day of pilot: ____
```

## 1. What were you trying to do?
(One sentence, your words.)
> ____________________________________________________________

## 2. What happened?
> ____________________________________________________________

## 3. How did it go?
- [ ] ✅ Did it easily
- [ ] ⚠️ Did it, but it was confusing / took too long
- [ ] ⛔ Couldn't do it

## 4. If confusing or blocked — where exactly?
(Which screen/button? What did you expect?)
> ____________________________________________________________

## 5. Signal type (tick one)
- [ ] Installation   [ ] First use / setup   [ ] Invoice   [ ] Payment   [ ] Expense
- [ ] Reports / tax  [ ] Backup / restore    [ ] Something looked **wrong** (a number)   [ ] Crash
- [ ] Missing feature   [ ] Speed / performance   [ ] Trust ("is my data OK/private?")

## 6. Did anything look **wrong** in your numbers? (Highest priority)
- [ ] No   [ ] Yes → what looked wrong? (invoice #, amount, report)
> ____________________________________________________________
> *(If yes: click Settings → Diagnostics → Create support bundle and send it. This is urgent.)*

## 7. Did you trust it?
- Numbers correct? [ ] yes [ ] unsure [ ] no
- Data safe? [ ] yes [ ] unsure [ ] no
- Comment:
> ____________________________________________________________

## 8. What would you want changed or added?
(We won't build everything — but we want to hear it.)
> ____________________________________________________________

## 9. Right now, would you pay for this?
- [ ] Yes   [ ] Maybe   [ ] Not yet   [ ] No
- What price would feel fair for your business? `[ ]`
- Why / why not?
> ____________________________________________________________

---

### Operator use only
```
Class (A correctness | B usability | C sales | D nice-to-have | E reject): ____
Severity (1–5): ____   Distinct businesses w/ same theme: ____
Support bundle attached: [ ]   Action: (fix now | defer | reject)   Ref: __________
```

**Rule:** a "something looked wrong" (Q6) is always triaged first and gated with a regression test if
confirmed. A feature request (Q8) is logged in `docs/product-decisions.md` and built only on the
evidence bar there — never because one person asked.
