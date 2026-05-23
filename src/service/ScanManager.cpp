#include "ScanManager.h"

ScanManager::ScanManager(
    AvDatabase& database,
    LicenseManager& licenseManager
)
    : database_(database)
    , licenseManager_(licenseManager)
    , scanner_(database)
{
}

void ScanManager::LoadDatabaseAfterActivation()
{
    if (licenseManager_.HasLicense()) {
        database_.LoadDemoDatabase();
    }
}

AvDatabaseInfo ScanManager::GetDatabaseInfo() const
{
    return database_.GetInfo();
}

ScanResult ScanManager::ScanFile(const std::filesystem::path& filePath)
{
    if (!licenseManager_.HasLicense()) {
        ScanResult result{};
        result.status = ScanStatus::LicenseRequired;
        result.scannedPath = filePath.wstring();
        return result;
    }

    return scanner_.ScanFile(filePath);
}

ScanResult ScanManager::ScanDirectory(const std::filesystem::path& directoryPath)
{
    ScanResult result{};
    result.scannedPath = directoryPath.wstring();

    if (!licenseManager_.HasLicense()) {
        result.status = ScanStatus::LicenseRequired;
        return result;
    }

    if (!std::filesystem::exists(directoryPath) ||
        !std::filesystem::is_directory(directoryPath)) {
        result.status = ScanStatus::Error;
        return result;
    }

    result.status = ScanStatus::Clean;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const ScanResult fileResult = scanner_.ScanFile(entry.path());

        ++result.scannedObjects;

        if (fileResult.status == ScanStatus::Infected) {
            result.status = ScanStatus::Infected;
            result.infectedObjects++;
            result.threatName = fileResult.threatName;
        }
    }

    return result;
}