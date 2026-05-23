#pragma once

#include "AuthManager.h"
#include "LicenseManager.h"

class FeatureGate
{
public:
    FeatureGate(
        aegis::AuthManager& authManager,
        LicenseManager& licenseManager
    );

    bool IsAntivirusAvailable() const;

private:
    aegis::AuthManager& authManager_;
    LicenseManager& licenseManager_;
};