#include "CommandHistory.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace Unkeny {

        void CommandHistory::Do(NkEditorCommand* cmd) noexcept {
            if (!cmd) return;

            // Effacer le futur (redo stack)
            for (nk_int32 i = (nk_int32)mCommands.Size() - 1; i >= mCursor; --i) {
                delete mCommands[i];
                mCommands.EraseAt((nk_usize)i);
            }

            // Tentative de fusion avec la commande précédente
            if (mCursor > 0 && mCommands.Size() > 0) {
                auto* prev = mCommands[mCursor - 1];
                if (prev->CanMergeWith(cmd)) {
                    prev->MergeWith(cmd);
                    delete cmd;
                    prev->Execute();
                    return;
                }
            }

            cmd->Execute();
            mCommands.PushBack(cmd);
            ++mCursor;

            Trim();
        }

        bool CommandHistory::Undo() noexcept {
            if (!CanUndo()) return false;
            --mCursor;
            mCommands[mCursor]->Undo();
            logger.Infof("[CommandHistory] Undo: {}\n", mCommands[mCursor]->Name());
            return true;
        }

        bool CommandHistory::Redo() noexcept {
            if (!CanRedo()) return false;
            mCommands[mCursor]->Execute();
            logger.Infof("[CommandHistory] Redo: {}\n", mCommands[mCursor]->Name());
            ++mCursor;
            return true;
        }

        void CommandHistory::Clear() noexcept {
            for (nk_usize i = 0; i < mCommands.Size(); ++i)
                delete mCommands[i];
            mCommands.Clear();
            mCursor      = 0;
            mSavedCursor = 0;
        }

        void CommandHistory::Trim() noexcept {
            while ((nk_uint32)mCommands.Size() > mMaxDepth) {
                delete mCommands[0];
                mCommands.EraseAt(0);
                --mCursor;
                if (mSavedCursor > 0) --mSavedCursor;
            }
        }

    } // namespace Unkeny
} // namespace nkentseu
