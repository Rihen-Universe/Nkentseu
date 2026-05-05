#pragma once
// =============================================================================
// Nkentseu/ECS/Components/Rendering/NkRenderComponents.h
// =============================================================================
// DUPLICATION RESOLUE : NkLightType etait defini 3 fois. Source unique ici.
// NkColor4/NkRect2D du NkRenderer.h ancien sont fusionnes ici.
// NkRenderer.h est SUPPRIME - son contenu est dans ce fichier.
// =============================================================================
#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

using namespace nkentseu::math;

namespace nkentseu {
    namespace ecs {

        struct NkColor4 {
            float32 r=1.f,g=1.f,b=1.f,a=1.f;
            NkColor4() noexcept = default;
            constexpr NkColor4(float32 r,float32 g,float32 b,float32 a=1.f) noexcept:r(r),g(g),b(b),a(a){}
            static NkColor4 White()   noexcept { return {1,1,1,1}; }
            static NkColor4 Black()   noexcept { return {0,0,0,1}; }
            static NkColor4 Red()     noexcept { return {1,0,0,1}; }
            static NkColor4 Green()   noexcept { return {0,1,0,1}; }
            static NkColor4 Blue()    noexcept { return {0,0,1,1}; }
            static NkColor4 Clear()   noexcept { return {0,0,0,0}; }
            static NkColor4 Yellow()  noexcept { return {1,1,0,1}; }
            static NkColor4 Cyan()    noexcept { return {0,1,1,1}; }
            static NkColor4 Magenta() noexcept { return {1,0,1,1}; }
            static NkColor4 FromU8(uint8 r,uint8 g,uint8 b,uint8 a=255) noexcept {
                return {r/255.f,g/255.f,b/255.f,a/255.f};
            }
        };

        struct NkRect2D {
            float32 x=0.f,y=0.f,w=1.f,h=1.f;
            NkRect2D() noexcept = default;
            constexpr NkRect2D(float32 x,float32 y,float32 w,float32 h) noexcept:x(x),y(y),w(w),h(h){}
        };

        /**
         * @enum NkLightType (ECS, uint8)
         * Mapping vers renderer::NkLightType effectue par NkRenderSystem.
         */
        enum class NkLightType : uint8 {
            Directional=0, Point, Spot, Area, Ambient
        };

        enum class NkBlendMode : uint8 {
            Opaque=0, AlphaBlend, Additive, Multiply, PremultAlpha
        };

        // Modes de rendu 2D
        enum class NkRenderer2DType : uint8 {
            None=0, Quad, Sprite, Text, Shape, Tilemap, Custom
        };

        struct NkMeshComponent {
            nk_uint64 meshHandle=0; NkString meshPath;
            nk_uint32 subMeshIndex=0;
            bool visible=true, castShadow=true, receiveShadow=true;
            uint8 _pad=0;
        };
        NK_COMPONENT(NkMeshComponent)

        struct NkMaterialSlot { nk_uint64 materialHandle=0; NkString materialPath; };
        struct NkMaterialComponent {
            static constexpr nk_uint32 kMaxSlots=8;
            NkMaterialSlot slots[kMaxSlots]={};
            nk_uint32 slotCount=0;
            void SetMaterial(nk_uint32 slot, const NkString& path) noexcept {
                if(slot<kMaxSlots){slots[slot].materialPath=path;slots[slot].materialHandle=0;if(slot>=slotCount)slotCount=slot+1;}
            }
        };
        NK_COMPONENT(NkMaterialComponent)

        struct NkSkinnedMeshComponent {
            nk_uint64 meshHandle=0; NkString meshPath;
            nk_uint64 skeletonHandle=0; NkString skeletonPath;
            static constexpr nk_uint32 kMaxBones=128;
            NkMat4f boneMatrices[kMaxBones];
            nk_uint32 boneCount=0;
            bool visible=true, castShadow=true;
        };
        NK_COMPONENT(NkSkinnedMeshComponent)

        enum class NkCameraProjection : nk_uint8 { Perspective=0, Orthographic };
        struct NkCameraComponent {
            NkCameraProjection projection=NkCameraProjection::Perspective;
            float32 fovDeg=60.f, nearClip=0.05f, farClip=1000.f;
            float32 orthoSize=5.f, aspect=16.f/9.f;
            nk_int32 priority=0;
            nk_uint64 renderTargetHandle=0;
            mutable NkMat4f viewMatrix, projMatrix, viewProjMatrix;
        };
        NK_COMPONENT(NkCameraComponent)

        struct NkLightComponent {
            NkLightType type=NkLightType::Directional;
            NkColor4 color=NkColor4::White();
            float32 intensity=1.f, range=10.f, innerAngle=20.f, outerAngle=40.f;
            bool castShadow=true;
            nk_uint8 _pad[3]={};
        };
        NK_COMPONENT(NkLightComponent)

        struct NkSpriteComponent {
            nk_uint64 textureHandle=0; NkString texturePath;
            NkRect2D uvRect={0.f,0.f,1.f,1.f};
            NkColor4 color=NkColor4::White();
            float32 pixelsPerUnit=100.f;
            NkBlendMode blendMode=NkBlendMode::AlphaBlend;
            bool flipX=false, flipY=false;
        };
        NK_COMPONENT(NkSpriteComponent)

        struct NkParticleSystemComponent {
            nk_uint64 particleHandle=0; NkString presetPath;
            bool playing=true, loop=true;
            float32 duration=5.f, speed=1.f;
        };
        NK_COMPONENT(NkParticleSystemComponent)

        struct NkBlendshapeComponent {
            static constexpr nk_uint32 kMaxShapes=64;
            float32 weights[kMaxShapes]={};
            nk_uint32 count=0; bool dirty=false;
        };
        NK_COMPONENT(NkBlendshapeComponent)

    } // namespace ecs
} // namespace nkentseu
