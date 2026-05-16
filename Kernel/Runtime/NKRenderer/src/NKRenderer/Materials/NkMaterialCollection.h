#pragma once
// =============================================================================
// NkMaterialCollection.h  — NKRenderer Phase M.2
//
// Material Parameter Collection (style Unreal MPC) : pool de parametres
// NOMMES partages entre plusieurs materiaux. UN seul UBO global, plusieurs
// shaders y referent. Un changement de param met a jour TOUS les materiaux
// qui le lisent sans toucher leur instance.
//
// Cas d'usage typiques :
//   - "GameTime" = float, anime les shaders sans uniform-par-instance
//   - "WindStrength" = float, animation foliage sur tous les materiaux feuilles
//   - "GlobalTint" = vec4, teinte globale (post-effect FX, hit feedback...)
//   - "SunColor" = vec3, partage entre PBR + foliage + atmosphere
//
// Layout UBO std140, alignement vec4 pour chaque slot pour simplifier.
// Limite : 64 params (1 KiB UBO). Suffisant pour la plupart des cas.
//
// Binding : set=0 binding=25 (slot libre apres les cookies du NkRender3D).
//
// Usage :
//   auto* mpc = renderer->GetMaterialCollection();
//   mpc->SetFloat("gameTime", ctx.totalTime);
//   mpc->SetVec4 ("globalTint", {1.f, 0.8f, 0.6f, 1.f});
//   // Bind automatique chaque frame (NkRender3D::UploadUBOs).
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/Functional/NkFunctional.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace renderer {

        class NkMaterialCollection {
            public:
                static constexpr uint32 kMaxParams = 64;
                static constexpr uint32 kBinding   = 25;  // set=0 binding=25

                NkMaterialCollection() = default;
                ~NkMaterialCollection() { Shutdown(); }

                bool Init(NkIDevice* device);
                void Shutdown();

                // Setters. Toutes les valeurs sont stockees en vec4 (alignement
                // std140 simplifie). Les float occupent .x, vec3 occupent .xyz,
                // vec4 occupent les 4 composantes.
                void SetFloat(const NkString& name, float32 v);
                void SetVec2 (const NkString& name, NkVec2f v);
                void SetVec3 (const NkString& name, NkVec3f v);
                void SetVec4 (const NkString& name, NkVec4f v);
                void SetColor(const NkString& name, NkVec4f c) { SetVec4(name, c); }
                void SetInt  (const NkString& name, int32 v);

                // Getters (renvoie zero si nom inconnu).
                NkVec4f Get(const NkString& name) const;
                int32   GetSlot(const NkString& name) const;

                // Upload : ecrit le UBO si dirty. Appele automatiquement par
                // NkRender3D au debut de chaque frame.
                void Upload();

                NkBufferHandle GetUBO() const { return mUBO; }

            private:
                // Reserve un slot pour le nom (cree si absent). Retourne -1 si plein.
                int32 ReserveSlot(const NkString& name);

                NkIDevice*                    mDevice = nullptr;
                NkBufferHandle                mUBO;
                // Hash explicite : NkHashMapDefaultHasher hash les octets bruts
                // du struct NkString (incluant le ptr heap), pas le contenu ->
                // 2 NkString avec meme texte = hash differents = Find echoue.
                // NkHash<NkString> (NkFunctional.h) fait FNV-1a sur le contenu.
                NkHashMap<NkString, int32,
                          memory::NkAllocator,
                          NkHash<NkString>>   mNameToSlot;
                NkVec4f                       mData[kMaxParams] = {};
                uint32                        mUsedSlots = 0;
                bool                          mDirty     = true;
        };

    } // namespace renderer
} // namespace nkentseu
