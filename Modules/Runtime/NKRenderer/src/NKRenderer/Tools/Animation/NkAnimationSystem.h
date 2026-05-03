#pragma once
// =============================================================================
// NkAnimationSystem.h  — NKRenderer v4.0  (Tools/Animation/)
//
// Système d'animation de RENDU généraliste.
// "Animation" = toute valeur qui évolue dans le temps pour produire un résultat
// visuel. NkAnimationSystem ne se limite PAS aux bones : il anime TOUT ce qui
// peut être rendu.
//
// Catégories d'animation supportées :
//   ─ Skeletal / Bones  : skinning GPU, squelette, onion skinning
//   ─ Morph targets     : blend shapes FACS, visage, déformation mesh
//   ─ UV / Texture      : scrolling UV, flip-book sprite, atlas séquence
//   ─ Couleur / Matériau: interpolation albedo, emissive, metallic, roughness
//   ─ Transform objet   : position, rotation, échelle (sans ECS)
//   ─ Caméra            : dolly, zoom, shake, cut
//   ─ Lumière           : intensité, couleur, position, clignotement
//   ─ Post-process      : DOF, bloom, exposition, color grading animés
//   ─ Propriétés custom : float/vec2/vec3/vec4 nommés
//
// Architecture :
//   NkAnimationClip   — données de keyframes (read-only pendant la lecture)
//   NkAnimationPlayer — joue un clip, maintient le temps courant
//   NkAnimationState  — snapshot évalué à un instant t (résultats prêts à lire)
//   NkAnimationSystem — gestionnaire central, update + application au renderer
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
namespace renderer {

    class NkRender3D;
    class NkMaterialInstance;
    class NkMeshSystem;
    struct NkPostConfig;

    // =========================================================================
    // Mode d'interpolation
    // =========================================================================
    enum class NkInterpMode : uint8 {
        NK_STEP,           // Valeur constante jusqu'au prochain keyframe
        NK_LINEAR,         // Lerp
        NK_CUBIC,          // Spline Catmull-Rom
        NK_EASE_IN,        // Acceleration au départ
        NK_EASE_OUT,       // Décélération à l'arrivée
        NK_EASE_IN_OUT,    // Les deux
        NK_BOUNCE,         // Rebond
        NK_ELASTIC,        // Élastique
        NK_BACK,           // Léger dépassement
    };

    // =========================================================================
    // Mode de lecture
    // =========================================================================
    enum class NkPlayMode : uint8 {
        NK_ONCE,       // Une seule fois, puis stop
        NK_LOOP,       // Boucle infinie
        NK_PING_PONG,  // Aller-retour
        NK_CLAMP,      // Figé à la dernière frame
    };

    // =========================================================================
    // Keyframe générique
    // =========================================================================
    template<typename T>
    struct NkKeyframe {
        float32      time;
        T            value;
        NkInterpMode interp = NkInterpMode::NK_LINEAR;
    };

    // =========================================================================
    // NkAnimationTrack<T> — courbe d'animation d'une valeur typée
    // =========================================================================
    template<typename T>
    class NkAnimationTrack {
    public:
        NkString name;
        bool     enabled = true;

        void AddKey(float32 t, const T& v,
                     NkInterpMode interp = NkInterpMode::NK_LINEAR) {
            NkKeyframe<T> kf; kf.time=t; kf.value=v; kf.interp=interp;
            uint32 i=0;
            while(i<(uint32)mKeys.Size() && mKeys[i].time<t) i++;
            mKeys.Insert(i, kf);
        }

        T Evaluate(float32 t) const {
            if (mKeys.Empty()) return T{};
            uint32 n=(uint32)mKeys.Size();
            if (t<=mKeys[0].time)   return mKeys[0].value;
            if (t>=mKeys[n-1].time) return mKeys[n-1].value;
            uint32 hi=1;
            while(hi<n && mKeys[hi].time<=t) hi++;
            uint32 lo=hi-1;
            float32 dt=mKeys[hi].time-mKeys[lo].time;
            float32 a=(dt>1e-6f)?(t-mKeys[lo].time)/dt:0.f;
            return Lerp(mKeys[lo].value, mKeys[hi].value,
                         EaseAlpha(a, mKeys[lo].interp));
        }

        bool    Empty()       const { return mKeys.Empty(); }
        float32 GetDuration() const {
            return mKeys.Empty()?0.f:mKeys[mKeys.Size()-1].time;
        }
        uint32  KeyCount()    const { return (uint32)mKeys.Size(); }

    private:
        NkVector<NkKeyframe<T>> mKeys;

        static float32 EaseAlpha(float32 a, NkInterpMode m) {
            switch(m) {
                case NkInterpMode::NK_STEP:       return 0.f;
                case NkInterpMode::NK_EASE_IN:    return a*a;
                case NkInterpMode::NK_EASE_OUT:   return 1.f-(1.f-a)*(1.f-a);
                case NkInterpMode::NK_EASE_IN_OUT:
                    return a<0.5f ? 2.f*a*a : 1.f-(-2.f*a+2.f)*(-2.f*a+2.f)*0.5f;
                case NkInterpMode::NK_BOUNCE: {
                    float32 n=1.f-a;
                    float32 d=2.75f,b=7.5625f;
                    if(n<1.f/d) return 1.f-b*n*n;
                    if(n<2.f/d){n-=1.5f/d; return 1.f-(b*n*n+0.75f);}
                    if(n<2.5f/d){n-=2.25f/d; return 1.f-(b*n*n+0.9375f);}
                    n-=2.625f/d; return 1.f-(b*n*n+0.984375f);
                }
                case NkInterpMode::NK_ELASTIC: {
                    float32 c4=(2.f*3.14159f)/3.f;
                    if(a<=0.f)return 0.f; if(a>=1.f)return 1.f;
                    return (float32)powf(2.f,-10.f*a)*sinf((a*10.f-0.75f)*c4)+1.f;
                }
                case NkInterpMode::NK_BACK: {
                    float32 c1=1.70158f,c3=c1+1.f;
                    return 1.f+c3*(a-1.f)*(a-1.f)*(a-1.f)+c1*(a-1.f)*(a-1.f);
                }
                default: return a;
            }
        }

        // Lerp générique — overloads dans .cpp
        T Lerp(const T& a, const T& b, float32 t) const;
    };

    // =========================================================================
    // NkAnimationClip — ensemble de tracks décrivant une animation complète
    // =========================================================================
    class NkAnimationClip {
    public:
        NkString  name;
        float32   duration  = 0.f;
        float32   fps       = 30.f;
        bool      loop      = true;

        // ── Skeletal ─────────────────────────────────────────────────────────
        NkVector<NkAnimationTrack<NkMat4f>> boneTracks;  // un par bone
        uint32 boneCount = 0;

        // ── Morph targets ─────────────────────────────────────────────────────
        NkVector<NkAnimationTrack<float32>> morphTracks;
        NkVector<NkString>                  morphNames;

        // ── UV / Sprite ───────────────────────────────────────────────────────
        NkAnimationTrack<NkVec2f> uvOffset;
        NkAnimationTrack<NkVec2f> uvScale;
        NkAnimationTrack<float32> uvRotation;
        NkAnimationTrack<int32>   spriteFrame;
        uint32 spriteAtlasCols = 1;
        uint32 spriteAtlasRows = 1;

        // ── Matériau ──────────────────────────────────────────────────────────
        NkAnimationTrack<NkVec4f> albedoColor;
        NkAnimationTrack<NkVec3f> emissiveColor;
        NkAnimationTrack<float32> emissiveStrength;
        NkAnimationTrack<float32> metallic;
        NkAnimationTrack<float32> roughness;
        NkAnimationTrack<float32> opacity;
        NkHashMap<NkString, NkAnimationTrack<float32>> customFloats;
        NkHashMap<NkString, NkAnimationTrack<NkVec4f>> customVec4s;

        // ── Transform objet ───────────────────────────────────────────────────
        NkAnimationTrack<NkVec3f> position;
        NkAnimationTrack<NkVec4f> rotation;  // quaternion (x,y,z,w)
        NkAnimationTrack<NkVec3f> scale;

        // ── Caméra ────────────────────────────────────────────────────────────
        NkAnimationTrack<NkVec3f> cameraPosition;
        NkAnimationTrack<NkVec3f> cameraTarget;
        NkAnimationTrack<float32> cameraFOV;
        NkAnimationTrack<float32> cameraDOFFocus;
        NkAnimationTrack<float32> cameraDOFAperture;

        // ── Lumière ───────────────────────────────────────────────────────────
        NkAnimationTrack<float32> lightIntensity;
        NkAnimationTrack<NkVec3f> lightColor;
        NkAnimationTrack<NkVec3f> lightPosition;
        NkAnimationTrack<float32> lightRange;

        // ── Post-process ──────────────────────────────────────────────────────
        NkAnimationTrack<float32> ppExposure;
        NkAnimationTrack<float32> ppSaturation;
        NkAnimationTrack<float32> ppContrast;
        NkAnimationTrack<float32> ppBloomStrength;
        NkAnimationTrack<float32> ppDOFFocus;
        NkAnimationTrack<float32> ppVignetteIntensity;

        // ── Helpers ───────────────────────────────────────────────────────────
        void RecalcDuration();

        void AddBoneKey(uint32 boneIdx, float32 time, const NkMat4f& mat,
                         NkInterpMode interp = NkInterpMode::NK_LINEAR);
        void ResizeBones(uint32 count);

        void BuildSpriteFlipBook(uint32 frameCount, float32 spriteFPS = 12.f);

        // Clips prédéfinis
        static NkAnimationClip* MakeSpinClip(const NkString& name,
                                               float32 rpm, NkVec3f axis = {0,1,0});
        static NkAnimationClip* MakeLightPulse(const NkString& name,
                                                float32 minI, float32 maxI,
                                                float32 freq = 1.f);
        static NkAnimationClip* MakeColorFade(const NkString& name,
                                               NkVec4f from, NkVec4f to,
                                               float32 dur,
                                               NkInterpMode interp = NkInterpMode::NK_EASE_IN_OUT);
        static NkAnimationClip* MakeCameraShake(const NkString& name,
                                                  float32 intensity, float32 dur,
                                                  float32 freq = 15.f);
        static NkAnimationClip* MakeProceduralWalk(uint32 boneCount, float32 dur = 1.f);
    };

    // =========================================================================
    // NkAnimationState — état évalué, prêt à être appliqué au renderer
    // =========================================================================
    struct NkAnimationState {
        // Skeletal
        NkVector<NkMat4f> boneMatrices;
        // Morph
        NkVector<float32> morphWeights;
        // UV
        NkVec2f  uvOffset     = {0,0};
        NkVec2f  uvScale      = {1,1};
        float32  uvRotation   = 0.f;
        int32    spriteFrame  = 0;
        NkVec4f  spriteUV     = {0,0,1,1};  // x0,y0,x1,y1 dans l'atlas
        // Matériau
        NkVec4f  albedo            = {1,1,1,1};
        NkVec3f  emissive          = {0,0,0};
        float32  emissiveStrength  = 0.f;
        float32  metallic          = 0.f;
        float32  roughness         = 0.5f;
        float32  opacity           = 1.f;
        // Transform
        NkVec3f  position  = {0,0,0};
        NkVec4f  rotation  = {0,0,0,1};
        NkVec3f  scale     = {1,1,1};
        NkMat4f  transform;
        // Caméra
        NkVec3f  camPos    = {0,0,5};
        NkVec3f  camTarget = {0,0,0};
        float32  camFOV    = 65.f;
        float32  camDOFFocus = 10.f;
        float32  camDOFApt  = 0.1f;
        // Lumière
        float32  lightIntensity = 1.f;
        NkVec3f  lightColor     = {1,1,1};
        NkVec3f  lightPos       = {0,5,0};
        float32  lightRange     = 10.f;
        // Post-process
        float32  ppExposure     = 1.f;
        float32  ppSaturation   = 1.f;
        float32  ppContrast     = 1.f;
        float32  ppBloom        = 0.04f;
        float32  ppDOFFocus     = 10.f;
        float32  ppVignette     = 0.f;
    };

    // =========================================================================
    // NkAnimationPlayer — joue un NkAnimationClip
    // =========================================================================
    class NkAnimationPlayer {
    public:
        NkString name;

        void SetClip(const NkAnimationClip* clip, bool autoResize = true);
        const NkAnimationClip* GetClip() const { return mClip; }

        void    Play  (NkPlayMode mode = NkPlayMode::NK_LOOP, float32 speed = 1.f);
        void    Pause ();
        void    Stop  ();
        void    SeekTo(float32 t);
        bool    IsPlaying()  const { return mPlaying; }
        float32 GetTime()    const { return mTime; }
        float32 GetNormTime()const;
        int32   GetFrame()   const;

        void Update(float32 dt);

        const NkAnimationState& GetState() const { return mState; }

        // Crossfade vers un autre clip
        void BlendTo(const NkAnimationClip* next, float32 blendDur = 0.25f);
        bool IsBlending() const { return mBlendT > 0.f; }

        // Événements temporels (markers)
        using MarkerFn = NkFunction<void(const NkString&)>;
        void AddMarker(float32 t, const NkString& ev);
        void SetMarkerCallback(MarkerFn fn) { mMarkerCb = fn; }

    private:
        const NkAnimationClip* mClip       = nullptr;
        const NkAnimationClip* mNextClip   = nullptr;
        float32  mTime       = 0.f;
        float32  mSpeed      = 1.f;
        bool     mPlaying    = false;
        NkPlayMode mMode     = NkPlayMode::NK_LOOP;
        float32  mBlendT     = 0.f;
        float32  mBlendDur   = 0.f;
        NkAnimationState mState;
        NkAnimationState mBlendState;
        MarkerFn mMarkerCb;

        struct Marker { float32 time; NkString name; bool fired = false; };
        NkVector<Marker> mMarkers;

        void Evaluate(const NkAnimationClip* clip, float32 t, NkAnimationState& out);
        void BlendStates(NkAnimationState& a, const NkAnimationState& b, float32 w);
        void WrapTime(float32& t, float32 dur);
        void ComputeSpriteUV(NkAnimationState& s, const NkAnimationClip* clip);
        void ComputeTransformMatrix(NkAnimationState& s);
    };

    // =========================================================================
    // NkAnimationSystem — gestionnaire central
    // =========================================================================
    class NkAnimationSystem {
    public:
        NkAnimationSystem() = default;
        ~NkAnimationSystem();

        bool Init(NkIDevice* device, NkRender3D* r3d);
        void Shutdown();

        // ── Update ────────────────────────────────────────────────────────────
        void Update(float32 dt);

        // ── Clips ─────────────────────────────────────────────────────────────
        NkAnimationClip* CreateClip (const NkString& name);
        NkAnimationClip* FindClip   (const NkString& name) const;
        void             DestroyClip(NkAnimationClip*& clip);

        // ── Players ───────────────────────────────────────────────────────────
        NkAnimationPlayer* CreatePlayer (const NkString& name = "");
        NkAnimationPlayer* FindPlayer   (const NkString& name) const;
        void               DestroyPlayer(NkAnimationPlayer*& player);

        // ─────────────────────────────────────────────────────────────────────
        // Application du state au renderer
        // ─────────────────────────────────────────────────────────────────────

        // Skeletal — soumet un mesh skinné
        void ApplySkinnedMesh(NkMeshHandle mesh, NkMatInstHandle mat,
                               const NkMat4f& baseWorld,
                               const NkAnimationPlayer& player);

        // Skeletal — onion skinning autour du frame courant
        void ApplyOnionSkin(NkMeshHandle mesh, NkMatInstHandle mat,
                             const NkMat4f& baseWorld,
                             const NkAnimationPlayer& player,
                             const int32* ghostOffsets, uint32 ghostCount,
                             NkVec4f pastColor   = {1.f,0.3f,0.3f,0.5f},
                             NkVec4f futureColor = {0.3f,0.3f,1.f,0.5f});

        // Transform — calcule la matrice world depuis le state
        NkMat4f ApplyTransform(const NkAnimationPlayer& player,
                                const NkMat4f& base = NkMat4f::Identity());

        // Matériau — écrit les paramètres animés dans une NkMaterialInstance
        void ApplyMaterial(NkMaterialInstance* inst, const NkAnimationPlayer& player);

        // Caméra — applique le state à une NkCamera3D
        void ApplyCamera(NkCamera3D& cam, const NkAnimationPlayer& player);

        // Lumière — applique le state à une NkLightDesc
        void ApplyLight(NkLightDesc& light, const NkAnimationPlayer& player);

        // Post-process — applique le state à un NkPostConfig
        void ApplyPostProcess(NkPostConfig& pp, const NkAnimationPlayer& player);

        // UV — retourne l'UV rect animé {u0,v0,u1,v1}
        NkVec4f GetAnimatedSpriteUV(const NkAnimationPlayer& player);

        // Squelette — dessine le squelette en debug
        void DrawSkeleton(const NkAnimationPlayer& player,
                           const NkMat4f& baseWorld,
                           const int32* parentIdx,
                           NkVec4f boneColor = {1.f,0.6f,0.f,1.f},
                           float32 boneSize  = 0.05f);

        // Morph targets CPU/GPU
        NkMeshHandle ApplyMorphTargets(NkMeshHandle base,
                                        const NkMeshHandle* targets,
                                        const float32*      weights,
                                        uint32              count,
                                        bool                useGPU = false);

        uint32 GetPlayerCount() const { return (uint32)mPlayers.Size(); }

    private:
        NkIDevice*  mDevice   = nullptr;
        NkRender3D* mR3D      = nullptr;

        NkVector<NkAnimationClip*>   mClips;
        NkVector<NkAnimationPlayer*> mPlayers;
        NkVector<NkMat4f>            mTmpBones;
        NkPipelineHandle             mMorphCompute;
    };

} // namespace renderer
} // namespace nkentseu
