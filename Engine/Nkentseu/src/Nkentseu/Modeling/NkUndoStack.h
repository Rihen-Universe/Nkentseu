#pragma once
// =============================================================================
// Nkentseu/Modeling/NkUndoStack.h
// =============================================================================
// Historique d'opérations undo/redo — Command pattern.
//
// DESIGN :
//   • Chaque opération d'édition est une NkEditCommand
//   • NkUndoStack::Execute(cmd) → Execute() + push dans l'historique
//   • Undo() → appelle cmd->Undo() et recule le pointeur courant
//   • Redo() → rejoue le cmd suivant
//   • Groupes : Begin/EndGroup() → une seule entrée undo pour N opérations
//   • Budget mémoire configurable (évite la croissance illimitée)
//
// USAGE :
//   NkUndoStack undo(50);  // max 50 états
//
//   // Opération atomique
//   auto cmd = new NkMoveVerticesCmd(mesh, selectedVerts, delta);
//   undo.Execute(cmd);     // Exécute et push
//
//   // Groupe d'opérations (compte comme une seule dans l'historique)
//   undo.BeginGroup("Bevel");
//   undo.Execute(new NkSplitEdgesCmd(...));
//   undo.Execute(new NkMoveVerticesCmd(...));
//   undo.EndGroup();
//
//   undo.Undo();   // Defait tout le groupe Bevel
//   undo.Redo();   // Rejoue le groupe Bevel
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {

    // =========================================================================
    // NkEditCommand — interface de commande
    // =========================================================================
    class NkEditCommand {
        public:
            static constexpr uint32 kMaxNameLen = 128u;
            char name[kMaxNameLen] = {};

            virtual ~NkEditCommand() noexcept = default;

            /**
             * @brief Exécute l'opération.
             * @note Appelé automatiquement par NkUndoStack::Execute().
             *       Ne pas appeler manuellement.
             */
            virtual void Execute() noexcept = 0;

            /**
             * @brief Annule l'opération (restore l'état avant Execute()).
             */
            virtual void Undo() noexcept = 0;

            /**
             * @brief Rejoue l'opération (après un Undo).
             * @note Par défaut, appelle simplement Execute().
             *       Surcharger pour une implémentation plus efficace.
             */
            virtual void Redo() noexcept { Execute(); }

            /**
             * @brief Taille estimée en mémoire pour le budget.
             * @note Surcharger pour des commandes avec grosses données.
             */
            [[nodiscard]] virtual nk_usize MemoryUsage() const noexcept {
                return sizeof(*this);
            }
    };

    // =========================================================================
    // NkCommandGroup — groupe de commandes (une seule entrée undo)
    // =========================================================================
    class NkCommandGroup : public NkEditCommand {
        public:
            explicit NkCommandGroup(const char* groupName) noexcept {
                std::strncpy(name, groupName, kMaxNameLen - 1);
            }

            ~NkCommandGroup() noexcept override {
                for (nk_usize i = 0; i < mCmds.Size(); ++i) delete mCmds[i];
            }

            void Add(NkEditCommand* cmd) noexcept { mCmds.PushBack(cmd); }

            void Execute() noexcept override {
                for (nk_usize i = 0; i < mCmds.Size(); ++i) mCmds[i]->Execute();
            }

            void Undo() noexcept override {
                // Undo dans l'ordre inverse
                for (nk_usize i = mCmds.Size(); i > 0; --i) mCmds[i-1]->Undo();
            }

            void Redo() noexcept override {
                for (nk_usize i = 0; i < mCmds.Size(); ++i) mCmds[i]->Redo();
            }

            [[nodiscard]] nk_usize MemoryUsage() const noexcept override {
                nk_usize total = sizeof(*this);
                for (nk_usize i = 0; i < mCmds.Size(); ++i)
                    total += mCmds[i]->MemoryUsage();
                return total;
            }

            [[nodiscard]] uint32 CommandCount() const noexcept {
                return static_cast<uint32>(mCmds.Size());
            }

        private:
            NkVector<NkEditCommand*> mCmds;
    };

    // =========================================================================
    // NkUndoStack
    // =========================================================================
    class NkUndoStack {
        public:
            explicit NkUndoStack(uint32 maxSteps = 100,
                                  nk_usize memBudget = 256 * 1024 * 1024) noexcept
                : mMaxSteps(maxSteps)
                , mMemBudget(memBudget)
            {}

            ~NkUndoStack() noexcept { Clear(); }

            NkUndoStack(const NkUndoStack&) = delete;
            NkUndoStack& operator=(const NkUndoStack&) = delete;

            // ── Opérations ────────────────────────────────────────────────

            /**
             * @brief Exécute une commande et la push dans l'historique.
             * @param cmd Commande à exécuter (ownership transféré).
             * @note Invalide le redo (supprime les commandes "en avant").
             */
            void Execute(NkEditCommand* cmd) noexcept;

            /**
             * @brief Annule la dernière opération.
             * @return true si un undo était possible.
             */
            bool Undo() noexcept;

            /**
             * @brief Rejoue la dernière opération annulée.
             * @return true si un redo était possible.
             */
            bool Redo() noexcept;

            // ── Groupes ───────────────────────────────────────────────────

            /**
             * @brief Commence un groupe. Toutes les Execute() suivantes
             *        sont regroupées en une seule entrée undo.
             */
            void BeginGroup(const char* name) noexcept;

            /**
             * @brief Termine le groupe et le push dans l'historique.
             */
            void EndGroup() noexcept;

            [[nodiscard]] bool IsGroupOpen() const noexcept {
                return mCurrentGroup != nullptr;
            }

            // ── État ──────────────────────────────────────────────────────

            [[nodiscard]] bool CanUndo() const noexcept {
                return mCurrent >= 0;
            }

            [[nodiscard]] bool CanRedo() const noexcept {
                return mCurrent < static_cast<int32>(mHistory.Size()) - 1;
            }

            [[nodiscard]] const char* GetUndoName() const noexcept {
                if (!CanUndo()) return nullptr;
                return mHistory[mCurrent]->name;
            }

            [[nodiscard]] const char* GetRedoName() const noexcept {
                if (!CanRedo()) return nullptr;
                return mHistory[mCurrent + 1]->name;
            }

            [[nodiscard]] uint32 HistorySize() const noexcept {
                return static_cast<uint32>(mHistory.Size());
            }

            [[nodiscard]] int32 CurrentIndex() const noexcept { return mCurrent; }

            /**
             * @brief Efface tout l'historique.
             */
            void Clear() noexcept;

            /**
             * @brief Marque l'état courant comme "propre" (sauvegardé).
             */
            void SetCleanState() noexcept { mCleanIndex = mCurrent; }

            /**
             * @brief Retourne true si le document a été modifié depuis la dernière sauvegarde.
             */
            [[nodiscard]] bool IsDirty() const noexcept {
                return mCurrent != mCleanIndex;
            }

            /**
             * @brief Mémoire utilisée par l'historique (bytes).
             */
            [[nodiscard]] nk_usize MemoryUsed() const noexcept;

        private:
            void TrimHistory() noexcept;   ///< Supprime les entrées les plus anciennes si budget dépassé
            void PushCommand(NkEditCommand* cmd) noexcept;

            NkVector<NkEditCommand*> mHistory;
            int32                   mCurrent      = -1;
            int32                   mCleanIndex   = -1;
            uint32                  mMaxSteps;
            nk_usize                mMemBudget;
            NkCommandGroup*         mCurrentGroup = nullptr;
    };

    // =========================================================================
    // Commandes prédéfinies utilitaires
    // =========================================================================

    /**
     * @brief Commande Lambda — pour les opérations simples sans état à restaurer.
     * @note À utiliser uniquement pour les opérations idempotentes ou quand
     *       l'état avant est factuellement capturé dans la closure.
     */
    class NkLambdaCommand : public NkEditCommand {
        public:
            using Fn = NkFunction<void()>;

            NkLambdaCommand(const char* n, Fn execFn, Fn undoFn) noexcept
                : mExec(static_cast<Fn&&>(execFn))
                , mUndo(static_cast<Fn&&>(undoFn))
            {
                std::strncpy(name, n, kMaxNameLen - 1);
            }

            void Execute() noexcept override { if (mExec) mExec(); }
            void Undo()    noexcept override { if (mUndo) mUndo(); }

        private:
            Fn mExec;
            Fn mUndo;
    };

} // namespace nkentseu
