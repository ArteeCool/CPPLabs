#include "framework.h"
#include "Lab5.h"
#include <commctrl.h>
#include <winioctl.h>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")

#ifndef CLIENT_ID
#define CLIENT_ID 0
#endif

constexpr wchar_t slotName[] = L"\\\\.\\mailslot\\lab5_variant5";
constexpr int startId = 1001, stopId = 1002, sendId = 1003;
HANDLE slot = INVALID_HANDLE_VALUE;
HWND output, statusBar;

void Append(const std::wstring& text) {
    int length = GetWindowTextLengthW(output);
    SendMessageW(output, EM_SETSEL, length, length);
    SendMessageW(output, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    SendMessageW(output, EM_SCROLLCARET, 0, 0);
}

void Error(const wchar_t* action, DWORD code = GetLastError()) {
    Append(std::wstring(action) + L": помилка " + std::to_wstring(code) + L"\r\n");
}

void Stop(HWND window) {
    KillTimer(window, 1);
    if (slot == INVALID_HANDLE_VALUE)
        return;

    CloseHandle(slot);
    slot = INVALID_HANDLE_VALUE;
    Append(L"Поштову скриньку закрито.\r\n");
    SetWindowTextW(statusBar, L"Сервер зупинено");
}

void Start(HWND window) {
    if (slot != INVALID_HANDLE_VALUE)
        return;

    slot = CreateMailslotW(slotName, 8192, 0, nullptr);
    if (slot == INVALID_HANDLE_VALUE) {
        Error(L"Створення скриньки");
        return;
    }
    if (!SetTimer(window, 1, 1000, nullptr)) {
        Error(L"Запуск таймера");
        Stop(window);
        return;
    }
    Append(std::wstring(L"Сервер запущено: ") + slotName + L"\r\n");
    SetWindowTextW(statusBar, L"Сервер працює, опитування щосекунди");
}

void Receive(HWND window) {
    for (int i = 0; i < 100 && slot != INVALID_HANDLE_VALUE; ++i) {
        DWORD size = 0, count = 0, read = 0;
        if (!GetMailslotInfo(slot, nullptr, &size, &count, nullptr)) {
            Error(L"Стан скриньки");
            Stop(window);
            return;
        }
        if (!count || size == MAILSLOT_NO_MESSAGE)
            return;

        std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1, L'\0');
        if (!ReadFile(slot, buffer.data(), size, &read, nullptr)) {
            Error(L"Читання");
            return;
        }

        if (read % sizeof(wchar_t))
            Append(L"Некоректне повідомлення.\r\n");
        else
            Append(std::wstring(buffer.data(), read / sizeof(wchar_t)) + L"\r\n");
    }
}

std::wstring Data()
{
    HDC dc = GetDC(nullptr);
    if (!dc)
        return L"Не вдалося отримати контекст екрана.\r\n";

    int width = GetDeviceCaps(dc, HORZRES);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    std::wstring text = L"Клієнт " + std::to_wstring(CLIENT_ID) + L", варіант 5\r\n";
    if (CLIENT_ID == 1) {
        MEMORYSTATUSEX memory{sizeof(memory)};
        if (GlobalMemoryStatusEx(&memory))
            text += L"Оперативна пам'ять: " + std::to_wstring(memory.ullTotalPhys / (1024 * 1024)) + L" МіБ\r\n";
        else
            text += L"Оперативна пам'ять: не вдалося визначити\r\n";
        text += L"Наявність зовнішнього диску: " + std::to_wstring(GetSystemMetrics(SM_REMOTECONTROL)) + L"\r\n";

        text += L"Ширина екрана (HORZRES): " + std::to_wstring(width) + L" пкс\r\n";
        text += L"SM_CXVIRTUALSCREEN (ширина віртуального екрана): " + std::to_wstring(GetSystemMetrics(SM_CXVIRTUALSCREEN)) + L" пкс\r\n";
        text += L"SM_REMOTECONTROL (віддалене керування сеансом): " + std::to_wstring(GetSystemMetrics(SM_REMOTECONTROL)) + L"\r\n";
    }
    else
    {
        RECT rect{};
        GetWindowRect(statusBar, &rect);
        text += L"Висота рядка стану додатку: " + std::to_wstring(rect.bottom - rect.top) + L" пкс\r\n";
        HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
        HWND tray = taskbar ? FindWindowExW(taskbar, nullptr, L"TrayNotifyWnd", nullptr) : nullptr;
        if (tray && GetWindowRect(tray, &rect))
            text += L"Ширина області сповіщень: " + std::to_wstring(rect.right - rect.left) + L" пкс\r\n";
        else
            text += L"Ширина області сповіщень: недоступна\r\n";
        text += L"DPI горизонтально (LOGPIXELSX): " + std::to_wstring(dpi) + L"\r\n";
        text += L"SM_CYEDGE (висота об'ємної межі): " + std::to_wstring(GetSystemMetrics(SM_CYEDGE)) + L" пкс\r\n";
        text += L"SM_CXEDGE (ширина об'ємної межі): " + std::to_wstring(GetSystemMetrics(SM_CXEDGE)) + L" пкс\r\n";
    }
    return text;
}

void Send()
{
    std::wstring text = Data();
    SetWindowTextW(output, text.c_str());
    HANDLE server = CreateFileW(slotName, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (server == INVALID_HANDLE_VALUE) {
        Error(L"Відкриття скриньки. Спочатку запустіть сервер");
        SetWindowTextW(statusBar, L"Скринька недоступна");
        return;
    }
    DWORD written = 0;
    DWORD size = static_cast<DWORD>(text.size() * sizeof(wchar_t));
    BOOL sent = WriteFile(server, text.data(), size, &written, nullptr);
    DWORD error = sent ? ERROR_WRITE_FAULT : GetLastError();
    CloseHandle(server);
    if (!sent || written != size)
        Error(L"Надсилання", error);
    else {
        Append(L"\r\nДані записано до поштової скриньки.\r\n");
        SetWindowTextW(statusBar, L"Дані надіслано");
    }
}

LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        HINSTANCE instance = reinterpret_cast<LPCREATESTRUCTW>(lParam)->hInstance;
        auto button = [&](const wchar_t* title, int id, int x, int width) {
            HWND control = CreateWindowW(L"BUTTON", title, WS_CHILD | WS_VISIBLE | WS_TABSTOP, x, 10, width, 30, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        };
        if (CLIENT_ID == 0) {
            button(L"Створити скриньку", startId, 10, 170);
            button(L"Закрити скриньку", stopId, 190, 170);
        }
        else {
            button(L"Надіслати дані", sendId, 10, 170);
        }

        output = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            10, 50, 600, 300, window, nullptr, instance, nullptr);

        SendMessageW(output, EM_SETLIMITTEXT, 1024 * 1024, 0);
        SendMessageW(output, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        statusBar = CreateWindowW(STATUSCLASSNAMEW, L"Готово", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, nullptr, instance, nullptr);
        if (!output || !statusBar)
            return -1;

        Append(L"Лабораторна робота №5. Варіант 5 (9 - 4).\r\n");
        break;
    }
    case WM_SIZE:
    {
        SendMessageW(statusBar, WM_SIZE, 0, 0);
        RECT rect{};
        GetWindowRect(statusBar, &rect);
        int width = LOWORD(lParam) - 20;
        int height = HIWORD(lParam) - (rect.bottom - rect.top) - 60;
        MoveWindow(output, 10, 50, width > 0 ? width : 0, height > 0 ? height : 0, TRUE);
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case startId:
            Start(window);
            break;
        case stopId:
            Stop(window);
            break;
        case sendId:
            Send();
            break;
        }
        break;
    case WM_TIMER:
        if (wParam == 1)
            Receive(window);
        break;
    case WM_DESTROY:
        Stop(window);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_LAB5));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"Lab5Mailslot";
    if (!RegisterClassW(&wc))
        return 1;

    std::wstring title = L"ЛБ 5 - " + (CLIENT_ID == 0 ? std::wstring(L"Сервер") : L"Клієнт " + std::to_wstring(CLIENT_ID));
    HWND window = CreateWindowW(wc.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 740, 420, nullptr, nullptr, instance, nullptr);
    if (!window)
        return 1;

    ShowWindow(window, show);
    MSG message{};
    int result;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return result == -1 ? 1 : static_cast<int>(message.wParam);
}