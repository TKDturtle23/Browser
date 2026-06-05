//
// Created by tkdtu on 6/4/2026.
//

#ifndef BROWSER_SOFTWAREBACKEND_H
#define BROWSER_SOFTWAREBACKEND_H
#include <unordered_map>
#include <vector>

#include "Color.h"
#include "Render/Backend/IRendererBackend.h"
#include "Render/Backend/OpenGL/Platform/IGLContext.h"

struct SoftwareRenderTarget {

     int width;
     int height;

     std::vector<Color> frontBuffer;
     std::vector<Color> backBuffer;
};
struct SoftwareWindow {

    Platform* platform = nullptr;

    RenderTargetID target =
        InvalidRenderTarget;
};

class SoftwareBackend
    : public IRenderBackend
{
public:

    RenderTargetID CreateRenderTarget(
        int width,
        int height
    ) override;

    void DestroyRenderTarget(
        RenderTargetID target
    ) override;

    void ResizeRenderTarget(
        RenderTargetID target,
        int width,
        int height
    ) override;

    WindowID RegisterWindow(Platform* context
    ) override;

    void UnregisterWindow(
        WindowID window
    ) override;

    void AttachRenderTarget(
        WindowID window,
        RenderTargetID target
    ) override;

    void BeginFrame() override;

    void SubmitCommand(
        const RenderCommand& cmd
    ) override;

    void EndFrame() override;

    void Present() override;
    void UpdateTextureSubImage(TextureID, int, int, int, int, const uint8_t*) override;
    TextureID CreateFontAtlas(int, int) override;
private:

    void ExecuteCommand(
        const RenderCommand& cmd
    );


private:

    std::vector<RenderCommand>
    commandBuffer;

    std::unordered_map<
        RenderTargetID,
        SoftwareRenderTarget
    > renderTargets;

    std::unordered_map<
        WindowID,
        SoftwareWindow
    > windows;

    RenderTargetID nextTargetID = 1;

    WindowID nextWindowID = 1;
};

#endif //BROWSER_SOFTWAREBACKEND_H
