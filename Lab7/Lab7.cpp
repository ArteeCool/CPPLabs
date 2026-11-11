#include "framework.h"
#include "Lab7.h"
#include "Pipe.h"
#include "SystemData.h"

#ifndef CLIENT_ID
#define CLIENT_ID 0
#endif

static PipeServer server;
static HWND output;
static HWND status;
static HFONT font;
static std::wstring journal;
enum { ID_START = 201, ID_STOP, ID_SEND };

static void SetStatus(const wchar_t* text)
{
    SetWindowTextW(status, text);
}

static void SendData()
{
    std::wstring text = ClientData(CLIENT_ID);
    SetWindowTextW(output, text.c_str());
    SetStatus(SendPipeMessage(text) ? L"Дані записано до каналу." :
        L"Не вдалося надіслати дані. Створіть канал на сервері й повторіть спробу.");
}

static LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH, L"Segoe UI");
        auto button = [&](const wchar_t* label, int x, int id)
        {
            HWND control = CreateWindowW(L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                x, 12, 195, 34, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        };
        if (CLIENT_ID == 0)
        {
            button(L"Створити канал", 12, ID_START);
            button(L"Закрити канал", 219, ID_STOP);
        }
        else button(L"Надіслати дані", 12, ID_SEND);
        status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            12, 58, 700, 48, window, nullptr, nullptr, nullptr);
        output = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE |
            WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            12, 110, 700, 340, window, nullptr, nullptr, nullptr);
        SendMessageW(status, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(output, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(output, EM_SETLIMITTEXT, 1000000, 0);
        SetStatus(CLIENT_ID == 0 ? L"Сервер зупинено. Створіть канал." : L"Готово до надсилання.");
        if (CLIENT_ID != 0) SetWindowTextW(output, ClientData(CLIENT_ID).c_str());
        return 0;
    }
    case WM_SIZE:
        MoveWindow(status, 12, 58, LOWORD(lParam) - 24, 48, TRUE);
        MoveWindow(output, 12, 110, LOWORD(lParam) - 24, HIWORD(lParam) - 122, TRUE);
        return 0;
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = { 780, 500 };
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_START:
            if (server.running) SetStatus(L"Канал уже створено.");
            else if (server.start())
            {
                journal.clear();
                SetWindowTextW(output, L"");
                SetStatus(L"Канал створено. Очікування даних від двох клієнтів.");
            }
            else SetStatus(L"Не вдалося створити канал. Перевірте, чи не працює інший сервер.");
            break;
        case ID_STOP:
            server.stop();
            SetStatus(L"Канал закрито.");
            break;
        case ID_SEND: SendData(); break;
        }
        return 0;
    case WM_DESTROY:
        server.stop();
        DeleteObject(font);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show)
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_LAB7));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"Lab7NamedPipe";
    if (!RegisterClassW(&wc)) return 1;
    std::wstring title = CLIENT_ID == 0 ? L"ЛБ 7 - Сервер - Варіант 8" :
        L"ЛБ 7 - Клієнт " + std::to_wstring(CLIENT_ID) + L" - Варіант 8";
    HWND window = CreateWindowW(wc.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 540, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    for (;;)
    {
        DWORD count = server.running ? 2 : 0;
        DWORD result = MsgWaitForMultipleObjects(count, count ? server.events : nullptr,
            FALSE, INFINITE, QS_ALLINPUT);
        if (result == WAIT_FAILED) return 1;
        if (result < WAIT_OBJECT_0 + count)
        {
            std::wstring text;
            if (!server.receive(result - WAIT_OBJECT_0, text))
            {
                server.stop();
                SetStatus(L"Помилка каналу. Створіть його повторно.");
            }
            else if (!text.empty())
            {
                journal += text + L"\r\n\r\n";
                SetWindowTextW(output, journal.c_str());
                SendMessageW(output, EM_SETSEL, journal.size(), journal.size());
                SendMessageW(output, EM_SCROLLCARET, 0, 0);
                SetStatus(L"Дані отримано через іменований канал.");
            }
        }
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) return static_cast<int>(msg.wParam);
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}
