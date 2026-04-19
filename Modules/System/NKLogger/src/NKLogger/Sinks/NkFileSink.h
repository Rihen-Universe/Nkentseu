// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkFileSink.h
// DESCRIPTION: Sink pour l'écriture dans un fichier.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/NkSink.h"
#include "NKLogger/NkSync.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"
#include <cstdio>

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
    // -------------------------------------------------------------------------
    // CLASSE: NkFileSink
    // DESCRIPTION: Sink pour l'écriture dans un fichier
    // -------------------------------------------------------------------------
    class NKLOGGER_API NkFileSink : public NkISink {
        public:
            // ---------------------------------------------------------------------
            // CONSTRUCTEURS
            // ---------------------------------------------------------------------

            /**
             * @brief Constructeur avec chemin de fichier
             * @param filename Chemin du fichier
             * @param truncate true pour tronquer le fichier existant
             */
            explicit NkFileSink(const NkString &filename, bool truncate = false);

            /**
             * @brief Destructeur
             */
            ~NkFileSink() override;

            // ---------------------------------------------------------------------
            // IMPLÉMENTATION DE NkISink
            // ---------------------------------------------------------------------

            /**
             * @brief Logge un message dans le fichier
             */
            void Log(const NkLogMessage &message) override;

            /**
             * @brief Force l'écriture des données en attente
             */
            void Flush() override;

            /**
             * @brief Définit le formatter pour ce sink
             */
            void SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) override;

            /**
             * @brief Définit le pattern de formatage
             */
            void SetPattern(const NkString &pattern) override;

            /**
             * @brief Obtient le formatter courant
             */
            NkFormatter *GetFormatter() const override;

            /**
             * @brief Obtient le pattern courant
             */
            NkString GetPattern() const override;

            // ---------------------------------------------------------------------
            // CONFIGURATION SPÉCIFIQUE AU FICHIER
            // ---------------------------------------------------------------------

            /**
             * @brief Ouvre le fichier (si non ouvert)
             * @return true si ouvert avec succès, false sinon
             */
            bool Open();

            /**
             * @brief Ferme le fichier
             */
            void Close();

            /**
             * @brief Vérifie si le fichier est ouvert
             * @return true si ouvert, false sinon
             */
            bool IsOpen() const;

            /**
             * @brief Obtient le nom du fichier
             * @return Nom du fichier
             */
            NkString GetFilename() const;

            /**
             * @brief Définit un nouveau nom de fichier
             * @param filename Nouveau nom de fichier
             */
            void SetFilename(const NkString &filename);

            /**
             * @brief Obtient la taille actuelle du fichier
             * @return Taille en octets
             */
            usize GetFileSize() const;

            /**
             * @brief Définit le mode d'ouverture (truncate/append)
             * @param truncate true pour tronquer, false pour append
             */
            void SetTruncate(bool truncate);

            /**
             * @brief Obtient le mode d'ouverture
             * @return true si truncate, false si append
             */
            bool GetTruncate() const;

        private:
            // ---------------------------------------------------------------------
            // MÉTHODES PRIVÉES
            // ---------------------------------------------------------------------

            /**
             * @brief Ouvre le fichier avec le mode approprié
             */
            bool OpenFile();

        protected:
            /**
             * @brief Ouvre le flux fichier sans prendre le mutex (mutex déjà détenu)
             */
            bool OpenUnlocked();

            /**
             * @brief Ferme le flux fichier sans prendre le mutex (mutex déjà détenu)
             */
            void CloseUnlocked();

            /**
             * @brief Retourne le nom de fichier sans prendre le mutex (mutex déjà détenu)
             */
            NkString GetFilenameUnlocked() const;

            /**
             * @brief Vérifie et gère la rotation de fichier si nécessaire
             */
            virtual void CheckRotation();

            // ---------------------------------------------------------------------
            // VARIABLES MEMBRE PRIVÉES
            // ---------------------------------------------------------------------

            /// NkFormatter pour ce sink
            memory::NkUniquePtr<NkFormatter> m_Formatter;

            /// Flux de fichier
            FILE *m_FileStream;

            /// Nom du fichier
            NkString m_Filename;

            /// Mode d'ouverture (truncate/append)
            bool m_Truncate;

        protected:
            /// Mutex pour la synchronisation thread-safe
            mutable threading::NkMutex m_Mutex;
    };

} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION - NkFileSink
// =============================================================================
//
//   NkFileSink sink("app.log", false);
//   sink.SetPattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
//
// =============================================================================
