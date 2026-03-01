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

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.
