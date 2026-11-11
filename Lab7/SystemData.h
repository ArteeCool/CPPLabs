#pragma once
#include <windows.h>
#include <shobjidl.h>
#include <string>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

inline std::wstring ClientData(int client)
{
    HDC dc = GetDC(nullptr);
    if (!dc) return L"";
    wchar_t text[1024];
    if (client == 1)
    {
        using GetVersion = LONG(WINAPI*)(OSVERSIONINFOW*);
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        auto getVersion = reinterpret_cast<GetVersion>(GetProcAddress(ntdll, "RtlGetVersion"));
        OSVERSIONINFOW version = {};
        version.dwOSVersionInfoSize = sizeof(version);
        std::wstring versionText = L"недоступна";
        if (getVersion && getVersion(&version) == 0)
            versionText = L"Windows NT " + std::to_wstring(version.dwMajorVersion) + L"." +
                std::to_wstring(version.dwMinorVersion) + L", збірка " + std::to_wstring(version.dwBuildNumber);
        swprintf_s(text, L"Клієнт 1\r\nВерсія ОС: %s\r\nКількість моніторів: %d\r\n"
            L"Глибина кольору (BITSPIXEL): %d біт/піксель\r\n"
            L"Ознака Windows Server 2003 R2 (SM_SERVERR2): %d",
            versionText.c_str(), GetSystemMetrics(SM_CMONITORS),
            GetDeviceCaps(dc, BITSPIXEL), GetSystemMetrics(SM_SERVERR2));
    }
    else
    {
        std::wstring colorText = L"недоступний";
        HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        IDesktopWallpaper* wallpaper = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_DesktopWallpaper, nullptr, CLSCTX_ALL,
            IID_PPV_ARGS(&wallpaper))))
        {
            COLORREF color;
            if (SUCCEEDED(wallpaper->GetBackgroundColor(&color)))
                colorText = L"RGB(" + std::to_wstring(GetRValue(color)) + L", " +
                    std::to_wstring(GetGValue(color)) + L", " + std::to_wstring(GetBValue(color)) + L")";
            wallpaper->Release();
        }
        if (SUCCEEDED(initialized)) CoUninitialize();
        swprintf_s(text, L"Клієнт 2\r\nКолір фону робочого столу: %s\r\n"
            L"Розмір іконки (SM_CXICON, SM_CYICON): %d x %d пкс\r\n"
            L"Горизонтальний DPI (LOGPIXELSX): %d\r\n"
            L"Ширина екрана (SM_CXSCREEN): %d пкс",
            colorText.c_str(), GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON),
            GetDeviceCaps(dc, LOGPIXELSX), GetSystemMetrics(SM_CXSCREEN));
    }
    ReleaseDC(nullptr, dc);
    return text;
}
