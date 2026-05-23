#include <windows.h>
#include <shellapi.h>

#include <string>

namespace {

constexpr wchar_t kWindowClassName[] = L"AegisDesktopGuardWindowClass";
constexpr wchar_t kMutexName[] = L"Local\\AegisDesktopGuardSingleInstance";
constexpr UINT kTrayIconId = 1001;
constexpr UINT kTrayMessage = WM_APP + 1;

constexpr UINT kMenuOpen = 2001;
constexpr UINT kMenuExit = 2002;
constexpr UINT kMainMenuExit = 3001;

HINSTANCE g_instance = nullptr;
HWND g_mainWindow = nullptr;
UINT g_taskbarCreatedMessage = 0;
NOTIFYICONDATAW g_trayIcon{};

void ShowMainWindow()
{
    ShowWindow(g_mainWindow, SW_SHOW);
    SetForegroundWindow(g_mainWindow);
}

void ExitApplication()
{
    Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
    PostQuitMessage(0);
}

HICON CreateTrayIcon()
{
    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, 32, 32);

    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memoryDc, bitmap));

    HBRUSH backgroundBrush = CreateSolidBrush(RGB(26, 88, 180));
    RECT backgroundRect{0, 0, 32, 32};
    FillRect(memoryDc, &backgroundRect, backgroundBrush);

    HPEN whitePen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
    HPEN oldPen = static_cast<HPEN>(SelectObject(memoryDc, whitePen));

    MoveToEx(memoryDc, 9, 17, nullptr);
    LineTo(memoryDc, 15, 23);
    LineTo(memoryDc, 24, 9);

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

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == g_taskbarCreatedMessage) {
        AddTrayIcon(window);
        return 0;
    }

    switch (message) {
    case WM_CREATE:
        CreateMainMenu(window);
        AddTrayIcon(window);
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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand)
{
    g_instance = instance;

    HANDLE mutexHandle = CreateMutexW(nullptr, TRUE, kMutexName);

    if (mutexHandle == nullptr) {
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutexHandle);
        return 0;
    }

    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");

    if (!RegisterMainWindowClass()) {
        CloseHandle(mutexHandle);
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
        460,
        nullptr,
        nullptr,
        g_instance,
        nullptr
    );

    if (g_mainWindow == nullptr) {
        CloseHandle(mutexHandle);
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

    ReleaseMutex(mutexHandle);
    CloseHandle(mutexHandle);

    return static_cast<int>(message.wParam);
}