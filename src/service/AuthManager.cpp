#include "AuthManager.h"

#include <chrono>

namespace
{

std::uint64_t NowSeconds()
{
    using namespace std::chrono;

    return duration_cast<seconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

}

namespace aegis
{

bool AuthManager::Login(
    const std::wstring& username,
    const std::wstring& password
)
{
    std::lock_guard lock(mutex_);

    const bool validCredentials =
        (username == L"admin"   && password == L"admin")   ||
        (username == L"alex"    && password == L"12345")   ||
        (username == L"student" && password == L"qwerty")  ||
        (username == L"tester"  && password == L"test123");

    if (!validCredentials) {
        return false;
    }

    state_.authenticated = true;
    state_.username = username;

    state_.accessToken = L"memory_access_token";
    state_.refreshToken = L"memory_refresh_token";

    state_.accessExpiresAt = NowSeconds() + 15 * 60;
    state_.refreshExpiresAt = NowSeconds() + 60 * 60;

    return true;
}

void AuthManager::Logout()
{
    std::lock_guard lock(mutex_);
    state_ = {};
}

bool AuthManager::IsAuthenticated() const
{
    std::lock_guard lock(mutex_);
    return state_.authenticated;
}

std::wstring AuthManager::GetUsername() const
{
    std::lock_guard lock(mutex_);
    return state_.username;
}

}