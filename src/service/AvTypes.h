#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class AvObjectType : std::uint32_t {
    Unknown = 0,
    PeFile = 1,
    PowerShellScript = 2
};

enum class ScanStatus : std::uint32_t {
    Clean = 0,
    Infected = 1,
    Error = 2,
    DatabaseNotLoaded = 3,
    LicenseRequired = 4
};

struct AvSignatureRecord final {
    std::uint64_t objectSignaturePrefix = 0;
    std::uint32_t objectSignatureLength = 0;

    std::vector<std::uint8_t> objectSignatureHash;

    std::uint64_t offsetBegin = 0;
    std::uint64_t offsetEnd = 0;

    AvObjectType objectType = AvObjectType::Unknown;

    std::vector<std::uint8_t> avRecordSignature;
};

struct AvDatabaseInfo final {
    bool loaded = false;
    std::wstring releaseDate;
    std::uint32_t recordCount = 0;
};

struct ScanResult final {
    ScanStatus status = ScanStatus::Error;
    std::wstring scannedPath;
    std::wstring threatName;
    std::uint32_t scannedObjects = 0;
    std::uint32_t infectedObjects = 0;
};