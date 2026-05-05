#pragma once
// =============================================================================
// Unkeny/Editor/CommandHistory.h
// =============================================================================
// Undo/redo via pattern Command.
// Chaque opération éditable implémente NkEditorCommand.
//
// Usage :
//   struct MoveCommand : NkEditorCommand {
//       void Execute() override { entity.SetPosition(newPos); }
//       void Undo()    override { entity.SetPosition(oldPos); }
//       const char* Name() const noexcept override { return "Move"; }
//   };
//   history.Do(new MoveCommand(...));
//   history.Undo();
//   history.Redo();
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace Unkeny {

        // =====================================================================
        // NkEditorCommand — interface
        // =====================================================================
        class NkEditorCommand {
        public:
            virtual ~NkEditorCommand() noexcept = default;
            virtual void Execute() = 0;
            virtual void Undo()    = 0;
            virtual const char* Name() const noexcept { return "Command"; }
            // Si true, cette commande peut fusionner avec la précédente du même type
            virtual bool CanMergeWith(const NkEditorCommand*) const noexcept { return false; }
            virtual void MergeWith(NkEditorCommand*) noexcept {}
        };

        // =====================================================================
        // CommandHistory
        // =====================================================================
        class CommandHistory {
        public:
            explicit CommandHistory(nk_uint32 maxDepth = 100) noexcept
                : mMaxDepth(maxDepth) {}

            ~CommandHistory() noexcept { Clear(); }

            // Non copiable
            CommandHistory(const CommandHistory&) = delete;
            CommandHistory& operator=(const CommandHistory&) = delete;

            // ── Exécution ─────────────────────────────────────────────────────
            // Exécute la commande et l'empile dans l'historique.
            // Efface le futur (redo stack) si la commande ne peut pas être mergée.
            void Do(NkEditorCommand* cmd) noexcept;

            // ── Undo / Redo ───────────────────────────────────────────────────
            bool Undo() noexcept;
            bool Redo() noexcept;

            // ── État ──────────────────────────────────────────────────────────
            [[nodiscard]] bool CanUndo() const noexcept {
                return mCursor > 0;
            }
            [[nodiscard]] bool CanRedo() const noexcept {
                return mCursor < (nk_int32)mCommands.Size();
            }

            [[nodiscard]] NkString UndoName() const noexcept {
                if (!CanUndo()) return "";
                return NkString(mCommands[mCursor - 1]->Name());
            }
            [[nodiscard]] NkString RedoName() const noexcept {
                if (!CanRedo()) return "";
                return NkString(mCommands[mCursor]->Name());
            }

            [[nodiscard]] nk_uint32 HistorySize() const noexcept {
                return (nk_uint32)mCommands.Size();
            }

            void Clear() noexcept;

            // Marque l'état courant comme "sauvegardé"
            void MarkSaved()   noexcept { mSavedCursor = mCursor; }
            bool IsModified()  const noexcept { return mCursor != mSavedCursor; }

        private:
            void Trim() noexcept; // élimine l'excédent si > mMaxDepth

            NkVector<NkEditorCommand*> mCommands;
            nk_int32                   mCursor      = 0; // index du prochain redo
            nk_int32                   mSavedCursor = 0;
            nk_uint32                  mMaxDepth;
        };

    } // namespace Unkeny
} // namespace nkentseu
