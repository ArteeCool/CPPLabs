#include "framework.h"
#include "Worker.h"
#include "Lab3.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

HWND g_hwnd;
HANDLE g_redrawEvent;
std::vector<std::shared_ptr<Worker>> g_workers;

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void                CreateWorker(POINT click_point);
void                EndClosestWorker(POINT click_point);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_LAB3, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LAB3));
    MSG msg;

    g_redrawEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    // Main message loop:
    while (true)
    {
        DWORD dwWait = MsgWaitForMultipleObjects(1, &g_redrawEvent, FALSE, INFINITE, QS_ALLINPUT);

        if (dwWait == WAIT_OBJECT_0) {
            if (g_hwnd) {
                InvalidateRect(g_hwnd, nullptr, TRUE);
            }
        }
        else if (dwWait == WAIT_OBJECT_0 + 1) {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    CloseHandle(g_redrawEvent);
                    return (int)msg.wParam;
                }
                if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }
    }

    return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LAB3));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_LAB3);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd) {
      return FALSE;
   }

   g_hwnd = hWnd;

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

    case WM_LBUTTONDOWN: {
        POINT mouse_pos;
        mouse_pos.x = LOWORD(lParam);
        mouse_pos.y = HIWORD(lParam);

        CreateWorker(mouse_pos);
        InvalidateRect(hWnd, nullptr, TRUE);
        break;
    }
    case WM_RBUTTONDOWN: {
        POINT mouse_pos;
        mouse_pos.x = LOWORD(lParam);
        mouse_pos.y = HIWORD(lParam);

        EndClosestWorker(mouse_pos);
        InvalidateRect(hWnd, nullptr, TRUE);
        break;
    }
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0, 0, 0));

            for (const auto& worker : g_workers) {
                POINT worker_point = worker->GetClickPoint();
                RECT drawRect{
                    .left = worker_point.x,
                    .top = worker_point.y,
                    .right = worker_point.x + 200,
                    .bottom = worker_point.y + 30
                };

                std::string text = std::to_string(worker->GetCounter()) + " | x: " + std::to_string(worker_point.x) + ", y: " + std::to_string(worker_point.y);
                DrawTextA(hdc, text.c_str(), -1, &drawRect, DT_LEFT | DT_WORDBREAK);
            }
            
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void CreateWorker(POINT click_point) {
    std::shared_ptr<Worker> worker = Worker::CreateWorker(click_point);
    g_workers.push_back(worker);
}

void EndClosestWorker(POINT click_point) {
    std::shared_ptr<Worker> target_worker = nullptr;
    double closest_distance = std::numeric_limits<double>().max();
    for (const auto& worker : g_workers) {
        POINT worker_cpoint = worker->GetClickPoint();
        double distance = std::hypot(worker_cpoint.x - click_point.x, worker_cpoint.y - click_point.y);
        if (distance < closest_distance) {
            closest_distance = distance;
            target_worker = worker;
        }
    }

    if (target_worker) {
        target_worker->ExitThread();
        std::erase(g_workers, target_worker);
    }
}