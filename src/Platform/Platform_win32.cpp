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

#endif
