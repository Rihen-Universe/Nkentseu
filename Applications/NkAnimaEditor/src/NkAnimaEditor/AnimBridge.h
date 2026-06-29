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
    using nkentseu::uint8;
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

    // ── Ragdoll physique (couplage NKPhysics) ─────────────────────────────────
    // Active/désactive la simulation : ON capture la pose courante, construit un
    // ragdoll depuis le squelette, et chaque AnimUpdate(dt) le simule -> le perso
    // réagit physiquement (chute/ballant), la pose suit la physique.
    void  AnimSetPhysics(bool on);
    bool  AnimPhysicsEnabled();

    // ── Édition de pose (§2 Pose Mode) ────────────────────────────────────────
    // Entre en édition : capture la pose courante comme pose de TRAVAIL, met la
    // lecture en pause. Le squelette affiché devient éditable.
    void  AnimBeginPoseEdit();
    bool  AnimInPoseEdit();
    void  AnimEndPoseEdit();                 // sort sans enregistrer
    // IK-drag : tire le joint `jointIdx` vers la cible MONDE (wx,wy,wz). Résout une
    // courte chaîne IK (FABRIK) et met à jour la pose de travail.
    void  AnimDragJoint(int32 jointIdx, float32 wx, float32 wy, float32 wz);
    // FK-rotate : fait pivoter le joint `jointIdx` sur lui-même de `deltaRadians`
    // autour de l'axe Z monde (normale du plan d'aperçu) ; les enfants suivent en
    // FK. Édition directe « os par os » façon Blender/Cascadeur (R).
    void  AnimRotateJoint(int32 jointIdx, float32 deltaRadians);
    // FK-translate : déplace le joint `jointIdx` de (dx,dy,dz) en MONDE ; tout le
    // sous-arbre suit (les enfants conservent leur offset local). Façon Blender (G).
    void  AnimTranslateJoint(int32 jointIdx, float32 dx, float32 dy, float32 dz);
    // Position MONDE d'un joint dans la pose de travail (pour l'ancrage du gizmo).
    void  AnimJointWorldPos(int32 jointIdx, float32& x, float32& y, float32& z);
    // Enregistre la pose de travail en pose-clé au curseur courant.
    void  AnimCommitPoseKey();

    // ── Viewport 3D embarqué (NKRenderer offscreen, device PARTAGÉ avec l'UI) ──
    // texId du viewport dans le backend NKGui (AddImage côté panneau / RegisterTexture
    // côté glue). Hors plage des atlas de police (0/1).
    static const uint32 ANIM_VIEWPORT_TEXID = 4001u;

    // Fournit le device NKRHI de l'éditeur (rhi.GetDevice()) : le viewport 3D le
    // PARTAGE (pas de 2e device, pas de readback). À appeler avant le 1er rendu.
    void  Anim3DSetSharedDevice(void* device);            // NkIDevice* (void* = header NKRHI-free)
    bool  Anim3DReady();                                   // moteur 3D dispo ?
    void  Anim3DOrbit(float32 dYaw, float32 dPitch, float32 dZoom);  // caméra interactive
    // Rend la pose courante dans l'offscreen via le command buffer FOURNI (celui de
    // l'éditeur, AVANT la passe UI). Lazy-init au 1er appel. cmd = NkICommandBuffer*.
    void  Anim3DRenderOffscreen(void* cmd);
    // Publie la texture offscreen dans le backend NKGui (guiBackend = NkGuiRHIBackend*)
    // sous `texId`, pour l'afficher via AddImage. Pas de copie (même device).
    void  Anim3DRegisterInto(void* guiBackend, uint32 texId);

    // Mode d'affichage du viewport (façon Blender) : Solide = albedo flat sans
    // éclairage (toujours visible, idéal posing) ; Rendu = PBR éclairé ; Filaire =
    // wireframe. Défaut = Solide.
    enum class NkAnimViewMode : int32 { SOLIDE = 0, RENDU = 1, FILAIRE = 2 };
    void           Anim3DSetViewMode(NkAnimViewMode mode);
    NkAnimViewMode Anim3DViewMode();

} // namespace nkanima
