#ifndef QUICK_A11Y_H
#define QUICK_A11Y_H

// Accessibility-tree audit (ACCT_A11Y=1). Walks the REAL QAccessible interface tree
// that a screen reader (NVDA/Narrator/UIA) consumes — not the QML source — and
// asserts every interactive control exposes a role + non-empty name. This validates
// that the declared Accessible.role/name actually reach the platform a11y API.
//
// Boundary: this proves the semantics are EXPOSED and named; the quality of the
// spoken experience (NVDA/Narrator phrasing, focus-follow, reading order) is a
// manual screen-reader pass — it cannot be automated without an AT client.

class QQmlApplicationEngine;

int runA11yAudit(QQmlApplicationEngine& engine);

#endif // QUICK_A11Y_H
