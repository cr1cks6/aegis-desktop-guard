#pragma once

#include <mutex>
#include <string>

class LicenseManager final {
public:
    bool HasLicense() const;

    std::wstring GetExpiresAt() const;

    bool Activate(const std::wstring& activationCode);

    void Clear();

private:
    mutable std::mutex mutex_;

    bool active_ = false;
    std::wstring expiresAt_;
};