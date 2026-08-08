#ifndef QUICK_C2TEST_H
#define QUICK_C2TEST_H

#include <QString>

// Deterministic, network-free regression harness for the commercial (C2) infrastructure layer.
// Returns the number of failed assertions (0 = all passed). Runs against an isolated scratch dir
// and never touches the user's real books. Invoked via ACCT_C2TEST=<scratchdir>.
int runC2Tests(const QString& scratchDir);

#endif // QUICK_C2TEST_H
