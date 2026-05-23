#pragma once

#include <windows.h>

namespace aegis {

inline constexpr wchar_t kServiceName[] = L"AegisDesktopGuardService";
inline constexpr wchar_t kServiceDisplayName[] = L"Aegis Desktop Guard Service";

inline constexpr wchar_t kGuiExecutableName[] = L"AegisDesktopGuard.exe";

inline constexpr wchar_t kRpcProtocolSequence[] = L"ncalrpc";
inline constexpr wchar_t kRpcEndpoint[] = L"AegisDesktopGuardRpcEndpoint";

inline constexpr wchar_t kGuiMutexName[] = L"Local\\AegisDesktopGuardSingleInstance";

inline constexpr DWORD kServiceStartTimeoutMs = 30000;
inline constexpr DWORD kServicePollIntervalMs = 500;

} // namespace aegis