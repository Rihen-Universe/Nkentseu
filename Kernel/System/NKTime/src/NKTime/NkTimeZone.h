// =============================================================================
// NKTime/NkTimeZone.h
// Fuseaux horaires et conversions UTC/local — sans dépendance STL.
//
// Design :
//  - Trois modes supportés :
//    * Local       : Fuseau système, DST géré par l'OS via localtime_r/s
//    * Utc         : UTC fixe, offset = 0, jamais de DST
//    * FixedOffset : Offset fixe en secondes (ex: UTC+5:30), jamais de DST
//  - Thread-safety : NkTimeZone est immuable après construction
//    Toutes les méthodes const sont thread-safe sans verrou
//  - Correction : ToLocal/ToUtc(NkTime) acceptent une NkDate optionnelle
//    pour le calcul DST, évitant l'effet de bord de GetCurrent()
//
// Règles d'application des macros :
//  - NKENTSEU_TIME_CLASS_EXPORT sur la classe uniquement
//  - PAS de NKENTSEU_TIME_API sur les méthodes (l'export de classe suffit)
//  - PAS de macro sur les fonctions inline définies dans le header
//
// Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
// Date : 2024-2026
// License : Apache-2.0
// =============================================================================

#pragma once

#ifndef NKENTSEU_TIME_NKTIMEZONE_H
#define NKENTSEU_TIME_NKTIMEZONE_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion de l'API d'export NKTime (remplace l'ancien NkTimeExport.h)
    // Inclusion des constantes de conversion centralisées
    // Inclusion des détections plateforme et types de base
    // Inclusion des classes temporelles pour les conversions

    #include "NKTime/NkTimeApi.h"
    #include "NKTime/NkTimeConstants.h"
    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKCore/NkTypes.h"
    #include "NKContainers/String/NkString.h"
    #include "NKTime/NkTimeSpan.h"
    #include "NKTime/NkDate.h"
    #include "NKTime/NkTimes.h"

    // -------------------------------------------------------------------------
    // SECTION 3 : NAMESPACE PRINCIPAL ET DÉCLARATION DE LA CLASSE
    // -------------------------------------------------------------------------
    // La classe NkTimeZone représente un fuseau horaire avec support des
    // conversions UTC/local et de l'heure d'été (DST) pour le mode Local.

    namespace nkentseu {

        // Classe exportée : les méthodes n'ont PAS besoin de macro d'export
        class NKENTSEU_TIME_CLASS_EXPORT NkTimeZone {

        public:

            // -------------------------------------------------------------
            // SOUS-SECTION 3.1 : Type de fuseau (enum class)
            // -------------------------------------------------------------
            // Définition des trois modes de fuseaux supportés.
            // Utilise uint8 pour un stockage compact et un alignement optimal.

            enum class NkKind : uint8 {
                NK_LOCAL = 0,        ///< Fuseau système (DST possible via OS)
                NK_UTC = 1,          ///< UTC fixe (offset = 0, pas de DST)
                NK_FIXED_OFFSET = 2  ///< Offset fixe en secondes (pas de DST)
            };

            // -------------------------------------------------------------
            // SOUS-SECTION 3.2 : Fabriques statiques (Factory Methods)
            // -------------------------------------------------------------
            // Méthodes de construction nommées pour une API expressive.
            // Toutes sont noexcept pour sécurité et performance.

            /// Obtient le fuseau système local.
            /// @return Instance NkTimeZone configurée pour le fuseau local
            /// @note Le DST est géré automatiquement par l'OS via localtime_r/s
            NKTIME_NODISCARD static NkTimeZone GetLocal() noexcept;

            /// Obtient le fuseau UTC fixe.
            /// @return Instance NkTimeZone avec offset = 0 et pas de DST
            NKTIME_NODISCARD static NkTimeZone GetUtc() noexcept;

            /**
             * @brief Crée un fuseau depuis un nom IANA ou un offset fixe.
             * @param ianaName Nom du fuseau : "Local", "UTC", "GMT", "Z",
             *                 "Etc/UTC", "UTC+HH", "UTC+HH:MM", "GMT-HH", etc.
             * @return Instance NkTimeZone configurée selon le nom fourni
             * @note Sans base de données IANA complète, les noms inconnus
             *       sont redirigés vers le fuseau local du système.
             */
            NKTIME_NODISCARD static NkTimeZone FromName(const NkString& ianaName) noexcept;

            // -------------------------------------------------------------
            // SOUS-SECTION 3.3 : Conversions temporelles (NkTime)
            // -------------------------------------------------------------
            // Méthodes de conversion entre UTC et temps local pour NkTime.
            // Le paramètre refDate est explicite pour éviter les effets de bord.

            /**
             * @brief Convertit un temps UTC en temps local.
             * @param utcTime Temps d'entrée en UTC
             * @param refDate Date de référence pour le calcul DST (optionnel)
             * @return NkTime représentant l'heure locale équivalente
             * @note Le paramètre refDate permet un calcul DST précis sans
             *       appeler NkDate::GetCurrent() en interne (effet de bord).
             */
            NKTIME_NODISCARD NkTime ToLocal(
                const NkTime& utcTime,
                const NkDate& refDate
            ) const noexcept;

            /**
             * @brief Convertit un temps local en UTC.
             * @param localTime Temps d'entrée en heure locale
             * @param refDate Date de référence pour le calcul DST (optionnel)
             * @return NkTime représentant l'heure UTC équivalente
             * @note Inverse de ToLocal : soustrait l'offset au lieu de l'ajouter.
             */
            NKTIME_NODISCARD NkTime ToUtc(
                const NkTime& localTime,
                const NkDate& refDate
            ) const noexcept;

            // -------------------------------------------------------------
            // SOUS-SECTION 3.4 : Conversions calendaires (NkDate)
            // -------------------------------------------------------------
            // Méthodes de conversion entre UTC et local pour les dates.
            // Ne gèrent pas l'heure, uniquement le changement de jour potentiel.

            /// Convertit une date UTC en date locale.
            /// @param utcDate Date d'entrée en UTC
            /// @return NkDate représentant la date locale équivalente
            /// @note Peut changer de jour si l'offset traverse minuit
            NKTIME_NODISCARD NkDate ToLocal(const NkDate& utcDate) const noexcept;

            /// Convertit une date locale en UTC.
            /// @param localDate Date d'entrée en heure locale
            /// @return NkDate représentant la date UTC équivalente
            /// @note Peut changer de jour si l'offset traverse minuit
            NKTIME_NODISCARD NkDate ToUtc(const NkDate& localDate) const noexcept;

            // -------------------------------------------------------------
            // SOUS-SECTION 3.5 : Informations sur le fuseau
            // -------------------------------------------------------------
            // Méthodes d'inspection des propriétés du fuseau horaire.

            /// Obtient le nom descriptif du fuseau.
            /// @return Référence constante vers le nom interne (pas de copie)
            /// @note Retourne "Local", "UTC", ou le nom IANA fourni
            NKTIME_NODISCARD const NkString& GetName() const noexcept;

            /// Obtient le type de fuseau.
            /// @return Valeur de l'enum NkKind (NK_LOCAL, NK_UTC, NK_FIXED_OFFSET)
            NKTIME_NODISCARD NkKind GetKind() const noexcept;

            /// Obtient l'offset fixe en secondes (pour le mode NK_FIXED_OFFSET).
            /// @return Offset en secondes (peut être négatif pour UTC-Ouest)
            /// @note Retourne 0 pour les modes NK_LOCAL et NK_UTC
            NKTIME_NODISCARD int32 GetFixedOffsetSeconds() const noexcept;

            /**
             * @brief Obtient l'offset UTC pour une date donnée.
             * @param date Date de référence (utilisée pour le calcul DST en mode Local)
             * @return NkTimeSpan représentant l'offset (peut être négatif)
             * @note Pour NK_LOCAL, l'offset varie selon la date (DST).
             *       Pour NK_UTC et NK_FIXED_OFFSET, l'offset est constant.
             */
            NKTIME_NODISCARD NkTimeSpan GetUtcOffset(const NkDate& date) const noexcept;

            /**
             * @brief Vérifie si la date tombe en heure d'été (DST).
             * @param date Date à tester
             * @return true si DST actif, false sinon
             * @note Retourne toujours false pour NK_UTC et NK_FIXED_OFFSET.
             *       Pour NK_LOCAL, délègue à l'OS via tm_isdst.
             */
            NKTIME_NODISCARD bool IsDaylightSavingTime(const NkDate& date) const noexcept;

            // -------------------------------------------------------------
            // SOUS-SECTION 3.6 : Opérateurs de comparaison
            // -------------------------------------------------------------
            // Comparaisons d'égalité entre instances de NkTimeZone.
            // Deux fuseaux sont égaux s'ils ont même type, nom et offset.

            /// Égalité : vrai si tous les membres sont identiques.
            /// @param o Autre fuseau à comparer
            /// @return true si égaux, false sinon
            NKTIME_NODISCARD bool operator==(const NkTimeZone& o) const noexcept;

            /// Inégalité : inverse de operator==
            /// @param o Autre fuseau à comparer
            /// @return true si différents, false sinon
            NKTIME_NODISCARD bool operator!=(const NkTimeZone& o) const noexcept;

        private:

            // -------------------------------------------------------------
            // SOUS-SECTION 3.7 : Constructeur privé
            // -------------------------------------------------------------
            // Constructeur interne utilisé par les fabriques statiques.
            // Empêche la construction directe avec des paramètres invalides.

            /// Constructeur privé avec paramètres de configuration.
            /// @param kind Type de fuseau (NK_LOCAL, NK_UTC, NK_FIXED_OFFSET)
            /// @param name Nom descriptif du fuseau
            /// @param fixedOffsetSeconds Offset fixe en secondes (pour NK_FIXED_OFFSET)
            NkTimeZone(
                NkKind kind,
                const NkString& name,
                int32 fixedOffsetSeconds = 0
            ) noexcept;

            // -------------------------------------------------------------
            // SOUS-SECTION 3.8 : Membres privés
            // -------------------------------------------------------------
            // Stockage interne des propriétés du fuseau.
            // L'ordre est optimisé pour l'alignement mémoire (8+4+4 = 16 bytes).

            NkKind mKind;                   ///< Type de fuseau (1 byte + padding)
            NkString mName;                 ///< Nom descriptif (variable size)
            int32 mFixedOffsetSeconds;      ///< Offset fixe en secondes (4 bytes)

        }; // class NkTimeZone

    } // namespace nkentseu

#endif // NKENTSEU_TIME_NKTIMEZONE_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKTIMEZONE.H
// =============================================================================
// La classe NkTimeZone permet de gérer les conversions entre UTC et les
// fuseaux horaires locaux, avec support de l'heure d'été (DST) pour le
// fuseau système.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Création de fuseaux via les fabriques statiques
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeZone.h"

    void TimeZoneCreationExample() {
        using namespace nkentseu;

        // Fuseau système local (DST géré automatiquement)
        NkTimeZone local = NkTimeZone::GetLocal();

        // Fuseau UTC fixe (offset = 0, jamais de DST)
        NkTimeZone utc = NkTimeZone::GetUtc();

        // Fuseau avec offset fixe : UTC+5:30 (Inde)
        NkTimeZone india = NkTimeZone::FromName(NkString("UTC+5:30"));

        // Fuseau avec offset fixe : UTC-8 (Pacifique)
        NkTimeZone pacific = NkTimeZone::FromName(NkString("UTC-8"));

        // Noms IANA reconnus (sans base de données complète)
        NkTimeZone gmt = NkTimeZone::FromName(NkString("GMT"));  // = UTC
        NkTimeZone z = NkTimeZone::FromName(NkString("Z"));      // = UTC

        // Nom inconnu : fallback vers le fuseau local
        NkTimeZone unknown = NkTimeZone::FromName(NkString("Europe/Paris"));
        // unknown est configuré comme GetLocal() sans base IANA
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Conversions UTC ↔ Local pour NkTime
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeZone.h"
    #include "NKTime/NkTimes.h"
    #include "NKTime/NkDate.h"
    #include "NKCore/Logger/NkLogger.h"

    void TimeConversionExample() {
        using namespace nkentseu;

        // Fuseaux de référence
        NkTimeZone utc = NkTimeZone::GetUtc();
        NkTimeZone local = NkTimeZone::GetLocal();

        // Heure UTC de référence
        NkTime utcTime(14, 30, 0);  // 14:30:00 UTC

        // Date de référence pour le calcul DST (importante pour précision)
        NkDate refDate(2024, 7, 15);  // 15 juillet 2024 (été dans l'hémisphère nord)

        // Conversion UTC → Local
        NkTime localTime = local.ToLocal(utcTime, refDate);
        // En été CET (UTC+2) : localTime = 16:30:00

        // Conversion Local → UTC
        NkTime backToUtc = local.ToUtc(localTime, refDate);
        // backToUtc == utcTime (conversion réversible)

        // Logging du résultat
        NK_LOG_INFO("UTC: {}, Local: {} (DST: {})",
                    utcTime.ToString(),
                    localTime.ToString(),
                    local.IsDaylightSavingTime(refDate) ? "yes" : "no");
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Conversions calendaires (changement de jour)
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeZone.h"
    #include "NKTime/NkDate.h"

    void DateConversionExample() {
        using namespace nkentseu;

        // Fuseaux avec grand offset
        NkTimeZone utc = NkTimeZone::GetUtc();
        NkTimeZone tokyo = NkTimeZone::FromName(NkString("UTC+9"));  // Japon

        // Date en UTC : 31 décembre 2024, 23:00 UTC
        NkDate utcDate(2024, 12, 31);

        // Conversion vers Tokyo : +9h → 1er janvier 2025
        NkDate tokyoDate = tokyo.ToLocal(utcDate);
        // tokyoDate = 2025-01-01 (changement de jour)

        // Conversion inverse : retour à la date UTC originale
        NkDate backToUtc = tokyo.ToUtc(tokyoDate);
        // backToUtc == utcDate

        // Important : les conversions de date ne gèrent que le jour,
        // pas l'heure précise. Pour une conversion complète date+heure,
        // utiliser NkDateTime (module séparé) ou combiner NkDate + NkTime.
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Inspection des propriétés du fuseau
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeZone.h"
    #include "NKCore/Logger/NkLogger.h"

    void TimeZoneInspectionExample() {
        using namespace nkentseu;

        NkTimeZone local = NkTimeZone::GetLocal();
        NkTimeZone utc = NkTimeZone::GetUtc();
        NkTimeZone fixed = NkTimeZone::FromName(NkString("UTC+5:30"));

        // Nom descriptif
        NK_LOG_INFO("Local name: {}", local.GetName().CStr());  // "Local"
        NK_LOG_INFO("UTC name: {}", utc.GetName().CStr());      // "UTC"
        NK_LOG_INFO("Fixed name: {}", fixed.GetName().CStr());  // "UTC+5:30"

        // Type de fuseau
        if (local.GetKind() == NkTimeZone::NkKind::NK_LOCAL) {
            NK_LOG_INFO("Local timezone uses system DST");
        }

        // Offset fixe (seulement pour NK_FIXED_OFFSET)
        int32 offset = fixed.GetFixedOffsetSeconds();  // 19800 = 5*3600 + 30*60
        NK_LOG_INFO("Fixed offset: {} seconds ({:+03d}:{:02d})",
                    offset,
                    offset / 3600,
                    (offset % 3600) / 60);

        // Offset dynamique (pour NK_LOCAL, dépend de la date/DST)
        NkDate summer(2024, 7, 15);
        NkDate winter(2024, 1, 15);
        NkTimeSpan offsetSummer = local.GetUtcOffset(summer);
        NkTimeSpan offsetWinter = local.GetUtcOffset(winter);

        NK_LOG_INFO("Local offset: summer={}, winter={}",
                    offsetSummer.ToString(),
                    offsetWinter.ToString());
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Détection de l'heure d'été (DST)
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeZone.h"
    #include "NKTime/NkDate.h"

    void DstDetectionExample() {
        using namespace nkentseu;

        NkTimeZone local = NkTimeZone::GetLocal();
        NkTimeZone utc = NkTimeZone::GetUtc();

        // Dates de test dans les deux saisons
        NkDate summerDate(2024, 7, 15);  // Été (hémisphère nord)
        NkDate winterDate(2024, 1, 15);  // Hiver (hémisphère nord)

        // Vérification DST pour le fuseau local
        bool summerDst = local.IsDaylightSavingTime(summerDate);
        bool winterDst = local.IsDaylightSavingTime(winterDate);

        // UTC n'a jamais de DST
        bool utcDst = utc.IsDaylightSavingTime(summerDate);  // Toujours false

        // Logging conditionnel
        if (summerDst && !winterDst) {
            NK_LOG_INFO("System uses DST: +1h in summer");
        } else if (!summerDst && winterDst) {
            NK_LOG_INFO("System uses DST: +1h in winter (hémisphère sud)");
        } else {
            NK_LOG_INFO("System does not use DST");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Comparaison de fuseaux
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeZone.h"
    #include <vector>

    void TimeZoneComparisonExample() {
        using namespace nkentseu;

        // Création de fuseaux
        NkTimeZone tz1 = NkTimeZone::GetUtc();
        NkTimeZone tz2 = NkTimeZone::FromName(NkString("UTC"));  // Identique à tz1
        NkTimeZone tz3 = NkTimeZone::FromName(NkString("GMT"));  // Aussi = UTC

        // Comparaison d'égalité
        if (tz1 == tz2) {
            // tz1 et tz2 sont considérés égaux
        }

        // Recherche dans un conteneur
        std::vector<NkTimeZone> timezones = {
            NkTimeZone::GetLocal(),
            NkTimeZone::GetUtc(),
            NkTimeZone::FromName(NkString("UTC+1"))
        };

        NkTimeZone target = NkTimeZone::GetUtc();
        auto it = std::find(timezones.begin(), timezones.end(), target);
        if (it != timezones.end()) {
            // Fuseau UTC trouvé dans la liste
        }

        // Note : L'égalité compare kind, name ET offset.
        // Deux fuseaux avec même offset mais noms différents ne sont pas égaux.
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Thread-safety et immutabilité
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeZone.h"
    #include <thread>
    #include <vector>

    void ThreadSafetyExample() {
        using namespace nkentseu;

        // NkTimeZone est immuable après construction : thread-safe par design
        const NkTimeZone sharedTz = NkTimeZone::GetLocal();

        // Plusieurs threads peuvent appeler les méthodes const simultanément
        // sans synchronisation externe
        std::vector<std::thread> workers;

        for (int i = 0; i < 4; ++i) {
            workers.emplace_back([&sharedTz]() {
                // Toutes ces appels sont thread-safe (méthodes const)
                NkDate refDate(2024, 6, 1);
                NkTime utcTime(12, 0, 0);

                NkTime local = sharedTz.ToLocal(utcTime, refDate);
                NkTimeSpan offset = sharedTz.GetUtcOffset(refDate);
                bool isDst = sharedTz.IsDaylightSavingTime(refDate);

                // Utilisation des résultats...
            });
        }

        for (auto& t : workers) {
            t.join();
        }

        // Important : Ne jamais modifier un NkTimeZone après construction.
        // Si un changement de fuseau est nécessaire, créer une nouvelle instance.
    }
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Pattern de conversion complète (date + heure)
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeZone.h"
    #include "NKTime/NkDate.h"
    #include "NKTime/NkTimes.h"

    namespace nkentseu {

        // Structure composite pour datetime complet (non fournie par défaut)
        struct DateTime {
            NkDate date;
            NkTime time;

            DateTime() = default;
            DateTime(const NkDate& d, const NkTime& t) : date(d), time(t) {}
        };

        // Conversion complète UTC ↔ Local pour datetime
        DateTime ConvertDateTime(
            const DateTime& input,
            const NkTimeZone& fromTz,
            const NkTimeZone& toTz
        ) {
            // Étape 1 : Convertir vers UTC si nécessaire
            DateTime utc;
            if (fromTz.GetKind() != NkTimeZone::NkKind::NK_UTC) {
                utc.date = fromTz.ToUtc(input.date);
                utc.time = fromTz.ToUtc(input.time, input.date);
            } else {
                utc = input;
            }

            // Étape 2 : Convertir depuis UTC vers le fuseau cible
            DateTime result;
            if (toTz.GetKind() != NkTimeZone::NkKind::NK_UTC) {
                result.date = toTz.ToLocal(utc.date);
                result.time = toTz.ToLocal(utc.time, utc.date);
            } else {
                result = utc;
            }

            return result;
        }

        void FullConversionExample() {
            DateTime utcTime(NkDate(2024, 12, 31), NkTime(23, 30, 0));
            NkTimeZone tokyo = NkTimeZone::FromName(NkString("UTC+9"));

            DateTime tokyoTime = ConvertDateTime(utcTime, NkTimeZone::GetUtc(), tokyo);
            // tokyoTime = 2025-01-01 08:30:00 (jour suivant + 9h)
        }

    } // namespace nkentseu
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================