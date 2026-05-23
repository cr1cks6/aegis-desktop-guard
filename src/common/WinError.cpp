#include "WinError.h"

namespace aegis {

std::wstring FormatWindowsError(DWORD errorCode)
{
    wchar_t* messageBuffer = nullptr;

    const DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&messageBuffer),
        0,
        nullptr
    );

    if (size == 0 || messageBuffer == nullptr) {
        return L"Unknown Windows error";
    }

    std::wstring message(messageBuffer, size);
    LocalFree(messageBuffer);

    return message;
}

std::wstring GetLastWindowsError()
{
    return FormatWindowsError(GetLastError());
}

} // namespace aegis