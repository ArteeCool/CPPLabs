#pragma once
#include <windows.h>
#include <string>

inline constexpr wchar_t PIPE_NAME[] = L"\\\\.\\pipe\\Lab7_Variant8";
inline constexpr DWORD MESSAGE_BYTES = 2048;

class PipeServer
{
    struct Connection
    {
        HANDLE pipe = INVALID_HANDLE_VALUE;
        OVERLAPPED operation = {};
        bool connecting = true;
        bool pending = false;
        DWORD bytes = 0;
        wchar_t buffer[MESSAGE_BYTES / sizeof(wchar_t)] = {};
    } connections[2];

    bool connect(Connection& c)
    {
        DisconnectNamedPipe(c.pipe);
        ResetEvent(c.operation.hEvent);
        c.connecting = true;
        c.pending = false;
        if (ConnectNamedPipe(c.pipe, &c.operation))
        {
            SetEvent(c.operation.hEvent);
            return true;
        }
        DWORD error = GetLastError();
        if (error == ERROR_IO_PENDING) c.pending = true;
        else if (error == ERROR_PIPE_CONNECTED) SetEvent(c.operation.hEvent);
        else return false;
        return true;
    }

public:
    HANDLE events[2] = {};
    bool running = false;
    PipeServer() = default;
    PipeServer(const PipeServer&) = delete;
    PipeServer& operator=(const PipeServer&) = delete;
    ~PipeServer() { stop(); }

    void stop()
    {
        for (auto& c : connections)
        {
            if (c.pipe != INVALID_HANDLE_VALUE)
            {
                if (c.pending)
                {
                    CancelIoEx(c.pipe, &c.operation);
                    DWORD ignored;
                    GetOverlappedResult(c.pipe, &c.operation, &ignored, TRUE);
                }
                DisconnectNamedPipe(c.pipe);
                CloseHandle(c.pipe);
            }
            if (c.operation.hEvent) CloseHandle(c.operation.hEvent);
            c = {};
        }
        events[0] = events[1] = nullptr;
        running = false;
    }

    bool start()
    {
        if (running) return true;
        for (int i = 0; i < 2; ++i)
        {
            auto& c = connections[i];
            c.operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            DWORD access = PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED;
            if (i == 0) access |= FILE_FLAG_FIRST_PIPE_INSTANCE;
            c.pipe = CreateNamedPipeW(PIPE_NAME, access,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                2, MESSAGE_BYTES, MESSAGE_BYTES, 2000, nullptr);
            if (!c.operation.hEvent || c.pipe == INVALID_HANDLE_VALUE || !connect(c))
            {
                stop();
                return false;
            }
            events[i] = c.operation.hEvent;
        }
        running = true;
        return true;
    }

    // Returns false only when a listener cannot be rearmed.
    bool receive(unsigned index, std::wstring& message)
    {
        message.clear();
        auto& c = connections[index];
        if (c.pending)
        {
            BOOL ok = GetOverlappedResult(c.pipe, &c.operation, &c.bytes, FALSE);
            c.pending = false;
            if (!ok) return connect(c);
        }
        if (c.connecting)
        {
            c.connecting = false;
            ResetEvent(c.operation.hEvent);
            if (ReadFile(c.pipe, c.buffer, sizeof(c.buffer), &c.bytes, &c.operation))
                SetEvent(c.operation.hEvent);
            else if (GetLastError() == ERROR_IO_PENDING) c.pending = true;
            else return connect(c);
            return true;
        }
        if (c.bytes >= sizeof(wchar_t) && c.bytes % sizeof(wchar_t) == 0)
        {
            size_t length = c.bytes / sizeof(wchar_t);
            if (c.buffer[length - 1] == L'\0') message.assign(c.buffer, length - 1);
        }
        return connect(c);
    }
};

inline bool SendPipeMessage(const std::wstring& text)
{
    if (text.empty() || (text.size() + 1) * sizeof(wchar_t) > MESSAGE_BYTES) return false;
    ULONGLONG deadline = GetTickCount64() + 2000;
    HANDLE pipe;
    for (;;)
    {
        pipe = CreateFileW(PIPE_NAME, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_PIPE_BUSY || GetTickCount64() >= deadline) return false;
        DWORD remaining = static_cast<DWORD>(deadline - GetTickCount64());
        if (!WaitNamedPipeW(PIPE_NAME, remaining)) return false;
    }
    OVERLAPPED operation = {};
    operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!operation.hEvent)
    {
        CloseHandle(pipe);
        return false;
    }
    DWORD written = 0;
    DWORD size = static_cast<DWORD>((text.size() + 1) * sizeof(wchar_t));
    bool ok = WriteFile(pipe, text.c_str(), size, &written, &operation) != FALSE;
    if (!ok && GetLastError() == ERROR_IO_PENDING)
    {
        if (WaitForSingleObject(operation.hEvent, 2000) == WAIT_OBJECT_0)
            ok = GetOverlappedResult(pipe, &operation, &written, FALSE) != FALSE;
        else
        {
            CancelIoEx(pipe, &operation);
            GetOverlappedResult(pipe, &operation, &written, TRUE);
        }
    }
    CloseHandle(operation.hEvent);
    CloseHandle(pipe);
    return ok && written == size;
}
