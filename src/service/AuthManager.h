#pragma once

#include "AuthState.h"

#include <mutex>
#include <string>

namespace aegis
{

class AuthManager
{
public:
    bool Login(
        const std::wstring& username,
        const std::wstring& password
    );

    void Logout();

    bool IsAuthenticated() const;

    std::wstring GetUsername() const;

private:
    mutable std::mutex mutex_;
    AuthState state_;
};

}