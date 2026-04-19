#pragma once
// -----------------------------------------------------------------------------
// FICHIER: NkECS/Components/Animation/NkAnimation.h
// DESCRIPTION: Composants d'animation et de mesh.
//   NkStaticMesh    — mesh statique (geometry + matériaux)
//   NkSkeletalMesh  — mesh avec squelette (skin)
//   NkSkeleton      — squelette (hiérarchie d'os)
//   NkAnimationClip — clip d'animation (keyframes)
//   NkAnimator      — contrôleur d'animation (state machine)
//   NkMeshEditor    — informations pour l'éditeur de mesh
//   NkBlendShape    — morph targets (blend shapes)
// -----------------------------------------------------------------------------

#include "../../NkECSDefines.h"
#include "../../Core/NkTypeRegistry.h"
#include "../Core/NkTransform.h"
#include <cstring>

namespace nkentseu { namespace ecs {

// =============================================================================
// NkStaticMesh — mesh 3D statique (pas de squelette)
// =============================================================================

enum class NkMeshTopology : uint8 {
    Triangles = 0,
    Quads     = 1,
    Lines     = 2,
    Points    = 3,
};

struct NkBoundingBox {
    NkVec3 min = {-0.5f,-0.5f,-0.5f};
    NkVec3 max = { 0.5f, 0.5f, 0.5f};
    [[nodiscard]] NkVec3 Center() const noexcept {
        return {(min.x+max.x)*0.5f, (min.y+max.y)*0.5f, (min.z+max.z)*0.5f};
    }
    [[nodiscard]] NkVec3 Extent() const noexcept {
        return {(max.x-min.x)*0.5f, (max.y-min.y)*0.5f, (max.z-min.z)*0.5f};
    }
};

struct NkStaticMesh {
    uint32          meshId          = 0;       // handle GPU
    char            meshPath[256]   = {};      // chemin source (FBX, OBJ, GLB...)
    uint32          subMeshCount    = 1;
    uint32          vertexCount     = 0;
    uint32          indexCount      = 0;
    NkMeshTopology  topology        = NkMeshTopology::Triangles;
    NkBoundingBox   bounds          = {};
    bool            isReadable      = false;   // CPU data accessible
    bool            generateMipMaps = true;

    // LODs (Levels of Detail)
    static constexpr uint32 kMaxLODs = 8u;
    uint32 lodMeshIds[kMaxLODs]  = {};
    float32 lodDistances[kMaxLODs] = {0,20,50,100,200,500,1000,2000};
    uint32 lodCount = 1;
};
NK_COMPONENT(NkStaticMesh)

// =============================================================================
// NkBone — description d'un os du squelette
// =============================================================================

struct NkBone {
    static constexpr uint32 kMaxBoneNameLen = 64u;
    char    name[kMaxBoneNameLen] = {};
    int32   parent        = -1;      // index du parent (-1 = racine)
    NkMat4  bindPose      = NkMat4::Identity(); // pose de repos (bind pose)
    NkMat4  inverseBindPose = NkMat4::Identity();
    NkVec3  localPosition = {};
    NkQuat  localRotation = NkQuat::Identity();
    NkVec3  localScale    = NkVec3::One();
};

// =============================================================================
// NkSkeleton — hiérarchie d'os (partageable entre plusieurs NkSkeletalMesh)
// =============================================================================

struct NkSkeleton {
    static constexpr uint32 kMaxBones = 256u;
    NkBone  bones[kMaxBones]          = {};
    uint32  boneCount                 = 0;
    char    skeletonPath[256]         = {};
    uint32  skeletonId                = 0;   // handle GPU (buffer d'os)

    // Matrices de transform finales (pose courante × inverse bind pose)
    NkMat4  skinMatrices[kMaxBones]   = {};

    [[nodiscard]] int32 FindBone(const char* name) const noexcept {
        for (uint32 i = 0; i < boneCount; ++i)
            if (std::strcmp(bones[i].name, name) == 0) return static_cast<int32>(i);
        return -1;
    }
};
NK_COMPONENT(NkSkeleton)

// =============================================================================
// NkSkeletalMesh — mesh avec squelette et skin
// =============================================================================

struct NkSkeletalMesh {
    uint32          meshId          = 0;
    char            meshPath[256]   = {};
    uint32          subMeshCount    = 1;
    uint32          vertexCount     = 0;
    uint32          indexCount      = 0;
    NkBoundingBox   bounds          = {};
    bool            isReadable      = false;

    uint32          skeletonId      = 0;     // référence à un NkSkeleton partagé
    uint32          materialIds[8]  = {};
    uint32          materialCount   = 1;

    // Blend shapes
    static constexpr uint32 kMaxBlendShapes = 64u;
    char    blendShapeNames[kMaxBlendShapes][64] = {};
    float32 blendShapeWeights[kMaxBlendShapes]   = {};
    uint32  blendShapeCount = 0;

    bool    castShadow    = true;
    bool    receiveShadow = true;
    bool    visible       = true;
    bool    updateWhenOffscreen = false;
};
NK_COMPONENT(NkSkeletalMesh)

// =============================================================================
// NkAnimationCurve — courbe d'animation (keyframes flottants)
// =============================================================================

struct NkKeyframe {
    float32 time  = 0.f;
    float32 value = 0.f;
    float32 inTangent  = 0.f;
    float32 outTangent = 0.f;
};

struct NkAnimationCurve {
    static constexpr uint32 kMaxKeyframes = 64u;
    NkKeyframe keys[kMaxKeyframes] = {};
    uint32     keyCount = 0;

    float32 Evaluate(float32 t) const noexcept {
        if (keyCount == 0) return 0.f;
        if (keyCount == 1 || t <= keys[0].time)   return keys[0].value;
        if (t >= keys[keyCount-1].time)             return keys[keyCount-1].value;

        // Trouve les deux keyframes encadrant t
        for (uint32 i = 0; i < keyCount - 1; ++i) {
            if (t >= keys[i].time && t <= keys[i+1].time) {
                const float32 dt = keys[i+1].time - keys[i].time;
                const float32 u  = (dt > 0.f) ? ((t - keys[i].time) / dt) : 0.f;
                // Hermite cubic interpolation
                const float32 u2 = u*u, u3 = u2*u;
                const float32 h00 = 2*u3-3*u2+1, h10 = u3-2*u2+u;
                const float32 h01 = -2*u3+3*u2,  h11 = u3-u2;
                return h00*keys[i].value + h10*dt*keys[i].outTangent
                     + h01*keys[i+1].value + h11*dt*keys[i+1].inTangent;
            }
        }
        return keys[keyCount-1].value;
    }
};

// =============================================================================
// NkAnimationClip — clip d'animation (ensemble de courbes pour des propriétés)
// =============================================================================

enum class NkWrapMode : uint8 {
    Once         = 0,  // joue une fois et s'arrête
    Loop         = 1,  // boucle
    PingPong     = 2,  // aller-retour
    ClampForever = 3,  // se fige sur la dernière frame
};

struct NkAnimationProperty {
    // Path vers la propriété animée (ex: "Arm.LocalRotation.x")
    char  path[256]      = {};
    char  propertyName[64] = {};
    uint32 boneIndex     = 0;  // index dans NkSkeleton
    uint32 curveIndex    = 0;  // index dans le tableau de courbes
};

struct NkAnimationClip {
    static constexpr uint32 kMaxProperties = 128u;
    static constexpr uint32 kMaxCurves     = 512u;

    char        name[128]   = {};
    char        clipPath[256] = {};
    float32     length       = 0.f;  // durée en secondes
    float32     frameRate    = 30.f;
    NkWrapMode  wrapMode     = NkWrapMode::Loop;
    bool        isLooping    = true;
    bool        hasRootMotion = false;
    uint32      clipId       = 0;    // handle GPU

    NkAnimationProperty properties[kMaxProperties] = {};
    uint32              propertyCount = 0;
    NkAnimationCurve    curves[kMaxCurves]         = {};
    uint32              curveCount = 0;
};
NK_COMPONENT(NkAnimationClip)

// =============================================================================
// NkAnimatorState — état dans la machine à états de l'Animator
// =============================================================================

struct NkAnimatorTransition {
    uint32  toStateIndex    = 0;
    char    conditionParam[64] = {};
    float32 conditionValue  = 0.f;
    float32 exitTime        = 1.f;    // temps normalisé [0..1] pour auto-transition
    float32 transitionDuration = 0.2f; // durée du blend en secondes
    bool    hasExitTime     = false;
    bool    conditionIsGreater = true;
};

struct NkAnimatorState {
    static constexpr uint32 kMaxTransitions = 16u;
    char    name[128]       = {};
    uint32  clipId          = 0;      // index du clip dans NkAnimator.clips
    float32 speed           = 1.f;
    float32 speedMultiplierParam = 0.f; // si > 0, multiplie speed par param
    bool    loop            = true;

    NkAnimatorTransition transitions[kMaxTransitions] = {};
    uint32               transitionCount = 0;
};

// =============================================================================
// NkAnimator — contrôleur d'animation (state machine)
// =============================================================================

struct NkAnimator {
    static constexpr uint32 kMaxStates  = 64u;
    static constexpr uint32 kMaxClips   = 64u;
    static constexpr uint32 kMaxParams  = 32u;
    static constexpr uint32 kMaxLayers  = 8u;

    // ── Clips référencés ──────────────────────────────────────────────────────
    uint32  clipIds[kMaxClips] = {};
    char    clipNames[kMaxClips][128] = {};
    uint32  clipCount = 0;

    // ── States ────────────────────────────────────────────────────────────────
    NkAnimatorState states[kMaxStates] = {};
    uint32          stateCount = 0;
    uint32          currentState = 0;
    uint32          nextState    = 0xFFFFFFFFu; // 0xFFFF = pas de transition
    float32         transitionProgress = 0.f;   // [0..1]
    float32         transitionDuration = 0.f;

    // ── Paramètres (float, int, bool, trigger) ────────────────────────────────
    struct Param {
        char    name[64] = {};
        float32 value    = 0.f;   // float ou bool (0/1) ou trigger (0/1)
        bool    isTrigger = false;
    };
    Param   params[kMaxParams] = {};
    uint32  paramCount = 0;

    // ── Playback ──────────────────────────────────────────────────────────────
    float32 time         = 0.f;    // temps dans l'état courant (secondes)
    float32 normalizedTime = 0.f;  // [0..1]
    float32 playbackSpeed = 1.f;
    bool    playing       = true;
    bool    applyRootMotion = false;

    // ── API ───────────────────────────────────────────────────────────────────
    void Play(const char* stateName) noexcept {
        for (uint32 i = 0; i < stateCount; ++i)
            if (std::strcmp(states[i].name, stateName) == 0) {
                currentState = i; time = 0.f; normalizedTime = 0.f; return;
            }
    }

    void SetFloat(const char* paramName, float32 value) noexcept {
        for (uint32 i = 0; i < paramCount; ++i)
            if (std::strcmp(params[i].name, paramName) == 0) { params[i].value = value; return; }
    }

    void SetBool(const char* paramName, bool value) noexcept {
        SetFloat(paramName, value ? 1.f : 0.f);
    }

    void SetTrigger(const char* paramName) noexcept {
        for (uint32 i = 0; i < paramCount; ++i)
            if (std::strcmp(params[i].name, paramName) == 0 && params[i].isTrigger) {
                params[i].value = 1.f; return;
            }
    }

    [[nodiscard]] float32 GetFloat(const char* paramName) const noexcept {
        for (uint32 i = 0; i < paramCount; ++i)
            if (std::strcmp(params[i].name, paramName) == 0) return params[i].value;
        return 0.f;
    }

    [[nodiscard]] bool IsInState(const char* stateName) const noexcept {
        return currentState < stateCount &&
               std::strcmp(states[currentState].name, stateName) == 0;
    }
};
NK_COMPONENT(NkAnimator)

// =============================================================================
// NkMeshEditor — informations pour l'éditeur de mesh procédural
// Permet de créer/modifier des meshes en temps réel (outil de modélisation)
// =============================================================================

struct NkMeshEditorVertex {
    NkVec3 position  = {};
    NkVec3 normal    = {};
    NkVec2 uv        = {};
    NkVec4 color     = {1,1,1,1};
    bool   selected  = false;
};

struct NkMeshEditorEdge {
    uint32 v0 = 0, v1 = 0;
    bool   selected = false;
    bool   sharp    = false;   // arête vive (sharp edge)
    bool   seam     = false;   // couture UV
};

struct NkMeshEditorFace {
    static constexpr uint32 kMaxFaceVerts = 8u;
    uint32 verts[kMaxFaceVerts] = {};
    uint32 vertCount = 0;
    bool   selected  = false;
    uint32 materialIndex = 0;
    NkVec3 faceNormal = {};
};

enum class NkMeshEditMode : uint8 {
    Object  = 0,  // transformation globale
    Vertex  = 1,  // édition de sommets
    Edge    = 2,  // édition d'arêtes
    Face    = 3,  // édition de faces
};

struct NkMeshEditor {
    static constexpr uint32 kMaxVerts = 65536u;
    static constexpr uint32 kMaxEdges = 131072u;
    static constexpr uint32 kMaxFaces = 32768u;

    NkMeshEditMode editMode  = NkMeshEditMode::Object;
    bool           isDirty   = false;   // mesh modifié → rebuild GPU
    bool           showWireframe = false;
    bool           snapToGrid   = false;
    float32        gridSize     = 0.1f;
    bool           proportionalEdit = false;
    float32        proportionalRadius = 1.f;
    bool           xMirror = false;     // symétrie X
    bool           yMirror = false;
    bool           zMirror = false;

    // Données de l'éditeur (sur le CPU — peut être très large)
    // En production, ces tableaux seraient des vecteurs alloués dynamiquement
    // Ici on utilise des tailles statiques pour la simplicité du component
    uint32 vertCount = 0;
    uint32 edgeCount = 0;
    uint32 faceCount = 0;

    // Les données réelles sont dans un buffer externe géré par NkMeshEditorSystem
    uint64 editorDataHandle = 0;    // handle vers les données editor CPU
    uint32 gpuMeshId        = 0;    // mesh GPU courant

    // Opérations enregistrées (undo/redo)
    uint32 undoStackDepth   = 0;
    uint32 undoStackMax     = 64u;
};
NK_COMPONENT(NkMeshEditor)

// =============================================================================
// NkIKChain — chaîne IK (Inverse Kinematics)
// =============================================================================

struct NkIKChain {
    static constexpr uint32 kMaxChainBones = 16u;
    char    effectorBone[64] = {};       // os effecteur (extrémité)
    char    rootBone[64]     = {};       // os racine de la chaîne
    NkVec3  target           = {};       // position cible world
    uint32  chainLength      = 3;       // nombre d'os dans la chaîne
    uint32  iterations       = 10;      // itérations FABRIK
    float32 tolerance        = 0.001f;  // convergence
    bool    enabled          = true;
    bool    hasTargetEntity  = false;
    NkEntityId targetEntity  = NkEntityId::Invalid();
    NkVec3  poleVector       = {0,1,0}; // vecteur pôle (pour IK 2 segments)
};
NK_COMPONENT(NkIKChain)

}} // namespace nkentseu::ecs
