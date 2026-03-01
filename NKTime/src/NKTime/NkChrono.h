#pragma once

/**
* @File NkChrono.h
* @Description Définit des utilitaires de mesure de temps haute précision
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Apache-2.0
*/

#include "Nkentseu/Platform/Export.h"
#include "Nkentseu/Platform/Platform.h"
#include "Nkentseu/Platform/Types.h"
#include <chrono>
#include <ratio>

namespace nkentseu {

    /**
    * @Struct NkElapsedTime
    * @Description Stocke le temps écoulé dans différentes unités
    *
    * @Members :
    * - (float64) nanoseconds : Temps en nanosecondes
    * - (float64) milliseconds : Temps en millisecondes
    * - (float64) seconds : Temps en secondes
    */
    struct NKENTSEU_API NkElapsedTime {
        float64 nanoseconds = 0.0;   ///< Temps écoulé en nanosecondes (1e-9 s)
        float64 milliseconds = 0.0;  ///< Temps écoulé en millisecondes (1e-3 s)
        float64 seconds = 0.0;       ///< Temps écoulé en secondes

        /**
        * @Function ToString
        * @Description Formatage automatique dans l'unité la plus appropriée
        * @return string Temps formaté (ex: "1.234 s", "45.678 ms")
        */
        std::string ToString() const;

        /**
        * @Friend ToString
        * @Description : Surcharge amie de la fonction ToString
        * @param (const NkElapsedTime&) elapsed : Temps ecouler à formater
        * @return (std::string) : Représentation formatée du temps ecouler
        */
        friend std::string ToString(const NkElapsedTime& elapsed) {
            return elapsed.ToString();
        }
    };

    /**
    * @Class NkChrono
    * @Description Chronomètre haute précision avec fonctionnalités de mesure
    *
    * @Fonctionnalités :
    * - Démarrage automatique à la construction
    * - Mesure du temps écoulé avec différentes unités
    * - Réinitialisation avec lecture du temps écoulé
    * - Précision nanoseconde
    */
    class NKENTSEU_API NkChrono {
    public:
        /// @Region: Constructeurs/Destructeur ------------------------------------

        /**
        * @Constructor NkChrono
        * @Description Initialise le chronomètre avec l'heure actuelle
        */
        NkChrono() noexcept;

        ~NkChrono() = default;

        /// @Region: Opérations de base ------------------------------------------

        /**
        * @Function Elapsed
        * @Description Calcule le temps écoulé depuis le démarrage
        * @return NkElapsedTime Temps écoulé dans toutes les unités
        */
        NkElapsedTime Elapsed() const noexcept;

        /**
        * @Function Reset
        * @Description Réinitialise le chronomètre et retourne le temps écoulé
        * @return NkElapsedTime Temps écoulé avant réinitialisation
        */
        NkElapsedTime Reset() noexcept;

    private:
        std::chrono::high_resolution_clock::time_point mStartTime; ///< Point de départ temporel
    };

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.
