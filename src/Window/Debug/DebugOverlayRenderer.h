#pragma once
#include <memory>
#include "Parser.h"
#include "Render/Renderer.h"

class DebugOverlayRenderer {
public:
    static void DrawBoxModel(RendererSurface& renderer, const Node* node, int topBarHeight);
};