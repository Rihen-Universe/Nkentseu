// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkDailyFileSink.h
// DESCRIPTION: Sink pour fichiers avec rotation quotidienne.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/Sinks/NkFileSink.h"

#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"

#include <ctime>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
        
    // -------------------------------------------------------------------------
    // CLASSE: NkDailyFileSink
    // DESCRIPTION: Sink avec rotation quotidienne de fichiers
    // -------------------------------------------------------------------------
    class NKLOGGER_API NkDailyFileSink : public NkFileSink {
        public:
            // ---------------------------------------------------------------------
            // CONSTRUCTEURS
            // ---------------------------------------------------------------------

            /**
             * @brief Constructeur avec configuration
             * @param filename Chemin du fichier (peut contenir des patterns de date)
             * @param hour Heure de rotation (0-23)
             * @param minute Minute de rotation (0-59)
             * @param maxDays Nombre maximum de jours à conserver (0 = illimité)
             */
            NkDailyFileSink(const NkString &filename, int hour = 0, int minute = 0, usize maxDays = 0);

            /**
             * @brief Destructeur
             */
            ~NkDailyFileSink() override;

            // ---------------------------------------------------------------------
            // IMPLÉMENTATION DE NkFileSink
            // ---------------------------------------------------------------------

            /**
             * @brief Logge un message avec vérification de rotation quotidienne
             */
            void Log(const NkLogMessage &message) override;

            // ---------------------------------------------------------------------
            // CONFIGURATION DE LA ROTATION QUOTIDIENNE
            // ---------------------------------------------------------------------

            /**
             * @brief Définit l'heure de rotation
             * @param hour Heure (0-23)
             * @param minute Minute (0-59)
             */
            void SetRotationTime(int hour, int minute);

            /**
             * @brief Obtient l'heure de rotation
             * @return Heure de rotation
             */
            int GetRotationHour() const;

            /**
             * @brief Obtient la minute de rotation
             * @return Minute de rotation
             */
            int GetRotationMinute() const;

            /**
             * @brief Définit le nombre maximum de jours à conserver
             * @param maxDays Nombre de jours (0 = illimité)
             */
            void SetMaxDays(usize maxDays);

            /**
             * @brief Obtient le nombre maximum de jours à conserver
             * @return Nombre de jours
             */
            usize GetMaxDays() const;

            /**
             * @brief Force la rotation du fichier
             * @return true si rotation réussie, false sinon
             */
            bool Rotate();

        private:
            // ---------------------------------------------------------------------
            // MÉTHODES PRIVÉES
            // ---------------------------------------------------------------------

            /**
             * @brief Vérifie et effectue la rotation si nécessaire
             */
            void CheckRotation() override;

            /**
             * @brief Effectue la rotation quotidienne
             */
            void PerformRotation();

            /**
             * @brief Nettoie les anciens fichiers
             */
            void CleanOldFiles();

            /**
             * @brief Génère le nom de fichier pour une date donnée
             * @param date Date
             * @return Nom de fichier
             */
            NkString GetFilenameForDate(const tm &date) const;

            /**
             * @brief Extrait la date d'un nom de fichier
             * @param filename Nom de fichier
             * @return Structure tm avec la date
             */
            tm ExtractDateFromFilename(const NkString &filename) const;

            /**
             * @brief Vérifie si une date est plus ancienne que maxDays
             * @param date Date à vérifier
             * @return true si trop ancienne, false sinon
             */
            bool IsDateTooOld(const tm &date) const;

            // ---------------------------------------------------------------------
            // VARIABLES MEMBRE PRIVÉES
            // ---------------------------------------------------------------------

            /// Heure de rotation (0-23)
            int m_RotationHour;

            /// Minute de rotation (0-59)
            int m_RotationMinute;

            /// Nombre maximum de jours à conserver
            usize m_MaxDays;

            /// Date du fichier courant
            tm m_CurrentDate;

            /// Dernière vérification de rotation
            uint64 m_LastCheck;
    };

} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION - NkDailyFileSink
// =============================================================================
//
//   NkDailyFileSink sink("logs/app-%Y-%m-%d.log", 0, 0, 7);
//   sink.SetPattern("[%H:%M:%S] [%l] %v");
//
// =============================================================================
