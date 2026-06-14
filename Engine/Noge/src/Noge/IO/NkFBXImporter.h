
#pragma once
// =============================================================================
// Nkentseu/IO/NkFBXImporter.h — Import FBX (Autodesk) legacy format
// =============================================================================
#include "NKECS/NkECSDefines.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "Nkentseu/Modeling/NkEditableMesh.h"
#include "Nkentseu/ECS/Components/Animation/NkAnimation.h"

namespace nkentseu {

    struct NkFBXImportOptions {
        bool importMeshes    = true;
        bool importSkeleton  = true;
        bool importAnimation = true;
        bool importMaterials = true;
        bool importLights    = false;
        bool importCameras   = false;
        float32 scaleFactor  = 0.01f;   // FBX en cm → metres
        bool    flipY        = false;
        bool    flipWinding  = false;
        bool    bakeGeomTransforms = true;
    };

    struct NkFBXScene {
        bool valid = false;
        NkVector<NkEditableMesh>       meshes;
        NkVector<ecs::NkSkeleton>      skeletons;
        NkVector<ecs::NkAnimationClip> animations;

        struct MaterialData {
            NkString name, diffuseTex, normalTex, roughnessTex;
            NkVec4f diffuseColor = {0.8f,0.8f,0.8f,1.f};
            float32 roughness = 0.5f, metallic = 0.f;
        };
        NkVector<MaterialData> materials;
        NkString lastError;
    };

    class NkFBXImporter {
    public:
        [[nodiscard]] NkFBXScene Import(const char* path,
                                         const NkFBXImportOptions& opts = {}) noexcept;
        [[nodiscard]] const NkString& GetLastError() const noexcept { return mLastError; }
    private:
        NkString mLastError;
    };
}
