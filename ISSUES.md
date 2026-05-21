# Code Issues

## transaction.h

1. Pure virtuals missing `const`:
   ```cpp
   // Wrong
   virtual double getEffectiveAmount()=0;
   virtual TransactionType getType()=0;
   virtual void display()=0;

   // Should be
   virtual double getEffectiveAmount() const = 0;
   virtual TransactionType getType() const = 0;
   virtual void display() const = 0;
   ```

2. `serialize` / `deserialize` missing buffer parameters:
   ```cpp
   // Wrong
   virtual void serialize()=0;
   virtual void deserialize()=0;

   // Should be
   virtual void serialize(char* buffer) const = 0;
   virtual void deserialize(const char* buffer) = 0;
   ```

3. Getter/setter naming — wrong convention and bogus parameters:
   ```cpp
   // Wrong
   unsigned short int id_getter() const;
   double amount_getter(double amount_get) const;
   unsigned short int category_id_getter(unsigned short int category_id_get) const;
   TransactionType type_getter(TransactionType type_get) const;
   bool isDeleted_getter(bool isDeleted_get) const;

   // Should be (no parameters, camelCase)
   unsigned short int getId() const;
   double getAmount() const;
   unsigned short int getCategoryId() const;
   TransactionType getType() const;
   bool getIsDeleted() const;
   ```

4. Member variable `category_id` should be `categoryId` (camelCase convention).

---

## IncomeTransaction.h

1. Missing constructor:
   ```cpp
   // Should be added
   IncomeTransaction(unsigned short int id, const char* description,
                     double amount, const char* date,
                     unsigned short int categoryId);
   ```

2. All overrides missing `const`:
   ```cpp
   // Wrong
   double getEffectiveAmount() override;
   TransactionType getType() override;
   void display() override;

   // Should be
   double getEffectiveAmount() const override;
   TransactionType getType() const override;
   void display() const override;
   ```

3. `serialize` / `deserialize` missing buffer parameters (same as base class):
   ```cpp
   // Wrong
   void serialize() override;
   void deserialize() override;

   // Should be
   void serialize(char* buffer) const override;
   void deserialize(const char* buffer) override;
   ```

---

## constants.h

- Missing record size constants needed before any serialization code is written:
  ```cpp
  // Must be added
  const unsigned short int TRANSACTION_RECORD_SIZE = 128;
  const unsigned short int ACCOUNT_RECORD_SIZE     = 160;
  const unsigned short int CATEGORY_RECORD_SIZE    = 48;
  const unsigned short int BUDGET_RECORD_SIZE      = 32;
  ```
