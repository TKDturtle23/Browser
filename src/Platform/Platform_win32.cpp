#include "Platform_Win32.h"

#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// Windows 11 rounded corner constants
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

enum DWM_WINDOW_CORNER_PREFERENCE {
    DWMWCP_DEFAULT      = 0,
    DWMWCP_DONOTROUND   = 1,
    DWMWCP_ROUND        = 2, // Standard Windows 11 Rounding
    DWMWCP_ROUNDSMALL   = 3  // Slight Rounding (like tool windows)
};

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
            if (platform->Primary) {
                PostQuitMessage(0);
            }


            return 0;
        }
        case WM_NCCALCSIZE: {
            // If wParam is TRUE, we can specify the client area.
            // Returning 0 tells Windows the client area covers the whole window,
            // which completely removes the native title bar and that pesky white line.
            if (wParam == TRUE) {
                return 0;
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        case WM_ENTERSIZEMOVE: {
            if (platform) {
                platform->resizing = true;
            }
            SetTimer(hwnd, 1, 16, nullptr); // ~60 FPS
            return 0;
        }
        case WM_NCHITTEST: {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);

            POINT pt = { xPos, yPos };
            ScreenToClient(hwnd, &pt);

            const int BORDER_WIDTH = 5;

            if (platform) {
                // 1. Keep your edge/corner resizing logic exactly the same
                bool isLeft   = (pt.x < BORDER_WIDTH);
                bool isRight  = (pt.x >= platform->windowWidth - BORDER_WIDTH);
                bool isTop    = (pt.y < BORDER_WIDTH);
                bool isBottom = (pt.y >= platform->windowHeight - BORDER_WIDTH);

                if (isTop && isLeft)     return HTTOPLEFT;
                if (isTop && isRight)    return HTTOPRIGHT;
                if (isBottom && isLeft)  return HTBOTTOMLEFT;
                if (isBottom && isRight) return HTBOTTOMRIGHT;
                if (isLeft)   return HTLEFT;
                if (isRight)  return HTRIGHT;
                if (isTop)    return HTTOP;
                if (isBottom) return HTBOTTOM;



                // 3. Check if the mouse is inside any of the allowed drag zones

                    if (pt.x >= platform->topBarHeight.x && pt.x < (platform->topBarHeight.x + platform->topBarHeight.width) &&
                        pt.y >= platform->topBarHeight.y && pt.y < (platform->topBarHeight.y + platform->topBarHeight.height)) {
                        return HTCAPTION; // Drag the window!
                        }

            }

            // 4. Anything else (including your tabs) behaves like normal UI
            return HTCLIENT;
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
        case WM_GETMINMAXINFO:
        {
            if (platform) {
                // Cast lParam to the MINMAXINFO structure
                LPMINMAXINFO lpMMI = reinterpret_cast<LPMINMAXINFO>(lParam);

                // Set the minimum width and height (in pixels)
                lpMMI->ptMinTrackSize.x = platform->min_width; // Minimum width
                lpMMI->ptMinTrackSize.y = platform->min_height; // Minimum height
                return 0; // Return 0 to indicate we handled the message
            }


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

Platform_Win32::Platform_Win32() {
    min_width = 100;
    min_height = 100;
}

Platform_Win32::~Platform_Win32() {
    Platform_Win32::CloseWindow();
}

bool Platform_Win32::OpenWindow(
    int width,
    int height,
    const char* title, bool PrimaryWindow
) {
Primary = PrimaryWindow;

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
    // Use WS_POPUP | WS_THICKFRAME to allow resizing but remove the native title bar
    DWORD windowStyle = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    RegisterClass(&wc);

    hwnd = CreateWindowEx(
        0,
        className,
        title,
        windowStyle,
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
    DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(
        hwnd,
        DWMWA_WINDOW_CORNER_PREFERENCE,
        &preference,
        sizeof(preference)
    );
    SetWindowLongPtr(
        hwnd,
        GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(this)
    );



    ShowWindow(hwnd, SW_SHOW);

    running = true;

    return true;
}

void Platform_Win32::CloseWindow() {

    if (hwnd) {

        DestroyWindow(hwnd);

        hwnd = nullptr;
    }

    running = false;
}

void* Platform_Win32::GetNativeHandle() const
{
    return hwnd;
}

void Platform_Win32::Present()
{

}


static Key TranslateWin32Key(WPARAM wParam) {
    // 1. Standard letters (Windows maps these directly to uppercase ASCII values)
    if (wParam >= 'A' && wParam <= 'Z') {
        return static_cast<Key>(static_cast<int>(Key::A) + (wParam - 'A'));
    }

    // 2. Standard top-row numbers
    if (wParam >= '0' && wParam <= '9') {
        return static_cast<Key>(static_cast<int>(Key::Num0) + (wParam - '0'));
    }

    // 3. Function Keys (F1 - F12)
    if (wParam >= VK_F1 && wParam <= VK_F12) {
        return static_cast<Key>(static_cast<int>(Key::F1) + (wParam - VK_F1));
    }

    // 4. Numpad Numbers (0 - 9)
    if (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9) {
        return static_cast<Key>(static_cast<int>(Key::Numpad0) + (wParam - VK_NUMPAD0));
    }

    // 5. Special and explicit key mappings
    switch (wParam) {
        // Core Control Keys
        case VK_ESCAPE:   return Key::Escape;
        case VK_SPACE:    return Key::Space;
        case VK_RETURN:   return Key::Return;
        case VK_BACK:     return Key::Backspace;
        case VK_TAB:      return Key::Tab;
        case VK_CAPITAL:  return Key::CapsLock;
        case VK_SCROLL:   return Key::ScrollLock;
        case VK_NUMLOCK:  return Key::NumLock;
        case VK_SNAPSHOT: return Key::PrintScreen;
        case VK_PAUSE:    return Key::Pause;

        // Navigation
        case VK_LEFT:     return Key::Left;
        case VK_RIGHT:    return Key::Right;
        case VK_UP:       return Key::Up;
        case VK_DOWN:     return Key::Down;
        case VK_INSERT:   return Key::Insert;
        case VK_DELETE:   return Key::Delete;
        case VK_HOME:     return Key::Home;
        case VK_END:      return Key::End;
        case VK_PRIOR:    return Key::PageUp;
        case VK_NEXT:     return Key::PageDown;

        // Modifiers
        // Note: Raw WM_KEYDOWN usually sends generic VK_SHIFT/VK_CONTROL/VK_MENU.
        // If your Windows message loop intercepts specific left/right virtual keys, these catch them:
        case VK_SHIFT:    return Key::LShift;
        case VK_LSHIFT:   return Key::LShift;
        case VK_RSHIFT:   return Key::RShift;
        case VK_CONTROL:  return Key::LCtrl;
        case VK_LCONTROL: return Key::LCtrl;
        case VK_RCONTROL: return Key::RCtrl;
        case VK_MENU:     return Key::LAlt; // VK_MENU is Alt
        case VK_LMENU:    return Key::LAlt;
        case VK_RMENU:    return Key::RAlt;
        case VK_LWIN:     return Key::LSystem;
        case VK_RWIN:     return Key::RSystem;
        case VK_APPS:     return Key::Menu;

        // Numpad Math
        case VK_DIVIDE:   return Key::NumpadDivide;
        case VK_MULTIPLY: return Key::NumpadMultiply;
        case VK_SUBTRACT: return Key::NumpadSubtract;
        case VK_ADD:      return Key::NumpadAdd;
        case VK_DECIMAL:  return Key::NumpadDecimal;

        // Punctuation & OEM Keys (US Standard Layout defaults)
        case VK_OEM_1:      return Key::Semicolon; // ';:'
        case VK_OEM_2:      return Key::Slash;     // '/?'
        case VK_OEM_3:      return Key::Backquote; // '`~'
        case VK_OEM_4:      return Key::LBracket;  // '[{'
        case VK_OEM_5:      return Key::Backslash; // '\|'
        case VK_OEM_6:      return Key::RBracket;  // ']}'
        case VK_OEM_7:      return Key::Quote;     // ''"'
        case VK_OEM_PLUS:   return Key::Equal;     // '=+'
        case VK_OEM_MINUS:  return Key::Hyphen;    // '-_'
        case VK_OEM_COMMA:  return Key::Comma;     // ',<'
        case VK_OEM_PERIOD: return Key::Period;    // '.>'

        default:            return Key::Unknown;
    }
}
bool Platform_Win32::PollEvent(Event& event) {
    // Check for pending resize from WindowProc first
    if (pendingResize) {
        pendingResize = false;
        event.type   = EventType::Resize;
        event.width  = windowWidth;
        event.height = windowHeight;
        return true;
    }

    MSG msg;
    // Internal loop to bypass unhandled messages (like WM_TIMER or WM_PAINT)
    // without spamming EventType::None back to the application loop.
    while (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        switch (msg.message) {
            case WM_QUIT: {
                event.type = EventType::Quit;
                running = false;
                return true;
            }

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN: {
                WPARAM wp = msg.wParam;
                uint32_t scancode = (msg.lParam & 0x00ff0000) >> 16;
                bool isExtended = (msg.lParam & 0x01000000) != 0;

                // Distinguish left and right modifiers explicitly
                if (wp == VK_SHIFT) {
                    wp = MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX);
                } else if (wp == VK_CONTROL) {
                    wp = isExtended ? VK_RCONTROL : VK_LCONTROL;
                } else if (wp == VK_MENU) {
                    wp = isExtended ? VK_RMENU : VK_LMENU;
                }

                Key myKey = TranslateWin32Key(wp);
                event.type = EventType::KeyPress;
                event.key = myKey;
                return true;
            }

            case WM_KEYUP:
            case WM_SYSKEYUP: {
                WPARAM wp = msg.wParam;
                uint32_t scancode = (msg.lParam & 0x00ff0000) >> 16;
                bool isExtended = (msg.lParam & 0x01000000) != 0;

                // Distinguish left and right modifiers explicitly
                if (wp == VK_SHIFT) {
                    wp = MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX);
                } else if (wp == VK_CONTROL) {
                    wp = isExtended ? VK_RCONTROL : VK_LCONTROL;
                } else if (wp == VK_MENU) {
                    wp = isExtended ? VK_RMENU : VK_LMENU;
                }

                Key myKey = TranslateWin32Key(wp);

                event.type = EventType::KeyRelease;
                event.key = myKey;
                return true;
            }
            case WM_MOUSEWHEEL: {
                event.type = EventType::MouseWheel;

                // Extraction requires pulling the high-word from wParam
                // and casting it to a short to preserve negative (downward) values.
                short rawDelta = GET_WHEEL_DELTA_WPARAM(msg.wParam);

                // Convert to a clean notch multiplier (e.g., +1 for up, -1 for down)
                event.WheelDelta = static_cast<int>(rawDelta) / WHEEL_DELTA;

                return true;
            }
            case WM_MOUSEMOVE: {
                event.type = EventType::MouseMove;
                event.x = GET_X_LPARAM(msg.lParam);
                event.y = GET_Y_LPARAM(msg.lParam);
                return true;
            }

            case WM_LBUTTONDOWN: {
                event.type = EventType::MouseButtonPress;
                event.button = 1;
                event.x = GET_X_LPARAM(msg.lParam);
                event.y = GET_Y_LPARAM(msg.lParam);
                return true;
            }
            case WM_RBUTTONDOWN: {
                event.type = EventType::MouseButtonPress;
                event.button = 2;
                event.x = GET_X_LPARAM(msg.lParam);
                event.y = GET_Y_LPARAM(msg.lParam);
                return true;
            }
            case WM_MBUTTONDOWN: {
                event.type = EventType::MouseButtonPress;
                event.button = 3;
                event.x = GET_X_LPARAM(msg.lParam);
                event.y = GET_Y_LPARAM(msg.lParam);
                return true;
            }

            case WM_LBUTTONUP: {
                event.type = EventType::MouseButtonRelease;
                event.button = 1;
                event.x = GET_X_LPARAM(msg.lParam);
                event.y = GET_Y_LPARAM(msg.lParam);
                return true;
            }
            case WM_RBUTTONUP: {
                event.type = EventType::MouseButtonRelease;
                event.button = 2;
                event.x = GET_X_LPARAM(msg.lParam);
                event.y = GET_Y_LPARAM(msg.lParam);
                return true;
            }
            case WM_MBUTTONUP: {
                event.type = EventType::MouseButtonRelease;
                event.button = 3;
                event.x = GET_X_LPARAM(msg.lParam);
                event.y = GET_Y_LPARAM(msg.lParam);
                return true;
            }
        }
    }

    return false;
}

int Platform_Win32::GetWidth() const {
    return windowWidth;
}


void Platform_Win32::Present(const std::vector<Color>& pixels) {
    // This serves exclusively as your Tier 3 CPU Software Renderer presentation path
    if (hwnd == nullptr) return;

    // Get a temporary device context handle on the fly
    HDC localHDC = GetDC(hwnd);
    if (!localHDC) return;

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int clientW = clientRect.right  - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;

    BITMAPINFO localBitmapInfo = {};
    localBitmapInfo.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    localBitmapInfo.bmiHeader.biWidth       = windowWidth;  // Match source vector width
    localBitmapInfo.bmiHeader.biHeight      = -windowHeight; // Top-down parsing
    localBitmapInfo.bmiHeader.biPlanes      = 1;
    localBitmapInfo.bmiHeader.biBitCount    = 32;
    localBitmapInfo.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(
        localHDC,
        0, 0, clientW, clientH,
        0, 0, windowWidth, windowHeight,
        pixels.data(),
        &localBitmapInfo,
        DIB_RGB_COLORS,
        SRCCOPY
    );

    ReleaseDC(hwnd, localHDC);
}
int Platform_Win32::GetHeight() const {
    return windowHeight;
}

bool Platform_Win32::IsRunning() const {
    return running;
}

void Platform_Win32::SetMinimumSize(int width, int height) {
min_width = width;
    min_height = height;
}
void Platform_Win32::MinimizeWindow() {
    ShowWindow(hwnd, SW_MINIMIZE);
}

void Platform_Win32::MaximizeOrRestoreWindow() {
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(hwnd, &wp);

    if (wp.showCmd == SW_SHOWMAXIMIZED) {
        ShowWindow(hwnd, SW_RESTORE);
    } else {
        ShowWindow(hwnd, SW_MAXIMIZE);
    }
}

bool Platform_Win32::Is_WindowZoomed() const {
    return IsZoomed(hwnd);
}
void* Platform_Win32::GetInstanceHandle() const {
    return instance;
}
#endif
