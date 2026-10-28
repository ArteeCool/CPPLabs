// Lab2.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Lab2.h"
#include <string>
#include <cmath>
#include <vector>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

int width = 600;
int height = 600;
float xMin = -6.0f;
float xMax = 6.0f;

int selectedFunction = 0;

double func(double x)
{
    switch (selectedFunction)
    {
    case 0: return x * x;
    case 1: return std::sin(x);
    case 2: return std::cos(x);
    case 3: return x;
    }

    return 0;
}

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_LAB2, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LAB2));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LAB2));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_LAB2);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, width, height, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_CREATE:
        {
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);

            int currentWidth = clientRect.right - clientRect.left;
            int currentHeight = clientRect.bottom - clientRect.top;

            int centerX = currentWidth / 2;
            int centerY = currentHeight / 2;

            HWND comboBox = CreateWindowExW(
                0,
                L"COMBOBOX",
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                CBS_DROPDOWNLIST | WS_VSCROLL,
                20, 20, 250, 200,
                hWnd,
                reinterpret_cast<HMENU>(1001),
                hInst,
                nullptr
            );

            const wchar_t* functions[] = {
                L"x^2",
                L"sin(x)",
                L"cos(x)",
                L"x"
            };

			for (auto function : functions)
			{
				SendMessageW(
					comboBox,
					CB_ADDSTRING,
					0,
					reinterpret_cast<LPARAM>(function)
				);
			}

            HFONT hFont = CreateFont(35, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");

            SendMessageW(
                comboBox,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(hFont),
                TRUE
            );

            return 0;
        }
        case WM_SIZE:
        {
            HWND textField = GetDlgItem(hWnd, 1001);

            if (textField)
            {
                int windowWidth = LOWORD(lParam);
                int windowHeight = HIWORD(lParam);
                int margin = 20;

                int width = windowWidth - margin * 2;
                int height = 40;

                MoveWindow(
                    textField,
                    margin, margin,
                    width > 0 ? width : 0,
                    height > 0 ? height : 0,
                    TRUE
                );
            }

            return 0;
        }
        case WM_COMMAND:
        {
            if (LOWORD(wParam) == 1001 &&
                HIWORD(wParam) == CBN_SELCHANGE)
            {
                HWND comboBox = reinterpret_cast<HWND>(lParam);

                LRESULT selection = SendMessageW(
                    comboBox, CB_GETCURSEL, 0, 0
                );

                if (selection != CB_ERR)
                {
                    selectedFunction = static_cast<int>(selection);
                    InvalidateRect(hWnd, nullptr, TRUE);
                }

                return 0;
            }

            int wmId = LOWORD(wParam);
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
            break;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT clientRect;
			GetClientRect(hWnd, &clientRect);

			int currentWidth = clientRect.right - clientRect.left;
            int currentHeight = clientRect.bottom - clientRect.top;

            const int left = 20, top = 85;
            const int plotWidth = currentWidth - 41;
            const int plotHeight = currentHeight - top - 21;
            if (plotWidth <= 0 || plotHeight <= 0 || xMax <= xMin)
            {
                EndPaint(hWnd, &ps);
                break;
            }

            std::vector<double> values(plotWidth + 1);
            double yMin = 0, yMax = 0;
            for (int i = 0; i <= plotWidth; ++i)
            {
                double normalizedX = static_cast<double>(i) / plotWidth;
                double x = xMin + normalizedX * (xMax - xMin);
                values[i] = func(x);
                if (values[i] < yMin) yMin = values[i];
                if (values[i] > yMax) yMax = values[i];
            }
            double padding = yMax > yMin ? (yMax - yMin) * 0.05 : 1.0;
            yMin -= padding;
            yMax += padding;

            auto toPixel = [&](double normalizedX, double normalizedY) -> POINT {
                return {
                    left + static_cast<LONG>(std::lround(normalizedX * plotWidth)),
                    top + static_cast<LONG>(std::lround((1.0 - normalizedY) * plotHeight))
                };
            };

            POINT origin = toPixel(-xMin / (xMax - xMin), -yMin / (yMax - yMin));
            if (xMin <= 0 && xMax >= 0)
            {
                MoveToEx(hdc, origin.x, top, nullptr);
                LineTo(hdc, origin.x, top + plotHeight);
            }
            MoveToEx(hdc, left, origin.y, nullptr);
            LineTo(hdc, left + plotWidth, origin.y);

            std::vector<POINT> graphPoints(plotWidth + 1);

            HPEN hRedPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hRedPen);

            for (int i = 0; i <= plotWidth; ++i) {
                double normalizedX = static_cast<double>(i) / plotWidth;
                double normalizedY = (values[i] - yMin) / (yMax - yMin);
                graphPoints[i] = toPixel(normalizedX, normalizedY);
            }

            Polyline(hdc, graphPoints.data(), static_cast<int>(graphPoints.size()));

            SelectObject(hdc, hOldPen);
            DeleteObject(hRedPen);

            EndPaint(hWnd, &ps);
            break;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
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
