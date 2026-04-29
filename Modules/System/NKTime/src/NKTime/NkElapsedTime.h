// =============================================================================
// NKTime/NkElapsedTime.h
// Résultat d'une mesure de temps — durée exprimée en 4 unités précalculées.
//
// Design :
//  - NkElapsedTime représente la SORTIE d'une mesure (NkChrono::Elapsed, Reset, Now)
//  - Les 4 unités (ns, µs, ms, s) sont calculées UNE SEULE FOIS à la construction
//    depuis la source de vérité `nanoseconds`. Accès ultérieur = lecture directe.
//  - Layout mémoire : 4 × float64 = 32 octets (alignement cache-friendly)
//
// Distinction NkElapsedTime vs NkDuration :
//  - NkElapsedTime : float64 interne — résultat de mesure, lecture seule, champs publics
//  - NkDuration    : int64 interne  — durée à spécifier, API mutable, méthodes privées
//
// Règles d'application des macros :
//  - NKENTSEU_TIME_CLASS_EXPORT sur la classe/struct uniquement
//  - PAS de NKENTSEU_TIME_API sur les méthodes (l'export de classe suffit)
//  - PAS de macro sur les fonctions constexpr/inline définies dans le header
//
// Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
// Date : 2024-2026
// License : Apache-2.0
// =============================================================================

#pragma once

#ifndef NKENTSEU_TIME_NKELAPSEDTIME_H
#define NKENTSEU_TIME_NKELAPSEDTIME_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion de l'API d'export NKTime (remplace l'ancien NkTimeExport.h)
    // Inclusion des constantes de conversion centralisées
    // Inclusion de la chaîne NKEntseu pour le formatage

    #include "NKTime/NkTimeApi.h"
    #include "NKTime/NkTimeConstants.h"
    #include "NKContainers/String/NkString.h"

    // -------------------------------------------------------------------------
    // SECTION 3 : NAMESPACE PRINCIPAL ET DÉCLARATION DU STRUCT
    // -------------------------------------------------------------------------
    // NkElapsedTime est un struct POD (Plain Old Data) optimisé pour la lecture.
    // Tous les champs sont publics pour un accès direct sans overhead.

    namespace nkentseu {

        // Struct exporté : les membres publics n'ont PAS besoin de macro d'export
        struct NKENTSEU_TIME_CLASS_EXPORT NkElapsedTime {

            // -------------------------------------------------------------
            // SOUS-SECTION 3.1 : Champs publics — lecture directe
            // -------------------------------------------------------------
            // Les 4 unités sont précalculées à la construction.
            // Accès ultérieur : zéro calcul, zéro branche, lecture mémoire pure.

            float64 nanoseconds;    ///< Durée en nanosecondes (source de vérité)
            float64 microseconds;   ///< Durée en microsecondes (= ns × 1e-3)
            float64 milliseconds;   ///< Durée en millisecondes (= ns × 1e-6)
            float64 seconds;        ///< Durée en secondes (= ns × 1e-9)

            // -------------------------------------------------------------
            // SOUS-SECTION 3.2 : Constructeurs
            // -------------------------------------------------------------

            /// Constructeur par défaut : durée nulle dans toutes les unités.
            /// @note constexpr pour utilisation dans des contextes compile-time.
            constexpr NkElapsedTime() noexcept
                : nanoseconds(0.0)
                , microseconds(0.0)
                , milliseconds(0.0)
                , seconds(0.0)
            {
            }

        private:

            /// Constructeur canonique privé : calcule toutes les unités depuis ns.
            /// @param ns Valeur en nanosecondes (source de vérité)
            /// @note Toutes les conversions sont faites ici, une seule fois.
            constexpr explicit NkElapsedTime(float64 ns) noexcept
                : nanoseconds(ns)
                , microseconds(ns * 1.0e-3)
                , milliseconds(ns * 1.0e-6)
                , seconds(ns * 1.0e-9)
            {
            }

        public:

            // -------------------------------------------------------------
            // SOUS-SECTION 3.3 : Fabriques statiques (Factory Methods)
            // -------------------------------------------------------------
            // Méthodes de construction nommées pour une API expressive.
            // Toutes sont constexpr et noexcept pour performance et sécurité.

            /// Crée une durée depuis un nombre de nanosecondes.
            /// @param ns Valeur en nanosecondes
            /// @return Nouvelle instance NkElapsedTime
            NKTIME_NODISCARD static constexpr NkElapsedTime FromNanoseconds(float64 ns) noexcept {
                return NkElapsedTime(ns);
            }

            /// Crée une durée depuis un nombre de microsecondes.
            /// @param us Valeur en microsecondes
            /// @return Nouvelle instance NkElapsedTime
            NKTIME_NODISCARD static constexpr NkElapsedTime FromMicroseconds(float64 us) noexcept {
                return NkElapsedTime(us * 1.0e3);
            }

            /// Crée une durée depuis un nombre de millisecondes.
            /// @param ms Valeur en millisecondes
            /// @return Nouvelle instance NkElapsedTime
            NKTIME_NODISCARD static constexpr NkElapsedTime FromMilliseconds(float64 ms) noexcept {
                return NkElapsedTime(ms * 1.0e6);
            }

            /// Crée une durée depuis un nombre de secondes.
            /// @param s Valeur en secondes
            /// @return Nouvelle instance NkElapsedTime
            NKTIME_NODISCARD static constexpr NkElapsedTime FromSeconds(float64 s) noexcept {
                return NkElapsedTime(s * 1.0e9);
            }

            // -------------------------------------------------------------
            // SOUS-SECTION 3.4 : Accesseurs nommés (synonymes fluides)
            // -------------------------------------------------------------
            // Méthodes de lecture avec noms explicites pour une API plus claire.
            // Retourne directement le champ précalculé : zéro calcul à l'appel.

            /// @return Durée en nanosecondes (lecture directe du champ)
            NKTIME_NODISCARD NKENTSEU_INLINE constexpr float64 ToNanoseconds() const noexcept {
                return nanoseconds;
            }

            /// @return Durée en microsecondes (lecture directe du champ)
            NKTIME_NODISCARD NKENTSEU_INLINE constexpr float64 ToMicroseconds() const noexcept {
                return microseconds;
            }

            /// @return Durée en millisecondes (lecture directe du champ)
            NKTIME_NODISCARD NKENTSEU_INLINE constexpr float64 ToMilliseconds() const noexcept {
                return milliseconds;
            }

            /// @return Durée en secondes (lecture directe du champ)
            NKTIME_NODISCARD NKENTSEU_INLINE constexpr float64 ToSeconds() const noexcept {
                return seconds;
            }

            // -------------------------------------------------------------
            // SOUS-SECTION 3.5 : Opérateurs arithmétiques
            // -------------------------------------------------------------
            // Toutes les opérations passent par le constructeur privé pour
            // garantir que les 4 unités restent cohérentes après calcul.

            /// Addition de deux durées mesurées.
            /// @param rhs Durée à ajouter
            /// @return Nouvelle durée résultante (unités recalculées)
            NKTIME_NODISCARD constexpr NkElapsedTime operator+(const NkElapsedTime& rhs) const noexcept {
                return NkElapsedTime(nanoseconds + rhs.nanoseconds);
            }

            /// Soustraction de deux durées mesurées.
            /// @param rhs Durée à soustraire
            /// @return Nouvelle durée résultante (unités recalculées)
            NKTIME_NODISCARD constexpr NkElapsedTime operator-(const NkElapsedTime& rhs) const noexcept {
                return NkElapsedTime(nanoseconds - rhs.nanoseconds);
            }

            /// Addition in-place : modifie cet objet.
            /// @param rhs Durée à ajouter
            /// @return Référence vers cet objet pour chaînage
            constexpr NkElapsedTime& operator+=(const NkElapsedTime& rhs) noexcept {
                *this = NkElapsedTime(nanoseconds + rhs.nanoseconds);
                return *this;
            }

            /// Soustraction in-place : modifie cet objet.
            /// @param rhs Durée à soustraire
            /// @return Référence vers cet objet pour chaînage
            constexpr NkElapsedTime& operator-=(const NkElapsedTime& rhs) noexcept {
                *this = NkElapsedTime(nanoseconds - rhs.nanoseconds);
                return *this;
            }

            /// Multiplication par un facteur scalaire.
            /// @param factor Facteur multiplicatif
            /// @return Nouvelle durée résultante (unités recalculées)
            NKTIME_NODISCARD constexpr NkElapsedTime operator*(float64 factor) const noexcept {
                return NkElapsedTime(nanoseconds * factor);
            }

            /// Division par un diviseur scalaire.
            /// @param divisor Diviseur
            /// @return Nouvelle durée résultante (unités recalculées)
            NKTIME_NODISCARD constexpr NkElapsedTime operator/(float64 divisor) const noexcept {
                return NkElapsedTime(nanoseconds / divisor);
            }

            // -------------------------------------------------------------
            // SOUS-SECTION 3.6 : Opérateurs de comparaison
            // -------------------------------------------------------------
            // Comparaisons basées sur la source de vérité : nanoseconds.
            // Toutes les méthodes sont constexpr et noexcept.

            /// Égalité : vrai si les nanosecondes sont identiques.
            /// @param rhs Autre durée à comparer
            /// @return true si égales, false sinon
            NKTIME_NODISCARD constexpr bool operator==(const NkElapsedTime& rhs) const noexcept {
                return nanoseconds == rhs.nanoseconds;
            }

            /// Inégalité : inverse de operator==
            /// @param rhs Autre durée à comparer
            /// @return true si différentes, false sinon
            NKTIME_NODISCARD constexpr bool operator!=(const NkElapsedTime& rhs) const noexcept {
                return nanoseconds != rhs.nanoseconds;
            }

            /// Infériorité stricte : vrai si cette durée est plus courte.
            /// @param rhs Autre durée à comparer
            /// @return true si *this < rhs, false sinon
            NKTIME_NODISCARD constexpr bool operator<(const NkElapsedTime& rhs) const noexcept {
                return nanoseconds < rhs.nanoseconds;
            }

            /// Infériorité ou égalité
            /// @param rhs Autre durée à comparer
            /// @return true si *this <= rhs, false sinon
            NKTIME_NODISCARD constexpr bool operator<=(const NkElapsedTime& rhs) const noexcept {
                return nanoseconds <= rhs.nanoseconds;
            }

            /// Supériorité stricte : vrai si cette durée est plus longue.
            /// @param rhs Autre durée à comparer
            /// @return true si *this > rhs, false sinon
            NKTIME_NODISCARD constexpr bool operator>(const NkElapsedTime& rhs) const noexcept {
                return nanoseconds > rhs.nanoseconds;
            }

            /// Supériorité ou égalité
            /// @param rhs Autre durée à comparer
            /// @return true si *this >= rhs, false sinon
            NKTIME_NODISCARD constexpr bool operator>=(const NkElapsedTime& rhs) const noexcept {
                return nanoseconds >= rhs.nanoseconds;
            }

            // -------------------------------------------------------------
            // SOUS-SECTION 3.7 : Formatage et sérialisation
            // -------------------------------------------------------------

            /// Formatage automatique avec unité adaptative.
            /// Choisit l'unité la plus lisible (ns/us/ms/s).
            /// @return NkString contenant la représentation textuelle
            /// @note Utilise 'us' (pas 'µs') pour compatibilité UTF-8
            NKTIME_NODISCARD NkString ToString() const;

            /// Fonction amie pour formatage via ADL (Argument-Dependent Lookup).
            /// @param e Objet NkElapsedTime à formater
            /// @return NkString contenant la représentation textuelle
            friend NkString ToString(const NkElapsedTime& e) {
                return e.ToString();
            }

        }; // struct NkElapsedTime

    } // namespace nkentseu

#endif // NKENTSEU_TIME_NKELAPSEDTIME_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKELAPSEDTIME.H
// =============================================================================
// NkElapsedTime représente le RÉSULTAT d'une mesure de temps.
// Contrairement à NkDuration (pour spécifier), il est optimisé pour la lecture.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Création via les fabriques statiques
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkElapsedTime.h"

    void ElapsedCreationExample() {
        using namespace nkentseu;

        // Création depuis différentes unités (conversions automatiques)
        NkElapsedTime e1 = NkElapsedTime::FromMilliseconds(123.456);
        NkElapsedTime e2 = NkElapsedTime::FromSeconds(2.5);
        NkElapsedTime e3 = NkElapsedTime::FromMicroseconds(500.0);

        // Accès direct aux champs précalculés (zéro calcul)
        float64 ns = e1.nanoseconds;    // 123456000.0
        float64 ms = e1.milliseconds;   // 123.456
        float64 s  = e1.seconds;        // 0.123456

        // Ou via les accesseurs nommés (synonymes)
        float64 alsoMs = e1.ToMilliseconds();  // 123.456
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation avec NkChrono pour mesurer du code
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkElapsedTime.h"
    #include "NKCore/Logger/NkLogger.h"

    void MeasureCodeExample() {
        using namespace nkentseu;

        // Hypothétique : NkChrono retourne NkElapsedTime
        // NkElapsedTime start = NkChrono::Now();

        // ... code à mesurer ...

        // NkElapsedTime elapsed = NkChrono::Elapsed(start);

        // Formatage automatique pour logging
        // NK_LOG_INFO("Execution time: {}", elapsed.ToString());
        // Output: "Execution time: 45.678 ms"

        // Accès précis si nécessaire
        // if (elapsed.nanoseconds > 100'000'000.0) {  // > 100ms
        //     NK_LOG_WARN("Slow execution detected");
        // }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Arithmétique des durées mesurées
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkElapsedTime.h"

    void ElapsedArithmeticExample() {
        using namespace nkentseu;

        // Mesures individuelles
        NkElapsedTime phase1 = NkElapsedTime::FromMilliseconds(50.0);
        NkElapsedTime phase2 = NkElapsedTime::FromMilliseconds(75.5);

        // Addition pour durée totale
        NkElapsedTime total = phase1 + phase2;  // 125.5 ms

        // Différence entre deux mesures
        NkElapsedTime delta = phase2 - phase1;  // 25.5 ms

        // Mise à jour in-place avec chaînage
        NkElapsedTime accumulator = NkElapsedTime::FromSeconds(0);
        accumulator += phase1;
        accumulator += phase2;
        // accumulator vaut maintenant 125.5 ms

        // Mise à l'échelle (ex: moyenne sur N itérations)
        NkElapsedTime average = total / 2.0;  // 62.75 ms
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Comparaisons et seuils de performance
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkElapsedTime.h"
    #include <vector>
    #include <algorithm>

    void PerformanceThresholdExample() {
        using namespace nkentseu;

        // Définition de seuils de performance
        const NkElapsedTime budget = NkElapsedTime::FromMilliseconds(16.67); // 60 FPS

        // Mesure d'une frame
        // NkElapsedTime frameTime = MeasureFrame();

        // Vérification du budget
        // if (frameTime > budget) {
        //     NK_LOG_WARN("Frame budget exceeded: {} > {}",
        //                 frameTime.ToString(), budget.ToString());
        // }

        // Tri de mesures pour analyse statistique
        std::vector<NkElapsedTime> samples;
        // ... collecte des samples ...
        std::sort(samples.begin(), samples.end());

        // Calcul de percentiles
        // NkElapsedTime p50 = samples[samples.size() * 50 / 100];
        // NkElapsedTime p99 = samples[samples.size() * 99 / 100];
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Formatage pour UI et reporting
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkElapsedTime.h"
    #include "NKCore/Logger/NkLogger.h"

    void ElapsedFormattingExample() {
        using namespace nkentseu;

        // Formatage automatique (unité adaptative)
        NkElapsedTime e1 = NkElapsedTime::FromNanoseconds(12345.0);
        NkElapsedTime e2 = NkElapsedTime::FromMilliseconds(45.678);
        NkElapsedTime e3 = NkElapsedTime::FromSeconds(2.5);

        NK_LOG_INFO("e1: {}", e1.ToString());  // "12.345 us"
        NK_LOG_INFO("e2: {}", e2.ToString());  // "45.678 ms"
        NK_LOG_INFO("e3: {}", e3.ToString());  // "2.500 s"

        // Formatage via fonction amie (ADL)
        NkString report = NkString("Duration: ") + ToString(e2);
        // report = "Duration: 45.678 ms"

        // Note : 'us' est utilisé plutôt que 'µs' pour éviter les problèmes
        // d'encodage dans les terminaux/logs non-UTF8.
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Intégration avec NkDuration (mesure vs spécification)
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkElapsedTime.h"
    #include "NKTime/NkDuration.h"

    namespace nkentseu {

        // Pattern : mesurer l'exécution vs spécifier un timeout
        void ExecuteWithMonitoring() {
            // Étape 1 : SPÉCIFIER un timeout avec NkDuration
            NkDuration timeout = NkDuration::FromSeconds(5);

            // Étape 2 : Exécuter et MESURER avec NkElapsedTime
            // NkElapsedTime start = NkChrono::Now();

            // bool success = ExecuteTask(timeout);

            // NkElapsedTime elapsed = NkChrono::Elapsed(start);

            // Étape 3 : Comparer mesure vs spécification
            // NkDuration elapsedAsDuration =
            //     NkDuration::FromNanoseconds(static_cast<int64>(elapsed.nanoseconds));

            // if (elapsedAsDuration > timeout) {
            //     NK_LOG_WARN("Task exceeded timeout: {} > {}",
            //                 elapsed.ToString(), timeout.ToString());
            // }
        }

        // Conversion si nécessaire entre les deux types
        NkDuration ElapsedToDuration(const NkElapsedTime& elapsed) {
            return NkDuration::FromNanoseconds(static_cast<int64>(elapsed.nanoseconds));
        }

        NkElapsedTime DurationToElapsed(const NkDuration& duration) {
            return NkElapsedTime::FromNanoseconds(
                static_cast<float64>(duration.ToNanoseconds()));
        }

    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Performance : accès direct vs accesseurs
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkElapsedTime.h"

    void AccessPatternExample() {
        using namespace nkentseu;

        NkElapsedTime elapsed = NkElapsedTime::FromMilliseconds(123.456);

        // Accès direct aux champs publics (le plus rapide)
        // Aucune fonction call, lecture mémoire pure
        float64 rawNs = elapsed.nanoseconds;

        // Accès via accesseurs nommés (même performance grâce à inline)
        // Utile pour une API plus expressive et documentée
        float64 namedNs = elapsed.ToNanoseconds();

        // Les deux sont équivalents en performance (constexpr + inline)
        // Choisir selon le contexte :
        // - Champs directs : code interne, performance critique
        // - Accesseurs : API publique, lisibilité, documentation auto
    }
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================