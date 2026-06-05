#pragma once

#ifdef _WIN32

#include <windows.h>

#include <vector>

#include "../Color.h"
#include "Platform.h"

class Platform_Win32 : public Platform{
public:
    Platform_Win32();
    ~Platform_Win32() override;

    bool OpenWindow(
        int width,
        int height,
        const char* title, bool PrimaryWindow
    ) override;

    void CloseWindow() override;

    void* GetNativeHandle() const override;
    void* GetInstanceHandle() const;
    void Present() override;
    bool PollEvent(
        Event& event
    ) override;
    void SetTopBarHeight(DragZone pixels) override { topBarHeight = pixels;};
    int GetWidth() const override;
    void Present(const std::vector<Color>& pixels);
    int GetHeight() const override;

    bool IsRunning() const override;
    void SetMinimumSize(int width, int height) override;

    void MinimizeWindow() override;

    void MaximizeOrRestoreWindow() override;
    bool Is_WindowZoomed() const override;
public:
    bool Primary =false;
    DragZone topBarHeight{};
    bool pendingResize = false;
    bool running = false;


    int windowWidth = 0;
    int windowHeight = 0;

private:

    HWND hwnd = nullptr;
    HINSTANCE instance = nullptr;

};

#endif