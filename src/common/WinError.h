#pragma once

#include <windows.h>

#include <string>

namespace aegis {

std::wstring FormatWindowsError(DWORD errorCode);

std::wstring GetLastWindowsError();

} // namespace aegis