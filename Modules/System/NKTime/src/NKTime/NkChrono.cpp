/**
* @File NkChrono.cpp
* @Description Implémentation des utilitaires de mesure temporelle
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Apache-2.0
*/

#include "pch.h"
#include "NkChrono.h"
#include <sstream>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <time.h>
    #include <unistd.h>
    #include <sched.h>
#endif

namespace nkentseu {

    /// @Region: Implémentation NkChrono -------------------------------------------

    /**
    * @Constructor NkChrono
    * @Description Initialise le point de départ temporel
    */
    NkChrono::NkChrono() noexcept :
        mStartTime(std::chrono::high_resolution_clock::now())
    {}

    /**
    * @Function Elapsed
    * @Description Calcule la durée depuis l'initialisation
    * @Note Utilise std::chrono pour une haute précision
    */
    NkElapsedTime NkChrono::Elapsed() const noexcept {
        const auto now = std::chrono::high_resolution_clock::now();
        const auto duration = now - mStartTime;

        NkElapsedTime result;
        result.nanoseconds = std::chrono::duration<float64, std::nano>(duration).count();
        result.milliseconds = result.nanoseconds * 1e-6;
        result.seconds = result.nanoseconds * 1e-9;

        return result;
    }

    /**
    * @Function Reset
    * @Description Réinitialise le chronomètre et retourne le dernier échantillon
    * @Note Opération atomique (mesure + reset)
    */
    NkElapsedTime NkChrono::Reset() noexcept {
        const auto elapsed = Elapsed();
        mStartTime = std::chrono::high_resolution_clock::now();
        return elapsed;
    }

    /// @Region: Implémentation NkElapsedTime --------------------------------------

    /**
    * @Function ToString
    * @Description Sélectionne automatiquement l'unité la plus lisible
    * @Exemples :
    * - "1.234 s" pour >1 seconde
    * - "123.456 ms" pour 1ms-999ms
    * - "789012 ns" pour <1ms
    */
    std::string NkElapsedTime::ToString() const {
        std::ostringstream oss;
        oss.precision(3); // 3 chiffres significatifs

        if(seconds >= 1.0) {
            oss << seconds << " s";
        } else if(milliseconds >= 1.0) {
            oss << milliseconds << " ms";
        } else {
            oss << nanoseconds << " ns";
        }

        return oss.str();
    }

    void NkChrono::Sleep(const NkDuration& duration) {
        #ifdef _WIN32
        ::Sleep(static_cast<DWORD>(duration.ToMilliseconds()));
        #else
        core::int64 microseconds = duration.ToMicroseconds();
        usleep(static_cast<useconds_t>(microseconds));
        #endif
    }
    
    void NkChrono::Sleep(core::int64 milliseconds) {
        Sleep(NkDuration::FromMilliseconds(milliseconds));
    }

    void NkChrono::Sleep(float64 milliseconds) {
        Sleep(NkDuration::FromMilliseconds(milliseconds));
    }

    void NkChrono::SleepMilliseconds(core::int64 milliseconds) {
        Sleep(NkDuration::FromMilliseconds(milliseconds));
    }

    void NkChrono::SleepMicroseconds(core::int64 microseconds) {
        Sleep(NkDuration::FromMicroseconds(microseconds));
    }

    void NkChrono::YieldThread() {
        #ifdef _WIN32
        ::SwitchToThread();
        #else
        ::sched_yield();
        #endif
    }

    core::int64 NkChrono::GetFrequency() {
        #ifdef _WIN32
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        return frequency.QuadPart;
        #else
        return 1000000000LL;  // 1 GHz (nanosecond resolution)
        #endif
    }

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.
