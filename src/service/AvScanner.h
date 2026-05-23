#pragma once

#include "AvDatabase.h"
#include "AvTypes.h"

#include <filesystem>
#include <vector>

class AvScanner final {
public:
    explicit AvScanner(AvDatabase& database);

    ScanResult ScanFile(const std::filesystem::path& filePath);

private:
    static AvObjectType DetectObjectType(const std::filesystem::path& filePath);
    static std::uint64_t ReadPrefix(const std::vector<std::uint8_t>& data, std::size_t offset);
    static std::vector<std::uint8_t> CalculateDemoHash(std::uint64_t prefix, std::uint32_t length);

private:
    AvDatabase& database_;
};