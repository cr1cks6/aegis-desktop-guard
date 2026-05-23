#include "LicenseManager.h"

bool LicenseManager::HasLicense() const
{
    std::lock_guard lock(mutex_);
    return active_;
}

std::wstring LicenseManager::GetExpiresAt() const
{
    std::lock_guard lock(mutex_);
    return expiresAt_;
}

bool LicenseManager::Activate(const std::wstring& activationCode)
{
    std::lock_guard lock(mutex_);

    // Учебная имитация HTTPS-активации.
    // Для проверки используем код: AEGIS-2026
    if (!(
    activationCode == L"AEGIS-2026" ||
    activationCode == L"STUDENT-2026" ||
    activationCode == L"PREMIUM-777"
)) {
    return false;
}

    active_ = true;
    expiresAt_ = L"2026-12-31";

    return true;
}

void LicenseManager::Clear()
{
    std::lock_guard lock(mutex_);
    active_ = false;
    expiresAt_.clear();
}