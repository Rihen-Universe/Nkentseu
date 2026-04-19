#pragma once
// =============================================================================
// Nkentseu/Anim2D/NkAtlas2D.h
// =============================================================================
// Atlas de sprites avec frames nommées et animations (style TexturePacker).
// =============================================================================
#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKRenderer/src/Core/NkRendererTypes.h"

namespace nkentseu {
    using namespace math;
    using namespace renderer;

    struct NkSpriteFrame {
        static constexpr uint32 kMaxName = 64u;
        char    name[kMaxName] = {};        // nom de la frame (ex: "run_001")
        NkVec4f uvRect = {0,0,1,1};         // {x, y, w, h} normalisés [0..1]
        NkVec2f pivot  = {0.5f, 0.5f};      // pivot normalisé [0..1]
        NkVec2f sizePx = {64, 64};          // taille originale en pixels
        NkVec2f trimOffset = {};            // offset de trim si sprite rogné
        bool    trimmed = false;
    };

    struct NkAnimation2D {
        static constexpr uint32 kMaxName = 64u;
        char    name[kMaxName]    = {};     // nom de l'animation (ex: "run")
        NkVector<uint32> frameIds;          // indices des frames dans l'atlas
        float32 fps       = 24.f;
        bool    loop      = true;
    };

    class NkAtlas2D {
    public:
        NkAtlas2D() noexcept = default;

        bool LoadFromJSON(const char* path) noexcept;  // Format TexturePacker
        bool LoadFromXML (const char* path) noexcept;  // Format LibGDX

        [[nodiscard]] const NkSpriteFrame* GetFrame(const char* name) const noexcept;
        [[nodiscard]] const NkSpriteFrame* GetFrame(uint32 idx) const noexcept;
        [[nodiscard]] const NkAnimation2D* GetAnimation(const char* name) const noexcept;

        [[nodiscard]] uint32 FrameCount() const noexcept { return static_cast<uint32>(mFrames.Size()); }
        [[nodiscard]] uint32 AnimCount()  const noexcept { return static_cast<uint32>(mAnims.Size()); }

        NkTextureHandle textureHandle;          // Texture GPU de l'atlas
        NkString        texturePath;

    private:
        NkVector<NkSpriteFrame>            mFrames;
        NkVector<NkAnimation2D>            mAnims;
        NkUnorderedMap<NkString, uint32>   mFrameIndex;  // name → index dans mFrames
        NkUnorderedMap<NkString, uint32>   mAnimIndex;   // name → index dans mAnims
    };

    // Composant ECS pour les entités utilisant un atlas
    struct NkAtlas2DComponent {
        NkAtlas2D*  atlas           = nullptr;   // atlas partagé (non-owned)
        NkString    currentAnim;                  // animation courante
        uint32      currentFrame    = 0;          // frame courante dans l'animation
        float32     frameTimer      = 0.f;        // timer pour avancer la frame
        bool        playing         = true;
        bool        loop            = true;
        float32     speed           = 1.f;       // multiplicateur de vitesse

        [[nodiscard]] const NkSpriteFrame* GetCurrentFrame() const noexcept;
        void Play(const char* animName)          noexcept;
        void Update(float32 dt)                  noexcept;
        void Stop()                              noexcept { playing = false; }
        void Pause()                             noexcept { playing = false; }
        void Resume()                            noexcept { playing = true; }
    };
    NK_COMPONENT(NkAtlas2DComponent)

} // namespace nkentseu