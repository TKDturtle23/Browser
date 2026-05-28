#include "Platform_Win32.h"

#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>

static LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
) {

    Platform_Win32* platform =
        reinterpret_cast<Platform_Win32*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA)
        );

    switch (uMsg) {

        case WM_CLOSE:
        case WM_DESTROY: {

            if (platform) {
                platform->running = false;
            }

            PostQuitMessage(0);

            return 0;
        }
        case WM_ENTERSIZEMOVE: {
            if (platform) {
                platform->resizing = true;
            }
            SetTimer(hwnd, 1, 16, nullptr); // ~60 FPS
            return 0;
        }

        case WM_EXITSIZEMOVE: {
            if (platform) {
                platform->resizing = false;
            }
            KillTimer(hwnd, 1);
            return 0;
        }
        case WM_SIZE: {
            if (platform) {
                if (wParam == SIZE_MINIMIZED)
                    return 0;

                platform->windowWidth  = LOWORD(lParam);
                platform->windowHeight = HIWORD(lParam);
                platform->pendingResize = true;
                platform->needsRedraw = true;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_TIMER: {
            if (platform && platform->resizing) {

                RECT rect;
                GetClientRect(hwnd, &rect);

                platform->windowWidth  = rect.right;
                platform->windowHeight = rect.bottom;

                platform->pendingResize = true;

                // FORCE render here (bypasses your blocked loop)
                if (platform->onRender) {
                    platform->onRender();  // you must provide this callback
                }
            }
            return 0;
        }
        case WM_PAINT: {
            if (platform) {
                platform->needsRedraw = true;
            }

            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProc(
                hwnd,
                uMsg,
                wParam,
                lParam
            );
    }
}

Platform_Win32::~Platform_Win32() {
    CloseWindow();
}

bool Platform_Win32::OpenWindow(
    int width,
    int height,
    const char* title
) {

    windowWidth = width;
    windowHeight = height;
    // Adjust so the CLIENT area is exactly width x height
    RECT rect = { 0, 0, width, height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    int adjustedWidth  = rect.right  - rect.left;
    int adjustedHeight = rect.bottom - rect.top;
    instance =
        GetModuleHandle(nullptr);

    const char* className =
        "SoftwareRendererWindow";

    WNDCLASS wc = {};

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClass(&wc);

    hwnd = CreateWindowEx(
        0,
        className,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        adjustedWidth,   // use adjusted size here
        adjustedHeight,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!hwnd) {
        return false;
    }

    SetWindowLongPtr(
        hwnd,
        GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(this)
    );

    hdc = GetDC(hwnd);

    bitmapInfo.bmiHeader.biSize =
        sizeof(BITMAPINFOHEADER);

    bitmapInfo.bmiHeader.biWidth =
        width;

    bitmapInfo.bmiHeader.biHeight =
        -height;

    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression =
        BI_RGB;

    ShowWindow(hwnd, SW_SHOW);

    running = true;

    return true;
}

void Platform_Win32::CloseWindow() {

    if (hdc) {

        ReleaseDC(hwnd, hdc);

        hdc = nullptr;
    }

    if (hwnd) {

        DestroyWindow(hwnd);

        hwnd = nullptr;
    }

    running = false;
}

void Platform_Win32::Present(
    const std::vector<Color>& pixels
) {

    if (!hdc)
        return;

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int clientW = clientRect.right  - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;

    bitmapInfo.bmiHeader.biWidth  =  clientW;
    bitmapInfo.bmiHeader.biHeight = -clientH;

    StretchDIBits(
        hdc,
        0, 0, clientW, clientH,
        0, 0, windowWidth, windowHeight,
        pixels.data(),
        &bitmapInfo,
        DIB_RGB_COLORS,
        SRCCOPY
    );


}

bool Platform_Win32::PollEvent(
    Event& event
) {
    // Check for pending resize from WindowProc first
    if (pendingResize) {
        pendingResize = false;
        event.type   = EventType::Resize;
        event.width  = windowWidth;
        event.height = windowHeight;
        return true;
    }
    MSG msg;

    if (!PeekMessage(
            &msg,
            nullptr,
            0,
            0,
            PM_REMOVE
        )) {
        return false;
    }

    TranslateMessage(&msg);
    DispatchMessage(&msg);

    switch (msg.message) {

        case WM_QUIT: {

            event.type = EventType::Quit;
            running = false;

            break;
        }


        case WM_KEYDOWN: {

            event.type = EventType::KeyPress;
            event.key =
                static_cast<int>(msg.wParam);

            break;
        }

        case WM_KEYUP: {

            event.type = EventType::KeyRelease;
            event.key =
                static_cast<int>(msg.wParam);

            break;
        }

        case WM_MOUSEMOVE: {

            event.type = EventType::MouseMove;

            event.x =
                GET_X_LPARAM(msg.lParam);

            event.y =
                GET_Y_LPARAM(msg.lParam);

            break;
        }

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {

            event.type =
                EventType::MouseButtonPress;

            break;
        }

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {

            event.type =
                EventType::MouseButtonRelease;

            break;
        }

        default:
            event.type = EventType::None;
            break;
    }

    return true;
}

int Platform_Win32::GetWidth() const {
    return windowWidth;
}

int Platform_Win32::GetHeight() const {
    return windowHeight;
}

bool Platform_Win32::IsRunning() const {
    return running;
}

#endif