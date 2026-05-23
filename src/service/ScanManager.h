#pragma once

#include "AvDatabase.h"
#include "AvScanner.h"
#include "AvTypes.h"
#include "LicenseManager.h"

#include <filesystem>

class ScanManager final {
public:
    ScanManager(
        AvDatabase& database,
        LicenseManager& licenseManager
    );

    void LoadDatabaseAfterActivation();

    AvDatabaseInfo GetDatabaseInfo() const;

    ScanResult ScanFile(const std::filesystem::path& filePath);

    ScanResult ScanDirectory(const std::filesystem::path& directoryPath);

private:
    AvDatabase& database_;
    LicenseManager& licenseManager_;
    AvScanner scanner_;
};