#include "AvScanner.h"

#include <algorithm>
#include <fstream>
#include <iterator>

AvScanner::AvScanner(AvDatabase& database)
    : database_(database)
{
}

AvObjectType AvScanner::DetectObjectType(const std::filesystem::path& filePath)
{
    std::wstring extension = filePath.extension().wstring();

    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        towlower
    );

    if (extension == L".exe" || extension == L".dll") {
        return AvObjectType::PeFile;
    }

    if (extension == L".ps1") {
        return AvObjectType::PowerShellScript;
    }

    return AvObjectType::Unknown;
}

std::uint64_t AvScanner::ReadPrefix(
    const std::vector<std::uint8_t>& data,
    std::size_t offset)
{
    std::uint64_t value = 0;

    for (std::size_t index = 0; index < 8; ++index) {
        value <<= 8;
        value |= data[offset + index];
    }

    return value;
}

std::vector<std::uint8_t> AvScanner::CalculateDemoHash(
    std::uint64_t prefix,
    std::uint32_t length)
{
    std::vector<std::uint8_t> hash;

    for (int i = 0; i < 8; ++i) {
        hash.push_back(
            static_cast<std::uint8_t>((prefix >> (i * 8)) & 0xFF)
        );
    }

    hash.push_back(static_cast<std::uint8_t>(length & 0xFF));
    hash.push_back(static_cast<std::uint8_t>((length >> 8) & 0xFF));

    return hash;
}

ScanResult AvScanner::ScanFile(const std::filesystem::path& filePath)
{
    ScanResult result{};
    result.scannedPath = filePath.wstring();
    result.scannedObjects = 1;

    if (!database_.IsLoaded()) {
        result.status = ScanStatus::DatabaseNotLoaded;
        return result;
    }

    std::ifstream file(filePath, std::ios::binary);

    if (!file) {
        result.status = ScanStatus::Error;
        return result;
    }

    std::vector<std::uint8_t> data{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };

    if (data.size() < 8) {
        result.status = ScanStatus::Clean;
        return result;
    }

    const AvObjectType objectType = DetectObjectType(filePath);

    for (std::size_t offset = 0; offset + 8 <= data.size(); ++offset) {
        const std::uint64_t prefix = ReadPrefix(data, offset);
        auto candidates = database_.FindByPrefix(prefix);

        if (candidates.empty()) {
            continue;
        }

        for (const auto& record : candidates) {
            if (record.objectType != objectType) {
                continue;
            }

            if (offset < record.offsetBegin || offset > record.offsetEnd) {
                continue;
            }

            if (record.objectSignatureLength < 8) {
                continue;
            }

            if (offset + record.objectSignatureLength > data.size()) {
                continue;
            }

            const auto calculatedHash = CalculateDemoHash(
                prefix,
                record.objectSignatureLength
            );

            if (calculatedHash != record.objectSignatureHash) {
                continue;
            }

            result.status = ScanStatus::Infected;
            result.threatName = L"Demo.Aegis.Signature";
            result.infectedObjects = 1;
            return result;
        }
    }

    result.status = ScanStatus::Clean;
    return result;
}