#include "Constants.h"
#include "AegisRpc.h"

#include <windows.h>
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
constexpr UINT kButtonScanFile = 4005;
constexpr UINT kButtonScanDirectory = 4006;

constexpr UINT kStatusTimer = 5001;

constexpr COLORREF kBackgroundColor = RGB(18, 22, 30);
constexpr COLORREF kPanelColor = RGB(30, 36, 48);
constexpr COLORREF kEditColor = RGB(42, 48, 62);
constexpr COLORREF kTextColor = RGB(235, 238, 245);
constexpr COLORREF kMutedTextColor = RGB(170, 178, 190);
constexpr COLORREF kAccentColor = RGB(0, 120, 215);
constexpr COLORREF kSuccessColor = RGB(70, 220, 120);
constexpr COLORREF kDangerColor = RGB(255, 90, 90);

HINSTANCE g_instance = nullptr;
HWND g_mainWindow = nullptr;
UINT g_taskbarCreatedMessage = 0;
NOTIFYICONDATAW g_trayIcon{};

HWND g_userLabel = nullptr;
HWND g_licenseLabel = nullptr;
HWND g_featureLabel = nullptr;
HWND g_databaseLabel = nullptr;
HWND g_scanResultLabel = nullptr;

HWND g_usernameEdit = nullptr;
HWND g_passwordEdit = nullptr;
HWND g_activationEdit = nullptr;
HWND g_filePathEdit = nullptr;
HWND g_directoryPathEdit = nullptr;

HFONT g_titleFont = nullptr;
HFONT g_textFont = nullptr;
HFONT g_buttonFont = nullptr;

HBRUSH g_backgroundBrush = nullptr;
HBRUSH g_panelBrush = nullptr;
HBRUSH g_editBrush = nullptr;

void InitializeVisualStyle()
{
    g_backgroundBrush = CreateSolidBrush(kBackgroundColor);
    g_panelBrush = CreateSolidBrush(kPanelColor);
    g_editBrush = CreateSolidBrush(kEditColor);

    g_titleFont = CreateFontW(
        30, 0, 0, 0, FW_SEMIBOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    g_textFont = CreateFontW(
        17, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    g_buttonFont = CreateFontW(
        16, 0, 0, 0, FW_SEMIBOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );
}

void ReleaseVisualStyle()
{
    if (g_titleFont != nullptr) {
        DeleteObject(g_titleFont);
        g_titleFont = nullptr;
    }

    if (g_textFont != nullptr) {
        DeleteObject(g_textFont);
        g_textFont = nullptr;
    }

    if (g_buttonFont != nullptr) {
        DeleteObject(g_buttonFont);
        g_buttonFont = nullptr;
    }

    if (g_backgroundBrush != nullptr) {
        DeleteObject(g_backgroundBrush);
        g_backgroundBrush = nullptr;
    }

    if (g_panelBrush != nullptr) {
        DeleteObject(g_panelBrush);
        g_panelBrush = nullptr;
    }

    if (g_editBrush != nullptr) {
        DeleteObject(g_editBrush);
        g_editBrush = nullptr;
    }
}

void SetControlFont(HWND control, HFONT font)
{
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

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
        AegisRpcBinding = nullptr;
    }
}

std::wstring GetWindowTextString(HWND window)
{
    const int length = GetWindowTextLengthW(window);

    if (length <= 0) {
        return {};
    }

    std::wstring value(static_cast<std::size_t>(length), L'\0');
    GetWindowTextW(window, value.data(), length + 1);

    return value;
}

std::wstring ScanStatusToText(int status)
{
    switch (status) {
    case 0:
        return L"CLEAN";
    case 1:
        return L"INFECTED";
    case 2:
        return L"ERROR";
    case 3:
        return L"DATABASE NOT LOADED";
    case 4:
        return L"LICENSE REQUIRED";
    default:
        return L"UNKNOWN";
    }
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

bool RpcGetDatabaseInfo(bool& loaded, unsigned long& recordCount, std::wstring& releaseDate)
{
    loaded = false;
    recordCount = 0;
    releaseDate.clear();

    if (!BindRpc()) {
        return false;
    }

    int rpcLoaded = 0;
    wchar_t* rpcReleaseDate = nullptr;
    int result = 1;

    RpcTryExcept
    {
        result = AegisGetAvDatabaseInfo(&rpcLoaded, &recordCount, &rpcReleaseDate);
    }
    RpcExcept(1)
    {
        result = 1;
    }
    RpcEndExcept

    if (result == 0) {
        loaded = rpcLoaded != 0;

        if (rpcReleaseDate != nullptr) {
            releaseDate = rpcReleaseDate;
            midl_user_free(rpcReleaseDate);
        }
    }

    UnbindRpc();
    return result == 0;
}

bool RpcScanFile(
    const std::wstring& path,
    int& status,
    std::wstring& threatName,
    unsigned long& scannedObjects,
    unsigned long& infectedObjects)
{
    status = 2;
    threatName.clear();
    scannedObjects = 0;
    infectedObjects = 0;

    if (!BindRpc()) {
        return false;
    }

    wchar_t* rpcThreatName = nullptr;
    int result = 1;

    RpcTryExcept
    {
        result = AegisScanFile(
            path.c_str(),
            &status,
            &rpcThreatName,
            &scannedObjects,
            &infectedObjects
        );
    }
    RpcExcept(1)
    {
        result = 1;
    }
    RpcEndExcept

    if (result == 0 && rpcThreatName != nullptr) {
        threatName = rpcThreatName;
        midl_user_free(rpcThreatName);
    }

    UnbindRpc();
    return result == 0;
}

bool RpcScanDirectory(
    const std::wstring& path,
    int& status,
    std::wstring& threatName,
    unsigned long& scannedObjects,
    unsigned long& infectedObjects)
{
    status = 2;
    threatName.clear();
    scannedObjects = 0;
    infectedObjects = 0;

    if (!BindRpc()) {
        return false;
    }

    wchar_t* rpcThreatName = nullptr;
    int result = 1;

    RpcTryExcept
    {
        result = AegisScanDirectory(
            path.c_str(),
            &status,
            &rpcThreatName,
            &scannedObjects,
            &infectedObjects
        );
    }
    RpcExcept(1)
    {
        result = 1;
    }
    RpcEndExcept

    if (result == 0 && rpcThreatName != nullptr) {
        threatName = rpcThreatName;
        midl_user_free(rpcThreatName);
    }

    UnbindRpc();
    return result == 0;
}

void RefreshUi()
{
    bool authenticated = false;
    std::wstring username;

    RpcGetCurrentUser(authenticated, username);

    if (authenticated) {
        SetWindowTextW(g_userLabel, (L"● Пользователь: " + username).c_str());
    } else {
        SetWindowTextW(g_userLabel, L"● Пользователь не вошёл");
    }

    bool licenseActive = false;
    std::wstring expiresAt;

    RpcGetLicense(licenseActive, expiresAt);

    if (licenseActive) {
        SetWindowTextW(g_licenseLabel, (L"● Лицензия активна до: " + expiresAt).c_str());
    } else {
        SetWindowTextW(g_licenseLabel, L"● Лицензия отсутствует");
    }

    bool antivirusAvailable = false;
    RpcIsAntivirusAvailable(antivirusAvailable);

    if (antivirusAvailable) {
        SetWindowTextW(g_featureLabel, L"● Антивирусная защита: активна");
    } else {
        SetWindowTextW(g_featureLabel, L"● Антивирусная защита: заблокирована");
    }

    bool dbLoaded = false;
    unsigned long dbRecords = 0;
    std::wstring dbDate;

    RpcGetDatabaseInfo(dbLoaded, dbRecords, dbDate);

    if (dbLoaded) {
        const std::wstring text =
            L"● Базы: " +
            std::to_wstring(dbRecords) +
            L" сигнатур | дата выпуска: " +
            dbDate;

        SetWindowTextW(g_databaseLabel, text.c_str());
    } else {
        SetWindowTextW(g_databaseLabel, L"● Антивирусные базы не загружены");
    }
}

void PerformFileScan()
{
    const std::wstring path = GetWindowTextString(g_filePathEdit);

    if (path.empty()) {
        MessageBoxW(g_mainWindow, L"Введите путь к файлу.", L"Aegis Desktop Guard", MB_ICONWARNING);
        return;
    }

    int status = 0;
    std::wstring threatName;
    unsigned long scannedObjects = 0;
    unsigned long infectedObjects = 0;

    if (!RpcScanFile(path, status, threatName, scannedObjects, infectedObjects)) {
        MessageBoxW(g_mainWindow, L"Ошибка RPC-сканирования файла.", L"Aegis Desktop Guard", MB_ICONERROR);
        return;
    }

    std::wstring result =
        L"Результат файла: " +
        ScanStatusToText(status) +
        L" | проверено: " +
        std::to_wstring(scannedObjects) +
        L" | заражено: " +
        std::to_wstring(infectedObjects);

    if (!threatName.empty()) {
        result += L" | угроза: " + threatName;
    }

    SetWindowTextW(g_scanResultLabel, result.c_str());
}

void PerformDirectoryScan()
{
    const std::wstring path = GetWindowTextString(g_directoryPathEdit);

    if (path.empty()) {
        MessageBoxW(g_mainWindow, L"Введите путь к папке.", L"Aegis Desktop Guard", MB_ICONWARNING);
        return;
    }

    int status = 0;
    std::wstring threatName;
    unsigned long scannedObjects = 0;
    unsigned long infectedObjects = 0;

    if (!RpcScanDirectory(path, status, threatName, scannedObjects, infectedObjects)) {
        MessageBoxW(g_mainWindow, L"Ошибка RPC-сканирования папки.", L"Aegis Desktop Guard", MB_ICONERROR);
        return;
    }

    std::wstring result =
        L"Результат папки: " +
        ScanStatusToText(status) +
        L" | проверено: " +
        std::to_wstring(scannedObjects) +
        L" | заражено: " +
        std::to_wstring(infectedObjects);

    if (!threatName.empty()) {
        result += L" | угроза: " + threatName;
    }

    SetWindowTextW(g_scanResultLabel, result.c_str());
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
    Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
    PostQuitMessage(0);
}

void AddTrayIcon(HWND window)
{
    ZeroMemory(&g_trayIcon, sizeof(g_trayIcon));

    g_trayIcon.cbSize = sizeof(g_trayIcon);
    g_trayIcon.hWnd = window;
    g_trayIcon.uID = kTrayIconId;
    g_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_trayIcon.uCallbackMessage = kTrayMessage;
    g_trayIcon.hIcon = LoadIconW(nullptr, IDI_SHIELD);

    wcscpy_s(g_trayIcon.szTip, L"Aegis Desktop Guard");

    Shell_NotifyIconW(NIM_ADD, &g_trayIcon);
}

void DrawPanel(HDC dc, int x, int y, int width, int height)
{
    RECT rect{x, y, x + width, y + height};
    FillRect(dc, &rect, g_panelBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(52, 60, 76));
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, borderPen));

    MoveToEx(dc, rect.left, rect.top, nullptr);
    LineTo(dc, rect.right, rect.top);
    LineTo(dc, rect.right, rect.bottom);
    LineTo(dc, rect.left, rect.bottom);
    LineTo(dc, rect.left, rect.top);

    SelectObject(dc, oldPen);
    DeleteObject(borderPen);
}

void CreateMainMenu(HWND window)
{
    HMENU menu = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();

    AppendMenuW(fileMenu, MF_STRING, kMainMenuExit, L"Выход");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"Файл");

    SetMenu(window, menu);
}

void CreateControls(HWND window)
{
    HWND title = CreateWindowW(
        L"STATIC",
        L"Aegis Desktop Guard",
        WS_CHILD | WS_VISIBLE,
        28, 24, 440, 38,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(title, g_titleFont);

    HWND subtitle = CreateWindowW(
        L"STATIC",
        L"Service-based antivirus dashboard",
        WS_CHILD | WS_VISIBLE,
        31, 62, 460, 24,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(subtitle, g_textFont);

    g_userLabel = CreateWindowW(
        L"STATIC",
        L"Пользователь",
        WS_CHILD | WS_VISIBLE,
        48, 122, 720, 24,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(g_userLabel, g_textFont);

    g_licenseLabel = CreateWindowW(
        L"STATIC",
        L"Лицензия",
        WS_CHILD | WS_VISIBLE,
        48, 152, 720, 24,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(g_licenseLabel, g_textFont);

    g_featureLabel = CreateWindowW(
        L"STATIC",
        L"Статус защиты",
        WS_CHILD | WS_VISIBLE,
        48, 182, 720, 24,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(g_featureLabel, g_textFont);

    g_databaseLabel = CreateWindowW(
        L"STATIC",
        L"Антивирусные базы",
        WS_CHILD | WS_VISIBLE,
        48, 212, 760, 24,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(g_databaseLabel, g_textFont);

    HWND loginText = CreateWindowW(
        L"STATIC",
        L"Аккаунт",
        WS_CHILD | WS_VISIBLE,
        48, 274, 160, 24,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(loginText, g_textFont);

    HWND loginLabel = CreateWindowW(
        L"STATIC",
        L"Логин:",
        WS_CHILD | WS_VISIBLE,
        48, 315, 90, 24,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(loginLabel, g_textFont);

    g_usernameEdit = CreateWindowW(
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        140, 312, 210, 28,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(g_usernameEdit, g_textFont);

    HWND passwordLabel = CreateWindowW(
        L"STATIC",
        L"Пароль:",
        WS_CHILD | WS_VISIBLE,
        48, 353, 90, 24,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(passwordLabel, g_textFont);

    g_passwordEdit = CreateWindowW(
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD | ES_AUTOHSCROLL,
        140, 350, 210, 28,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(g_passwordEdit, g_textFont);

    HWND loginButton = CreateWindowW(
        L"BUTTON",
        L"Войти",
        WS_CHILD | WS_VISIBLE,
        370, 311, 130, 31,
        window,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kButtonLogin)),
        g_instance,
        nullptr
    );
    SetControlFont(loginButton, g_buttonFont);

    HWND logoutButton = CreateWindowW(
        L"BUTTON",
        L"Выйти",
        WS_CHILD | WS_VISIBLE,
        370, 349, 130, 31,
        window,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kButtonLogout)),
        g_instance,
        nullptr
    );
    SetControlFont(logoutButton, g_buttonFont);

    HWND activationText = CreateWindowW(
        L"STATIC",
        L"Активация продукта",
        WS_CHILD | WS_VISIBLE,
        48, 420, 260, 24,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(activationText, g_textFont);

    g_activationEdit = CreateWindowW(
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        48, 456, 300, 28,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(g_activationEdit, g_textFont);

    HWND activateButton = CreateWindowW(
        L"BUTTON",
        L"Активировать",
        WS_CHILD | WS_VISIBLE,
        370, 454, 150, 31,
        window,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kButtonActivate)),
        g_instance,
        nullptr
    );
    SetControlFont(activateButton, g_buttonFont);

    HWND scanText = CreateWindowW(
        L"STATIC",
        L"Сканирование",
        WS_CHILD | WS_VISIBLE,
        560, 274, 180, 24,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(scanText, g_textFont);

    HWND fileLabel = CreateWindowW(
        L"STATIC",
        L"Файл:",
        WS_CHILD | WS_VISIBLE,
        560, 315, 80, 24,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(fileLabel, g_textFont);

    g_filePathEdit = CreateWindowW(
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        630, 312, 330, 28,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(g_filePathEdit, g_textFont);

    HWND scanFileButton = CreateWindowW(
        L"BUTTON",
        L"Сканировать файл",
        WS_CHILD | WS_VISIBLE,
        970, 310, 170, 31,
        window,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kButtonScanFile)),
        g_instance,
        nullptr
    );
    SetControlFont(scanFileButton, g_buttonFont);

    HWND directoryLabel = CreateWindowW(
        L"STATIC",
        L"Папка:",
        WS_CHILD | WS_VISIBLE,
        560, 353, 80, 24,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(directoryLabel, g_textFont);

    g_directoryPathEdit = CreateWindowW(
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        630, 350, 330, 28,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(g_directoryPathEdit, g_textFont);

    HWND scanDirButton = CreateWindowW(
        L"BUTTON",
        L"Сканировать папку",
        WS_CHILD | WS_VISIBLE,
        970, 348, 170, 31,
        window,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kButtonScanDirectory)),
        g_instance,
        nullptr
    );
    SetControlFont(scanDirButton, g_buttonFont);

    g_scanResultLabel = CreateWindowW(
        L"STATIC",
        L"Результаты сканирования появятся здесь",
        WS_CHILD | WS_VISIBLE,
        560, 420, 570, 90,
        window,
        nullptr,
        g_instance,
        nullptr
    );
    SetControlFont(g_scanResultLabel, g_textFont);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == g_taskbarCreatedMessage) {
        AddTrayIcon(window);
        return 0;
    }

    switch (message) {
    case WM_CREATE:
        InitializeVisualStyle();
        CreateMainMenu(window);
        CreateControls(window);
        SetTimer(window, kStatusTimer, 3000, nullptr);
        AddTrayIcon(window);
        RefreshUi();
        return 0;

    case WM_ERASEBKGND:
    {
        RECT rect{};
        GetClientRect(window, &rect);
        FillRect(reinterpret_cast<HDC>(wParam), &rect, g_backgroundBrush);
        return 1;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(window, &ps);

        DrawPanel(dc, 28, 105, 1090, 145);
        DrawPanel(dc, 28, 260, 500, 245);
        DrawPanel(dc, 540, 260, 600, 260);

        EndPaint(window, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, kTextColor);
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(g_backgroundBrush);
    }

    case WM_CTLCOLOREDIT:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, kTextColor);
        SetBkColor(dc, kEditColor);
        return reinterpret_cast<LRESULT>(g_editBrush);
    }

    case WM_CTLCOLORBTN:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, kTextColor);
        SetBkColor(dc, kPanelColor);
        return reinterpret_cast<LRESULT>(g_panelBrush);
    }

    case WM_TIMER:
        if (wParam == kStatusTimer) {
            RefreshUi();
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kButtonLogin:
        {
            const std::wstring username = GetWindowTextString(g_usernameEdit);
            const std::wstring password = GetWindowTextString(g_passwordEdit);

            if (!RpcLogin(username, password)) {
                MessageBoxW(window, L"Ошибка входа.", L"Aegis Desktop Guard", MB_ICONERROR);
            }

            RefreshUi();
            return 0;
        }

        case kButtonLogout:
            RpcLogout();
            RefreshUi();
            return 0;

        case kButtonActivate:
        {
            const std::wstring code = GetWindowTextString(g_activationEdit);

            if (!RpcActivate(code)) {
                MessageBoxW(window, L"Ошибка активации.", L"Aegis Desktop Guard", MB_ICONERROR);
            }

            RefreshUi();
            return 0;
        }

        case kButtonScanFile:
            PerformFileScan();
            return 0;

        case kButtonScanDirectory:
            PerformDirectoryScan();
            return 0;

        case kMainMenuExit:
            RequestServiceStop();
            ExitApplication();
            return 0;

        default:
            return 0;
        }

    case kTrayMessage:
        if (LOWORD(lParam) == WM_LBUTTONUP || LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            ShowWindow(window, SW_SHOW);
            SetForegroundWindow(window);
            return 0;
        }

        if (LOWORD(lParam) == WM_RBUTTONUP) {
            HMENU menu = CreatePopupMenu();

            AppendMenuW(menu, MF_STRING, kMenuOpen, L"Открыть");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING, kMenuExit, L"Выход");

            POINT cursor{};
            GetCursorPos(&cursor);

            SetForegroundWindow(window);

            const UINT command = TrackPopupMenu(
                menu,
                TPM_RETURNCMD | TPM_NONOTIFY,
                cursor.x,
                cursor.y,
                0,
                window,
                nullptr
            );

            DestroyMenu(menu);

            if (command == kMenuOpen) {
                ShowWindow(window, SW_SHOW);
                SetForegroundWindow(window);
            } else if (command == kMenuExit) {
                RequestServiceStop();
                ExitApplication();
            }

            return 0;
        }

        return 0;

    case WM_CLOSE:
        ShowWindow(window, SW_HIDE);
        return 0;

    case WM_DESTROY:
        KillTimer(window, kStatusTimer);
        Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
        ReleaseVisualStyle();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

bool HasArgument(PWSTR commandLine, const std::wstring& expected)
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
    HANDLE mutex = CreateMutexW(nullptr, TRUE, aegis::kGuiMutexName);

    if (mutex == nullptr || GetLastError() == 183) {
        return 0;
    }

    g_instance = instance;
    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_SHIELD);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (RegisterClassExW(&windowClass) == 0) {
        CloseHandle(mutex);
        return 0;
    }

    g_mainWindow = CreateWindowExW(
        0,
        kWindowClassName,
        L"Aegis Desktop Guard",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1190,
        650,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (g_mainWindow == nullptr) {
        CloseHandle(mutex);
        return 0;
    }

    const bool startHidden = HasArgument(commandLine, L"--background");

    if (!startHidden) {
        ShowWindow(g_mainWindow, showCommand);
        UpdateWindow(g_mainWindow);
    }

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    CloseHandle(mutex);

    return static_cast<int>(message.wParam);
}