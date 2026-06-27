#pragma once
// =============================================================================
// AnimBridge.h — pont anim <-> UI. Interface en types FOUNDATION uniquement
// (float/NkVec3f/NkVector). N'inclut NI NKRenderer NI l'Editor Kit : évite le
// conflit de types nkentseu::renderer::NkBlendMode/NkVertex2D défini À LA FOIS
// par NKRenderer ET NKCanvas (Editor Kit). Toute la logique anim (clip/player/
// editor/glTF) vit dans AnimBridge.cpp (seul TU à inclure NKRenderer).
// =============================================================================
#include "NKMath/NKMath.h"
#include "NKContainers/NKContainers.h"

namespace nkanima {

    using nkentseu::float32;
    using nkentseu::int32;
    using nkentseu::uint32;
    using nkentseu::math::NkVec3f;
    template<typename T> using NkVector = nkentseu::NkVector<T>;

    // Cycle de vie / lecture
    bool   AnimInit(const char* modelPath);    // charge + bake ; true si OK
    bool   AnimLoaded();
    void   AnimUpdate(float32 dt);             // avance le player si en lecture
    bool   AnimIsPlaying();
    void   AnimSetPlaying(bool playing);

    // Curseur / temps
    float32 AnimCursor();
    void    AnimSetCursor(float32 t);
    void    AnimSeek(float32 t);               // seek player + curseur (scrub)
    float32 AnimDuration();
    float32 AnimFps();

    // Poses-clés
    uint32 AnimKeyCount();
    void   AnimGetKeyTimes(NkVector<float32>& out);
    bool   AnimIsSelected(float32 t);
    void   AnimSelectKey(float32 t);
    void   AnimClearSelection();
    void   AnimInsertKeyAtCursor();
    void   AnimDeleteSelected();
    void   AnimMoveKey(float32 tOld, float32 tNew);
    void   AnimUndo();
    void   AnimRedo();

    // Squelette 2D (positions MONDE des joints + parent) à la pose courante.
    // (En mode édition, renvoie la pose de TRAVAIL éditée.)
    uint32 AnimJointCount();
    void   AnimGetSkeleton(NkVector<NkVec3f>& outPos, NkVector<int32>& outParent);

    // ── Édition de pose (§2 Pose Mode) ────────────────────────────────────────
    // Entre en édition : capture la pose courante comme pose de TRAVAIL, met la
    // lecture en pause. Le squelette affiché devient éditable.
    void  AnimBeginPoseEdit();
    bool  AnimInPoseEdit();
    void  AnimEndPoseEdit();                 // sort sans enregistrer
    // IK-drag : tire le joint `jointIdx` vers la cible MONDE (wx,wy,wz). Résout une
    // courte chaîne IK (FABRIK) et met à jour la pose de travail.
    void  AnimDragJoint(int32 jointIdx, float32 wx, float32 wy, float32 wz);
    // Enregistre la pose de travail en pose-clé au curseur courant.
    void  AnimCommitPoseKey();

} // namespace nkanima
