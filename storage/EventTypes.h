#ifndef STORAGE_EVENT_TYPES_H
#define STORAGE_EVENT_TYPES_H

#include <cstdint>

// Authoritative accounting event types. NEVER renumber or reuse a value — historical
// events on disk reference these numbers forever. Append new types at the end. Each
// type's payload has its own schema version (the EventLog frame's `schema` field) so
// a payload can evolve independently, migrated at replay time.
enum class EventType : uint16_t {
    CustomerCreated  = 1,   // payload: Customer record (id embedded)
    CustomerRenamed  = 2,   // payload: full Customer snapshot after rename (correction-style)
    InvoiceCreated   = 3,   // payload: Invoice record + line count + InvoiceLine records (stable line ids)
    InvoiceCorrected = 4,   // payload: new Invoice snapshot + line set; append-only correction
    CustomerUpdated  = 5,   // payload: full Customer snapshot after a field edit
    PeriodClosed     = 6,   // payload: [label 32][startDate 10][endDate 10][u64 closedAtSeq]
    PeriodReopened   = 7,   // payload: [label 32]
    InvoiceVoided    = 8,   // payload: [u32 targetId][u32 relatedId]  — mark not-effective in place
    InvoiceReversed  = 9,   // payload: [u32 targetId][u32 relatedId]  — link original → its negating entry
    PaymentRecorded  = 10,  // payload: [u32 paymentId][u32 customerId][i64 amountCents][date 10]
    PaymentAllocated = 11,  // payload: [u32 allocId][u32 paymentId][u32 invoiceId][i64 amountCents][date 10]
    AllocationReversed = 12,// payload: [u32 allocId]  — reverse a settlement allocation (append-only)
    AccountOpened    = 13,  // payload: [u32 accountId][u8 type][name 31]
    JournalEntryPosted = 14,// payload: [u32 entryId][u32 reversesEntryId][date 10][u16 n][(u32 acct)(i64 amount)*n]
    EngineVersionStamp = 15,// payload (governance version vector, 14 bytes, positional u16 —
                            //   order fixed forever): [schema][replay][postingPolicy]
                            //   [statement][snapshot][eventLogFormat][engineBuild].
                            //   Authoritative record of which versions authored the history
                            //   from this seq forward. Appended at genesis (adoption/cutover)
                            //   and at every version transition. See CompatibilityManifest.h
                            //   / docs/compatibility-governance.md.
    SupplierCreated  = 16,  // payload: Supplier record (id embedded) — mirrors CustomerCreated
    SupplierUpdated  = 17,  // payload: full Supplier snapshot after a field edit
    ExpenseCreated   = 18,  // payload: Expense record (id embedded) — mirrors InvoiceCreated
    ExpenseCorrected = 19,  // payload: full Expense snapshot; append-only correction (mirrors InvoiceCorrected)
    ExpenseVoided    = 20,  // payload: [u32 targetId][u32 relatedId] — mark not-effective in place
    ExpenseReversed  = 21,  // payload: [u32 targetId][u32 relatedId] — link original → its negating expense
    TaxCodeCreated   = 22,  // payload: TaxCode record (56 B): id/family/version/type/ratePermille/effectiveDate/name.
                            //   Append-only tax policy; a rate change is a NEW version of the same family. The tax
                            //   AMOUNT an invoice/expense pays is captured in its JournalEntryPosted (immutable), so
                            //   a future rate change never reinterprets a historically-authored transaction.
};

#endif // STORAGE_EVENT_TYPES_H
