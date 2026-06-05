//
// Created by tkdtu on 6/4/2026.
//
#include "IRendererBackend.h"
#include "Software/SoftwareBackend.h"
#include "OpenGL/OpenGLBackend.h"
#include <iostream>

std::shared_ptr<IRenderBackend> backend = nullptr;

std::shared_ptr<IRenderBackend> IRenderBackend::GetRenderBackend(PreferredBackend pref)
{
    if (backend) {
        return backend;
    }

    // --- TIER 1: Attempt the Preferred Backend ---
    try {
        if (pref == PreferredBackend::OpenGL) {
            backend = std::make_shared<OpenGLBackend>();
            return backend;
        }
        else if (pref == PreferredBackend::Software) {
            backend = std::make_shared<SoftwareBackend>();
            return backend;
        }
        // If you add Vulkan later:
        // else if (pref == PrefferedBackend::Vulkan) { ... }
    }
    catch (const std::exception& e) {
        std::cerr << "Preferred backend failed to initialize: " << e.what() << "\n";
        backend = nullptr; // Reset to ensure fallback triggers
    }

    // --- TIER 2: Fallback to OpenGL ---
    if (!backend && pref != PreferredBackend::OpenGL) {
        try {
            std::cout << "Falling back to OpenGL Backend...\n";
            backend = std::make_shared<OpenGLBackend>();
            return backend;
        }
        catch (const std::exception& e) {
            std::cerr << "OpenGL fallback failed: " << e.what() << "\n";
            backend = nullptr;
        }
    }

    // --- TIER 3: Ultimate Fallback to Software Renderer ---
    if (!backend) {
        std::cout << "Falling back to pure CPU Software Backend (Guaranteed to work)...\n";
        backend = std::make_shared<SoftwareBackend>();
    }

    return backend;
}