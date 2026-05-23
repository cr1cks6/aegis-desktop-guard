#include "Constants.h"
#include "WinError.h"

#include "AuthManager.h"
#include "LicenseManager.h"
#include "FeatureGate.h"

#include "AvDatabase.h"
#include "ScanManager.h"

#include <windows.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <rpc.h>
#include <winerror.h>

#include "AegisRpc.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Userenv.lib")
#pragma comment(lib, "Rpcrt4.lib")

namespace {

SERVICE_STATUS g_serviceStatus{};
SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
HANDLE g_stopEvent = nullptr;

std::vector<PROCESS_INFORMATION> g_guiProcesses;

aegis::AuthManager g_authManager;
LicenseManager g_licenseManager;
FeatureGate g_featureGate(g_authManager, g_licenseManager);

AvDatabase g_avDatabase;
ScanManager g_scanManager(g_avDatabase, g_licenseManager);

void SetServiceState(DWORD state)
{
    g_serviceStatus.dwCurrentState = state;
    SetServiceStatus(g_statusHandle, &g_serviceStatus);
}

std::filesystem::path GetServiceDirectory()
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

bool LaunchGuiInSession(DWORD sessionId)
{
    if (sessionId == 0) {
        return false;
    }

    HANDLE userToken = nullptr;

    if (!WTSQueryUserToken(sessionId, &userToken)) {
        return false;
    }

    HANDLE duplicatedToken = nullptr;

    if (!DuplicateTokenEx(
            userToken,
            TOKEN_ALL_ACCESS,
            nullptr,
            SecurityIdentification,
            TokenPrimary,
            &duplicatedToken)) {
        CloseHandle(userToken);
        return false;
    }

    void* environment = nullptr;
    CreateEnvironmentBlock(&environment, duplicatedToken, FALSE);

    const auto guiPath = GetServiceDirectory() / aegis::kGuiExecutableName;
    std::wstring commandLine = L"\"" + guiPath.wstring() + L"\" --background";

    const std::wstring workingDirectory = GetServiceDirectory().wstring();

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.lpDesktop = const_cast<LPWSTR>(L"winsta0\\default");

    PROCESS_INFORMATION processInfo{};

    const BOOL created = CreateProcessAsUserW(
        duplicatedToken,
        nullptr,
        commandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_UNICODE_ENVIRONMENT,
        environment,
        workingDirectory.c_str(),
        &startupInfo,
        &processInfo
    );

    if (environment != nullptr) {
        DestroyEnvironmentBlock(environment);
    }

    CloseHandle(duplicatedToken);
    CloseHandle(userToken);

    if (!created) {
        return false;
    }

    g_guiProcesses.push_back(processInfo);
    return true;
}

void LaunchGuiForAllSessions()
{
    WTS_SESSION_INFOW* sessions = nullptr;
    DWORD sessionCount = 0;

    if (!WTSEnumerateSessionsW(
            WTS_CURRENT_SERVER_HANDLE,
            0,
            1,
            &sessions,
            &sessionCount)) {
        return;
    }

    for (DWORD i = 0; i < sessionCount; ++i) {
        if (sessions[i].SessionId != 0 &&
            sessions[i].State == WTSActive) {
            LaunchGuiInSession(sessions[i].SessionId);
        }
    }

    WTSFreeMemory(sessions);
}

void StopAllGuiProcesses()
{
    for (auto& process : g_guiProcesses) {
        if (process.hProcess != nullptr) {
            TerminateProcess(process.hProcess, 0);
            CloseHandle(process.hProcess);
        }

        if (process.hThread != nullptr) {
            CloseHandle(process.hThread);
        }
    }

    g_guiProcesses.clear();
}

DWORD WINAPI RpcServerThread(LPVOID)
{
    RPC_STATUS status = RpcServerUseProtseqEpW(
        reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(aegis::kRpcProtocolSequence)),
        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
        reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(aegis::kRpcEndpoint)),
        nullptr
    );

    if (status != 0) {
        SetEvent(g_stopEvent);
        return status;
    }

    status = RpcServerRegisterIf2(
        AegisRpc_v1_0_s_ifspec,
        nullptr,
        nullptr,
        RPC_IF_ALLOW_LOCAL_ONLY,
        RPC_C_LISTEN_MAX_CALLS_DEFAULT,
        static_cast<unsigned int>(-1),
        nullptr
    );

    if (status != 0) {
        SetEvent(g_stopEvent);
        return status;
    }

    status = RpcServerListen(
        1,
        RPC_C_LISTEN_MAX_CALLS_DEFAULT,
        FALSE
    );

    return status;
}

void WINAPI ServiceControlHandler(DWORD controlCode)
{
    if (controlCode == SERVICE_CONTROL_SESSIONCHANGE) {
        LaunchGuiForAllSessions();
    }
}

void WINAPI ServiceMain(DWORD, LPWSTR*)
{
    g_statusHandle = RegisterServiceCtrlHandlerW(
        aegis::kServiceName,
        ServiceControlHandler
    );

    if (g_statusHandle == nullptr) {
        return;
    }

    g_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_SESSIONCHANGE;
    g_serviceStatus.dwWin32ExitCode = 0;
    g_serviceStatus.dwCheckPoint = 0;
    g_serviceStatus.dwWaitHint = 0;

    SetServiceState(SERVICE_START_PENDING);

    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    if (g_stopEvent == nullptr) {
        SetServiceState(SERVICE_STOPPED);
        return;
    }

    LaunchGuiForAllSessions();

    HANDLE rpcThread = CreateThread(
        nullptr,
        0,
        RpcServerThread,
        nullptr,
        0,
        nullptr
    );

    if (rpcThread == nullptr) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
        SetServiceState(SERVICE_STOPPED);
        return;
    }

    SetServiceState(SERVICE_RUNNING);

    WaitForSingleObject(g_stopEvent, INFINITE);

    SetServiceState(SERVICE_STOP_PENDING);

    RpcMgmtStopServerListening(nullptr);
    RpcServerUnregisterIf(nullptr, nullptr, FALSE);

    StopAllGuiProcesses();

    WaitForSingleObject(rpcThread, 3000);
    CloseHandle(rpcThread);

    CloseHandle(g_stopEvent);
    g_stopEvent = nullptr;

    SetServiceState(SERVICE_STOPPED);
}

} // namespace

extern "C" void AegisStopService()
{
    if (g_stopEvent != nullptr) {
        SetEvent(g_stopEvent);
    }
}

extern "C" int AegisGetCurrentUser(int* authenticated, wchar_t** username)
{
    if (authenticated == nullptr || username == nullptr) {
        return 1;
    }

    *authenticated = g_authManager.IsAuthenticated() ? 1 : 0;

    const std::wstring currentUsername = g_authManager.GetUsername();

    const size_t size =
        (currentUsername.size() + 1) * sizeof(wchar_t);

    *username =
        static_cast<wchar_t*>(midl_user_allocate(size));

    if (*username == nullptr) {
        return 2;
    }

    wcscpy_s(
        *username,
        currentUsername.size() + 1,
        currentUsername.c_str()
    );

    return 0;
}

extern "C" int AegisLogin(
    const wchar_t* username,
    const wchar_t* password)
{
    if (username == nullptr || password == nullptr) {
        return 1;
    }

    return g_authManager.Login(username, password)
        ? 0
        : 2;
}

extern "C" int AegisLogout()
{
    g_authManager.Logout();
    g_licenseManager.Clear();

    return 0;
}

extern "C" int AegisGetLicenseInfo(
    int* active,
    wchar_t** expiresAt)
{
    if (active == nullptr || expiresAt == nullptr) {
        return 1;
    }

    *active = g_licenseManager.HasLicense() ? 1 : 0;

    const std::wstring expiration =
        g_licenseManager.GetExpiresAt();

    const size_t size =
        (expiration.size() + 1) * sizeof(wchar_t);

    *expiresAt =
        static_cast<wchar_t*>(midl_user_allocate(size));

    if (*expiresAt == nullptr) {
        return 2;
    }

    wcscpy_s(
        *expiresAt,
        expiration.size() + 1,
        expiration.c_str()
    );

    return 0;
}

extern "C" int AegisActivateProduct(
    const wchar_t* activationCode)
{
    if (activationCode == nullptr) {
        return 1;
    }

    if (!g_authManager.IsAuthenticated()) {
        return 3;
    }

    if (!g_licenseManager.Activate(activationCode)) {
        return 2;
    }

    g_scanManager.LoadDatabaseAfterActivation();

    return 0;
}

extern "C" int AegisIsAntivirusAvailable(int* available)
{
    if (available == nullptr) {
        return 1;
    }

    *available =
        g_featureGate.IsAntivirusAvailable()
        ? 1
        : 0;

    return 0;
}

extern "C" int AegisGetAvDatabaseInfo(
    int* loaded,
    unsigned long* recordCount,
    wchar_t** releaseDate)
{
    if (loaded == nullptr ||
        recordCount == nullptr ||
        releaseDate == nullptr) {
        return 1;
    }

    const AvDatabaseInfo info =
        g_scanManager.GetDatabaseInfo();

    *loaded = info.loaded ? 1 : 0;
    *recordCount = info.recordCount;

    const size_t size =
        (info.releaseDate.size() + 1) * sizeof(wchar_t);

    *releaseDate =
        static_cast<wchar_t*>(midl_user_allocate(size));

    if (*releaseDate == nullptr) {
        return 2;
    }

    wcscpy_s(
        *releaseDate,
        info.releaseDate.size() + 1,
        info.releaseDate.c_str()
    );

    return 0;
}

extern "C" int AegisScanFile(
    const wchar_t* filePath,
    int* status,
    wchar_t** threatName,
    unsigned long* scannedObjects,
    unsigned long* infectedObjects)
{
    if (filePath == nullptr ||
        status == nullptr ||
        threatName == nullptr ||
        scannedObjects == nullptr ||
        infectedObjects == nullptr) {
        return 1;
    }

    const ScanResult result =
        g_scanManager.ScanFile(filePath);

    *status =
        static_cast<int>(result.status);

    *scannedObjects =
        result.scannedObjects;

    *infectedObjects =
        result.infectedObjects;

    const size_t size =
        (result.threatName.size() + 1) * sizeof(wchar_t);

    *threatName =
        static_cast<wchar_t*>(midl_user_allocate(size));

    if (*threatName == nullptr) {
        return 2;
    }

    wcscpy_s(
        *threatName,
        result.threatName.size() + 1,
        result.threatName.c_str()
    );

    return 0;
}

extern "C" int AegisScanDirectory(
    const wchar_t* directoryPath,
    int* status,
    wchar_t** threatName,
    unsigned long* scannedObjects,
    unsigned long* infectedObjects)
{
    if (directoryPath == nullptr ||
        status == nullptr ||
        threatName == nullptr ||
        scannedObjects == nullptr ||
        infectedObjects == nullptr) {
        return 1;
    }

    const ScanResult result =
        g_scanManager.ScanDirectory(directoryPath);

    *status =
        static_cast<int>(result.status);

    *scannedObjects =
        result.scannedObjects;

    *infectedObjects =
        result.infectedObjects;

    const size_t size =
        (result.threatName.size() + 1) * sizeof(wchar_t);

    *threatName =
        static_cast<wchar_t*>(midl_user_allocate(size));

    if (*threatName == nullptr) {
        return 2;
    }

    wcscpy_s(
        *threatName,
        result.threatName.size() + 1,
        result.threatName.c_str()
    );

    return 0;
}

extern "C" void* __RPC_USER midl_user_allocate(size_t size)
{
    return std::malloc(size);
}

extern "C" void __RPC_USER midl_user_free(void* pointer)
{
    std::free(pointer);
}

int wmain()
{
    SERVICE_TABLE_ENTRYW serviceTable[] = {
        {
            const_cast<LPWSTR>(aegis::kServiceName),
            ServiceMain
        },
        {
            nullptr,
            nullptr
        }
    };

    StartServiceCtrlDispatcherW(serviceTable);

    return 0;
}