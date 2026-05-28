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
    MouseButtonRelease
};

struct Event {
    EventType type = EventType::None;

    int x = 0;
    int y = 0;

    int width = 0;
    int height = 0;

    uint32_t key = 0;
};

class Platform {
public:
    virtual ~Platform() = default;

    virtual bool OpenWindow(
        int width,
        int height,
        const char* title
    ) = 0;

    virtual void CloseWindow() = 0;

    virtual void Present(
        const std::vector<Color>& pixels
    ) = 0;

    virtual bool PollEvent(
        Event& event
    ) = 0;

    virtual int GetWidth() const = 0;

    virtual int GetHeight() const = 0;

    virtual bool IsRunning() const = 0;

    bool resizing = false;
    bool needsRedraw = false;
    std::function<void()> onRender;
};
