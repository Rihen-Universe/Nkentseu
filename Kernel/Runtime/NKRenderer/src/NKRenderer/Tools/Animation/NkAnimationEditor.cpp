// =============================================================================
// NkAnimationEditor.cpp — logique d'édition de timeline (sans UI)
// =============================================================================
#include "NkAnimationEditor.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace renderer {

        void NkAnimationEditor::SetClip(NkAnimationClip* clip) {
            mClip = clip;
            mCursor = 0.f;
            mSelected.Clear();
            mUndo.Clear();
            mRedo.Clear();
        }

        void NkAnimationEditor::SetCursor(float32 t) {
            if (t < 0.f) t = 0.f;
            float32 dur = Duration();
            if (dur > 0.f && t > dur) t = dur;
            mCursor = SnapTime(t);
        }

        float32 NkAnimationEditor::SnapTime(float32 t) const {
            if (mSnap <= 1e-6f) return t;
            float32 n = t / mSnap;
            // arrondi au plus proche
            float32 f = n - (float32)(int32)n;
            int32 k = (int32)n + ((f >= 0.5f) ? 1 : 0);
            return (float32)k * mSnap;
        }

        // ── Lecture ───────────────────────────────────────────────────────────────
        NkVector<float32> NkAnimationEditor::GetPoseKeyTimes() const {
            NkVector<float32> times;
            if (!mClip) return times;
            // Toutes les pistes d'os partagent les mêmes temps (bake) ; on prend la
            // piste qui a le plus de clés comme référence (robuste si édition partielle).
            uint32 best = 0, bestCount = 0;
            for (uint32 b=0; b<(uint32)mClip->boneTracks.Size(); ++b)
                if (mClip->boneTracks[b].KeyCount() > bestCount) { bestCount = mClip->boneTracks[b].KeyCount(); best = b; }
            if (bestCount == 0) return times;
            const auto& tr = mClip->boneTracks[best];
            for (uint32 i=0;i<tr.KeyCount();++i) times.PushBack(tr.GetKey(i).time);
            return times;
        }

        uint32 NkAnimationEditor::PoseKeyCount() const {
            uint32 best = 0;
            if (mClip) for (uint32 b=0;b<(uint32)mClip->boneTracks.Size();++b)
                best = NkMax(best, mClip->boneTracks[b].KeyCount());
            return best;
        }

        // ── Primitives non-annulables ───────────────────────────────────────────────
        void NkAnimationEditor::doInsert(float32 t, const NkVector<NkMat4f>& vals,
                                         const NkVector<uint8>& interps) {
            if (!mClip) return;
            uint32 nb = (uint32)mClip->boneTracks.Size();
            for (uint32 b=0; b<nb; ++b) {
                NkMat4f v = (b < (uint32)vals.Size()) ? vals[b] : NkMat4f::Identity();
                NkInterpMode im = (b < (uint32)interps.Size()) ? (NkInterpMode)interps[b] : NkInterpMode::NK_LINEAR;
                // remplace une clé existante au même temps si présente
                int32 ex = mClip->boneTracks[b].FindKeyAtTime(t, mTol);
                if (ex >= 0) mClip->boneTracks[b].RemoveKeyAt((uint32)ex);
                mClip->boneTracks[b].AddKey(t, v, im);
            }
            mClip->RecalcDuration();
        }

        NkAnimationEditor::Cmd NkAnimationEditor::capturePose(float32 t) const {
            Cmd c; c.type = NkAnimEditKind::DELETE_KEY; c.t0 = t;
            if (!mClip) return c;
            uint32 nb = (uint32)mClip->boneTracks.Size();
            c.values.Resize(nb); c.interps.Resize(nb);
            for (uint32 b=0;b<nb;++b) {
                int32 i = mClip->boneTracks[b].FindKeyAtTime(t, mTol);
                if (i >= 0) { c.values[b] = mClip->boneTracks[b].GetKey((uint32)i).value;
                              c.interps[b] = (uint8)mClip->boneTracks[b].GetKey((uint32)i).interp; }
                else { c.values[b] = NkMat4f::Identity(); c.interps[b] = (uint8)NkInterpMode::NK_LINEAR; }
            }
            return c;
        }

        void NkAnimationEditor::doDelete(float32 t) {
            if (!mClip) return;
            for (uint32 b=0;b<(uint32)mClip->boneTracks.Size();++b) {
                int32 i = mClip->boneTracks[b].FindKeyAtTime(t, mTol);
                if (i >= 0) mClip->boneTracks[b].RemoveKeyAt((uint32)i);
            }
            mClip->RecalcDuration();
        }

        void NkAnimationEditor::doMove(float32 tOld, float32 tNew) {
            if (!mClip) return;
            for (uint32 b=0;b<(uint32)mClip->boneTracks.Size();++b) {
                int32 i = mClip->boneTracks[b].FindKeyAtTime(tOld, mTol);
                if (i >= 0) mClip->boneTracks[b].MoveKey((uint32)i, tNew);
            }
            mClip->RecalcDuration();
        }

        void NkAnimationEditor::pushUndo(const Cmd& c) {
            mUndo.PushBack(c);
            mRedo.Clear();                 // toute nouvelle action invalide le redo
            if (mUndo.Size() > 256) mUndo.Erase(mUndo.Begin());  // borne mémoire
        }

        // ── Opérations publiques (annulables) ───────────────────────────────────────
        void NkAnimationEditor::InsertPoseKey(const NkVector<NkMat4f>& boneLocals) {
            if (!mClip) return;
            float32 t = mCursor;
            uint32 nb = (uint32)mClip->boneTracks.Size();
            NkVector<uint8> interps; interps.Resize(nb);
            for (uint32 b=0;b<nb;++b) interps[b] = (uint8)NkInterpMode::NK_LINEAR;
            doInsert(t, boneLocals, interps);
            Cmd c; c.type = NkAnimEditKind::INSERT_KEY; c.t0 = t;  // undo = delete(t)
            pushUndo(c);
        }

        void NkAnimationEditor::DeletePoseKeyAt(float32 t) {
            if (!mClip) return;
            Cmd c = capturePose(t);        // mémorise pour l'undo (re-insert)
            doDelete(t);
            pushUndo(c);
        }

        void NkAnimationEditor::MovePoseKey(float32 tOld, float32 tNew) {
            if (!mClip) return;
            tNew = SnapTime(tNew); if (tNew < 0.f) tNew = 0.f;
            doMove(tOld, tNew);
            Cmd c; c.type = NkAnimEditKind::MOVE_KEY; c.t0 = tOld; c.t1 = tNew;
            pushUndo(c);
            // suit la sélection
            for (uint32 i=0;i<(uint32)mSelected.Size();++i) { float32 d=mSelected[i]-tOld; if(d<0)d=-d; if(d<=mTol) mSelected[i]=tNew; }
        }

        // ── Sélection ───────────────────────────────────────────────────────────────
        bool NkAnimationEditor::IsSelected(float32 t) const {
            for (uint32 i=0;i<(uint32)mSelected.Size();++i){ float32 d=mSelected[i]-t; if(d<0)d=-d; if(d<=mTol) return true; }
            return false;
        }
        void NkAnimationEditor::SelectKey(float32 t, bool addToSelection) {
            if (!addToSelection) mSelected.Clear();
            if (!IsSelected(t)) mSelected.PushBack(t);
        }
        void NkAnimationEditor::DeleteSelected() {
            NkVector<float32> sel = mSelected;   // copie (DeletePoseKeyAt ne touche pas mSelected)
            for (uint32 i=0;i<(uint32)sel.Size();++i) DeletePoseKeyAt(sel[i]);
            mSelected.Clear();
        }
        void NkAnimationEditor::MoveSelected(float32 dt) {
            // déplace de droite à gauche ou inverse selon le signe pour éviter collisions
            NkVector<float32> sel = mSelected;
            if (dt >= 0.f) { for (int32 i=(int32)sel.Size()-1;i>=0;--i) MovePoseKey(sel[(uint32)i], sel[(uint32)i]+dt); }
            else           { for (uint32 i=0;i<(uint32)sel.Size();++i) MovePoseKey(sel[i], sel[i]+dt); }
        }

        // ── Undo / Redo ─────────────────────────────────────────────────────────────
        void NkAnimationEditor::Undo() {
            if (mUndo.Empty() || !mClip) return;
            Cmd c = mUndo[mUndo.Size()-1]; mUndo.Erase(mUndo.End()-1);
            Cmd inv;   // commande inverse, poussée dans redo
            switch (c.type) {
                case NkAnimEditKind::INSERT_KEY:   // annuler insert = capturer puis delete
                    inv = capturePose(c.t0); inv.type = NkAnimEditKind::INSERT_KEY;  // pour redo : re-insert
                    inv.t0 = c.t0;
                    doDelete(c.t0);
                    break;
                case NkAnimEditKind::DELETE_KEY:   // annuler delete = re-insert les valeurs mémorisées
                    doInsert(c.t0, c.values, c.interps);
                    inv = c; inv.type = NkAnimEditKind::DELETE_KEY;   // pour redo : delete
                    break;
                case NkAnimEditKind::MOVE_KEY:     // annuler move = move retour
                    doMove(c.t1, c.t0);
                    inv = c; inv.type = NkAnimEditKind::MOVE_KEY; inv.t0 = c.t1; inv.t1 = c.t0;
                    break;
            }
            mRedo.PushBack(inv);
        }

        void NkAnimationEditor::Redo() {
            if (mRedo.Empty() || !mClip) return;
            Cmd c = mRedo[mRedo.Size()-1]; mRedo.Erase(mRedo.End()-1);
            Cmd inv;
            switch (c.type) {
                case NkAnimEditKind::INSERT_KEY:   // refaire insert (valeurs mémorisées par l'undo)
                    doInsert(c.t0, c.values, c.interps);
                    inv = capturePose(c.t0); inv.type = NkAnimEditKind::INSERT_KEY; inv.t0 = c.t0;
                    break;
                case NkAnimEditKind::DELETE_KEY:
                    inv = capturePose(c.t0); inv.type = NkAnimEditKind::DELETE_KEY; inv.t0 = c.t0;
                    doDelete(c.t0);
                    break;
                case NkAnimEditKind::MOVE_KEY:
                    doMove(c.t0, c.t1);
                    inv = c; inv.type = NkAnimEditKind::MOVE_KEY; inv.t0 = c.t1; inv.t1 = c.t0;
                    break;
            }
            mUndo.PushBack(inv);
        }

    } // namespace renderer
} // namespace nkentseu
