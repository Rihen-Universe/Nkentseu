// =============================================================================
// NKTime/NkDuration.h
// Durée à spécifier — API pour sleep, timeout, délais. int64 interne en ns.
//
// Design :
//  - Distinction claire NkDuration vs NkElapsedTime :
//    * NkDuration    : int64 interne en ns. Utilisée pour SPÉCIFIER une durée
//                      (NkChrono::Sleep, timeout réseau, période d'un timer).
//                      Mutable, opérateurs +=/-= disponibles, peut être négative.
//    * NkElapsedTime : float64 interne en ns. RÉSULTAT d'une mesure de chrono.
//                      4 champs précalculés en lecture directe, non mutable.
//  - Toutes les méthodes sont constexpr pour évaluation à la compilation
//  - Aucune dépendance STL pour compatibilité embarquée et performance
//
// Règles d'application des macros :
//  - NKENTSEU_TIME_CLASS_EXPORT sur la classe uniquement
//  - PAS de NKENTSEU_TIME_API sur les méthodes (l'export de classe suffit)
//  - PAS de macro sur les fonctions constexpr/inline définies dans le header
//
// Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
// Date : 2024-2026
// License : Apache-2.0
// =============================================================================

#pragma once

#ifndef NKENTSEU_TIME_NKDURATION_H
#define NKENTSEU_TIME_NKDURATION_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion de l'API d'export NKTime (remplace l'ancien NkTimeExport.h)
    // Inclusion des constantes de conversion centralisées
    // Inclusion des types de base NKCore et de la chaîne NKEntseu

    #include "NKTime/NkTimeApi.h"
    #include "NKTime/NkTimeConstants.h"
    #include "NKCore/NkTypes.h"
    #include "NKContainers/String/NkString.h"

    // -------------------------------------------------------------------------
    // SECTION 3 : NAMESPACE PRINCIPAL ET DÉCLARATION DE LA CLASSE
    // -------------------------------------------------------------------------
    // La classe NkDuration représente une durée temporelle stockée en nanosecondes.
    // Elle est conçue pour être utilisée comme paramètre d'API (sleep, timeout).

    namespace nkentseu {

        // Classe exportée : les méthodes n'ont PAS besoin de macro d'export
        class NKENTSEU_TIME_CLASS_EXPORT NkDuration {
            public:

                // -------------------------------------------------------------
                // SOUS-SECTION 3.1 : Constructeurs
                // -------------------------------------------------------------
                // Tous les constructeurs sont constexpr pour permettre l'évaluation
                // à la compilation et l'utilisation dans des contextes constexpr.

                /// Constructeur par défaut : durée nulle (0 nanoseconde).
                constexpr NkDuration() noexcept
                    : mNanoseconds(0)
                {
                }

                /// Constructeur explicite depuis un nombre de nanosecondes.
                /// @param nanoseconds Valeur de la durée en nanosecondes (peut être négative)
                /// @note Explicit pour éviter les conversions implicites accidentelles.
                constexpr explicit NkDuration(int64 nanoseconds) noexcept
                    : mNanoseconds(nanoseconds)
                {
                }

                // Constructeur de copie par défaut (constexpr pour compatibilité)
                constexpr NkDuration(const NkDuration&) noexcept = default;

                // Opérateur d'affectation par défaut (constexpr pour compatibilité)
                constexpr NkDuration& operator=(const NkDuration&) noexcept = default;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.2 : Fabriques statiques (Factory Methods)
                // -------------------------------------------------------------
                // Méthodes de construction nommées pour une API expressive.
                // Toutes sont constexpr et noexcept pour performance et sécurité.
                // Surcharge int64/float64 pour flexibilité d'usage.

                /// Crée une durée depuis un nombre de nanosecondes.
                /// @param ns Valeur en nanosecondes
                /// @return Nouvelle instance NkDuration
                NKTIME_NODISCARD static constexpr NkDuration FromNanoseconds(int64 ns) noexcept {
                    return NkDuration(ns);
                }

                /// Crée une durée depuis un nombre de microsecondes (version entière).
                /// @param us Valeur en microsecondes
                /// @return Nouvelle instance NkDuration
                NKTIME_NODISCARD static constexpr NkDuration FromMicroseconds(int64 us) noexcept {
                    return NkDuration(us * time::NS_PER_MICROSECOND);
                }

                /// Crée une durée depuis un nombre de microsecondes (version flottante).
                /// @param us Valeur en microsecondes (peut avoir une partie décimale)
                /// @return Nouvelle instance NkDuration (tronquée en int64)
                NKTIME_NODISCARD static constexpr NkDuration FromMicroseconds(float64 us) noexcept {
                    return NkDuration(static_cast<int64>(us * static_cast<float64>(time::NS_PER_MICROSECOND)));
                }

                /// Crée une durée depuis un nombre de millisecondes (version entière).
                /// @param ms Valeur en millisecondes
                /// @return Nouvelle instance NkDuration
                NKTIME_NODISCARD static constexpr NkDuration FromMilliseconds(int64 ms) noexcept {
                    return NkDuration(ms * time::NS_PER_MILLISECOND);
                }

                /// Crée une durée depuis un nombre de millisecondes (version flottante).
                /// @param ms Valeur en millisecondes (peut avoir une partie décimale)
                /// @return Nouvelle instance NkDuration (tronquée en int64)
                NKTIME_NODISCARD static constexpr NkDuration FromMilliseconds(float64 ms) noexcept {
                    return NkDuration(static_cast<int64>(ms * static_cast<float64>(time::NS_PER_MILLISECOND)));
                }

                /// Crée une durée depuis un nombre de secondes (version entière).
                /// @param s Valeur en secondes
                /// @return Nouvelle instance NkDuration
                NKTIME_NODISCARD static constexpr NkDuration FromSeconds(int64 s) noexcept {
                    return NkDuration(s * time::NS_PER_SECOND);
                }

                /// Crée une durée depuis un nombre de secondes (version flottante).
                /// @param s Valeur en secondes (peut avoir une partie décimale)
                /// @return Nouvelle instance NkDuration (tronquée en int64)
                NKTIME_NODISCARD static constexpr NkDuration FromSeconds(float64 s) noexcept {
                    return NkDuration(static_cast<int64>(s * static_cast<float64>(time::NS_PER_SECOND)));
                }

                /// Crée une durée depuis un nombre de minutes (version entière).
                /// @param m Valeur en minutes
                /// @return Nouvelle instance NkDuration
                NKTIME_NODISCARD static constexpr NkDuration FromMinutes(int64 m) noexcept {
                    return NkDuration(m * time::NS_PER_MINUTE);
                }

                /// Crée une durée depuis un nombre de minutes (version flottante).
                /// @param m Valeur en minutes (peut avoir une partie décimale)
                /// @return Nouvelle instance NkDuration (tronquée en int64)
                NKTIME_NODISCARD static constexpr NkDuration FromMinutes(float64 m) noexcept {
                    return NkDuration(static_cast<int64>(m * static_cast<float64>(time::NS_PER_MINUTE)));
                }

                /// Crée une durée depuis un nombre d'heures (version entière).
                /// @param h Valeur en heures
                /// @return Nouvelle instance NkDuration
                NKTIME_NODISCARD static constexpr NkDuration FromHours(int64 h) noexcept {
                    return NkDuration(h * time::NS_PER_HOUR);
                }

                /// Crée une durée depuis un nombre d'heures (version flottante).
                /// @param h Valeur en heures (peut avoir une partie décimale)
                /// @return Nouvelle instance NkDuration (tronquée en int64)
                NKTIME_NODISCARD static constexpr NkDuration FromHours(float64 h) noexcept {
                    return NkDuration(static_cast<int64>(h * static_cast<float64>(time::NS_PER_HOUR)));
                }

                /// Crée une durée depuis un nombre de jours (version entière).
                /// @param d Valeur en jours
                /// @return Nouvelle instance NkDuration
                NKTIME_NODISCARD static constexpr NkDuration FromDays(int64 d) noexcept {
                    return NkDuration(d * time::NS_PER_DAY);
                }

                /// Crée une durée depuis un nombre de jours (version flottante).
                /// @param d Valeur en jours (peut avoir une partie décimale)
                /// @return Nouvelle instance NkDuration (tronquée en int64)
                NKTIME_NODISCARD static constexpr NkDuration FromDays(float64 d) noexcept {
                    return NkDuration(static_cast<int64>(d * static_cast<float64>(time::NS_PER_DAY)));
                }

                // -------------------------------------------------------------
                // SOUS-SECTION 3.3 : Conversions vers unités temporelles
                // -------------------------------------------------------------
                // Méthodes de conversion depuis la durée interne (nanosecondes)
                // vers différentes unités. Versions entières (tronquées) et
                // flottantes (précises) disponibles pour chaque unité.

                /// @return Durée en nanosecondes (valeur interne exacte)
                NKTIME_NODISCARD constexpr int64 ToNanoseconds() const noexcept {
                    return mNanoseconds;
                }

                /// @return Durée en microsecondes (tronquée vers zéro)
                NKTIME_NODISCARD constexpr int64 ToMicroseconds() const noexcept {
                    return mNanoseconds / time::NS_PER_MICROSECOND;
                }

                /// @return Durée en microsecondes (valeur flottante précise)
                NKTIME_NODISCARD constexpr float64 ToMicrosecondsF() const noexcept {
                    return static_cast<float64>(mNanoseconds) / static_cast<float64>(time::NS_PER_MICROSECOND);
                }

                /// @return Durée en millisecondes (tronquée vers zéro)
                NKTIME_NODISCARD constexpr int64 ToMilliseconds() const noexcept {
                    return mNanoseconds / time::NS_PER_MILLISECOND;
                }

                /// @return Durée en millisecondes (valeur flottante précise)
                NKTIME_NODISCARD constexpr float64 ToMillisecondsF() const noexcept {
                    return static_cast<float64>(mNanoseconds) / static_cast<float64>(time::NS_PER_MILLISECOND);
                }

                /// @return Durée en secondes (tronquée vers zéro)
                NKTIME_NODISCARD constexpr int64 ToSeconds() const noexcept {
                    return mNanoseconds / time::NS_PER_SECOND;
                }

                /// @return Durée en secondes (valeur flottante précise)
                NKTIME_NODISCARD constexpr float64 ToSecondsF() const noexcept {
                    return static_cast<float64>(mNanoseconds) / static_cast<float64>(time::NS_PER_SECOND);
                }

                /// @return Durée en minutes (tronquée vers zéro)
                NKTIME_NODISCARD constexpr int64 ToMinutes() const noexcept {
                    return mNanoseconds / time::NS_PER_MINUTE;
                }

                /// @return Durée en minutes (valeur flottante précise)
                NKTIME_NODISCARD constexpr float64 ToMinutesF() const noexcept {
                    return static_cast<float64>(mNanoseconds) / static_cast<float64>(time::NS_PER_MINUTE);
                }

                /// @return Durée en heures (tronquée vers zéro)
                NKTIME_NODISCARD constexpr int64 ToHours() const noexcept {
                    return mNanoseconds / time::NS_PER_HOUR;
                }

                /// @return Durée en heures (valeur flottante précise)
                NKTIME_NODISCARD constexpr float64 ToHoursF() const noexcept {
                    return static_cast<float64>(mNanoseconds) / static_cast<float64>(time::NS_PER_HOUR);
                }

                /// @return Durée en jours (tronquée vers zéro)
                NKTIME_NODISCARD constexpr int64 ToDays() const noexcept {
                    return mNanoseconds / time::NS_PER_DAY;
                }

                /// @return Durée en jours (valeur flottante précise)
                NKTIME_NODISCARD constexpr float64 ToDaysF() const noexcept {
                    return static_cast<float64>(mNanoseconds) / static_cast<float64>(time::NS_PER_DAY);
                }

                // -------------------------------------------------------------
                // SOUS-SECTION 3.4 : Opérateurs arithmétiques binaires
                // -------------------------------------------------------------
                // Opérateurs retournant un nouvel objet (sans modifier *this).
                // Tous sont constexpr et noexcept pour optimisation maximale.

                /// Addition de deux durées.
                /// @param o Durée à ajouter
                /// @return Nouvelle durée résultante
                NKTIME_NODISCARD constexpr NkDuration operator+(const NkDuration& o) const noexcept {
                    return NkDuration(mNanoseconds + o.mNanoseconds);
                }

                /// Soustraction de deux durées.
                /// @param o Durée à soustraire
                /// @return Nouvelle durée résultante
                NKTIME_NODISCARD constexpr NkDuration operator-(const NkDuration& o) const noexcept {
                    return NkDuration(mNanoseconds - o.mNanoseconds);
                }

                /// Multiplication par un scalaire flottant.
                /// @param s Facteur de multiplication
                /// @return Nouvelle durée résultante (tronquée en int64)
                NKTIME_NODISCARD constexpr NkDuration operator*(float64 s) const noexcept {
                    return NkDuration(static_cast<int64>(mNanoseconds * s));
                }

                /// Division par un scalaire flottant.
                /// @param s Diviseur
                /// @return Nouvelle durée résultante (tronquée en int64)
                NKTIME_NODISCARD constexpr NkDuration operator/(float64 s) const noexcept {
                    return NkDuration(static_cast<int64>(mNanoseconds / s));
                }

                /// Division de deux durées : retourne le ratio (sans unité).
                /// @param o Durée diviseur
                /// @return Ratio float64 de cette durée sur l'autre
                NKTIME_NODISCARD constexpr float64 operator/(const NkDuration& o) const noexcept {
                    return static_cast<float64>(mNanoseconds) / static_cast<float64>(o.mNanoseconds);
                }

                /// Négation unaire : inverse le signe de la durée.
                /// @return Durée de même magnitude mais signe opposé
                NKTIME_NODISCARD constexpr NkDuration operator-() const noexcept {
                    return NkDuration(-mNanoseconds);
                }

                // -------------------------------------------------------------
                // SOUS-SECTION 3.5 : Opérateurs arithmétiques composés
                // -------------------------------------------------------------
                // Opérateurs modifiant l'objet courant (in-place).
                // Retournent *this pour permettre le chaînage d'opérations.

                /// Ajout in-place d'une durée.
                /// @param o Durée à ajouter
                /// @return Référence vers cet objet pour chaînage
                constexpr NkDuration& operator+=(const NkDuration& o) noexcept {
                    mNanoseconds += o.mNanoseconds;
                    return *this;
                }

                /// Soustraction in-place d'une durée.
                /// @param o Durée à soustraire
                /// @return Référence vers cet objet pour chaînage
                constexpr NkDuration& operator-=(const NkDuration& o) noexcept {
                    mNanoseconds -= o.mNanoseconds;
                    return *this;
                }

                /// Multiplication in-place par un scalaire.
                /// @param s Facteur de multiplication
                /// @return Référence vers cet objet pour chaînage
                constexpr NkDuration& operator*=(float64 s) noexcept {
                    mNanoseconds = static_cast<int64>(mNanoseconds * s);
                    return *this;
                }

                /// Division in-place par un scalaire.
                /// @param s Diviseur
                /// @return Référence vers cet objet pour chaînage
                constexpr NkDuration& operator/=(float64 s) noexcept {
                    mNanoseconds = static_cast<int64>(mNanoseconds / s);
                    return *this;
                }

                // -------------------------------------------------------------
                // SOUS-SECTION 3.6 : Opérateurs de comparaison
                // -------------------------------------------------------------
                // Comparaisons basées sur la valeur interne en nanosecondes.
                // Toutes les méthodes sont constexpr et noexcept.

                /// Égalité : vrai si les deux durées ont la même valeur en ns.
                /// @param o Autre durée à comparer
                /// @return true si égales, false sinon
                NKTIME_NODISCARD constexpr bool operator==(const NkDuration& o) const noexcept {
                    return mNanoseconds == o.mNanoseconds;
                }

                /// Inégalité : inverse de operator==
                /// @param o Autre durée à comparer
                /// @return true si différentes, false sinon
                NKTIME_NODISCARD constexpr bool operator!=(const NkDuration& o) const noexcept {
                    return mNanoseconds != o.mNanoseconds;
                }

                /// Infériorité stricte : vrai si cette durée est plus courte.
                /// @param o Autre durée à comparer
                /// @return true si *this < o, false sinon
                NKTIME_NODISCARD constexpr bool operator<(const NkDuration& o) const noexcept {
                    return mNanoseconds < o.mNanoseconds;
                }

                /// Infériorité ou égalité
                /// @param o Autre durée à comparer
                /// @return true si *this <= o, false sinon
                NKTIME_NODISCARD constexpr bool operator<=(const NkDuration& o) const noexcept {
                    return mNanoseconds <= o.mNanoseconds;
                }

                /// Supériorité stricte : vrai si cette durée est plus longue.
                /// @param o Autre durée à comparer
                /// @return true si *this > o, false sinon
                NKTIME_NODISCARD constexpr bool operator>(const NkDuration& o) const noexcept {
                    return mNanoseconds > o.mNanoseconds;
                }

                /// Supériorité ou égalité
                /// @param o Autre durée à comparer
                /// @return true si *this >= o, false sinon
                NKTIME_NODISCARD constexpr bool operator>=(const NkDuration& o) const noexcept {
                    return mNanoseconds >= o.mNanoseconds;
                }

                // -------------------------------------------------------------
                // SOUS-SECTION 3.7 : Utilitaires de prédicat
                // -------------------------------------------------------------
                // Méthodes d'inspection rapide de l'état de la durée.

                /// Retourne la valeur absolue de cette durée (toujours positive).
                /// @return Nouvelle durée avec magnitude positive
                NKTIME_NODISCARD constexpr NkDuration Abs() const noexcept {
                    return NkDuration(mNanoseconds < 0 ? -mNanoseconds : mNanoseconds);
                }

                /// Teste si cette durée est négative.
                /// @return true si mNanoseconds < 0, false sinon
                NKTIME_NODISCARD constexpr bool IsNegative() const noexcept {
                    return mNanoseconds < 0;
                }

                /// Teste si cette durée est nulle.
                /// @return true si mNanoseconds == 0, false sinon
                NKTIME_NODISCARD constexpr bool IsZero() const noexcept {
                    return mNanoseconds == 0;
                }

                /// Teste si cette durée est positive.
                /// @return true si mNanoseconds > 0, false sinon
                NKTIME_NODISCARD constexpr bool IsPositive() const noexcept {
                    return mNanoseconds > 0;
                }

                // -------------------------------------------------------------
                // SOUS-SECTION 3.8 : Formatage et sérialisation
                // -------------------------------------------------------------
                // Méthodes de conversion vers des représentations textuelles.
                // Non-constexpr car elles utilisent snprintf (allocation dynamique).

                /// Formatage automatique avec unité adaptative.
                /// Choisit l'unité la plus lisible (ns/us/ms/s/min/h/d).
                /// @return NkString contenant la représentation textuelle
                NKTIME_NODISCARD NkString ToString() const;

                /// Formatage précis : toujours en nanosecondes.
                /// Utile pour le logging debug ou les comparaisons exactes.
                /// @return NkString au format "X ns"
                NKTIME_NODISCARD NkString ToStringPrecise() const;

                // -------------------------------------------------------------
                // SOUS-SECTION 3.9 : Constantes prédéfinies
                // -------------------------------------------------------------
                // Valeurs communes pour éviter la répétition de code.

                /// Durée nulle (0 nanoseconde).
                /// @return Instance constexpr représentant zéro
                NKTIME_NODISCARD static constexpr NkDuration Zero() noexcept {
                    return NkDuration(0);
                }

                /// Durée maximale représentable (INT64_MAX nanosecondes).
                /// @return Instance constexpr représentant la valeur maximale
                NKTIME_NODISCARD static constexpr NkDuration Max() noexcept {
                    return NkDuration(NKENTSEU_INT64_MAX);
                }

                /// Durée minimale représentable (INT64_MIN nanosecondes).
                /// @return Instance constexpr représentant la valeur minimale
                NKTIME_NODISCARD static constexpr NkDuration Min() noexcept {
                    return NkDuration(NKENTSEU_INT64_MIN);
                }

            private:

                // -------------------------------------------------------------
                // SOUS-SECTION 3.10 : Membre privé
                // -------------------------------------------------------------
                // Stockage interne unique : durée en nanosecondes (int64).
                // Permet une représentation compacte et des opérations efficaces.

                int64 mNanoseconds;

        }; // class NkDuration

        // -------------------------------------------------------------------------
        // SECTION 4 : OPÉRATEURS NON-MEMBRES
        // -------------------------------------------------------------------------
        // Opérateurs globaux pour supporter la syntaxe scalaire * durée.
        // Définis hors de la classe pour permettre l'ADL et la symétrie.

        /// Multiplication scalaire * durée (ordre inversé).
        /// @param scalar Facteur multiplicatif
        /// @param d Durée à multiplier
        /// @return Nouvelle durée résultante
        /// @note Permet d'écrire : 2.5 * duration au lieu de duration * 2.5
        NKTIME_NODISCARD NKENTSEU_INLINE constexpr NkDuration operator*(float64 scalar, const NkDuration& d) noexcept {
            return d * scalar;
        }

    } // namespace nkentseu

#endif // NKENTSEU_TIME_NKDURATION_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKDURATION.H
// =============================================================================
// La classe NkDuration permet de spécifier des durées temporelles avec une
// précision nanoseconde, pour les APIs de sleep, timeout, et délais divers.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Création via les fabriques statiques
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDuration.h"

    void DurationCreationExample() {
        using namespace nkentseu;

        // Création depuis différentes unités
        NkDuration d1 = NkDuration::FromMilliseconds(500);      // 500 ms
        NkDuration d2 = NkDuration::FromSeconds(2.5f);          // 2.5 s
        NkDuration d3 = NkDuration::FromMinutes(1);             // 1 min
        NkDuration d4 = NkDuration::FromMicroseconds(1500.75);  // 1500.75 us

        // Création directe depuis nanosecondes
        NkDuration d5 = NkDuration::FromNanoseconds(1'000'000); // 1 ms

        // Utilisation dans une API de sleep (exemple hypothétique)
        // NkChrono::Sleep(d1);  // Endort le thread pendant 500ms
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Conversions entre unités
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDuration.h"
    #include "NKCore/Logger/NkLogger.h"

    void DurationConversionExample() {
        using namespace nkentseu;

        NkDuration timeout = NkDuration::FromSeconds(30);

        // Conversions entières (tronquées)
        int64 ms = timeout.ToMilliseconds();    // 30000
        int64 s  = timeout.ToSeconds();         // 30

        // Conversions flottantes (précises)
        float64 preciseMs = timeout.ToMillisecondsF();  // 30000.0
        float64 preciseS  = timeout.ToSecondsF();       // 30.0

        // Conversion vers nanosecondes (valeur interne exacte)
        int64 ns = timeout.ToNanoseconds();  // 30'000'000'000

        NK_LOG_INFO("Timeout: {} ms / {} s / {} ns", ms, s, ns);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Arithmétique des durées
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDuration.h"

    void DurationArithmeticExample() {
        using namespace nkentseu;

        // Durées de base
        NkDuration base = NkDuration::FromSeconds(10);
        NkDuration offset = NkDuration::FromMilliseconds(500);

        // Addition et soustraction
        NkDuration total = base + offset;        // 10.5 s
        NkDuration diff  = base - offset;        // 9.5 s

        // Opérateurs composés (modification in-place)
        NkDuration accumulator = NkDuration::Zero();
        accumulator += NkDuration::FromSeconds(1);
        accumulator += NkDuration::FromMilliseconds(250);
        // accumulator vaut maintenant 1.25 s

        // Multiplication et division par scalaire
        NkDuration doubled = base * 2.0;         // 20 s
        NkDuration halved  = base / 2.0;         // 5 s
        NkDuration scaled  = 1.5 * base;         // 15 s (opérateur non-membre)

        // Ratio entre deux durées (sans unité)
        float64 ratio = total / base;            // 1.5
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Comparaisons et prédicats
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDuration.h"
    #include <algorithm>

    void DurationComparisonExample() {
        using namespace nkentseu;

        NkDuration shortDur = NkDuration::FromMilliseconds(100);
        NkDuration longDur  = NkDuration::FromSeconds(5);

        // Comparaisons directes
        if (shortDur < longDur) {
            // Court est bien plus court que long
        }

        // Prédicats d'état
        if (shortDur.IsPositive() && !shortDur.IsZero()) {
            // Durée valide et non-nulle
        }

        // Valeur absolue pour durées potentiellement négatives
        NkDuration negative = NkDuration::FromSeconds(-10);
        NkDuration positive = negative.Abs();  // 10 s (positif)

        // Tri de durées dans un conteneur
        std::vector<NkDuration> delays = {
            NkDuration::FromSeconds(3),
            NkDuration::FromMilliseconds(500),
            NkDuration::FromSeconds(1)
        };
        std::sort(delays.begin(), delays.end());
        // delays est maintenant trié : 500ms, 1s, 3s
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Formatage pour logging et UI
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDuration.h"
    #include "NKCore/Logger/NkLogger.h"

    void DurationFormattingExample() {
        using namespace nkentseu;

        // Formatage automatique (unité adaptative)
        NkDuration d1 = NkDuration::FromNanoseconds(500);
        NkDuration d2 = NkDuration::FromMilliseconds(1234);
        NkDuration d3 = NkDuration::FromSeconds(90);

        NK_LOG_INFO("d1: {}", d1.ToString());  // "500ns"
        NK_LOG_INFO("d2: {}", d2.ToString());  // "1.23ms"
        NK_LOG_INFO("d3: {}", d3.ToString());  // "1.50min"

        // Formatage précis (toujours en nanosecondes)
        NK_LOG_INFO("d1 precise: {}", d1.ToStringPrecise());  // "500 ns"
        NK_LOG_INFO("d2 precise: {}", d2.ToStringPrecise());  // "1234000000 ns"

        // Utilisation dans une interface utilisateur
        NkString uiLabel = NkString("Délai: ") + d3.ToString();
        // uiLabel = "Délai: 1.50min"
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Utilisation dans des APIs de timeout et sleep
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDuration.h"

    namespace nkentseu {
    namespace network {

        // Exemple d'API réseau utilisant NkDuration pour les timeouts
        class TcpClient {
        public:
            // Connection avec timeout configurable
            bool Connect(const char* host, uint16 port, NkDuration timeout) {
                if (timeout.IsZero() || timeout.IsNegative()) {
                    // Timeout invalide : utiliser une valeur par défaut
                    timeout = NkDuration::FromSeconds(30);
                }

                // Implémentation de la connexion avec timeout...
                // (code réseau hypothétique)
                return true;
            }

            // Envoi de données avec timeout d'écriture
            bool Send(const void* data, size_t size, NkDuration writeTimeout) {
                // Attente maximale de writeTimeout pour l'envoi complet
                // Retourne false si timeout expiré
                return true;
            }
        };

    } // namespace network

    namespace threading {

        // Exemple d'API de threading utilisant NkDuration pour sleep
        class Thread {
        public:
            // Endort le thread courant pour la durée spécifiée
            static void Sleep(NkDuration duration) {
                if (duration.IsNegative()) {
                    duration = duration.Abs();  // Normalisation défensive
                }
                // Appel système de sleep avec conversion vers l'unité native...
            }

            // Attente sur condition avec timeout
            bool WaitFor(NkDuration maxWait) {
                // Retourne true si la condition est satisfaite avant timeout
                // Retourne false si maxWait est écoulé
                return true;
            }
        };

    } // namespace threading
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Constantes prédéfinies et valeurs sentinelles
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDuration.h"

    void DurationConstantsExample() {
        using namespace nkentseu;

        // Utilisation des constantes prédéfinies
        NkDuration zero = NkDuration::Zero();  // Durée nulle

        // Vérification rapide avec IsZero()
        if (zero.IsZero()) {
            // Traitement spécial pour durée nulle (pas d'attente)
        }

        // Valeurs extrêmes pour configuration
        NkDuration infinite = NkDuration::Max();   // Timeout "infini"
        NkDuration invalid  = NkDuration::Min();   // Valeur sentinelle d'erreur

        // Pattern : timeout par défaut avec override optionnel
        void ProcessWithTimeout(NkDuration customTimeout = NkDuration::Zero()) {
            NkDuration effective = customTimeout.IsZero()
                ? NkDuration::FromSeconds(30)  // Valeur par défaut
                : customTimeout;                // Valeur personnalisée

            // Utilisation de effective...
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Intégration avec NkElapsedTime (mesure vs spécification)
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkDuration.h"
    // #include "NKTime/NkElapsedTime.h"  // Module séparé

    namespace nkentseu {

        // Pattern : spécifier une durée, mesurer un résultat
        void MeasureExecution() {
            // Étape 1 : SPÉCIFIER une durée cible avec NkDuration
            NkDuration targetDuration = NkDuration::FromMilliseconds(100);

            // Étape 2 : Exécuter et MESURER avec NkElapsedTime (hypothétique)
            // NkElapsedTime elapsed = Chrono::Measure([]() {
            //     // Code à mesurer...
            // });

            // Étape 3 : Comparer mesure vs cible
            // NkDuration elapsedAsDuration =
            //     NkDuration::FromNanoseconds(static_cast<int64>(elapsed.nanoseconds));

            // if (elapsedAsDuration > targetDuration) {
            //     NK_LOG_WARN("Execution exceeded target: {} > {}",
            //                 elapsedAsDuration.ToString(),
            //                 targetDuration.ToString());
            // }
        }

        // Conversion entre les deux types si nécessaire
        NkDuration ConvertElapsedTimeToDuration(float64 elapsedNs) {
            return NkDuration::FromNanoseconds(static_cast<int64>(elapsedNs));
        }

    } // namespace nkentseu
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================