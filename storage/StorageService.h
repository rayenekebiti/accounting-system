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
#include <memory>
#include <string>
#include <stdexcept>

// Layer 3 — StorageService (facade / singleton).
// Owns all repositories; the UI never touches a repository constructor directly.
//
// Usage:
//   StorageService::instance().initialize("/path/to/data/dir");
//   StorageService::instance().customers().save(cust);
class StorageService {
    std::unique_ptr<TransactionRepository> transactions_;
    std::unique_ptr<AccountRepository>     accounts_;
    std::unique_ptr<CategoryRepository>    categories_;
    std::unique_ptr<BudgetRepository>      budgets_;
    std::unique_ptr<CustomerRepository>    customers_;
    std::unique_ptr<SupplierRepository>    suppliers_;
    std::unique_ptr<ProductRepository>     products_;
    std::unique_ptr<InvoiceRepository>     invoices_;
    std::unique_ptr<PaymentRepository>     payments_;

    StorageService() = default;

public:
    static StorageService& instance()
    {
        static StorageService inst;
        return inst;
    }

    // Opens (or creates) all data files under dataDir.
    // Returns false if any file fails to open; throws std::runtime_error on I/O errors.
    bool initialize(const std::string& dataDir)
    {
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
        } catch (const std::exception&) {
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
