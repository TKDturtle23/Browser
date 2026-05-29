#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../../Render/Renderer.h"



enum class Key;
// Simple UI identifier type
using UIID = uint32_t;

struct IOState {
    int mouseX = 0;
    int mouseY = 0;
    bool mouseLeftDown = false;
    bool mouseLeftClicked = false; // True for the single frame the click happens

    // Simple keyboard buffer for text entry
    char lastTypedChar = 0;
    bool backspacePressed = false;
    bool enterPressed = false;
};

class InterfaceManager {
public:
    InterfaceManager(int initialWidth, int initialHeight);
    ~InterfaceManager() = default;

    // Window Management
    void Resize(int newWidth, int newHeight);
    const std::vector<Color>& GetFrontBuffer() const;

    // --- Input Injection Hooks ---
    // Call these from your OS window event loop (GLFW, SDL, Win32, etc.)
    void InjectMouseMove(int x, int y);
    void InjectMouseButton(bool leftDown);
    void InjectKeyChar(Key key, bool shiftPressed);

    // --- Immediate Mode Lifecycle ---
    void BeginFrame();
    void EndFrame();

    // --- Immediate Mode Widgets ---
    // Returns true if the button was clicked this frame
    bool Button(const std::string& label, int width, int height);

    // Returns true if the tab was clicked/selected
    bool Tab(const std::string& id_str, std::string& title, bool isActive, int width, int height);

    // Handles text insertion and deletion directly modifying the passed string
    bool AddressBar(const std::string& id_str, std::string& text, int width, int height);

    // Layout layout helpers (mimicking ImGui::SameLine)
    void SameLine(int spacing = 5);
    void NewLine(int spacing = 5);
    void SetCursor(int x, int y);

private:
    // Helper to generate quick hashes for widget IDs
    UIID GetID(const std::string& str);
    bool IsMouseOver(int x, int y, int w, int h) const;

    std::unique_ptr<Renderer> renderer;
    int windowWidth;
    int windowHeight;

    // Current layout cursor state (resets every frame)
    int cursorX;
    int cursorY;
    int maxRowHeight;

    // IO / Event state
    IOState io;
    bool lastMouseState = false;

    // IMUI State Tracking
    UIID activeID = 0;
    UIID hotID = 0;
    UIID focusID = 0;

    // --- State persistence for text fields ---
    int cursorIndex = 0;      // Character index where the | cursor sits
    int selectStartIndex = -1; // Index where selection started (-1 means no selection)
    bool isDraggingText = false;
    Font font;
};