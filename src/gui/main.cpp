#include "Constants.h"
#include "AegisRpc.h"

#include <windows.h>
#include <winerror.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <winsvc.h>
#include <rpc.h>

#include <cstdlib>
#include <string>

namespace {

constexpr wchar_t kWindowClassName[] = L"AegisDesktopGuardWindowClass";
constexpr UINT kTrayIconId = 1001;
constexpr UINT kTrayMessage = WM_APP + 1;

constexpr UINT kMenuOpen = 2001;
constexpr UINT kMenuExit = 2002;
constexpr UINT kMainMenuExit = 3001;

constexpr UINT kButtonLogin = 4001;
constexpr UINT kButtonLogout = 4002;
constexpr UINT kButtonActivate = 4003;
constexpr UINT kButtonRefresh = 4004;
constexpr UINT kLicensePollTimer = 5001;

HINSTANCE g_instance = nullptr;
HWND g_mainWindow = nullptr;
UINT g_taskbarCreatedMessage = 0;
NOTIFYICONDATAW g_trayIcon{};

HWND g_userStatusLabel = nullptr;
HWND g_licenseStatusLabel = nullptr;
HWND g_featureStatusLabel = nullptr;
HWND g_usernameEdit = nullptr;
HWND g_passwordEdit = nullptr;
HWND g_activationEdit = nullptr;

bool BindRpc()
{
    RPC_WSTR stringBinding = nullptr;

    RPC_STATUS status = RpcStringBindingComposeW(
        nullptr,
        reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(aegis::kRpcProtocolSequence)),
        nullptr,
        reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(aegis::kRpcEndpoint)),
        nullptr,
        &stringBinding
    );

    if (status != 0) {
        return false;
    }

    status = RpcBindingFromStringBindingW(stringBinding, &AegisRpcBinding);
    RpcStringFreeW(&stringBinding);

    return status == 0;
}

void UnbindRpc()
{
    if (AegisRpcBinding != nullptr) {
        RpcBindingFree(&AegisRpcBinding);
    }
}

std::wstring GetWindowTextString(HWND window)
{
    const int length = GetWindowTextLengthW(window);

    if (length <= 0) {
        return {};
    }

    std::wstring value(static_cast<size_t>(length), L'\0');
    GetWindowTextW(window, value.data(), length + 1);

    return value;
}

bool RpcGetCurrentUser(bool& authenticated, std::wstring& username)
{
    authenticated = false;
    username.clear();

    if (!BindRpc()) {
        return false;
    }

    int rpcAuthenticated = 0;
    wchar_t* rpcUsername = nullptr;
    int result = 1;

    RpcTryExcept
    {
        result = AegisGetCurrentUser(&rpcAuthenticated, &rpcUsername);
    }
    RpcExcept(1)
    {
        result = 1;
    }
    RpcEndExcept

    if (result == 0) {
        authenticated = rpcAuthenticated != 0;

        if (rpcUsername != nullptr) {
            username = rpcUsername;
            midl_user_free(rpcUsername);
        }
    }

    UnbindRpc();
    return result == 0;
}

bool RpcLogin(const std::wstring& username, const std::wstring& password)
{
    if (!BindRpc()) {
        return false;
    }

    int result = 1;

    RpcTryExcept
    {
        result = AegisLogin(username.c_str(), password.c_str());
    }
    RpcExcept(1)
    {
        result = 1;
    }
    RpcEndExcept

    UnbindRpc();
    return result == 0;
}

bool RpcLogout()
{
    if (!BindRpc()) {
        return false;
    }

    int result = 1;

    RpcTryExcept
    {
        result = AegisLogout();
    }
    RpcExcept(1)
    {
        result = 1;
    }
    RpcEndExcept

    UnbindRpc();
    return result == 0;
}

bool RpcGetLicense(bool& active, std::wstring& expiresAt)
{
    active = false;
    expiresAt.clear();

    if (!BindRpc()) {
        return false;
    }

    int rpcActive = 0;
    wchar_t* rpcExpiresAt = nullptr;
    int result = 1;

    RpcTryExcept
    {
        result = AegisGetLicenseInfo(&rpcActive, &rpcExpiresAt);
    }
    RpcExcept(1)
    {
        result = 1;
    }
    RpcEndExcept

    if (result == 0) {
        active = rpcActive != 0;

        if (rpcExpiresAt != nullptr) {
            expiresAt = rpcExpiresAt;
            midl_user_free(rpcExpiresAt);
        }
    }

    UnbindRpc();
    return result == 0;
}

bool RpcActivate(const std::wstring& code)
{
    if (!BindRpc()) {
        return false;
    }

    int result = 1;

    RpcTryExcept
    {
        result = AegisActivateProduct(code.c_str());
    }
    RpcExcept(1)
    {
        result = 1;
    }
    RpcEndExcept

    UnbindRpc();
    return result == 0;
}

bool RpcIsAntivirusAvailable(bool& available)
{
    available = false;

    if (!BindRpc()) {
        return false;
    }

    int rpcAvailable = 0;
    int result = 1;

    RpcTryExcept
    {
        result = AegisIsAntivirusAvailable(&rpcAvailable);
    }
    RpcExcept(1)
    {
        result = 1;
    }
    RpcEndExcept

    if (result == 0) {
        available = rpcAvailable != 0;
    }

    UnbindRpc();
    return result == 0;
}

void RefreshMainScreen()
{
    bool authenticated = false;
    std::wstring username;

    if (RpcGetCurrentUser(authenticated, username) && authenticated) {
        SetWindowTextW(
            g_userStatusLabel,
            (L"Пользователь: " + username).c_str()
        );
    } else {
        SetWindowTextW(g_userStatusLabel, L"Пользователь не вошёл в аккаунт");
    }

    bool licenseActive = false;
    std::wstring expiresAt;

    if (RpcGetLicense(licenseActive, expiresAt) && licenseActive) {
        SetWindowTextW(
            g_licenseStatusLabel,
            (L"Лицензия активна до: " + expiresAt).c_str()
        );
    } else {
        SetWindowTextW(g_licenseStatusLabel, L"Лицензия отсутствует");
    }

    bool available = false;

    if (RpcIsAntivirusAvailable(available) && available) {
        SetWindowTextW(g_featureStatusLabel, L"Антивирусная функциональность: доступна");
    } else {
        SetWindowTextW(g_featureStatusLabel, L"Антивирусная функциональность: заблокирована");
    }
}

void ShowMainWindow()
{
    RefreshMainScreen();
    ShowWindow(g_mainWindow, SW_SHOW);
    SetForegroundWindow(g_mainWindow);
}

void RequestServiceStop()
{
    if (!BindRpc()) {
        return;
    }

    RpcTryExcept
    {
        AegisStopService();
    }
    RpcExcept(1)
    {
    }
    RpcEndExcept

    UnbindRpc();
}

void ExitApplication()
{
    RequestServiceStop();
    Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
    PostQuitMessage(0);
}

HICON CreateTrayIcon()
{
    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, 32, 32);

    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memoryDc, bitmap));

    HBRUSH backgroundBrush = CreateSolidBrush(RGB(30, 100, 190));
    RECT backgroundRect{0, 0, 32, 32};
    FillRect(memoryDc, &backgroundRect, backgroundBrush);

    HPEN whitePen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
    HPEN oldPen = static_cast<HPEN>(SelectObject(memoryDc, whitePen));

    MoveToEx(memoryDc, 8, 17, nullptr);
    LineTo(memoryDc, 14, 23);
    LineTo(memoryDc, 25, 8);

    SelectObject(memoryDc, oldPen);
    SelectObject(memoryDc, oldBitmap);

    DeleteObject(whitePen);
    DeleteObject(backgroundBrush);

    ICONINFO iconInfo{};
    iconInfo.fIcon = TRUE;
    iconInfo.hbmColor = bitmap;
    iconInfo.hbmMask = bitmap;

    HICON icon = CreateIconIndirect(&iconInfo);

    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);

    return icon;
}

void AddTrayIcon(HWND window)
{
    ZeroMemory(&g_trayIcon, sizeof(g_trayIcon));

    g_trayIcon.cbSize = sizeof(g_trayIcon);
    g_trayIcon.hWnd = window;
    g_trayIcon.uID = kTrayIconId;
    g_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_trayIcon.uCallbackMessage = kTrayMessage;
    g_trayIcon.hIcon = CreateTrayIcon();

    wcscpy_s(g_trayIcon.szTip, L"Aegis Desktop Guard");

    Shell_NotifyIconW(NIM_ADD, &g_trayIcon);
}

void ShowTrayMenu(HWND window)
{
    HMENU menu = CreatePopupMenu();

    AppendMenuW(menu, MF_STRING, kMenuOpen, L"Открыть");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"Выход");

    POINT cursorPosition{};
    GetCursorPos(&cursorPosition);

    SetForegroundWindow(window);

    TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON,
        cursorPosition.x,
        cursorPosition.y,
        0,
        window,
        nullptr
    );

    DestroyMenu(menu);
}

void CreateMainMenu(HWND window)
{
    HMENU mainMenu = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();

    AppendMenuW(fileMenu, MF_STRING, kMainMenuExit, L"Выход");
    AppendMenuW(mainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"Файл");

    SetMenu(window, mainMenu);
}

void CreateMainControls(HWND window)
{
    CreateWindowW(L"STATIC", L"Aegis Desktop Guard",
        WS_CHILD | WS_VISIBLE,
        30, 35, 350, 25,
        window, nullptr, g_instance, nullptr);

    g_userStatusLabel = CreateWindowW(L"STATIC", L"Пользователь: проверка...",
        WS_CHILD | WS_VISIBLE,
        30, 75, 500, 25,
        window, nullptr, g_instance, nullptr);

    g_licenseStatusLabel = CreateWindowW(L"STATIC", L"Лицензия: проверка...",
        WS_CHILD | WS_VISIBLE,
        30, 105, 500, 25,
        window, nullptr, g_instance, nullptr);

    g_featureStatusLabel = CreateWindowW(L"STATIC", L"Функциональность: проверка...",
        WS_CHILD | WS_VISIBLE,
        30, 135, 500, 25,
        window, nullptr, g_instance, nullptr);

    CreateWindowW(L"STATIC", L"Логин:",
        WS_CHILD | WS_VISIBLE,
        30, 190, 120, 25,
        window, nullptr, g_instance, nullptr);

    g_usernameEdit = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        150, 185, 250, 28,
        window, nullptr, g_instance, nullptr);

    CreateWindowW(L"STATIC", L"Пароль:",
        WS_CHILD | WS_VISIBLE,
        30, 225, 120, 25,
        window, nullptr, g_instance, nullptr);

    g_passwordEdit = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
        150, 220, 250, 28,
        window, nullptr, g_instance, nullptr);

    CreateWindowW(L"BUTTON", L"Войти",
        WS_CHILD | WS_VISIBLE,
        420, 185, 120, 30,
        window, reinterpret_cast<HMENU>(kButtonLogin), g_instance, nullptr);

    CreateWindowW(L"BUTTON", L"Выйти из аккаунта",
        WS_CHILD | WS_VISIBLE,
        420, 220, 160, 30,
        window, reinterpret_cast<HMENU>(kButtonLogout), g_instance, nullptr);

    CreateWindowW(L"STATIC", L"Код активации:",
        WS_CHILD | WS_VISIBLE,
        30, 285, 120, 25,
        window, nullptr, g_instance, nullptr);

    g_activationEdit = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        150, 280, 250, 28,
        window, nullptr, g_instance, nullptr);

    CreateWindowW(L"BUTTON", L"Активировать",
        WS_CHILD | WS_VISIBLE,
        420, 280, 140, 30,
        window, reinterpret_cast<HMENU>(kButtonActivate), g_instance, nullptr);

    CreateWindowW(L"BUTTON", L"Обновить статус",
        WS_CHILD | WS_VISIBLE,
        30, 340, 160, 30,
        window, reinterpret_cast<HMENU>(kButtonRefresh), g_instance, nullptr);

    CreateWindowW(L"STATIC", L"Тестовый код активации: AEGIS-2026",
        WS_CHILD | WS_VISIBLE,
        30, 390, 400, 25,
        window, nullptr, g_instance, nullptr);
}

bool StartServiceIfNeeded()
{
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);

    if (manager == nullptr) {
        return false;
    }

    SC_HANDLE service = OpenServiceW(
        manager,
        aegis::kServiceName,
        SERVICE_QUERY_STATUS | SERVICE_START
    );

    if (service == nullptr) {
        CloseServiceHandle(manager);
        return false;
    }

    SERVICE_STATUS_PROCESS status{};
    DWORD bytesNeeded = 0;

    QueryServiceStatusEx(
        service,
        SC_STATUS_PROCESS_INFO,
        reinterpret_cast<LPBYTE>(&status),
        sizeof(status),
        &bytesNeeded
    );

    if (status.dwCurrentState == SERVICE_RUNNING) {
        CloseServiceHandle(service);
        CloseServiceHandle(manager);
        return false;
    }

    StartServiceW(service, 0, nullptr);

    for (DWORD elapsed = 0; elapsed < aegis::kServiceStartTimeoutMs;
         elapsed += aegis::kServicePollIntervalMs) {
        Sleep(aegis::kServicePollIntervalMs);

        QueryServiceStatusEx(
            service,
            SC_STATUS_PROCESS_INFO,
            reinterpret_cast<LPBYTE>(&status),
            sizeof(status),
            &bytesNeeded
        );

        if (status.dwCurrentState == SERVICE_RUNNING) {
            break;
        }
    }

    CloseServiceHandle(service);
    CloseServiceHandle(manager);

    return true;
}

DWORD GetParentProcessId()
{
    DWORD currentProcessId = GetCurrentProcessId();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (!Process32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        return 0;
    }

    do {
        if (entry.th32ProcessID == currentProcessId) {
            CloseHandle(snapshot);
            return entry.th32ParentProcessID;
        }
    } while (Process32NextW(snapshot, &entry));

    CloseHandle(snapshot);
    return 0;
}

std::wstring GetProcessName(DWORD processId)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (!Process32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        return {};
    }

    do {
        if (entry.th32ProcessID == processId) {
            std::wstring name = entry.szExeFile;
            CloseHandle(snapshot);
            return name;
        }
    } while (Process32NextW(snapshot, &entry));

    CloseHandle(snapshot);
    return {};
}

bool IsStartedByService()
{
    const DWORD parentId = GetParentProcessId();

    if (parentId == 0) {
        return false;
    }

    const std::wstring parentName = GetProcessName(parentId);

    return parentName == L"AegisDesktopGuardService.exe";
}

void HandleLogin()
{
    const std::wstring username = GetWindowTextString(g_usernameEdit);
    const std::wstring password = GetWindowTextString(g_passwordEdit);

    if (RpcLogin(username, password)) {
        MessageBoxW(g_mainWindow, L"Вход выполнен успешно.", L"Aegis", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(g_mainWindow, L"Ошибка входа. Проверьте логин и пароль.", L"Aegis", MB_OK | MB_ICONERROR);
    }

    RefreshMainScreen();
}

void HandleLogout()
{
    RpcLogout();
    MessageBoxW(g_mainWindow, L"Выход из аккаунта выполнен.", L"Aegis", MB_OK | MB_ICONINFORMATION);
    RefreshMainScreen();
}

void HandleActivation()
{
    const std::wstring code = GetWindowTextString(g_activationEdit);

    if (RpcActivate(code)) {
        MessageBoxW(g_mainWindow, L"Продукт успешно активирован.", L"Aegis", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(g_mainWindow, L"Ошибка активации. Используй код AEGIS-2026 после входа в аккаунт.", L"Aegis", MB_OK | MB_ICONERROR);
    }

    RefreshMainScreen();
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == g_taskbarCreatedMessage) {
        AddTrayIcon(window);
        return 0;
    }

    switch (message) {
    case WM_CREATE:
        CreateMainMenu(window);
        CreateMainControls(window);
        AddTrayIcon(window);
        SetTimer(window, kLicensePollTimer, 5000, nullptr);
        RefreshMainScreen();
        return 0;

    case WM_TIMER:
        if (wParam == kLicensePollTimer) {
            RefreshMainScreen();
        }
        return 0;

    case kTrayMessage:
        if (lParam == WM_LBUTTONUP) {
            ShowMainWindow();
        } else if (lParam == WM_RBUTTONUP) {
            ShowTrayMenu(window);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kButtonLogin:
            HandleLogin();
            return 0;

        case kButtonLogout:
            HandleLogout();
            return 0;

        case kButtonActivate:
            HandleActivation();
            return 0;

        case kButtonRefresh:
            RefreshMainScreen();
            return 0;

        case kMenuOpen:
            ShowMainWindow();
            return 0;

        case kMenuExit:
        case kMainMenuExit:
            ExitApplication();
            return 0;

        default:
            return 0;
        }

    case WM_CLOSE:
        ShowWindow(window, SW_HIDE);
        return 0;

    case WM_DESTROY:
        KillTimer(window, kLicensePollTimer);
        Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

bool RegisterMainWindowClass()
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = g_instance;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_SHIELD);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    return RegisterClassExW(&windowClass) != 0;
}

bool HasArgument(LPWSTR commandLine, const std::wstring& expected)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(commandLine, &argc);

    if (argv == nullptr) {
        return false;
    }

    bool found = false;

    for (int i = 1; i < argc; ++i) {
        if (argv[i] == expected) {
            found = true;
            break;
        }
    }

    LocalFree(argv);
    return found;
}

} // namespace

extern "C" void* __RPC_USER midl_user_allocate(size_t size)
{
    return std::malloc(size);
}

extern "C" void __RPC_USER midl_user_free(void* pointer)
{
    std::free(pointer);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand)
{
    g_instance = instance;

    const bool serviceWasStarted = StartServiceIfNeeded();

    if (serviceWasStarted) {
        return 0;
    }

    if (!IsStartedByService()) {
        return 0;
    }

    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");

    if (!RegisterMainWindowClass()) {
        return 1;
    }

    g_mainWindow = CreateWindowExW(
        0,
        kWindowClassName,
        L"Aegis Desktop Guard",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        760,
        520,
        nullptr,
        nullptr,
        g_instance,
        nullptr
    );

    if (g_mainWindow == nullptr) {
        return 1;
    }

    const bool startHidden = HasArgument(commandLine, L"--background");

    if (!startHidden) {
        ShowWindow(g_mainWindow, showCommand);
        UpdateWindow(g_mainWindow);
    }

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}