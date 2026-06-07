#ifndef CATEGORY_REPOSITORY_H
#define CATEGORY_REPOSITORY_H
#include "BinaryRecordFile.h"
#include "../core/Category.h"
#include <vector>
#include <cstring>

// Layer 2 — CategoryRepository.
// Non-polymorphic: one Category type, already has serialize/deserialize.
class CategoryRepository {
    BinaryRecordFile file_;

    static constexpr int CAT_DELETED_OFFSET = 38;  // unsigned char flag

public:
    explicit CategoryRepository(const std::string& path)
        : file_(path, CATEGORY_RECORD_SIZE) {}

    uint16_t save(Category& cat)
    {
        uint16_t id = static_cast<uint16_t>(file_.count());
        cat.setId(id);
        char buf[CATEGORY_RECORD_SIZE];
        cat.serialize(buf);
        return file_.append(buf);
    }

    bool update(const Category& cat)
    {
        char buf[CATEGORY_RECORD_SIZE];
        cat.serialize(buf);
        return file_.update(cat.getId(), buf);
    }

    bool remove(uint16_t id)
    {
        char buf[CATEGORY_RECORD_SIZE];
        if (!file_.read(id, buf)) return false;
        unsigned char flag = 1u;
        std::memcpy(buf + CAT_DELETED_OFFSET, &flag, sizeof(flag));
        return file_.update(id, buf);
    }

    Category load(uint16_t id)
    {
        char buf[CATEGORY_RECORD_SIZE];
        Category cat;
        if (!file_.read(id, buf)) return cat;
        cat.deserialize(buf);
        return cat;
    }

    std::vector<Category> loadAll()
    {
        std::vector<Category> result;
        char buf[CATEGORY_RECORD_SIZE];
        const std::size_t n = file_.count();
        for (std::size_t i = 0; i < n; ++i) {
            if (!file_.read(static_cast<uint16_t>(i), buf)) continue;
            unsigned char flag;
            std::memcpy(&flag, buf + CAT_DELETED_OFFSET, sizeof(flag));
            if (flag) continue;
            Category cat;
            cat.deserialize(buf);
            if (cat.isValid()) result.push_back(cat);
        }
        return result;
    }
};

#endif
