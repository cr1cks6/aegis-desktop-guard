#include "AvDatabase.h"

#include <array>

namespace {

std::uint64_t MakePrefix(const std::array<std::uint8_t, 8>& bytes)
{
    std::uint64_t value = 0;

    for (std::uint8_t byte : bytes) {
        value <<= 8;
        value |= byte;
    }

    return value;
}

std::vector<std::uint8_t> DemoHash(std::uint64_t prefix, std::uint32_t length)
{
    std::vector<std::uint8_t> hash;

    for (int i = 0; i < 8; ++i) {
        hash.push_back(static_cast<std::uint8_t>((prefix >> (i * 8)) & 0xFF));
    }

    hash.push_back(static_cast<std::uint8_t>(length & 0xFF));
    hash.push_back(static_cast<std::uint8_t>((length >> 8) & 0xFF));

    return hash;
}

} // namespace

void AvDatabase::LoadDemoDatabase()
{
    std::lock_guard lock(mutex_);

    records_.clear();

    const std::array<std::uint8_t, 8> peBytes{
        'A', 'E', 'G', 'I', 'S', 'P', 'E', '!'
    };

    const std::uint64_t pePrefix = MakePrefix(peBytes);

    AvSignatureRecord peRecord{};
    peRecord.objectSignaturePrefix = pePrefix;
    peRecord.objectSignatureLength = 8;
    peRecord.objectSignatureHash = DemoHash(pePrefix, peRecord.objectSignatureLength);
    peRecord.offsetBegin = 0;
    peRecord.offsetEnd = 4096;
    peRecord.objectType = AvObjectType::PeFile;
    peRecord.avRecordSignature = {0xAA, 0xBB, 0xCC};

    records_[pePrefix].push_back(peRecord);

    const std::array<std::uint8_t, 8> psBytes{
        'A', 'E', 'G', 'I', 'S', 'P', 'S', '!'
    };

    const std::uint64_t psPrefix = MakePrefix(psBytes);

    AvSignatureRecord psRecord{};
    psRecord.objectSignaturePrefix = psPrefix;
    psRecord.objectSignatureLength = 8;
    psRecord.objectSignatureHash = DemoHash(psPrefix, psRecord.objectSignatureLength);
    psRecord.offsetBegin = 0;
    psRecord.offsetEnd = 8192;
    psRecord.objectType = AvObjectType::PowerShellScript;
    psRecord.avRecordSignature = {0xDD, 0xEE, 0xFF};

    records_[psPrefix].push_back(psRecord);

    loaded_ = true;
    releaseDate_ = L"2026-05-23";
    recordCount_ = 2;
}

bool AvDatabase::IsLoaded() const
{
    std::lock_guard lock(mutex_);
    return loaded_;
}

AvDatabaseInfo AvDatabase::GetInfo() const
{
    std::lock_guard lock(mutex_);

    AvDatabaseInfo info{};
    info.loaded = loaded_;
    info.releaseDate = releaseDate_;
    info.recordCount = recordCount_;

    return info;
}

std::vector<AvSignatureRecord> AvDatabase::FindByPrefix(std::uint64_t prefix) const
{
    std::lock_guard lock(mutex_);

    const auto iterator = records_.find(prefix);

    if (iterator == records_.end()) {
        return {};
    }

    return iterator->second;
}