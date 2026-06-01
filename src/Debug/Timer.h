//
// Created by tkdtu on 5/30/2026.
//

#ifndef BROWSER_TIMER_H
#define BROWSER_TIMER_H
#include <chrono>


#include <chrono>

class Timer {
public:
    // Starts the timer
    void StartTimer();

    // Stops the timer
    void StopTimer();

    // Completing your line: Returns the duration in standard floating-point seconds
    std::chrono::duration<double> GetElapsed() const;

    // Helper method to get elapsed time directly in milliseconds
    double GetElapsedMilliseconds() const;

private:
    std::chrono::time_point<std::chrono::steady_clock> m_StartTime;
    std::chrono::time_point<std::chrono::steady_clock> m_EndTime;
    bool m_IsRunning = false;
};


#endif //BROWSER_TIMER_H
