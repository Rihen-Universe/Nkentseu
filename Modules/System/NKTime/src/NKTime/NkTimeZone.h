#pragma once
/**
 * @File    NkTimeZone.h
 * @Brief   Fuseaux horaires et conversions UTC/local — sans dépendance STL.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Trois modes supportés :
 *    Local       : Fuseau système, DST géré par l'OS via localtime_r/s.
 *    Utc         : UTC fixe, offset = 0, jamais de DST.
 *    FixedOffset : Offset fixe en secondes (ex: UTC+5:30), jamais de DST.
 *
 *  Thread-safety : NkTimeZone est immuable après construction. Toutes les
 *  méthodes const sont thread-safe sans verrou.
 *
 *  Correction vs version originale :
 *    ToLocal(NkTime) et ToUtc(NkTime) acceptent maintenant une NkDate
 *    optionnelle pour le calcul DST, au lieu d'appeler NkDate::GetCurrent()
 *    en interne (effet de bord caché dans une méthode const).
 */

#include "NKTime/NkTimeExport.h"
#include "NKTime/NkTimeConstants.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKTime/NkTimeSpan.h"
#include "NKTime/NkDate.h"
#include "NKTime/NkTimes.h"

namespace nkentseu {

    class NKENTSEU_TIME_API NkTimeZone {
        public:

            // ── Type de fuseau ───────────────────────────────────────────────────

            enum class NkKind : uint8 {
                NK_LOCAL       = 0, ///< Fuseau système (DST possible)
                NK_UTC         = 1, ///< UTC fixe (offset = 0, pas de DST)
                NK_FIXED_OFFSET = 2  ///< Offset fixe en secondes (pas de DST)
            };

            // ── Fabriques statiques ──────────────────────────────────────────────

            /// Fuseau système local.
            NKTIME_NODISCARD static NkTimeZone GetLocal() noexcept;

            /// UTC (offset = 0, pas de DST).
            NKTIME_NODISCARD static NkTimeZone GetUtc() noexcept;

            /**
             * @Brief Crée un fuseau depuis un nom IANA ou un offset fixe.
             * @param ianaName "Local", "UTC", "GMT", "Z", "Etc/UTC",
             *                 "UTC+HH", "UTC+HH:MM", "GMT-HH", etc.
             * @note Sans base de données IANA, les noms inconnus sont redirigés
             *       vers le fuseau local du système.
             */
            NKTIME_NODISCARD static NkTimeZone FromName(const NkString& ianaName) noexcept;

            // ── Conversions ──────────────────────────────────────────────────────

            /**
             * @Brief Convertit un temps UTC en temps local.
             * @param utcTime  Temps en UTC.
             * @param refDate  Date de référence pour le calcul DST (défaut : date système).
             * @note Le paramètre refDate est explicite pour éviter l'effet de bord
             *       de GetCurrent() dans une méthode const.
             */
            NKTIME_NODISCARD NkTime ToLocal(const NkTime& utcTime,
                                            const NkDate& refDate) const noexcept;

            /**
             * @Brief Convertit un temps local en UTC.
             * @param localTime Temps local.
             * @param refDate   Date de référence pour le calcul DST.
             */
            NKTIME_NODISCARD NkTime ToUtc(const NkTime& localTime,
                                        const NkDate& refDate) const noexcept;

            NKTIME_NODISCARD NkDate ToLocal(const NkDate& utcDate)   const noexcept;
            NKTIME_NODISCARD NkDate ToUtc  (const NkDate& localDate) const noexcept;

            // ── Informations ─────────────────────────────────────────────────────

            /// Nom descriptif du fuseau (référence constante — pas de copie).
            NKTIME_NODISCARD const NkString& GetName() const noexcept;

            NKTIME_NODISCARD NkKind GetKind()               const noexcept;
            NKTIME_NODISCARD int32  GetFixedOffsetSeconds() const noexcept;

            /**
             * @Brief Offset UTC pour une date donnée.
             * @param date Date de référence (utilisée pour DST local).
             * @return NkTimeSpan représentant l'offset (peut être négatif).
             */
            NKTIME_NODISCARD NkTimeSpan GetUtcOffset(const NkDate& date) const noexcept;

            /**
             * @Brief Vérifie si la date tombe en heure d'été (DST).
             * @return Toujours false pour Utc et FixedOffset.
             */
            NKTIME_NODISCARD bool IsDaylightSavingTime(const NkDate& date) const noexcept;

            // ── Comparaisons ─────────────────────────────────────────────────────

            NKTIME_NODISCARD bool operator==(const NkTimeZone& o) const noexcept;
            NKTIME_NODISCARD bool operator!=(const NkTimeZone& o) const noexcept;

        private:
            NkTimeZone(NkKind kind, const NkString& name, int32 fixedOffsetSeconds = 0) noexcept;

            NkKind   mKind;
            NkString mName;
            int32    mFixedOffsetSeconds;
    };

} // namespace nkentseu