// =============================================================================
// NkMaterial.cpp
// =============================================================================
#include "pch/pch.h"
#include "NKRenderer/Core/NkMaterial.h"

namespace nkentseu {

    // =========================================================================
    // NkMaterial (base)
    // =========================================================================

    void NkMaterial::SetTexture(NkMaterialTextureSlot slot, NkTextureRef tex) noexcept {
        uint32 idx = static_cast<uint32>(slot);
        if (idx < static_cast<uint32>(NkMaterialTextureSlot::NK_COUNT)) {
            mTextures[idx] = tex;
        }
    }

    NkTextureRef NkMaterial::GetTexture(NkMaterialTextureSlot slot) const noexcept {
        uint32 idx = static_cast<uint32>(slot);
        if (idx < static_cast<uint32>(NkMaterialTextureSlot::NK_COUNT))
            return mTextures[idx];
        return {};
    }

    bool NkMaterial::HasTexture(NkMaterialTextureSlot slot) const noexcept {
        uint32 idx = static_cast<uint32>(slot);
        if (idx >= static_cast<uint32>(NkMaterialTextureSlot::NK_COUNT)) return false;
        return mTextures[idx].IsValid() && mTextures[idx]->IsValid();
    }

    // =========================================================================
    // NkPBRMaterial — PBR Unreal Engine style
    // =========================================================================

    static void BindTexIfPresent(NkIDevice* device, NkDescSetHandle set,
                                  uint32 binding, NkTextureRef& tex) {
        if (tex.IsValid() && tex->IsValid()) {
            device->BindTextureSampler(set, binding, tex->GetHandle(), tex->GetSampler());
        }
    }

    bool NkPBRMaterial::Init(NkIDevice* device) noexcept {
        if (!device) return false;
        mDevice = device;

        mUBO = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkPBRUniforms)));
        if (!mUBO.IsValid()) return false;

        // Descriptor set layout :
        //   binding 0  — UBO uniforms
        //   binding 1  — albedo
        //   binding 2  — normal
        //   binding 3  — ORM
        //   binding 4  — emission
        //   binding 5  — specular
        //   binding 6  — opacity
        //   binding 7  — height
        //   binding 8  — anisotropy
        NkDescriptorSetLayoutDesc layout;
        layout.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
        for (uint32 b = 1; b <= TEX_BINDING_COUNT; ++b) {
            layout.Add(b, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
        }

        mDescSetLayout = device->CreateDescriptorSetLayout(layout);
        if (!mDescSetLayout.IsValid()) return false;

        mDescSet = device->AllocateDescriptorSet(mDescSetLayout);
        if (!mDescSet.IsValid()) return false;

        device->BindUniformBuffer(mDescSet, 0, mUBO, sizeof(NkPBRUniforms));

        mDirty = true;
        return Upload(device);
    }

    void NkPBRMaterial::SyncTextureFlags() noexcept {
        mUniforms.texFlags = 0;
        if (HasTexture(NkMaterialTextureSlot::NK_ALBEDO))    mUniforms.texFlags |= NK_PBRTEX_ALBEDO;
        if (HasTexture(NkMaterialTextureSlot::NK_NORMAL))    mUniforms.texFlags |= NK_PBRTEX_NORMAL;
        if (HasTexture(NkMaterialTextureSlot::NK_ORM))       mUniforms.texFlags |= NK_PBRTEX_ORM;
        if (HasTexture(NkMaterialTextureSlot::NK_EMISSION))  mUniforms.texFlags |= NK_PBRTEX_EMISSION;
        if (HasTexture(NkMaterialTextureSlot::NK_SPECULAR))  mUniforms.texFlags |= NK_PBRTEX_SPECULAR;
        if (HasTexture(NkMaterialTextureSlot::NK_OPACITY))   mUniforms.texFlags |= NK_PBRTEX_OPACITY;
        if (HasTexture(NkMaterialTextureSlot::NK_HEIGHT))    mUniforms.texFlags |= NK_PBRTEX_HEIGHT;
        if (HasTexture(NkMaterialTextureSlot::NK_ANISOTROPY))mUniforms.texFlags |= NK_PBRTEX_ANISOTROPY;
        mUniforms.alphaCutoff = mAlphaCutoff;
        mUniforms.blendMode   = static_cast<uint32>(mBlendMode);
    }

    bool NkPBRMaterial::Upload(NkIDevice* device) noexcept {
        if (!device || !mUBO.IsValid() || !mDescSet.IsValid()) return false;
        if (!mDirty) return true;
        mDirty = false;

        SyncTextureFlags();
        device->WriteBuffer(mUBO, &mUniforms, sizeof(NkPBRUniforms));

        BindTexIfPresent(device, mDescSet, 1, mTextures[static_cast<uint32>(NkMaterialTextureSlot::NK_ALBEDO)]);
        BindTexIfPresent(device, mDescSet, 2, mTextures[static_cast<uint32>(NkMaterialTextureSlot::NK_NORMAL)]);
        BindTexIfPresent(device, mDescSet, 3, mTextures[static_cast<uint32>(NkMaterialTextureSlot::NK_ORM)]);
        BindTexIfPresent(device, mDescSet, 4, mTextures[static_cast<uint32>(NkMaterialTextureSlot::NK_EMISSION)]);
        BindTexIfPresent(device, mDescSet, 5, mTextures[static_cast<uint32>(NkMaterialTextureSlot::NK_SPECULAR)]);
        BindTexIfPresent(device, mDescSet, 6, mTextures[static_cast<uint32>(NkMaterialTextureSlot::NK_OPACITY)]);
        BindTexIfPresent(device, mDescSet, 7, mTextures[static_cast<uint32>(NkMaterialTextureSlot::NK_HEIGHT)]);
        BindTexIfPresent(device, mDescSet, 8, mTextures[static_cast<uint32>(NkMaterialTextureSlot::NK_ANISOTROPY)]);

        return true;
    }

    void NkPBRMaterial::Destroy() noexcept {
        if (!mDevice) return;
        if (mDescSet.IsValid())       { mDevice->FreeDescriptorSet(mDescSet); }
        if (mDescSetLayout.IsValid()) { mDevice->DestroyDescriptorSetLayout(mDescSetLayout); }
        if (mUBO.IsValid())           { mDevice->DestroyBuffer(mUBO); }
        mDevice = nullptr;
    }

    // =========================================================================
    // NkFilmMaterial — Principled BSDF (UPBGE / Blender Cycles style)
    // =========================================================================

    bool NkFilmMaterial::Init(NkIDevice* device) noexcept {
        if (!device) return false;
        mDevice = device;

        mUBO = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkFilmUniforms)));
        if (!mUBO.IsValid()) return false;

        // binding 0 : UBO
        // bindings 1-13 : toutes les textures (NkMaterialTextureSlot)
        NkDescriptorSetLayoutDesc layout;
        layout.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
        for (uint32 b = 1; b <= TEX_BINDING_COUNT; ++b) {
            layout.Add(b, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
        }

        mDescSetLayout = device->CreateDescriptorSetLayout(layout);
        if (!mDescSetLayout.IsValid()) return false;

        mDescSet = device->AllocateDescriptorSet(mDescSetLayout);
        if (!mDescSet.IsValid()) return false;

        device->BindUniformBuffer(mDescSet, 0, mUBO, sizeof(NkFilmUniforms));

        mDirty = true;
        return Upload(device);
    }

    bool NkFilmMaterial::Upload(NkIDevice* device) noexcept {
        if (!device || !mUBO.IsValid() || !mDescSet.IsValid()) return false;
        if (!mDirty) return true;
        mDirty = false;

        // Sync flags base PBR
        mUniforms.base.texFlags = 0;
        if (HasTexture(NkMaterialTextureSlot::NK_ALBEDO))   mUniforms.base.texFlags |= NK_PBRTEX_ALBEDO;
        if (HasTexture(NkMaterialTextureSlot::NK_NORMAL))   mUniforms.base.texFlags |= NK_PBRTEX_NORMAL;
        if (HasTexture(NkMaterialTextureSlot::NK_ORM))      mUniforms.base.texFlags |= NK_PBRTEX_ORM;
        if (HasTexture(NkMaterialTextureSlot::NK_EMISSION)) mUniforms.base.texFlags |= NK_PBRTEX_EMISSION;
        mUniforms.base.alphaCutoff = mAlphaCutoff;
        mUniforms.base.blendMode   = static_cast<uint32>(mBlendMode);

        // Sync flags film
        mUniforms.filmTexFlags = 0;
        if (HasTexture(NkMaterialTextureSlot::NK_SUBSURFACE))      mUniforms.filmTexFlags |= NK_FILMTEX_SUBSURFACE;
        if (HasTexture(NkMaterialTextureSlot::NK_CLEARCOAT))       mUniforms.filmTexFlags |= NK_FILMTEX_CLEARCOAT;
        if (HasTexture(NkMaterialTextureSlot::NK_CLEARCOAT_NORMAL))mUniforms.filmTexFlags |= NK_FILMTEX_CC_NORMAL;
        if (HasTexture(NkMaterialTextureSlot::NK_SHEEN))           mUniforms.filmTexFlags |= NK_FILMTEX_SHEEN;
        if (HasTexture(NkMaterialTextureSlot::NK_TRANSMISSION))    mUniforms.filmTexFlags |= NK_FILMTEX_TRANSMISSION;

        device->WriteBuffer(mUBO, &mUniforms, sizeof(NkFilmUniforms));

        for (uint32 slot = 0; slot < static_cast<uint32>(NkMaterialTextureSlot::NK_COUNT); ++slot) {
            BindTexIfPresent(device, mDescSet, slot + 1,
                             mTextures[slot]);
        }

        return true;
    }

    void NkFilmMaterial::Destroy() noexcept {
        if (!mDevice) return;
        if (mDescSet.IsValid())       { mDevice->FreeDescriptorSet(mDescSet); }
        if (mDescSetLayout.IsValid()) { mDevice->DestroyDescriptorSetLayout(mDescSetLayout); }
        if (mUBO.IsValid())           { mDevice->DestroyBuffer(mUBO); }
        mDevice = nullptr;
    }

} // namespace nkentseu
