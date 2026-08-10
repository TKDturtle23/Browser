#include "EventDispatcher.h"

#include <iostream>

bool EventDispatcher::DispatchEvents(WindowManager& wm) {
    Event event;
    bool polledAnyEvent = false;

    while (wm.platform->PollEvent(event)) {
        polledAnyEvent = true;

        bool isMouseEvent = (event.type == EventType::MouseButtonPress  ||
                             event.type == EventType::MouseButtonRelease ||
                             event.type == EventType::MouseMove);

        bool hitUITopBar = isMouseEvent && (event.y < TOP_WIDTH);

        if (event.type == EventType::Resize) {
            wm.renderer->Resize(event.width, event.height);
            wm.ui_manager->Resize(event.width, TOP_WIDTH);
            wm.tabs[wm.activeTabIndex].manager->Resize(event.width, event.height - TOP_WIDTH);
            wm.tabs[wm.activeTabIndex].manager->Update();
            wm.platform->needsRedraw = true;
        }
        else if (event.type == EventType::KeyPress) {
            if (event.key == Key::LShift || event.key == Key::RShift) {
                wm.ShiftPressed = true;
                wm.tabs[wm.activeTabIndex].manager->SetShiftHeld(true);
            }
            else if (event.key == Key::LCtrl || event.key == Key::RCtrl) {
                wm.tabs[wm.activeTabIndex].manager->SetCtrlHeld(true);
            }

            if (event.key == Key::F12) {
                if (wm.debugWindow->IsOpen()) {
                    wm.debugWindow->Close();
                } else {
                    wm.debugWindow->Open();
                    wm.FeedDebugDOM();
                }
            }

            wm.ui_manager->InjectKeyChar(event.key, wm.ShiftPressed);
            wm.platform->needsRedraw = true;
        }
        else if (event.type == EventType::KeyRelease) {
            if (event.key == Key::LShift || event.key == Key::RShift) {
                wm.ShiftPressed = false;
                wm.tabs[wm.activeTabIndex].manager->SetShiftHeld(false);
            } else if (event.key == Key::LCtrl || event.key == Key::RCtrl) {
                wm.tabs[wm.activeTabIndex].manager->SetCtrlHeld(false);
            }
        }
        else if (isMouseEvent) {
            if (hitUITopBar) {
                if (event.type == EventType::MouseButtonPress  && event.button == 1) {
                    wm.ui_manager->InjectMouseButton(true);
                }

                if (event.type == EventType::MouseButtonRelease && event.button == 1) {
                    wm.ui_manager->InjectMouseButton(false);
                }
                if (event.type == EventType::MouseMove) {
                    wm.ui_manager->InjectMouseMove(event.x, event.y);
                    wm.mouse_x = event.x;
                    wm.mouse_y = event.y;
                }
            } else {
                if (event.type == EventType::MouseMove) {
                    if (wm.mouse_y > TOP_WIDTH) {
                        wm.tabs[wm.activeTabIndex].manager->MoveMouse(event.x, event.y - TOP_WIDTH);
                    }
                    wm.mouse_x = event.x;
                    wm.mouse_y = event.y;
                }
                if (event.type == EventType::MouseButtonPress  && event.button == 1)
                    wm.tabs[wm.activeTabIndex].manager->SetMouseClicked(true);
                if (event.type == EventType::MouseButtonRelease && event.button == 1)
                    wm.tabs[wm.activeTabIndex].manager->SetMouseClicked(false);
            }
            wm.platform->needsRedraw = true;
        }
    }

    if (wm.debugWindow->IsOpen()) {
        Event debugEvent;
        while (wm.debugWindow->GetPlatform()->PollEvent(debugEvent)) {
            wm.debugWindow->HandleEvent(debugEvent);
        }
    }

    return polledAnyEvent;
}
