#pragma once

#include "AvTypes.h"

#include <map>
#include <mutex>
#include <vector>

class AvDatabase final {
public:
    void LoadDemoDatabase();

    bool IsLoaded() const;

    AvDatabaseInfo GetInfo() const;

    std::vector<AvSignatureRecord> FindByPrefix(std::uint64_t prefix) const;

private:
    mutable std::mutex mutex_;

    bool loaded_ = false;
    std::wstring releaseDate_;
    std::uint32_t recordCount_ = 0;

    std::map<std::uint64_t, std::vector<AvSignatureRecord>> records_;
};