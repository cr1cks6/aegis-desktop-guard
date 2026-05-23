#pragma once

#include <string>

struct AuthState final {
    bool authenticated = false;

    std::wstring username;

    // JWT храним только внутри службы, клиентам не отдаём.
    std::wstring accessToken;
    std::wstring refreshToken;

    unsigned long long accessExpiresAt = 0;
    unsigned long long refreshExpiresAt = 0;
};