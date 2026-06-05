//
// Created by tkdtu on 6/4/2026.
//

#include "OpenGLBackend.h"
#include "Platform/Platform_Win32.h"
#include <algorithm>
#include <ranges>
#include <stdexcept>

static void CheckShader(GLuint shader, const char* label) {
#ifdef _DEBUG
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        throw std::runtime_error(std::string(label) + " shader compile error:\n" + log);
    }
#endif
}

static void CheckProgram(GLuint program, const char* label) {
#ifdef _DEBUG
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        throw std::runtime_error(std::string(label) + " program link error:\n" + log);
    }
#endif
}

static GLuint CompileProgram(const char* vsSource, const char* fsSource, const char* label) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSource, nullptr);
    glCompileShader(vs);
    CheckShader(vs, label);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSource, nullptr);
    glCompileShader(fs);
    CheckShader(fs, label);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    CheckProgram(prog, label);

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

OpenGLBackend::OpenGLBackend() {}

OpenGLBackend::~OpenGLBackend() {
    if (hRC) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(hRC);
    }

    for (auto& [id, target] : renderTargets) {
        glDeleteFramebuffers(1, &target.fbo);
        glDeleteTextures(1, &target.colorTexture);
    }

    if (batchVBO)      glDeleteBuffers(1, &batchVBO);
    if (batchVAO)      glDeleteVertexArrays(1, &batchVAO);
    if (batchShader)   glDeleteProgram(batchShader);
    if (circleShader)  glDeleteProgram(circleShader);
    if (whiteTexture)  glDeleteTextures(1, &whiteTexture);

    for (auto& [id, tex] : nativeTextures)
        glDeleteTextures(1, &tex);
}


void OpenGLBackend::InitBatchPipeline() {
    // --- VBO (immutable storage, client-updatable) ---
    glCreateBuffers(1, &batchVBO);
    glNamedBufferStorage(batchVBO, MAX_BATCH_VERTICES * sizeof(Vertex2D),
                         nullptr, GL_DYNAMIC_STORAGE_BIT);

    // --- VAO layout ---
    glCreateVertexArrays(1, &batchVAO);

    glEnableVertexArrayAttrib(batchVAO, 0);
    glVertexArrayAttribFormat(batchVAO, 0, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex2D, x));
    glVertexArrayAttribBinding(batchVAO, 0, 0);

    glEnableVertexArrayAttrib(batchVAO, 1);
    glVertexArrayAttribFormat(batchVAO, 1, 4, GL_FLOAT, GL_FALSE, offsetof(Vertex2D, r));
    glVertexArrayAttribBinding(batchVAO, 1, 0);

    glEnableVertexArrayAttrib(batchVAO, 2);
    glVertexArrayAttribFormat(batchVAO, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex2D, u));
    glVertexArrayAttribBinding(batchVAO, 2, 0);

    glVertexArrayVertexBuffer(batchVAO, 0, batchVBO, 0, sizeof(Vertex2D));

    // --- Standard quad shader (Y-down, origin top-left) ---
    const char* quadVS = R"(
        #version 460 core
        layout(location = 0) in vec2 inPos;
        layout(location = 1) in vec4 inColor;
        layout(location = 2) in vec2 inUV;

        layout(location = 0) out vec4 fragColor;
        layout(location = 1) out vec2 fragUV;

        uniform mat4 uProjection;

        void main() {
            fragColor = inColor;
            fragUV    = inUV;
            gl_Position = uProjection * vec4(inPos, 0.0, 1.0);
        }
    )";

    const char* quadFS = R"(
        #version 460 core
        layout(location = 0) in vec4 fragColor;
        layout(location = 1) in vec2 fragUV;

        layout(location = 0) out vec4 outColor;
        layout(binding  = 0) uniform sampler2D uTexture;

        void main() {
            outColor = fragColor * texture(uTexture, fragUV);
        }
    )";

    batchShader = CompileProgram(quadVS, quadFS, "quad");
    batchShaderProjLoc = glGetUniformLocation(batchShader, "uProjection");

    // --- Circle shader ---
    // UV is passed in as [-1, 1] NDC so we can do an analytic SDF in the FS.
    // Uses the same Y-down projection as the quad shader — no special matrix needed.
    const char* circleVS = R"(
        #version 460 core
        layout(location = 0) in vec2 inPos;
        layout(location = 1) in vec4 inColor;
        layout(location = 2) in vec2 inUV;

        layout(location = 0) out vec4 fragColor;
        layout(location = 1) out vec2 fragUV;

        uniform mat4 uProjection;

        void main() {
            fragColor   = inColor;
            fragUV      = inUV;   // [-1,1] disc space
            gl_Position = uProjection * vec4(inPos, 0.0, 1.0);
        }
    )";

    const char* circleFS = R"(
        #version 460 core
        layout(location = 0) in vec4 fragColor;
        layout(location = 1) in vec2 fragUV;

        layout(location = 0) out vec4 outColor;

        // uMask: (minU, minV, maxU, maxV) — set to (-1,-1,1,1) for a full circle,
        // or clamp one axis to 0 for a quarter-circle corner.
        uniform vec4 uMask;

        void main() {
            // Discard fragments outside the active quadrant mask
            if (fragUV.x < uMask.x || fragUV.y < uMask.y ||
                fragUV.x > uMask.z || fragUV.y > uMask.w) discard;

            float d     = length(fragUV);
            float alpha = smoothstep(1.0, 0.95, d);
            if (alpha == 0.0) discard;

            outColor = vec4(fragColor.rgb, fragColor.a * alpha);
        }
    )";

    circleShader        = CompileProgram(circleVS, circleFS, "circle");
    circleShaderProjLoc = glGetUniformLocation(circleShader, "uProjection");
    circleShaderMaskLoc = glGetUniformLocation(circleShader, "uMask");

    // --- 1×1 white fallback texture ---
    uint32_t whitePixel = 0xFFFFFFFF;
    glCreateTextures(GL_TEXTURE_2D, 1, &whiteTexture);
    glTextureStorage2D(whiteTexture, 1, GL_RGBA8, 1, 1);
    glTextureSubImage2D(whiteTexture, 0, 0, 0, 1, 1,
                        GL_RGBA, GL_UNSIGNED_BYTE, &whitePixel);

    currentBoundTexture = whiteTexture;

    InitRenderState();
}

void OpenGLBackend::InitRenderState() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

// ---------------------------------------------------------------------------
// Projection — single Y-down ortho, shared by all shaders
// ---------------------------------------------------------------------------

void OpenGLBackend::BuildOrthoProjection(float* m16, float width, float height) {
    // Maps x: [0, width] -> [-1, 1], y: [0, height] -> [1, -1]  (Y-down)
    m16[ 0] =  2.f / width;  m16[ 1] = 0.f;            m16[ 2] = 0.f;  m16[ 3] = 0.f;
    m16[ 4] = 0.f;           m16[ 5] = -2.f / height;  m16[ 6] = 0.f;  m16[ 7] = 0.f;
    m16[ 8] = 0.f;           m16[ 9] = 0.f;            m16[10] = -1.f; m16[11] = 0.f;
    m16[12] = -1.f;          m16[13] =  1.f;           m16[14] = 0.f;  m16[15] = 1.f;
}


void OpenGLBackend::FlushBatch() {
    if (batchVertices.empty()) return;

    glNamedBufferSubData(batchVBO, 0,
                         batchVertices.size() * sizeof(Vertex2D),
                         batchVertices.data());

    GLuint shader   = (activeShaderOverride != 0) ? activeShaderOverride : batchShader;
    GLint  projLoc  = (shader == circleShader)    ? circleShaderProjLoc  : batchShaderProjLoc;

    glUseProgram(shader);
    glBindVertexArray(batchVAO);

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);

    float proj[16];
    BuildOrthoProjection(proj, static_cast<float>(vp[2]), static_cast<float>(vp[3]));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, proj);

    // Upload mask uniform when using the circle shader
    if (shader == circleShader) {
        glUniform4fv(circleShaderMaskLoc, 1, activeMask);
    }

    glBindTextureUnit(0, currentBoundTexture);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batchVertices.size()));

    batchVertices.clear();
}


void OpenGLBackend::PushQuad(float x, float y, float w, float h,
                              Color color, GLuint textureHandle,
                              float u0, float v0, float u1, float v1) {
    if (textureHandle == 0) textureHandle = whiteTexture;

    if (textureHandle != currentBoundTexture || batchVertices.size() + 6 > MAX_BATCH_VERTICES) {
        FlushBatch();
        currentBoundTexture = textureHandle;
    }

    const float r = color.r / 255.f, g = color.g / 255.f,
                b = color.b / 255.f, a = color.a / 255.f;

    batchVertices.push_back({ x,     y,     r, g, b, a, u0, v0 });
    batchVertices.push_back({ x + w, y,     r, g, b, a, u1, v0 });
    batchVertices.push_back({ x,     y + h, r, g, b, a, u0, v1 });
    batchVertices.push_back({ x + w, y,     r, g, b, a, u1, v0 });
    batchVertices.push_back({ x + w, y + h, r, g, b, a, u1, v1 });
    batchVertices.push_back({ x,     y + h, r, g, b, a, u0, v1 });
}

// ---------------------------------------------------------------------------
// RegisterWindow — context bootstrap happens exactly once
// ---------------------------------------------------------------------------

WindowID OpenGLBackend::RegisterWindow(Platform* platform) {
    const WindowID id = nextWindowID++;

    Platform_Win32* win32Plat = static_cast<Platform_Win32*>(platform);
    HWND hwnd = reinterpret_cast<HWND>(win32Plat->GetNativeHandle());
    HDC  hdc  = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |
                       PFD_DOUBLEBUFFER   | PFD_SUPPORT_COMPOSITION;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = 32;
    pfd.cDepthBits   = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType   = PFD_MAIN_PLANE;

    int pixelFormat = ChoosePixelFormat(hdc, &pfd);
    if (!pixelFormat || !SetPixelFormat(hdc, pixelFormat, &pfd)) {
        ReleaseDC(hwnd, hdc);
        throw std::runtime_error("Failed to set pixel format.");
    }

    // Only create the shared HGLRC once (on first window registration)
    if (!hRC) {
        HGLRC tempRC = wglCreateContext(hdc);
        if (!tempRC) { ReleaseDC(hwnd, hdc); throw std::runtime_error("Temp GL context failed."); }
        wglMakeCurrent(hdc, tempRC);

        if (!gladLoadWGL(hdc, reinterpret_cast<GLADloadfunc>(wglGetProcAddress))) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(tempRC);
            ReleaseDC(hwnd, hdc);
            throw std::runtime_error("Failed to load WGL extensions.");
        }

        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
            WGL_CONTEXT_MINOR_VERSION_ARB, 6,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            WGL_CONTEXT_FLAGS_ARB,
#ifdef _DEBUG
            WGL_CONTEXT_DEBUG_BIT_ARB,
#else
            0,
#endif
            0
        };

        hRC = wglCreateContextAttribsARB(hdc, 0, attribs);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(tempRC);

        if (!hRC) { ReleaseDC(hwnd, hdc); throw std::runtime_error("Modern GL context failed."); }

        wglMakeCurrent(hdc, hRC);

        if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(wglGetProcAddress))) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(hRC);
            hRC = nullptr;
            ReleaseDC(hwnd, hdc);
            throw std::runtime_error("GLAD failed to load OpenGL functions.");
        }

        // One-time GPU resource setup once we have a valid context
        InitBatchPipeline();
    }

    windows[id] = OpenGLWindow{ .platform = platform, .hdc = hdc, .target = 0 };
    return id;
}

RenderTargetID OpenGLBackend::CreateRenderTarget(int width, int height) {
    const RenderTargetID id = nextTargetID++;

    OpenGLRenderTarget target{};
    target.width = width;
    target.height = height;

    glCreateTextures(GL_TEXTURE_2D, 1, &target.colorTexture);
    glTextureStorage2D(target.colorTexture, 1, GL_RGBA8, width, height);

    glTextureParameteri(target.colorTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(target.colorTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCreateFramebuffers(1, &target.fbo);
    glNamedFramebufferTexture(target.fbo, GL_COLOR_ATTACHMENT0, target.colorTexture, 0);

    if (glCheckNamedFramebufferStatus(target.fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &target.fbo);
        glDeleteTextures(1, &target.colorTexture);
        return 0;
    }

    renderTargets[id] = target;
    return id;
}

void OpenGLBackend::DestroyRenderTarget(RenderTargetID target) {
    auto it = renderTargets.find(target);
    if (it != renderTargets.end()) {
        glDeleteFramebuffers(1, &it->second.fbo);
        glDeleteTextures(1, &it->second.colorTexture);
        renderTargets.erase(it);
    }
}

void OpenGLBackend::ResizeRenderTarget(RenderTargetID target, int width, int height) {
    auto it = renderTargets.find(target);
    if (it == renderTargets.end()) return;

    glDeleteFramebuffers(1, &it->second.fbo);
    glDeleteTextures(1, &it->second.colorTexture);

    it->second.width = width;
    it->second.height = height;

    glCreateTextures(GL_TEXTURE_2D, 1, &it->second.colorTexture);
    glTextureStorage2D(it->second.colorTexture, 1, GL_RGBA8, width, height);
    glTextureParameteri(it->second.colorTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(it->second.colorTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCreateFramebuffers(1, &it->second.fbo);
    glNamedFramebufferTexture(it->second.fbo, GL_COLOR_ATTACHMENT0, it->second.colorTexture, 0);
}


void OpenGLBackend::UnregisterWindow(WindowID window) {
    auto it = windows.find(window);
    if (it != windows.end()) {
        if (it->second.hdc && it->second.platform) {
            HWND hwnd = reinterpret_cast<HWND>(static_cast<Platform_Win32*>(it->second.platform)->GetNativeHandle());
            ReleaseDC(hwnd, it->second.hdc);
        }
        windows.erase(it);
    }
}

void OpenGLBackend::AttachRenderTarget(WindowID window, RenderTargetID target) {
    auto it = windows.find(window);
    if (it != windows.end()) {
        it->second.target = target;
    }
}

void OpenGLBackend::BeginFrame() {
    commandBuffer.clear();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLBackend::SubmitCommand(const RenderCommand& cmd) {
    commandBuffer.push_back(cmd);
}


void OpenGLBackend::EndFrame() {
    RenderTargetID activeTarget = 0;

    for (const auto& cmd : commandBuffer) {
        auto targetIt = renderTargets.find(cmd.target);
        if (targetIt == renderTargets.end()) continue;

        // --- FIXED: If the destination target changes, flush the previous target's batch
        // completely before updating framebuffers or switching WGL contexts!
        if (cmd.target != activeTarget) {
            FlushBatch();
            activeTarget = cmd.target;
        }

        OpenGLWindow* targetWindow = nullptr;
        for (auto& winData : windows | std::views::values) {
            if (winData.target == cmd.target) {
                targetWindow = &winData;
                break;
            }
        }

        if (targetWindow && targetWindow->hdc) {
            if (!wglMakeCurrent(targetWindow->hdc, hRC)) {
                DWORD error = GetLastError();
                // Context switch failed! This safely prevents drawing to an incorrect window surface
                continue;
            }
        }

        const auto& target = targetIt->second;
        glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
        glViewport(0, 0, target.width, target.height);

        ExecuteCommand(cmd);
    }

    // Final clean out flush pass for the remaining commands in the buffer
    FlushBatch();
    commandBuffer.clear();
}
void OpenGLBackend::ExecuteCommand(const RenderCommand& cmd) {
    switch (cmd.type) {
        case RenderCommandType::Clear: {
            FlushBatch();
            float clearColor[4] = {
                cmd.clear.color.r / 255.0f,
                cmd.clear.color.g / 255.0f,
                cmd.clear.color.b / 255.0f,
                cmd.clear.color.a / 255.0f
            };
            glClearBufferfv(GL_COLOR, 0, clearColor);
            break;
        }

        case RenderCommandType::FillRect: {
            const auto& r = cmd.fillRect;
            PushQuad(static_cast<float>(r.x), static_cast<float>(r.y),
                     static_cast<float>(r.w), static_cast<float>(r.h),
                     r.color, 0, 0.0f, 0.0f, 1.0f, 1.0f);
            break;
        }

        case RenderCommandType::DrawRect: {
            const auto& r = cmd.drawRect;
            float fx = static_cast<float>(r.x);
            float fy = static_cast<float>(r.y);
            float fw = static_cast<float>(r.w);
            float fh = static_cast<float>(r.h);

            // Draw border outlines using thin structural quads
            PushQuad(fx, fy, fw, 1.0f, r.color, 0, 0.f, 0.f, 1.f, 1.f);          // Top
            PushQuad(fx, fy + fh - 1.0f, fw, 1.0f, r.color, 0, 0.f, 0.f, 1.f, 1.f); // Bottom
            PushQuad(fx, fy + 1.0f, 1.0f, fh - 2.0f, r.color, 0, 0.f, 0.f, 1.f, 1.f); // Left
            PushQuad(fx + fw - 1.0f, fy + 1.0f, 1.0f, fh - 2.0f, r.color, 0, 0.f, 0.f, 1.f, 1.f); // Right
            break;
        }

        case RenderCommandType::DrawLine: {
            const auto& l = cmd.drawLine;
            float dx = static_cast<float>(l.x1 - l.x0);
            float dy = static_cast<float>(l.y1 - l.y0);
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.0001f) break;

            float thickness = static_cast<float>(std::max(1, l.thickness));

            // Calculate normal offset vector for thickness generation
            float nx = (-dy / len) * (thickness * 0.5f);
            float ny = (dx / len) * (thickness * 0.5f);

            float x0 = static_cast<float>(l.x0);
            float y0 = static_cast<float>(l.y0);
            float x1 = static_cast<float>(l.x1);
            float y1 = static_cast<float>(l.y1);

            // Construct explicit rotated rectangle points for line thickness
            Vertex2D v0 = { x0 - nx, y0 - ny, l.color.r/255.f, l.color.g/255.f, l.color.b/255.f, l.color.a/255.f, 0.f, 0.f };
            Vertex2D v1 = { x1 - nx, y1 - ny, l.color.r/255.f, l.color.g/255.f, l.color.b/255.f, l.color.a/255.f, 1.f, 0.f };
            Vertex2D v2 = { x0 + nx, y0 + ny, l.color.r/255.f, l.color.g/255.f, l.color.b/255.f, l.color.a/255.f, 0.f, 1.f };
            Vertex2D v3 = { x1 + nx, y1 + ny, l.color.r/255.f, l.color.g/255.f, l.color.b/255.f, l.color.a/255.f, 1.f, 1.f };

            if (whiteTexture != currentBoundTexture || batchVertices.size() + 6 >= MAX_BATCH_VERTICES) {
                FlushBatch();
                currentBoundTexture = whiteTexture;
            }

            batchVertices.push_back(v0); batchVertices.push_back(v1); batchVertices.push_back(v2);
            batchVertices.push_back(v1); batchVertices.push_back(v3); batchVertices.push_back(v2);
            break;
        }

        case RenderCommandType::DrawPixel: {
            const auto& p = cmd.drawPixel;
            PushQuad(static_cast<float>(p.x), static_cast<float>(p.y), 1.0f, 1.0f,
                     p.color, 0, 0.0f, 0.0f, 1.0f, 1.0f);
            break;
        }

        case RenderCommandType::DrawGlyph: {
            const auto& g = cmd.drawGlyph;
            GLuint nativeTex = nativeTextures[g.glyph.textureAssetID];
            PushQuad(static_cast<float>(g.x), static_cast<float>(g.y),
                     static_cast<float>(g.glyph.width), static_cast<float>(g.glyph.height),
                     g.color, nativeTex, g.glyph.u0, g.glyph.v0, g.glyph.u1, g.glyph.v1);
            break;
        }

        case RenderCommandType::BlitRenderTarget: {
            // Framebuffer texture references require flushing operations before sampling
            FlushBatch();
            const auto& b = cmd.blitRenderTarget;
            auto srcIt = renderTargets.find(b.source);
            if (srcIt == renderTargets.end()) break;

            GLuint srcTex = srcIt->second.colorTexture;

            // Map normalization coordinates based on matching resolution metrics
            float tw = static_cast<float>(srcIt->second.width);
            float th = static_cast<float>(srcIt->second.height);


                float u0 =  static_cast<float>(b.srcX)      / tw;
                float u1 =  static_cast<float>(b.srcX + b.w) / tw;

                // Flip V: FBO textures are stored bottom-up, so v0 and v1 are swapped
                float v0 =  static_cast<float>(b.srcY + b.h) / th;  // was srcY
                float v1 =  static_cast<float>(b.srcY)        / th;  // was srcY + b.h

            Color whiteMask = { 255, 255, 255, 255 };
            PushQuad(static_cast<float>(b.dstX), static_cast<float>(b.dstY),
                     static_cast<float>(b.w), static_cast<float>(b.h),
                     whiteMask, srcTex, u0, v0, u1, v1);
            break;
        }


        case RenderCommandType::DrawCircle: {
            const auto& c = cmd.drawCircle;
            FlushBatch();

            const float r  = static_cast<float>(c.radius);
            const float cx = static_cast<float>(c.cx) - r;
            const float cy = static_cast<float>(c.cy) - r;

            // Full-circle mask
            const float fullMask[4] = { -1.f, -1.f, 1.f, 1.f };
            std::copy(fullMask, fullMask + 4, activeMask);

            activeShaderOverride = circleShader;
            PushQuad(cx, cy, r * 2.f, r * 2.f, c.color, whiteTexture,
                     -1.f, -1.f, 1.f, 1.f);
            FlushBatch();
            activeShaderOverride = 0;
            break;
        }

        case RenderCommandType::FillRectBeveled: {
            const auto& b     = cmd.fillRectBeveled;
            const float rad   = static_cast<float>(b.radius);
            const float fx    = static_cast<float>(b.x);
            const float fy    = static_cast<float>(b.y);
            const float fw    = static_cast<float>(b.w);
            const float fh    = static_cast<float>(b.h);

            // --- 1. Fill interior cross (no rounding shader needed) ---
            // Horizontal bar
            PushQuad(fx + rad, fy, fw - rad * 2.f, fh, b.color, 0,
                     0.f, 0.f, 1.f, 1.f);
            // Left cap
            PushQuad(fx, fy + rad, rad, fh - rad * 2.f, b.color, 0,
                     0.f, 0.f, 1.f, 1.f);
            // Right cap
            PushQuad(fx + fw - rad, fy + rad, rad, fh - rad * 2.f, b.color, 0,
                     0.f, 0.f, 1.f, 1.f);
            FlushBatch();

            // --- 2. Rounded corners using quarter-circle masks ---
            activeShaderOverride = circleShader;

            // Each corner gets a different quadrant mask so only the
            // correct quarter of the disc is drawn:
            //   Top-left:     U in [-1,0], V in [-1,0]
            //   Top-right:    U in [0,1],  V in [-1,0]
            //   Bottom-left:  U in [-1,0], V in [0,1]
            //   Bottom-right: U in [0,1],  V in [0,1]

            struct Corner { float qx, qy, maskMinU, maskMinV, maskMaxU, maskMaxV; };
            Corner corners[4] = {
                { fx,              fy,              -1.f, -1.f, 0.f,  0.f  }, // TL
                { fx + fw - rad*2, fy,               0.f, -1.f, 1.f,  0.f  }, // TR
                { fx,              fy + fh - rad*2, -1.f,  0.f, 0.f,  1.f  }, // BL
                { fx + fw - rad*2, fy + fh - rad*2,  0.f,  0.f, 1.f,  1.f  }, // BR
            };

            for (auto& corner : corners) {
                activeMask[0] = corner.maskMinU;
                activeMask[1] = corner.maskMinV;
                activeMask[2] = corner.maskMaxU;
                activeMask[3] = corner.maskMaxV;

                PushQuad(corner.qx, corner.qy, rad * 2.f, rad * 2.f,
                         b.color, whiteTexture, -1.f, -1.f, 1.f, 1.f);
                FlushBatch(); // each corner has a different mask — must flush per corner
            }

            activeShaderOverride = 0;
            break;
        }
        case RenderCommandType::DrawWavyLineInt: {
            const auto& w = cmd.drawWavyLineInt;
            float amp = static_cast<float>(w.amplitude);
            float freq = 6.28318f / static_cast<float>(w.wavelength > 0 ? w.wavelength : 1);

            // Build the structural mesh using segmented line-strip links
            for (int x = w.startX; x < w.endX; x += 2) {
                int nextX = std::min(x + 2, w.endX);
                float y0 = static_cast<float>(w.y) + std::sin(static_cast<float>(x) * freq) * amp;
                float y1 = static_cast<float>(w.y) + std::sin(static_cast<float>(nextX) * freq) * amp;

                RenderCommand lineCmd;
                lineCmd.type = RenderCommandType::DrawLine;
                lineCmd.drawLine = { x, static_cast<int>(y0), nextX, static_cast<int>(y1), 1, w.color };
                ExecuteCommand(lineCmd);
            }
            break;
        }

        case RenderCommandType::DrawWavyLineFloat: {
            const auto& w = cmd.drawWavyLineFloat;
            float dx = static_cast<float>(w.endX - w.startX);
            float dy = w.endY - w.startY;
            float totalLen = std::sqrt(dx * dx + dy * dy);
            if (totalLen < 0.001f) break;

            float dirX = dx / totalLen;
            float dirY = dy / totalLen;
            float normX = -dirY;
            float normY = dirX;

            float step = 2.0f;
            float prevX = static_cast<float>(w.startX);
            float prevY = w.startY;

            for (float d = step; d <= totalLen; d += step) {
                float currentX = static_cast<float>(w.startX) + dirX * d;
                float currentY = w.startY + dirY * d;

                // Evaluate the dynamic sine offset along the normal orientation vectors
                float sineOffset = std::sin(d * w.frequency) * w.amplitude;
                float p1x = currentX + normX * sineOffset;
                float p1y = currentY + normY * sineOffset;

                float p0x = prevX + normX * std::sin((d - step) * w.frequency) * w.amplitude;
                float p0y = prevY + normY * std::sin((d - step) * w.frequency) * w.amplitude;

                RenderCommand lineCmd;
                lineCmd.type = RenderCommandType::DrawLine;
                lineCmd.drawLine = { static_cast<int>(p0x), static_cast<int>(p0y),
                                     static_cast<int>(p1x), static_cast<int>(p1y), w.thickness, w.color };
                ExecuteCommand(lineCmd);

                prevX = currentX;
                prevY = currentY;
            }
            break;
        }

        default:
            break;
    }
}

void OpenGLBackend::Present() {
    // Loop through every open window surface independently
    for (auto& [windowID, window] : windows) {
        if (!window.platform || window.target == 0 || !window.hdc) {
            continue;
        }

        auto targetIt = renderTargets.find(window.target);
        if (targetIt == renderTargets.end()) {
            continue;
        }

        auto& target = targetIt->second;

        // Switch context to target window surface before handling blits
        wglMakeCurrent(window.hdc, hRC);

        // Blit intermediate target texture down to the default system frame window surface (0)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, target.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        Platform_Win32* win32Plat = static_cast<Platform_Win32*>(window.platform);
        glBlitFramebuffer(
    0, 0, target.width, target.height,           // src: normal, no flip
    0, 0, win32Plat->GetWidth(), win32Plat->GetHeight(),
    GL_COLOR_BUFFER_BIT, GL_NEAREST
);
        glFlush();
        // Swap the Win32 front and back buffers safely
        SwapBuffers(window.hdc);
    }
}

TextureID OpenGLBackend::CreateFontAtlas(int width, int height) {
    TextureID id = nextTextureID++;
    GLuint texHandle;

    glCreateTextures(GL_TEXTURE_2D, 1, &texHandle);
    glTextureStorage2D(texHandle, 1, GL_R8, width, height);

    GLint swizzleMask[] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };
    glTextureParameteriv(texHandle, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
    glTextureParameteri(texHandle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texHandle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texHandle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texHandle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    nativeTextures[id] = texHandle;
    return id;
}

void OpenGLBackend::UpdateTextureSubImage(TextureID texture, int x, int y,
                                          int width, int height, const uint8_t* pixels) {
    GLuint texHandle = nativeTextures[texture];
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage2D(texHandle, 0, x, y, width, height, GL_RED, GL_UNSIGNED_BYTE, pixels);
}