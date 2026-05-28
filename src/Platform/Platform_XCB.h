#pragma once

#include "Platform.h"

#ifdef __linux__

#include <xcb/xcb.h>

class Platform_XCB : public Platform {
public:
    Platform_XCB() = default;
    ~Platform_XCB() override;

    bool OpenWindow(
        int width,
        int height,
        const char* title
    ) override;

    void CloseWindow() override;

    void Present(
        const std::vector<Color>& pixels
    ) override;

    bool PollEvent(
        Event& event
    ) override;

    int GetWidth() const override;
    int GetHeight() const override;

    bool IsRunning() const override;

private:
    xcb_connection_t* connection = nullptr;
    xcb_screen_t* screen = nullptr;

    xcb_window_t window = 0;
    xcb_gcontext_t graphicsContext = 0;

    int windowWidth = 0;
    int windowHeight = 0;

    bool running = true;
};

#endif