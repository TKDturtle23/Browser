#pragma once
#include "Window/WindowManager.h"

class EventDispatcher {
public:
    static bool DispatchEvents(WindowManager& wm);
};
