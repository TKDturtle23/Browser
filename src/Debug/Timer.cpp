//
// Created by tkdtu on 5/30/2026.
//

#include "Timer.h"

void Timer::StartTimer() {
    m_StartTime = std::chrono::steady_clock::now();
    m_IsRunning = true;
}

void Timer::StopTimer() {
    if (m_IsRunning) {
        m_EndTime = std::chrono::steady_clock::now();
        m_IsRunning = false;
    }
}

std::chrono::duration<double> Timer::GetElapsed() const {
    if (m_IsRunning) {
        return std::chrono::steady_clock::now() - m_StartTime;
    }
    return m_EndTime - m_StartTime;
}

double Timer::GetElapsedMilliseconds() const {
    return std::chrono::duration<double, std::milli>(GetElapsed()).count();
}
