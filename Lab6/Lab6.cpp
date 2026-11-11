#include "framework.h"
#include "Lab6.h"
#include "Mapping.h"
#include <string>

#ifndef CLIENT_ID
#define CLIENT_ID 0
#endif

static Mapping mapping;
static HWND output;
static HWND status;
static HFONT font;
static unsigned seen[2] = {};
enum { ID_START = 101, ID_READ, ID_STOP, ID_SEND };

static void SetStatus(const wchar_t* text)
{
    SetWindowTextW(status, text);
}

static std::wstring ClientData()
{
    HDC dc = GetDC(nullptr);
    if (!dc) return L"";
    wchar_t text[512];
    if (CLIENT_ID == 1)
    {
        swprintf_s(text, L"Клієнт 1\r\nВисота меню (SM_CYMENU): %d пкс\r\n"
            L"Наявність миші: %s\r\nШирина екрана (HORZRES): %d пкс",
            GetSystemMetrics(SM_CYMENU),
            GetSystemMetrics(SM_MOUSEPRESENT) ? L"так" : L"ні",
            GetDeviceCaps(dc, HORZRES));
    }
    else
    {
        swprintf_s(text, L"Клієнт 2\r\nРозмір іконки: %d x %d пкс\r\n"
            L"Ширина вертикальної смуги прокрутки: %d пкс\r\n"
            L"Глибина кольору (BITSPIXEL): %d біт/піксель",
            GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON),
            GetSystemMetrics(SM_CXVSCROLL), GetDeviceCaps(dc, BITSPIXEL));
    }
    ReleaseDC(nullptr, dc);
    return text;
}

static void ReadData(bool automatic)
{
    SharedData snapshot = {};
    if (!mapping.read(snapshot))
    {
        SetStatus(L"Читання недоступне. Створіть проєкцію або повторіть спробу.");
        return;
    }
    std::wstring text;
    for (int i = 0; i < 2; ++i)
    {
        if (snapshot.revision[i]) text += snapshot.message[i];
        else text += L"Клієнт " + std::to_wstring(i + 1) + L": даних ще немає.";
        text += L"\r\n\r\n";
        seen[i] = snapshot.revision[i];
    }
    SetWindowTextW(output, text.c_str());
    SetStatus(automatic ? L"Дані оновлено автоматично за подією." : L"Проєкцію прочитано вручну.");
}

static void StartServer()
{
    if (mapping.file)
    {
        SetStatus(L"Проєкцію вже створено.");
        return;
    }
    if (!mapping.create())
    {
        SetStatus(L"Не вдалося створити проєкцію. Можливо, інший сервер уже працює.");
        return;
    }
    seen[0] = seen[1] = 0;
    ReadData(false);
    SetStatus(L"Проєкцію створено. Автоматичне оновлення увімкнено.");
}

static void SendData()
{
    std::wstring text = ClientData();
    if (text.empty())
    {
        SetStatus(L"Не вдалося отримати характеристики екрана.");
        return;
    }
    SetWindowTextW(output, text.c_str());
    Mapping client;
    if (!client.open() || !client.send(CLIENT_ID - 1, text))
    {
        SetStatus(L"Надсилання не виконано. Створіть проєкцію на сервері.");
        return;
    }
    SetStatus(L"Дані записано у спільну пам’ять. Сервер сповіщено.");
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
        auto button = [&](const wchar_t* label, int x, int width, int id)
        {
            HWND control = CreateWindowW(L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                x, 12, width, 34, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        };
        if (CLIENT_ID == 0)
        {
            button(L"Створити проєкцію", 12, 195, ID_START);
            button(L"Прочитати", 219, 150, ID_READ);
            button(L"Закрити проєкцію", 381, 195, ID_STOP);
        }
        else button(L"Надіслати дані", 12, 195, ID_SEND);
        status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            12, 58, 680, 48, window, nullptr, nullptr, nullptr);
        output = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE |
            WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            12, 110, 680, 290, window, nullptr, nullptr, nullptr);
        SendMessageW(status, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(output, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SetStatus(CLIENT_ID == 0 ? L"Сервер зупинено. Створіть проєкцію." : L"Готово до надсилання.");
        if (CLIENT_ID != 0) SetWindowTextW(output, ClientData().c_str());
        return 0;
    }
    case WM_SIZE:
        MoveWindow(status, 12, 58, LOWORD(lParam) - 24, 48, TRUE);
        MoveWindow(output, 12, 110, LOWORD(lParam) - 24, HIWORD(lParam) - 122, TRUE);
        return 0;
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = { 730, 420 };
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_START: StartServer(); break;
        case ID_READ: ReadData(false); break;
        case ID_STOP:
            SetStatus(mapping.stop() ? L"Проєкцію закрито." : L"Проєкція зайнята. Повторіть закриття.");
            break;
        case ID_SEND: SendData(); break;
        }
        return 0;
    case WM_DESTROY:
        mapping.stop();
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
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_LAB6));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"Lab6FileMapping";
    if (!RegisterClassW(&wc)) return 1;
    std::wstring title = CLIENT_ID == 0 ? L"ЛБ 6 - Сервер - Варіант 1" : L"ЛБ 6 - Клієнт " + std::to_wstring(CLIENT_ID) + L" - Варіант 1";
    HWND window = CreateWindowW(wc.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 760, 480, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);

    for (;;)
    {
        DWORD count = mapping.changed ? 1 : 0;
        DWORD result = MsgWaitForMultipleObjects(count, count ? &mapping.changed : nullptr,
            FALSE, INFINITE, QS_ALLINPUT);
        if (result == WAIT_FAILED) return 1;
        if (count && result == WAIT_OBJECT_0)
        {
            SharedData snapshot = {};
            if (mapping.read(snapshot) &&
                (snapshot.revision[0] != seen[0] || snapshot.revision[1] != seen[1])) ReadData(true);
        }
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                return static_cast<int>(msg.wParam);
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}
