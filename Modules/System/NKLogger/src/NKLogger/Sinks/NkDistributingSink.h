// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkDistributingSink.h
// DESCRIPTION: Sink qui distribue les messages à plusieurs sous-sinks.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/NkSink.h"
#include "NKLogger/NkSync.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
        
    // -------------------------------------------------------------------------
    // CLASSE: NkDistributingSink
    // DESCRIPTION: Sink qui distribue les messages à plusieurs sous-sinks
    // -------------------------------------------------------------------------
    class NKLOGGER_API NkDistributingSink : public NkISink {
        public:
            // ---------------------------------------------------------------------
            // CONSTRUCTEURS
            // ---------------------------------------------------------------------

            /**
             * @brief Constructeur par défaut
             */
            NkDistributingSink();

            /**
             * @brief Constructeur avec liste initiale de sinks
             * @param sinks Liste de sinks à ajouter
             */
            explicit NkDistributingSink(const NkVector<memory::NkSharedPtr<NkISink>> &sinks);

            /**
             * @brief Destructeur
             */
            ~NkDistributingSink() override;

            // ---------------------------------------------------------------------
            // IMPLÉMENTATION DE NkISink
            // ---------------------------------------------------------------------

            /**
             * @brief Distribue un message à tous les sous-sinks
             */
            void Log(const NkLogMessage &message) override;

            /**
             * @brief Force le flush de tous les sous-sinks
             */
            void Flush() override;

            /**
             * @brief Définit le formatter pour tous les sous-sinks
             */
            void SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) override;

            /**
             * @brief Définit le pattern de formatage pour tous les sous-sinks
             */
            void SetPattern(const NkString &pattern) override;

            /**
             * @brief Obtient le formatter (du premier sink)
             */
            NkFormatter *GetFormatter() const override;

            /**
             * @brief Obtient le pattern (du premier sink)
             */
            NkString GetPattern() const override;

            // ---------------------------------------------------------------------
            // GESTION DES SOUS-SINKS
            // ---------------------------------------------------------------------

            /**
             * @brief Ajoute un sous-sink
             * @param sink Sink à ajouter
             */
            void AddSink(memory::NkSharedPtr<NkISink> sink);

            /**
             * @brief Supprime un sous-sink
             * @param sink Sink à supprimer
             */
            void RemoveSink(memory::NkSharedPtr<NkISink> sink);

            /**
             * @brief Supprime tous les sous-sinks
             */
            void ClearSinks();

            /**
             * @brief Obtient la liste des sous-sinks
             * @return Vecteur des sous-sinks
             */
            NkVector<memory::NkSharedPtr<NkISink>> GetSinks() const;

            /**
             * @brief Obtient le nombre de sous-sinks
             * @return Nombre de sous-sinks
             */
            usize GetSinkCount() const;

            /**
             * @brief Vérifie si un sink spécifique est présent
             * @param sink Sink à rechercher
             * @return true si présent, false sinon
             */
            bool ContainsSink(memory::NkSharedPtr<NkISink> sink) const;

        private:
            // ---------------------------------------------------------------------
            // VARIABLES MEMBRE PRIVÉES
            // ---------------------------------------------------------------------

            /// Liste des sous-sinks
            NkVector<memory::NkSharedPtr<NkISink>> m_Sinks;

            /// Mutex pour la synchronisation thread-safe
            mutable threading::NkMutex m_Mutex;
    };

} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION - NkDistributingSink
// =============================================================================
//
//   NkDistributingSink sink;
//   sink.AddSink(memory::MakeShared<NkConsoleSink>());
//   sink.AddSink(memory::MakeShared<NkFileSink>("app.log"));
//
// =============================================================================
