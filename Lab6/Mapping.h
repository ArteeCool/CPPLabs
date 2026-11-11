#pragma once
#include <windows.h>
#include <string>

struct SharedData
{
    BOOL active;
    unsigned revision[2];
    wchar_t message[2][512];
};

class Mapping
{
public:
    HANDLE file = nullptr;
    HANDLE mutex = nullptr;
    HANDLE changed = nullptr;
    SharedData* data = nullptr;

    Mapping() = default;
    Mapping(const Mapping&) = delete;
    Mapping& operator=(const Mapping&) = delete;
    ~Mapping() { close(); }

    void close()
    {
        if (data) UnmapViewOfFile(data);
        if (changed) CloseHandle(changed);
        if (mutex) CloseHandle(mutex);
        if (file) CloseHandle(file);
        data = nullptr;
        changed = mutex = file = nullptr;
    }

    bool lock()
    {
        DWORD result = WaitForSingleObject(mutex, 2000);
        return result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
    }

    bool create()
    {
        file = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
            0, sizeof(SharedData), L"Local\\Lab6_Variant1_Memory");
        DWORD error = GetLastError();
        if (!file || error == ERROR_ALREADY_EXISTS)
        {
            close();
            return false;
        }
        mutex = CreateMutexW(nullptr, FALSE, L"Local\\Lab6_Variant1_Mutex");
        changed = CreateEventW(nullptr, FALSE, FALSE, L"Local\\Lab6_Variant1_Changed");
        data = static_cast<SharedData*>(MapViewOfFile(file, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData)));
        if (!mutex || !changed || !data || !lock())
        {
            close();
            return false;
        }
        *data = {};
        data->active = TRUE;
        ResetEvent(changed);
        ReleaseMutex(mutex);
        return true;
    }

    bool open()
    {
        file = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"Local\\Lab6_Variant1_Memory");
        mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, L"Local\\Lab6_Variant1_Mutex");
        changed = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, L"Local\\Lab6_Variant1_Changed");
        if (file) data = static_cast<SharedData*>(MapViewOfFile(file, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData)));
        if (file && mutex && changed && data) return true;
        close();
        return false;
    }

    bool send(int client, const std::wstring& text)
    {
        if (!data || client < 0 || client > 1 || text.size() >= 512 || !lock()) return false;
        bool sent = data->active != FALSE;
        if (sent)
        {
            wcscpy_s(data->message[client], text.c_str());
            ++data->revision[client];
            sent = SetEvent(changed) != FALSE;
        }
        ReleaseMutex(mutex);
        return sent;
    }

    bool read(SharedData& snapshot)
    {
        if (!data || !lock()) return false;
        snapshot = *data;
        ReleaseMutex(mutex);
        return true;
    }

    bool stop()
    {
        if (!data) return true;
        if (!lock()) return false;
        data->active = FALSE;
        ReleaseMutex(mutex);
        close();
        return true;
    }
};
