// =============================================================================
// NkAnimationEditor.h — NkAnima M1.c : logique d'édition de timeline (SANS UI)
// -----------------------------------------------------------------------------
// Couche moteur, réutilisable et testable sans interface : édite les poses-clés
// d'un NkAnimationClip squelettique (boneTracks bone-local). Modèle « pose-clé »
// à la Cascadeur : un keyframe = un temps t où TOUS les os ont une clé. Fournit
// curseur (playhead), snap, sélection, insert/delete/move, et undo/redo.
//
// L'UI (panneau NKGui/Editor Kit) n'aura qu'à : lire GetPoseKeyTimes()/GetCursor()
// pour dessiner, et appeler InsertPoseKey/DeletePoseKeyAt/MoveSelected/Undo/Redo
// sur interaction. Aucune dépendance UI ici.
// =============================================================================
#pragma once

#include "NKRenderer/Tools/Animation/NkAnimationSystem.h"
#include "NKContainers/NKContainers.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
    namespace renderer {

        class NkAnimationEditor {
            public:
                // Attache un clip (non possédé). Réinitialise undo/sélection.
                void SetClip(NkAnimationClip* clip);
                NkAnimationClip* GetClip() const { return mClip; }

                // ── Curseur / playhead d'édition ──────────────────────────────────────
                void    SetCursor(float32 t);
                float32 GetCursor() const { return mCursor; }
                void    SetSnap(float32 step) { mSnap = step; }  // 0 = pas de snap
                float32 SnapTime(float32 t) const;

                // ── Lecture pour l'UI ─────────────────────────────────────────────────
                // Temps triés des poses-clés (pour dessiner les losanges de la timeline).
                NkVector<float32> GetPoseKeyTimes() const;
                uint32  PoseKeyCount() const;
                float32 Duration() const { return mClip ? mClip->duration : 0.f; }
                float32 Fps()      const { return mClip ? mClip->fps : 30.f; }

                // ── Opérations sur poses-clés (toutes annulables) ─────────────────────
                // Insère une pose-clé au curseur (une clé par os = boneLocals[bone]).
                void InsertPoseKey(const NkVector<NkMat4f>& boneLocals);
                // Supprime la pose-clé au temps ~t (toutes les pistes).
                void DeletePoseKeyAt(float32 t);
                // Déplace la pose-clé de tOld vers tNew (snap appliqué).
                void MovePoseKey(float32 tOld, float32 tNew);

                // ── Sélection (par temps de pose-clé) ─────────────────────────────────
                void SelectKey(float32 t, bool addToSelection = false);
                void ClearSelection() { mSelected.Clear(); }
                const NkVector<float32>& Selection() const { return mSelected; }
                bool  IsSelected(float32 t) const;
                void  DeleteSelected();
                void  MoveSelected(float32 dt);

                // ── Undo / Redo ───────────────────────────────────────────────────────
                bool CanUndo() const { return !mUndo.Empty(); }
                bool CanRedo() const { return !mRedo.Empty(); }
                void Undo();
                void Redo();

            private:
                // Type d'opération d'édition (valeurs en UPPER_SNAKE_CASE ; suffixe _KEY
                // pour éviter la collision avec la macro Windows DELETE).
                enum class NkAnimEditKind : uint8 { INSERT_KEY, DELETE_KEY, MOVE_KEY };

                struct Cmd {
                    NkAnimEditKind type = NkAnimEditKind::INSERT_KEY;
                    float32 t0 = 0.f, t1 = 0.f;     // INSERT/DELETE: t0 ; MOVE: t0->t1
                    NkVector<NkMat4f> values;        // valeur par os (insert/delete)
                    NkVector<uint8>   interps;       // interp par os (insert/delete)
                };

                // Primitives non-annulables (utilisées par les ops + undo/redo).
                void doInsert(float32 t, const NkVector<NkMat4f>& vals, const NkVector<uint8>& interps);
                void doDelete(float32 t);
                void doMove(float32 tOld, float32 tNew);
                Cmd  capturePose(float32 t) const;   // valeurs+interps des clés à ~t
                void pushUndo(const Cmd& c);

                NkAnimationClip*  mClip = nullptr;
                float32           mCursor = 0.f;
                float32           mSnap   = 0.f;
                float32           mTol    = 1e-3f;   // tolérance "même temps"
                NkVector<float32> mSelected;
                NkVector<Cmd>     mUndo, mRedo;
        };

    } // namespace renderer
} // namespace nkentseu
