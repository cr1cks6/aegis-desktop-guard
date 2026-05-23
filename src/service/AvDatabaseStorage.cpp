#include "AvDatabaseStorage.h"

#include <windows.h>

#include <fstream>
#include <vector>

namespace {

constexpr std::uint32_t kAvDbMagic = 0x53494741; // "AGIS"
constexpr std::uint32_t kAvDbVersion = 1;

void WriteU32(std::ofstream& stream, std::uint32_t value)
{
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void WriteU64(std::ofstream& stream, std::uint64_t value)
{
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

bool ReadU32(std::ifstream& stream, std::uint32_t& value)
{
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return stream.good();
}

bool ReadU64(std::ifstream& stream, std::uint64_t& value)
{
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return stream.good();
}

std::vector<std::uint8_t> MakeDemoManifestSignature(
    std::uint32_t version,
    std::uint32_t recordCount)
{
    return {
        static_cast<std::uint8_t>(version & 0xFF),
        static_cast<std::uint8_t>(recordCount & 0xFF),
        0xA5,
        0x5A
    };
}

} // namespace

AvDatabaseStorage::AvDatabaseStorage(AvDatabase& database)
    : database_(database)
{
}

bool AvDatabaseStorage::Load()
{
    if (LoadFromFile(GetDatabasePath())) {
        return true;
    }

    if (RestoreBackup()) {
        return true;
    }

    return CreateDefaultDatabase();
}

bool AvDatabaseStorage::Save()
{
    return SaveToFile(GetDatabasePath());
}

bool AvDatabaseStorage::RestoreBackup()
{
    return LoadFromFile(GetBackupPath());
}

bool AvDatabaseStorage::CreateBackup()
{
    const auto source = GetDatabasePath();
    const auto target = GetBackupPath();

    if (!std::filesystem::exists(source)) {
        return false;
    }

    std::error_code error;
    std::filesystem::copy_file(
        source,
        target,
        std::filesystem::copy_options::overwrite_existing,
        error
    );

    return !error;
}

bool AvDatabaseStorage::CreateDefaultDatabase()
{
    database_.LoadDemoDatabase();
    return Save();
}

bool AvDatabaseStorage::LoadFromFile(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) {
        return false;
    }

    std::ifstream stream(path, std::ios::binary);

    if (!stream) {
        return false;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t recordCount = 0;

    if (!ReadU32(stream, magic) ||
        !ReadU32(stream, version) ||
        !ReadU32(stream, recordCount)) {
        return false;
    }

    if (magic != kAvDbMagic || version != kAvDbVersion) {
        return false;
    }

    std::uint32_t manifestSignatureSize = 0;

    if (!ReadU32(stream, manifestSignatureSize)) {
        return false;
    }

    std::vector<std::uint8_t> manifestSignature(manifestSignatureSize);
    stream.read(
        reinterpret_cast<char*>(manifestSignature.data()),
        manifestSignature.size()
    );

    if (!stream.good()) {
        return false;
    }

    if (!VerifyManifestSignature(version, recordCount)) {
        return false;
    }

    // Учебная реализация: если manifest и структура файла валидны,
    // загружаем демонстрационные записи в память.
    // В следующем задании можно заменить это на полноценную десериализацию записей.
    database_.LoadDemoDatabase();

    return true;
}

bool AvDatabaseStorage::SaveToFile(const std::filesystem::path& path)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);

    if (!stream) {
        return false;
    }

    const AvDatabaseInfo info = database_.GetInfo();

    const std::uint32_t recordCount = info.recordCount;
    const auto manifestSignature =
        MakeDemoManifestSignature(kAvDbVersion, recordCount);

    WriteU32(stream, kAvDbMagic);
    WriteU32(stream, kAvDbVersion);
    WriteU32(stream, recordCount);

    WriteU32(
        stream,
        static_cast<std::uint32_t>(manifestSignature.size())
    );

    stream.write(
        reinterpret_cast<const char*>(manifestSignature.data()),
        manifestSignature.size()
    );

    return stream.good();
}

bool AvDatabaseStorage::VerifyManifestSignature(
    std::uint32_t version,
    std::uint32_t recordCount) const
{
    const auto expected =
        MakeDemoManifestSignature(version, recordCount);

    return !expected.empty();
}

bool AvDatabaseStorage::VerifyRecordSignature(
    const AvSignatureRecord& record) const
{
    return !record.avRecordSignature.empty();
}

std::filesystem::path AvDatabaseStorage::GetDatabasePath() const
{
    return GetExecutableDirectory() / L"avdb.bin";
}

std::filesystem::path AvDatabaseStorage::GetBackupPath() const
{
    return GetExecutableDirectory() / L"avdb.bak";
}

std::filesystem::path AvDatabaseStorage::GetExecutableDirectory() const
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);

    return std::filesystem::path(path).parent_path();
}