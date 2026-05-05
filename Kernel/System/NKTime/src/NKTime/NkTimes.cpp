// =============================================================================
// NKTime/NkTimes.cpp
// Implémentation de la classe NkTime.
//
// Design :
//  - Toutes les méthodes respectent la noexcept-specification du header
//  - Utilisation exclusive des constantes de NkTimeConstants.h
//  - Gestion multiplateforme via NKPlatform/NkPlatformDetect.h
//  - Assertions NKENTSEU_ASSERT_MSG pour le debugging en mode développement
//
// Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
// Date : 2024-2026
// License : Apache-2.0
// =============================================================================

// -------------------------------------------------------------------------
// SECTION 1 : INCLUSIONS (ordre strict requis)
// -------------------------------------------------------------------------
// 1. Precompiled header en premier (obligatoire pour la compilation MSVC/Clang)
// 2. Header correspondant au fichier .cpp
// 3. Headers du projet NKEntseu
// 4. Headers système

#include "pch.h"
#include "NKTime/NkTimes.h"
#include "NKCore/Assert/NkAssert.h"

#include <cstdio>
#include <ctime>

// Inclusion conditionnelle des headers Windows
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE ET USING
// -------------------------------------------------------------------------
// Utilisation du namespace time pour accéder aux constantes de conversion.
// Limité à ce fichier .cpp pour éviter la pollution des espaces de noms.

namespace nkentseu {

    // Alias local pour raccourcir l'accès aux constantes (scope fichier uniquement)
    using namespace time;

    // =============================================================================
    //  Constructeurs
    // =============================================================================

    NkTime::NkTime() noexcept
        : mHour(0)
        , mMinute(0)
        , mSecond(0)
        , mMillisecond(0)
        , mNanosecond(0)
    {
        // Initialisation explicite à minuit (00:00:00.000.000000)
        // La liste d'initialisation garantit l'ordre et l'efficacité
    }

    NkTime::NkTime(
        int32 hour,
        int32 minute,
        int32 second,
        int32 millisecond,
        int32 nanosecond
    )
        : mHour(hour)
        , mMinute(minute)
        , mSecond(second)
        , mMillisecond(millisecond)
        , mNanosecond(nanosecond)
    {
        // Normalisation automatique des valeurs potentiellement hors plage
        Normalize();
    }

    NkTime::NkTime(int64 totalNanoseconds) noexcept {
        // Extraction de la composante nanoseconde résiduelle [0, 999999]
        mNanosecond = static_cast<int32>(totalNanoseconds % NANOSECONDS_PER_MILLISECOND);
        totalNanoseconds /= NANOSECONDS_PER_MILLISECOND;

        // Extraction de la composante milliseconde [0, 999]
        mMillisecond = static_cast<int32>(totalNanoseconds % MILLISECONDS_PER_SECOND);
        totalNanoseconds /= MILLISECONDS_PER_SECOND;

        // Extraction de la composante seconde [0, 59]
        mSecond = static_cast<int32>(totalNanoseconds % SECONDS_PER_MINUTE_I32);
        totalNanoseconds /= SECONDS_PER_MINUTE_I32;

        // Extraction de la composante minute [0, 59]
        mMinute = static_cast<int32>(totalNanoseconds % MINUTES_PER_HOUR);
        totalNanoseconds /= MINUTES_PER_HOUR;

        // Extraction de la composante heure [0, 23] avec modulo pour bouclage
        mHour = static_cast<int32>(totalNanoseconds % HOURS_PER_DAY);
    }

    // =============================================================================
    //  Mutateurs (Setters)
    // =============================================================================
    // Chaque setter valide l'entrée via NKENTSEU_ASSERT_MSG en mode debug.
    // En release, les assertions sont désactivées pour la performance.

    void NkTime::SetHour(int32 hour) {
        NKENTSEU_ASSERT_MSG(
            hour >= 0 && hour < HOURS_PER_DAY,
            "Hour must be in range [0, 23]"
        );
        mHour = hour;
    }

    void NkTime::SetMinute(int32 minute) {
        NKENTSEU_ASSERT_MSG(
            minute >= 0 && minute < MINUTES_PER_HOUR,
            "Minute must be in range [0, 59]"
        );
        mMinute = minute;
    }

    void NkTime::SetSecond(int32 second) {
        NKENTSEU_ASSERT_MSG(
            second >= 0 && second < SECONDS_PER_MINUTE_I32,
            "Second must be in range [0, 59]"
        );
        mSecond = second;
    }

    void NkTime::SetMillisecond(int32 millis) {
        NKENTSEU_ASSERT_MSG(
            millis >= 0 && millis < MILLISECONDS_PER_SECOND,
            "Millisecond must be in range [0, 999]"
        );
        mMillisecond = millis;
    }

    void NkTime::SetNanosecond(int32 nano) {
        NKENTSEU_ASSERT_MSG(
            nano >= 0 && nano < NANOSECONDS_PER_MILLISECOND,
            "Nanosecond must be in range [0, 999999]"
        );
        mNanosecond = nano;
    }

    // =============================================================================
    //  Opérateurs arithmétiques
    // =============================================================================
    // Implémentation via conversion en int64 (nanosecondes totales) pour :
    //  - Garantir la cohérence des calculs
    //  - Gérer automatiquement les dépassements de plage
    //  - Simplifier la logique de propagation des retenues

    NkTime& NkTime::operator+=(const NkTime& o) noexcept {
        // Conversion des deux opérandes en nanosecondes totales
        // Addition puis reconstruction via le constructeur int64
        *this = NkTime(static_cast<int64>(*this) + static_cast<int64>(o));
        return *this;
    }

    NkTime& NkTime::operator-=(const NkTime& o) noexcept {
        // Soustraction avec gestion des résultats négatifs
        // Le constructeur int64 normalise via modulo pour boucler sur 24h
        *this = NkTime(static_cast<int64>(*this) - static_cast<int64>(o));
        return *this;
    }

    NkTime NkTime::operator+(const NkTime& o) const noexcept {
        // Version const retournant un nouvel objet (sans modifier *this)
        return NkTime(static_cast<int64>(*this) + static_cast<int64>(o));
    }

    NkTime NkTime::operator-(const NkTime& o) const noexcept {
        // Version const retournant un nouvel objet (sans modifier *this)
        return NkTime(static_cast<int64>(*this) - static_cast<int64>(o));
    }

    // =============================================================================
    //  Opérateurs de comparaison
    // =============================================================================
    // Toutes les comparaisons sont déléguées à la conversion int64 pour :
    //  - Assurer une sémantique cohérente (ordre chronologique)
    //  - Éviter la duplication de logique de comparaison composant par composant
    //  - Bénéficier de l'optimisation des comparaisons entières par le CPU

    bool NkTime::operator==(const NkTime& o) const noexcept {
        return static_cast<int64>(*this) == static_cast<int64>(o);
    }

    bool NkTime::operator!=(const NkTime& o) const noexcept {
        return !(*this == o);
    }

    bool NkTime::operator<(const NkTime& o) const noexcept {
        return static_cast<int64>(*this) < static_cast<int64>(o);
    }

    bool NkTime::operator<=(const NkTime& o) const noexcept {
        return !(o < *this);
    }

    bool NkTime::operator>(const NkTime& o) const noexcept {
        return (o < *this);
    }

    bool NkTime::operator>=(const NkTime& o) const noexcept {
        return !(*this < o);
    }

    // =============================================================================
    //  Conversions de type
    // =============================================================================

    NkTime::operator int64() const noexcept {
        // Calcul du total de secondes depuis minuit
        const int64 totalSec =
            static_cast<int64>(mSecond)
            + static_cast<int64>(SECONDS_PER_MINUTE_I32) * (
                static_cast<int64>(mMinute)
                + static_cast<int64>(MINUTES_PER_HOUR) * static_cast<int64>(mHour)
            );

        // Combinaison avec les sous-secondes pour obtenir les nanosecondes totales
        return static_cast<int64>(mNanosecond)
            + static_cast<int64>(mMillisecond) * static_cast<int64>(NANOSECONDS_PER_MILLISECOND)
            + totalSec * static_cast<int64>(NS_PER_SECOND);
    }

    NkTime::operator double() const noexcept {
        // Conversion précise en secondes avec partie décimale nanoseconde
        return static_cast<double>(static_cast<int64>(*this)) / 1.0e9;
    }

    NkTime::operator float() const noexcept {
        // Conversion en simple précision (perte de précision nanoseconde acceptée)
        return static_cast<float>(static_cast<double>(*this));
    }

    // =============================================================================
    //  Méthode statique : GetCurrent
    // =============================================================================

    NkTime NkTime::GetCurrent() {
        // Structure POSIX pour timestamp haute résolution
        timespec ts = {};

        // -------------------------------------------------------------------------
        // Acquisition du timestamp système (implémentation multiplateforme)
        // -------------------------------------------------------------------------
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            // Windows : utilisation de GetSystemTimeAsFileTime (précision 100ns)
            FILETIME fileTime{};
            ::GetSystemTimeAsFileTime(&fileTime);

            // Combinaison des parties haute et basse en entier 64 bits
            ULARGE_INTEGER ticks{};
            ticks.LowPart = fileTime.dwLowDateTime;
            ticks.HighPart = fileTime.dwHighDateTime;

            // Offset entre l'époque Windows (1601) et Unix (1970) en nanosecondes
            constexpr uint64 kWinToUnixEpochOffsetNs = 11644473600ULL * 1000000000ULL;

            // Conversion des ticks Windows (100ns) vers nanosecondes Unix
            const uint64 nowNs = static_cast<uint64>(ticks.QuadPart) * 100ULL;
            const uint64 unixNs = (nowNs >= kWinToUnixEpochOffsetNs)
                ? (nowNs - kWinToUnixEpochOffsetNs)
                : 0ULL;

            // Remplissage de la structure timespec
            ts.tv_sec = static_cast<time_t>(unixNs / NS_PER_SECOND);
            ts.tv_nsec = static_cast<long>(unixNs % NS_PER_SECOND);
        #else
            // POSIX (Linux, macOS, BSD) : clock_gettime avec CLOCK_REALTIME
            ::clock_gettime(CLOCK_REALTIME, &ts);
        #endif

        // -------------------------------------------------------------------------
        // Conversion UTC vers heure locale
        // -------------------------------------------------------------------------
        const time_t utcSec = static_cast<time_t>(ts.tv_sec);
        tm local = {};

        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            // Windows : version thread-safe de localtime
            ::localtime_s(&local, &utcSec);
        #else
            // POSIX : version thread-safe de localtime
            ::localtime_r(&utcSec, &local);
        #endif

        // -------------------------------------------------------------------------
        // Construction et retour de l'objet NkTime
        // -------------------------------------------------------------------------
        return NkTime(
            local.tm_hour,
            local.tm_min,
            local.tm_sec,
            static_cast<int32>(ts.tv_nsec / 1'000'000L),      // Millisecondes
            static_cast<int32>(ts.tv_nsec % 1'000'000L)       // Nanosecondes résiduelles
        );
    }

    // =============================================================================
    //  Formatage : ToString
    // =============================================================================

    NkString NkTime::ToString() const {
        // Buffer local pour le formatage (taille suffisante pour le format)
        char buf[32];

        // Formatage ISO 8601 étendu : HH:MM:SS.mmm.nnnnnn
        ::snprintf(
            buf,
            sizeof(buf),
            "%02d:%02d:%02d.%03d.%06d",
            mHour,
            mMinute,
            mSecond,
            mMillisecond,
            mNanosecond % NANOSECONDS_PER_MILLISECOND
        );

        // Construction de la chaîne NKEntseu depuis le buffer C
        return NkString(buf);
    }

    // =============================================================================
    //  Méthodes internes privées
    // =============================================================================

    void NkTime::Normalize() noexcept {
        // Application de modulo pour ramener chaque composant dans sa plage valide
        // L'ordre est important : on commence par les plus petites unités

        mNanosecond %= NANOSECONDS_PER_MILLISECOND;
        mMillisecond %= MILLISECONDS_PER_SECOND;
        mSecond %= SECONDS_PER_MINUTE_I32;
        mMinute %= MINUTES_PER_HOUR;
        mHour %= HOURS_PER_DAY;

        // Note : Cette normalisation simple par modulo ne gère pas les valeurs négatives
        // Pour une gestion complète, ajouter une correction post-modulo si nécessaire
    }

    void NkTime::Validate() const {
        // Assertions de validation pour le mode debug uniquement
        // Ces vérifications aident à détecter les bugs durant le développement

        NKENTSEU_ASSERT_MSG(
            mHour >= 0 && mHour < HOURS_PER_DAY,
            "Invalid hour value"
        );
        NKENTSEU_ASSERT_MSG(
            mMinute >= 0 && mMinute < MINUTES_PER_HOUR,
            "Invalid minute value"
        );
        NKENTSEU_ASSERT_MSG(
            mSecond >= 0 && mSecond < SECONDS_PER_MINUTE_I32,
            "Invalid second value"
        );
        NKENTSEU_ASSERT_MSG(
            mMillisecond >= 0 && mMillisecond < MILLISECONDS_PER_SECOND,
            "Invalid millisecond value"
        );
        NKENTSEU_ASSERT_MSG(
            mNanosecond >= 0 && mNanosecond < NANOSECONDS_PER_MILLISECOND,
            "Invalid nanosecond value"
        );
    }

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION
// =============================================================================
/*
    Performance :
    - Toutes les méthodes inline du header sont définies in-class pour permettre
      l'inlining par le compilateur sans coût d'appel de fonction.

    Thread-safety :
    - NkTime::GetCurrent() est thread-safe car elle utilise uniquement des
      fonctions système thread-safe (localtime_s/r, clock_gettime).
    - Les instances de NkTime ne sont PAS thread-safe : synchronisation externe
      requise pour l'accès concurrent à une même instance.

    Précision :
    - La précision nanoseconde est supportée sur les plateformes modernes.
    - Sur Windows, la précision effective dépend de la résolution du timer système.
    - Sur Linux, clock_gettime(CLOCK_REALTIME) offre typiquement une précision
      nanoseconde si le noyau et le matériel le supportent.

    Portabilité :
    - Aucune dépendance STL pour compatibilité avec les environnements embarqués.
    - Gestion conditionnelle des APIs Windows vs POSIX via NKPlatform.
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================