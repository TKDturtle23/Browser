//
// Created by tkdtu on 5/27/2026.
//
#pragma once

#include <vector>
#include <stdint.h>
#pragma once

#include <cstdint>
#include <vector>
#include <memory>

class Platform;
#include <functional>
std::unique_ptr<Platform>
CreatePlatform();
#include "../Color.h"

enum class EventType {
    None,
    Quit,
    Resize,
    KeyPress,
    KeyRelease,
    MouseMove,
    MouseButtonPress,
    MouseButtonRelease,
    MouseWheel,
};
enum class Key {
    Unknown = 0,

    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Numbers
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    // Function Keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // State / Control Keys
    Escape, Space, Return, Backspace, Tab, CapsLock, ScrollLock, NumLock, PrintScreen, Pause,

    // Navigation
    Left, Right, Up, Down,
    Insert, Delete, Home, End, PageUp, PageDown,

    // Modifiers (Note: Win32 mapping notes below)
    LShift, RShift, LCtrl, RCtrl, LAlt, RAlt, LSystem, RSystem, Menu,

    // Numpad
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    NumpadDivide, NumpadMultiply, NumpadSubtract, NumpadAdd, NumpadDecimal, NumpadEnter,

    // Punctuation / Miscellaneous
    Semicolon, Slash, Equal, Hyphen, LBracket, RBracket, Comma, Period, Quote, Backquote, Backslash
};
struct Event {
    EventType type = EventType::None;

    int x = 0;              // Mouse X coordinate
    int y = 0;              // Mouse Y coordinate
    int button = 0;         // 1 = Left, 2 = Right, 3 = Middle
    int WheelDelta = 0;
    int width = 0;
    int height = 0;

    Key key = Key::Unknown;       // Virtual Key Code (e.g., VK_ESCAPE, VK_SPACE)

};
struct DragZone {
    int x, y, width, height;
};
class Platform {
public:
    virtual ~Platform() = default;
    virtual void SetMinimumSize(int width, int height) = 0;
    virtual bool OpenWindow(
        int width,
        int height,
        const char* title, bool PrimaryWindow // Primary window needs to be the last window to close
    ) = 0;

    virtual void CloseWindow() = 0;

    virtual void Present(
        const std::vector<Color>& pixels
    ) = 0;

    virtual bool PollEvent(
        Event& event
    ) = 0;
    virtual void SetTopBarHeight(DragZone pixels) = 0;
    virtual int GetWidth() const = 0;

    virtual int GetHeight() const = 0;

    virtual bool IsRunning() const = 0;

    virtual void MinimizeWindow() = 0;
    virtual bool Is_WindowZoomed() const = 0;
    virtual void MaximizeOrRestoreWindow() = 0;
    bool resizing = false;
    bool needsRedraw = false;
    std::function<void()> onRender;
    int min_width = 0, min_height = 0;
protected:

};
