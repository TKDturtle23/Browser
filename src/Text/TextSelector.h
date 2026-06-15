#pragma once
#include "../Layout/LayoutGenerator.h" // Adjust based on your Layout Box definitions
#include "Window/ViewportManager.h"           // To access IO state definitions safely

namespace Engine::UI {

    class TextSelector {
    public:
        static LayoutBox* SnapToNearestRun(LayoutBox& root, int mx, int my);
        static void UpdateAndApplySelection(LayoutBox& root, const ViewportIO& io, PersistentSelection& selection, LayoutRenderer& layoutRenderer, Platform *platform);
    private:

    };

}