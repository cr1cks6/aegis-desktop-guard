#include "FeatureGate.h"

FeatureGate::FeatureGate(
    aegis::AuthManager& authManager,
    LicenseManager& licenseManager
)
    : authManager_(authManager),
      licenseManager_(licenseManager)
{
}

bool FeatureGate::IsAntivirusAvailable() const
{
    return authManager_.IsAuthenticated() &&
           licenseManager_.HasLicense();
}