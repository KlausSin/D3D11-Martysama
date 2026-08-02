
#pragma once

#include <cmath>
#include <cstdint>
#include <exception>
#include <profileapi.h>
#include <synchapi.h>
#include "Singleton.h"

namespace DX
{
    // Helper class for animation and simulation timing.
    class StepTimer : public CSingleton<StepTimer>
    {
    public:
        StepTimer() noexcept(false) :
            m_elapsedTicks(0),
            m_totalTicks(0),
            m_leftOverTicks(0),
            m_frameCount(0),
            m_framesPerSecond(0),
            m_framesThisSecond(0),
            m_qpcSecondCounter(0),
            m_isFixedTimeStep(false),
            m_targetElapsedTicks(TicksPerSecond / 60),
            m_targetFPS(120),
            m_frameLimitEnabled(true)
        {
            if (!QueryPerformanceFrequency(&m_qpcFrequency))
            {
                throw std::exception("QueryPerformanceFrequency");
            }

            if (!QueryPerformanceCounter(&m_qpcLastTime))
            {
                throw std::exception("QueryPerformanceCounter");
            }

            // Initialize max delta to 1/10 of a second.
            m_qpcMaxDelta = static_cast<uint64_t>(m_qpcFrequency.QuadPart / 10);

            // Initialize frame limiter
            m_qpcFrameStart = m_qpcLastTime;
            m_qpcTargetFrameTime = m_qpcFrequency.QuadPart / m_targetFPS;
        }

        // Get elapsed time since the previous Update call.
        uint64_t GetElapsedTicks() const noexcept { return m_elapsedTicks; }
        double GetElapsedSeconds() const noexcept { return TicksToSeconds(m_elapsedTicks); }
        double GetElapsedMillieSeconds() const noexcept { return TicksToMillieSeconds(m_elapsedTicks); }

        // Get total time since the start of the program.
        uint64_t GetTotalTicks() const noexcept { return m_totalTicks; }
        double GetTotalSeconds() const noexcept { return TicksToSeconds(m_totalTicks); }
        double GetTotalMillieSeconds() const noexcept { return TicksToMillieSeconds(m_totalTicks); }

        // Get total number of updates since start of the program.
        uint32_t GetFrameCount() const noexcept { return m_frameCount; }

        // Get the current framerate.
        uint32_t GetFramesPerSecond() const noexcept { return m_framesPerSecond; }
        // Get the current framerate.
        uint32_t GetFramesThisSecond() const noexcept { return m_framesThisSecond; }

        // Set whether to use fixed or variable timestep mode.
        void SetFixedTimeStep(bool isFixedTimestep) noexcept { m_isFixedTimeStep = isFixedTimestep; }

        void SetTargetElapsedTicks(uint64_t targetElapsed) noexcept { m_targetElapsedTicks = targetElapsed; }
        void SetTargetElapsedSeconds(double targetElapsed) noexcept { m_targetElapsedTicks = SecondsToTicks(targetElapsed); }

        static const uint64_t TicksPerSecond = 10000000;
        static const uint64_t TicksPerMillieSecond = 10000;

        static constexpr double TicksToSeconds(uint64_t ticks) noexcept { return static_cast<double>(ticks) / TicksPerSecond; }
        static constexpr uint64_t SecondsToTicks(double seconds) noexcept { return static_cast<uint64_t>(seconds * TicksPerSecond); }

        static constexpr double TicksToMillieSeconds(uint64_t ticks) noexcept { return static_cast<double>(ticks) / TicksPerMillieSecond; }
        static constexpr uint64_t MillieSecondsToTicks(double seconds) noexcept { return static_cast<uint64_t>(seconds * TicksPerMillieSecond); }


        void ResetElapsedTime()
        {
            if (!QueryPerformanceCounter(&m_qpcLastTime))
            {
                throw std::exception("QueryPerformanceCounter");
            }

            m_leftOverTicks = 0;
            m_framesPerSecond = 0;
            m_framesThisSecond = 0;
            m_qpcSecondCounter = 0;
            m_qpcFrameStart = m_qpcLastTime;
        }

        // Frame limiter settings
        void SetTargetFPS(uint32_t fps) noexcept
        {
            m_targetFPS = fps;
            if (fps > 0)
            {
                m_qpcTargetFrameTime = m_qpcFrequency.QuadPart / fps;
                m_frameLimitEnabled = true;
            }
            else
            {
                m_frameLimitEnabled = false;
            }
        }

        uint32_t GetTargetFPS() const noexcept { return m_targetFPS; }
        bool IsFrameLimitEnabled() const noexcept { return m_frameLimitEnabled; }

        void WaitForNextFrame()
        {
            if (!m_frameLimitEnabled || m_targetFPS == 0)
                return;

            LARGE_INTEGER currentTime;
            QueryPerformanceCounter(&currentTime);

            int64_t elapsed = currentTime.QuadPart - m_qpcFrameStart.QuadPart;
            int64_t remaining = static_cast<int64_t>(m_qpcTargetFrameTime) - elapsed;

            if (remaining > 0)
            {
                // Convert remaining QPC ticks to milliseconds
                int64_t remainingMs = (remaining * 1000) / m_qpcFrequency.QuadPart;

                if (remainingMs > 2)
                {
                    Sleep(static_cast<DWORD>(remainingMs - 2));
                }

                // Busy-wait for remaining time (precise)
                do
                {
                    QueryPerformanceCounter(&currentTime);
                    elapsed = currentTime.QuadPart - m_qpcFrameStart.QuadPart;
                } while (elapsed < static_cast<int64_t>(m_qpcTargetFrameTime));
            }

            // Mark start of next frame
            QueryPerformanceCounter(&m_qpcFrameStart);
        }

        template<typename TUpdate>
        void Tick(const TUpdate& update)
        {
            // Query the current time.
            LARGE_INTEGER currentTime;

            if (!QueryPerformanceCounter(&currentTime))
            {
                throw std::exception("QueryPerformanceCounter");
            }

            uint64_t timeDelta = static_cast<uint64_t>(currentTime.QuadPart - m_qpcLastTime.QuadPart);

            m_qpcLastTime = currentTime;
            m_qpcSecondCounter += timeDelta;

            if (timeDelta > m_qpcMaxDelta)
            {
                timeDelta = m_qpcMaxDelta;
            }

            timeDelta *= TicksPerSecond;
            timeDelta /= static_cast<uint64_t>(m_qpcFrequency.QuadPart);

            uint32_t lastFrameCount = m_frameCount;

            if (m_isFixedTimeStep)
            {
                // Fixed timestep update logic


                if (static_cast<uint64_t>(std::abs(static_cast<int64_t>(timeDelta - m_targetElapsedTicks))) < TicksPerSecond / 4000)
                {
                    timeDelta = m_targetElapsedTicks;
                }

                m_leftOverTicks += timeDelta;

                while (m_leftOverTicks >= m_targetElapsedTicks)
                {
                    m_elapsedTicks = m_targetElapsedTicks;
                    m_totalTicks += m_targetElapsedTicks;
                    m_leftOverTicks -= m_targetElapsedTicks;
                    m_frameCount++;

                    update();
                }
            }
            else
            {
                // Variable timestep update logic.
                m_elapsedTicks = timeDelta;
                m_totalTicks += timeDelta;
                m_leftOverTicks = 0;
                m_frameCount++;

                update();
            }

            // Track the current framerate.
            if (m_frameCount != lastFrameCount)
            {
                m_framesThisSecond++;
            }

            if (m_qpcSecondCounter >= static_cast<uint64_t>(m_qpcFrequency.QuadPart))
            {
                m_framesPerSecond = m_framesThisSecond;
                m_framesThisSecond = 0;
                m_qpcSecondCounter %= static_cast<uint64_t>(m_qpcFrequency.QuadPart);
            }
        }

    private:
        // Source timing data uses QPC units.
        LARGE_INTEGER m_qpcFrequency;
        LARGE_INTEGER m_qpcLastTime;
        uint64_t m_qpcMaxDelta;

        // Derived timing data uses a canonical tick format.
        uint64_t m_elapsedTicks;
        uint64_t m_totalTicks;
        uint64_t m_leftOverTicks;

        // Members for tracking the framerate.
        uint32_t m_frameCount;
        uint32_t m_framesPerSecond;
        uint32_t m_framesThisSecond;
        uint64_t m_qpcSecondCounter;

        // Members for configuring fixed timestep mode.
        bool m_isFixedTimeStep;
        uint64_t m_targetElapsedTicks;

        // Members for frame limiting.
        uint32_t m_targetFPS;
        bool m_frameLimitEnabled;
        LARGE_INTEGER m_qpcFrameStart;
        uint64_t m_qpcTargetFrameTime;
    };
}
