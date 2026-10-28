#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include "framework.h"
#include "SocketApp.h"
#include <iphlpapi.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#ifndef APP_ROLE
#define APP_ROLE 0
#endif

namespace
{
    constexpr UINT SocketEvent = WM_APP + 1;
    constexpr int ConnectButton = 2001;
    constexpr int SendButton = 2002;
    constexpr int Port = 54004;
    constexpr size_t BufferLimit = 65536;
    constexpr int Role = APP_ROLE;
    HWND output, status, address, connectButton, sendButton;
    HFONT font;
    SOCKET listener = INVALID_SOCKET;
    SOCKET connection = INVALID_SOCKET;
    bool started = false;
    bool connected = false;

    struct Peer
    {
        std::string input;
        std::string output;
    };

    std::map<SOCKET, Peer> peers;

    std::string Encode(const std::wstring& text)
    {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
        std::string result(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), result.data(), size, nullptr, nullptr);
        return result;
    }

    std::wstring Decode(const std::string& text)
    {
        int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), (int)text.size(), nullptr, 0);
        if (!size)
            return L"Некоректне UTF-8 повідомлення";
        std::wstring result(size, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), (int)text.size(), result.data(), size);
        return result;
    }

    void Log(const std::wstring& text)
    {
        if (GetWindowTextLengthW(output) > 60000) SetWindowTextW(output, L"");
        std::wstring line = text + L"\r\n\r\n";
        SendMessageW(output, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
        SendMessageW(output, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
        SendMessageW(output, EM_SCROLLCARET, 0, 0);
    }

    void Error(const wchar_t* operation, int code)
    {
        Log(std::wstring(operation) + L". Код помилки: " + std::to_wstring(code));
    }

    void ClosePeer(SOCKET socket)
    {
        WSAAsyncSelect(socket, nullptr, 0, 0);
        closesocket(socket);
        peers.erase(socket);
        if (socket == connection)
        {
            connection = INVALID_SOCKET;
            connected = false;
            SetWindowTextW(status, L"Не підключено");
            EnableWindow(connectButton, TRUE);
            EnableWindow(address, TRUE);
            EnableWindow(sendButton, FALSE);
        }
    }

    bool Flush(SOCKET socket)
    {
        auto& pending = peers.at(socket).output;
        while (!pending.empty())
        {
            int sent = send(socket, pending.data(), (int)pending.size(), 0);
            if (sent == SOCKET_ERROR)
            {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK)
                    return true;
                Error(L"Не вдалося надіслати дані", error);
                ClosePeer(socket);
                return false;
            }
            if (!sent)
            {
                ClosePeer(socket);
                return false;
            }
            pending.erase(0, sent);
        }
        return true;
    }

    bool Queue(SOCKET socket, const std::wstring& text)
    {
        auto& pending = peers.at(socket).output;
        std::string packet = Encode(text) + "\n\n";
        if (pending.size() + packet.size() > BufferLimit)
        {
            Log(L"Черга надсилання переповнена");
            ClosePeer(socket);
            return false;
        }
        pending += packet;
        return Flush(socket);
    }

    std::wstring CoreCount()
    {
        DWORD size = 0;
        GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &size);
        if (!size)
            return L"недоступно";
        std::vector<unsigned char> data(size);
        auto info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(data.data());
        if (!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &size))
            return L"недоступно";
        unsigned count = 0;
        for (DWORD offset = 0; offset < size;)
        {
            auto item = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(data.data() + offset);
            if (!item->Size || item->Size > size - offset)
                return L"недоступно";
            if (item->Relationship == RelationProcessorCore) ++count;
            offset += item->Size;
        }
        return std::to_wstring(count);
    }

    std::wstring NetworkState()
    {
        PMIB_IF_TABLE2 table = nullptr;
        if (GetIfTable2(&table) != NO_ERROR)
            return L"недоступно";

        bool active = false;
        for (ULONG i = 0; i < table->NumEntries; ++i)
        {
            const auto& row = table->Table[i];
            if (row.Type != IF_TYPE_SOFTWARE_LOOPBACK && row.OperStatus == IfOperStatusUp)
                active = true;
        }
        FreeMibTable(table);
        return active ? L"підключено (мережевий адаптер активний)" : L"відключено";
    }

    std::wstring CollectData()
    {
        HDC dc = GetDC(nullptr);
        if (!dc)
            return L"Не вдалося отримати параметри екрана";

        std::wstring text = L"Клієнт " + std::to_wstring(Role) + L"\r\n";
        if (Role == 1)
        {
            text += L"Кількість процесорних ядер: " + CoreCount();
            text += L"\r\nСтан мережі: " + NetworkState();
            text += L"\r\nШирина екрана: " + std::to_wstring(GetDeviceCaps(dc, HORZRES)) + L" пкс";
        }
        else
        {
            NONCLIENTMETRICSW metrics{};
            metrics.cbSize = sizeof(metrics);
            text += L"Розмір шрифту меню: ";
            if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
            {
                int height = abs(metrics.lfMenuFont.lfHeight);
                int dpi = GetDeviceCaps(dc, LOGPIXELSY);
                text += std::to_wstring(height) + L" пкс";
                if (dpi > 0)
                    text += L" (" + std::to_wstring(MulDiv(height, 72, dpi)) + L" пт)";
            }
            else
            {
                text += L"недоступно";
            }

            int depth = GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES);
            int colors = GetDeviceCaps(dc, NUMCOLORS);
            text += L"\r\nКолірна глибина панелі: " + std::to_wstring(depth) + L" біт/піксель";
            text += L"\r\nКількість кольорів: ";
            
            if (colors >= 0)
                text += std::to_wstring(colors);
            else if (depth == 24 || depth == 32)
                text += L"16 777 216 (24 біти RGB; NUMCOLORS = -1)";
            else if (depth > 0 && depth < 24)
                text += std::to_wstring(1ULL << depth) + L" (за глибиною кольору)";
            else text += L"понад 256 (NUMCOLORS = -1)";
        }

        ReleaseDC(nullptr, dc);
        return text;
    }

    void Read(SOCKET socket)
    {
        char buffer[4096];
        while (peers.count(socket))
        {
            int count = recv(socket, buffer, sizeof(buffer), 0);
            if (count == SOCKET_ERROR)
            {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK)
                    return;
                Error(L"Помилка отримання", error);
                ClosePeer(socket);
                return;
            }
            if (!count)
            {
                Log(L"З'єднання закрито");
                ClosePeer(socket);
                return;
            }
            auto& input = peers.at(socket).input;
            input.append(buffer, count);
            if (input.size() > BufferLimit)
            {
                Log(L"Перевищено розмір повідомлення");
                ClosePeer(socket);
                return;
            }
            size_t end;
            while ((end = input.find("\n\n")) != std::string::npos)
            {
                std::wstring text = Decode(input.substr(0, end));
                input.erase(0, end + 2);
                Log(text);
                if (Role == 0 && !Queue(socket, L"Сервер отримав дані"))
                    return;
            }
        }
    }

    void StartServer(HWND window)
    {
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == INVALID_SOCKET)
        {
            Error(L"Не вдалося створити сокет", WSAGetLastError());
            return;
        }
        BOOL exclusive = TRUE;
        setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusive, sizeof(exclusive));
        sockaddr_in endpoint{};
        endpoint.sin_family = AF_INET;
        endpoint.sin_port = htons(Port);
        endpoint.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(listener, (sockaddr*)&endpoint, sizeof(endpoint)) == SOCKET_ERROR || listen(listener, SOMAXCONN) == SOCKET_ERROR ||
            WSAAsyncSelect(listener, window, SocketEvent, FD_ACCEPT) == SOCKET_ERROR)
        {
            Error(L"Не вдалося запустити сервер", WSAGetLastError());
            closesocket(listener);
            listener = INVALID_SOCKET;
            SetWindowTextW(status, L"Сервер не запущено");
            return;
        }
        SetWindowTextW(status, L"Сервер працює TCP :54004");
        Log(L"Очікування клієнтів. На цьому комп'ютері використовуйте 127.0.0.1");
    }

    void Connect(HWND window)
    {
        wchar_t host[64]{};
        GetWindowTextW(address, host, 64);
        sockaddr_in endpoint{};
        endpoint.sin_family = AF_INET;
        endpoint.sin_port = htons(Port);
        if (InetPtonW(AF_INET, host, &endpoint.sin_addr) != 1)
        {
            Log(L"Введіть IPv4-адресу сервера, наприклад 127.0.0.1");
            return;
        }
        connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (connection == INVALID_SOCKET)
        {
            Error(L"Не вдалося створити сокет", WSAGetLastError());
            return;
        }
        peers.emplace(connection, Peer{});
        if (WSAAsyncSelect(connection, window, SocketEvent, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
        {
            Error(L"Помилка налаштування сокета", WSAGetLastError());
            ClosePeer(connection);
            return;
        }
        SetWindowTextW(status, L"Підключення…");
        EnableWindow(connectButton, FALSE);
        EnableWindow(address, FALSE);
        if (connect(connection, (sockaddr*)&endpoint, sizeof(endpoint)) == SOCKET_ERROR)
        {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK)
            {
                Error(L"Не вдалося підключитися", error);
                ClosePeer(connection);
            }
        }
    }

    HWND Control(HWND parent, const wchar_t* type, const wchar_t* text, DWORD style, int id)
    {
        HWND control = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style,
            0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
        return control;
    }
}

bool HandleSocketApp(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        std::wstring title = (Role == 0) ? L"ЛБ 4 Сервер" : L"ЛБ 4 Клієнт " + std::to_wstring(Role);
        SetWindowTextW(window, title.c_str());
        font = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        status = Control(window, L"STATIC", L"Не підключено", SS_LEFT, 0);
        output = Control(window, L"EDIT", L"", WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0);
        SendMessageW(output, EM_SETLIMITTEXT, 131072, 0);
        
        if (Role != 0)
        {
            address = Control(window, L"EDIT", L"127.0.0.1", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 0);
            connectButton = Control(window, L"BUTTON", L"Підключити", WS_TABSTOP, ConnectButton);
            sendButton = Control(window, L"BUTTON", L"Надіслати дані", WS_TABSTOP, SendButton);
            EnableWindow(sendButton, FALSE);
            Log(CollectData());
        }

        WSADATA data{};
        int error = WSAStartup(MAKEWORD(2, 2), &data);
        started = (error == 0);
        if (!started)
        {
            Error(L"Не вдалося запустити Winsock", error);
            if (connectButton)
                EnableWindow(connectButton, FALSE);
        }
        else if (Role == 0)
            StartServer(window);

        return true;
    }
    case WM_SIZE:
    {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        MoveWindow(status, 20, 18, width - 40, 26, TRUE);
        int top = Role == 0 ? 58 : 102;
        if (Role != 0)
        {
            MoveWindow(address, 20, 56, 170, 30, TRUE);
            MoveWindow(connectButton, 202, 56, 132, 30, TRUE);
            MoveWindow(sendButton, 346, 56, 160, 30, TRUE);
        }
        MoveWindow(output, 20, top, (std::max)(width - 40, 1), (std::max)(height - top - 20, 1), TRUE);
        return true;
    }
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = { 560, 300 };
        return true;
    case WM_COMMAND:
        if (LOWORD(wParam) == ConnectButton && started && connection == INVALID_SOCKET)
        {
            Connect(window);
            return true;
        }
        if (LOWORD(wParam) == SendButton && connected)
        {
            std::wstring data = CollectData();
            if (Queue(connection, data))
                Log(L"Надсилання:\r\n" + data);
            return true;
        }
        break;
    case SocketEvent:
    {
        SOCKET socket = (SOCKET)wParam;
        int event = WSAGETSELECTEVENT(lParam);
        int error = WSAGETSELECTERROR(lParam);
        if (socket != listener && !peers.count(socket))
            return true;
        if (error)
        {
            Error(L"Помилка з'єднання", error);
            if (socket != listener)
                ClosePeer(socket);
            return true;
        }
        if (socket == listener && event == FD_ACCEPT)
        {
            for (;;)
            {
                SOCKET client = accept(listener, nullptr, nullptr);
                if (client == INVALID_SOCKET)
                {
                    int code = WSAGetLastError();
                    if (code != WSAEWOULDBLOCK)
                        Error(L"Помилка приймання клієнта", code);
                    break;
                }
                if (peers.size() >= 16 || WSAAsyncSelect(client, window, SocketEvent, FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
                {
                    closesocket(client);
                    continue;
                }
                peers.emplace(client, Peer{});
                Log(L"Клієнт підключився");
            }
        }
        else if (event == FD_CONNECT)
        {
            connected = true;
            SetWindowTextW(status, L"Підключено TCP :54004");
            EnableWindow(sendButton, TRUE);
            Log(L"З'єднання встановлено");
        }
        else if (event == FD_READ)
            Read(socket);
        else if (event == FD_WRITE)
            Flush(socket);
        else if (event == FD_CLOSE)
        {
            Read(socket);
            if (peers.count(socket)) 
                ClosePeer(socket);
        }
        return true;
    }
    case WM_DESTROY:
        while (!peers.empty())
            ClosePeer(peers.begin()->first);
        if (listener != INVALID_SOCKET)
            closesocket(listener);
        if (started)
            WSACleanup();
        if (font)
            DeleteObject(font);
        break;
    }
    return false;
}
