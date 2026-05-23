#pragma once

#include "AvDatabase.h"
#include "AvTypes.h"

#include <filesystem>

class AvDatabaseStorage
{
public:
    explicit AvDatabaseStorage(AvDatabase& database);

    bool Load();

    bool Save();

    bool RestoreBackup();

    bool CreateBackup();

    bool CreateDefaultDatabase();

private:
    bool LoadFromFile(const std::filesystem::path& path);

    bool SaveToFile(const std::filesystem::path& path);

    bool VerifyManifestSignature(
        std::uint32_t version,
        std::uint32_t recordCount
    ) const;

    bool VerifyRecordSignature(const AvSignatureRecord& record) const;

    std::filesystem::path GetDatabasePath() const;

    std::filesystem::path GetBackupPath() const;

    std::filesystem::path GetExecutableDirectory() const;

private:
    AvDatabase& database_;
};