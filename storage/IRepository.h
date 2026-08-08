#pragma once
#include <cstdint>
#include <optional>
#include <vector>

// Abstract repository interface. Both the binary-file backend and the optional
// SQLite backend implement this contract so StorageService can swap backends
// via a compile-time flag (ACCT_USE_SQLITE).
//
// Entity-specific query methods (findByCustomer, findByInvoice, …) are declared
// in the concrete repository headers — they are not part of this interface because
// they vary by entity type and cannot be expressed generically without higher-level
// abstractions (query builders, etc.).
template<class Entity>
class IRepository {
public:
    virtual ~IRepository() = default;

    // Persist a new entity. Assigns and returns the storage ID.
    virtual uint32_t save(Entity& entity) = 0;

    // Overwrite an existing record. Returns false if id not found.
    virtual bool update(const Entity& entity) = 0;

    // Soft-delete: sets the isDeleted flag. Returns false if id not found.
    virtual bool remove(uint32_t id) = 0;

    // Load one entity by id. Returns empty optional if not found or deleted.
    virtual std::optional<Entity> find(uint32_t id) = 0;

    // Load all non-deleted entities.
    virtual std::vector<Entity> loadAll() = 0;
};
