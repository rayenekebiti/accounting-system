#ifndef STORAGE_SERVICE_H
#define STORAGE_SERVICE_H
#include "TransactionRepositories.h"
#include "AccountRepository.h"
#include "CategoryRepository.h"
#include "BudgetRepository.h"
#include "CustomerRepository.h"
#include "SupplierRepository.h"
#include "ProductRepository.h"
#include "InvoiceRepository.h"
#include "PaymentRepository.h"
#include <QLockFile>
#include <QString>
#include <memory>
#include <string>
#include <stdexcept>

// Layer 3 — StorageService (facade / singleton).
// Owns all repositories; the UI never touches a repository constructor directly.
//
// Usage:
//   if (!StorageService::instance().initialize("/path/to/data/dir")) { ... }
//   StorageService::instance().customers().save(cust);
class StorageService {
    std::unique_ptr<QLockFile>          lockFile_;
    std::unique_ptr<TransactionRepository> transactions_;
    std::unique_ptr<AccountRepository>     accounts_;
    std::unique_ptr<CategoryRepository>    categories_;
    std::unique_ptr<BudgetRepository>      budgets_;
    std::unique_ptr<CustomerRepository>    customers_;
    std::unique_ptr<SupplierRepository>    suppliers_;
    std::unique_ptr<ProductRepository>     products_;
    std::unique_ptr<InvoiceRepository>     invoices_;
    std::unique_ptr<PaymentRepository>     payments_;
    std::string                            initError_;

    StorageService() = default;

public:
    static StorageService& instance()
    {
        static StorageService inst;
        return inst;
    }

    // Returns the human-readable reason the last initialize() call failed.
    const std::string& lastInitError() const { return initError_; }

    // Opens (or creates) all data files under dataDir.
    // Acquires an exclusive lock to prevent two instances from running simultaneously.
    // Returns false and sets lastInitError() if the lock cannot be acquired
    // or any file fails to open.
    bool initialize(const std::string& dataDir)
    {
        initError_.clear();

        // Acquire an exclusive file lock so a second instance fails fast.
        const QString lockPath = QString::fromStdString(dataDir + "/accountingpro.lock");
        lockFile_ = std::make_unique<QLockFile>(lockPath);
        if (!lockFile_->tryLock(300)) {
            lockFile_.reset();
            initError_ =
                "AccountingPro is already running, "
                "or the data folder is locked by another process.";
            return false;
        }

        try {
            transactions_ = std::make_unique<TransactionRepository>(dataDir + "/transactions.dat");
            accounts_     = std::make_unique<AccountRepository>    (dataDir + "/accounts.dat");
            categories_   = std::make_unique<CategoryRepository>   (dataDir + "/categories.dat");
            budgets_      = std::make_unique<BudgetRepository>     (dataDir + "/budgets.dat");
            customers_    = std::make_unique<CustomerRepository>   (dataDir + "/customers.dat");
            suppliers_    = std::make_unique<SupplierRepository>   (dataDir + "/suppliers.dat");
            products_     = std::make_unique<ProductRepository>    (dataDir + "/products.dat");
            invoices_     = std::make_unique<InvoiceRepository>    (dataDir + "/invoices.dat");
            payments_     = std::make_unique<PaymentRepository>    (dataDir + "/payments.dat");
            return true;
        } catch (const std::exception& e) {
            initError_ = e.what();
            lockFile_.reset();
            return false;
        }
    }

    TransactionRepository& transactions()
    {
        if (!transactions_) throw std::logic_error("StorageService not initialized");
        return *transactions_;
    }
    AccountRepository& accounts()
    {
        if (!accounts_) throw std::logic_error("StorageService not initialized");
        return *accounts_;
    }
    CategoryRepository& categories()
    {
        if (!categories_) throw std::logic_error("StorageService not initialized");
        return *categories_;
    }
    BudgetRepository& budgets()
    {
        if (!budgets_) throw std::logic_error("StorageService not initialized");
        return *budgets_;
    }
    CustomerRepository& customers()
    {
        if (!customers_) throw std::logic_error("StorageService not initialized");
        return *customers_;
    }
    SupplierRepository& suppliers()
    {
        if (!suppliers_) throw std::logic_error("StorageService not initialized");
        return *suppliers_;
    }
    ProductRepository& products()
    {
        if (!products_) throw std::logic_error("StorageService not initialized");
        return *products_;
    }
    InvoiceRepository& invoices()
    {
        if (!invoices_) throw std::logic_error("StorageService not initialized");
        return *invoices_;
    }
    PaymentRepository& payments()
    {
        if (!payments_) throw std::logic_error("StorageService not initialized");
        return *payments_;
    }

    bool isInitialized() const { return customers_ != nullptr; }

    // Non-copyable, non-movable singleton.
    StorageService(const StorageService&)            = delete;
    StorageService& operator=(const StorageService&) = delete;
};

#endif
