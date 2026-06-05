//
// Created by tkdtu on 6/4/2026.
//

#ifndef BROWSER_OPENGLBACKEND_H
#define BROWSER_OPENGLBACKEND_H

#include <unordered_map>
#include <vector>

#include "ImageWindow.h"
#include <glad/wgl.h>

#include "Platform/IGLContext.h"
#include "Render/Backend/IRendererBackend.h"
struct Vertex2D {
    float x, y;       // Position
    float r, g, b, a; // Color
    float u, v;       // Texture Coordinates (UVs)
};
class OpenGLBackend : public IRenderBackend {
public:
    OpenGLBackend();
    ~OpenGLBackend() override;

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
    TextureID CreateFontAtlas(int width, int height) override;

    void UpdateTextureSubImage(TextureID texture, int x, int y,
                                       int width, int height,
                                       const uint8_t* pixels) override;
private:
    HGLRC primaryRC;
    // Core hardware structures mirroring your software architecture
    struct OpenGLWindow {
        Platform *platform;
        HDC hdc;   // owns or borrows depending on lifetime contract
        RenderTargetID target = 0;
    };

    struct OpenGLRenderTarget {
        int width;
        int height;
        GLuint fbo;          // Framebuffer Object Handle
        GLuint colorTexture; // Color Attachment Texture Handle
    };

    // Translation dispatcher for specific primitives
    void ExecuteCommand(const RenderCommand& cmd);

    // Asset and Instance Maps
    std::unordered_map<WindowID, OpenGLWindow> windows;
    std::unordered_map<RenderTargetID, OpenGLRenderTarget> renderTargets;
    std::vector<RenderCommand> commandBuffer;

    RenderTargetID nextTargetID = 1;
    WindowID nextWindowID = 1;

    GLuint batchVAO = 0;
    GLuint batchVBO = 0;
    std::vector<Vertex2D> batchVertices;
    GLuint batchShader = 0;

    const size_t MAX_BATCH_VERTICES = 60000; // Allows ~10,000 rectangles per single draw call

    void InitBatchPipeline();
    void InitRenderState();
    void BuildOrthoProjection(float* m16, float width, float height);
    void FlushBatch();
    void PushQuad(float x, float y, float w, float h, Color color, GLuint textureHandle, float u0, float v0, float u1, float v1);
    GLuint circleShader = 0;
    GLuint whiteTexture = 0;       // 1x1 fallback texture for solid shapes
    GLuint currentBoundTexture = 0; // Tracks what texture the current batch is using

    std::unordered_map<TextureID, GLuint> nativeTextures;
    TextureID nextTextureID = 1;
    HGLRC hRC = nullptr; // Shared core rendering context pointer instance
    GLuint activeShaderOverride = 0;

    GLint  batchShaderProjLoc  = -1;
    GLint  circleShaderProjLoc = -1;
    GLint  circleShaderMaskLoc = -1;
    float  activeMask[4]       = { -1.f, -1.f, 1.f, 1.f };

};

#endif //BROWSER_OPENGLBACKEND_H