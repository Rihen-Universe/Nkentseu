#pragma once
// =============================================================================
// NkTextureAsset.h  — NKRenderer Phase H
//
// Asset-based texture loading. Le .nkasset contient des METADATA legeres
// (chemin source, format cible, mips, srgb, sampler config) et REFERENCE un
// fichier image standard (PNG/JPEG/HDR/EXR) sur disque via sourceFilePath.
//
// Avantages vs payload embedded :
//   - Asset .nkasset reste petit (~200 bytes au lieu de 1+ MB).
//   - Le PNG source reste editable directement (pas besoin de re-importer).
//   - Hot-reload trivial : NkFileWatcher sur le PNG -> reload texture.
//   - L'utilisateur peut remplacer le PNG sans toucher l'asset.
//
// Format binaire .nkasset Texture2D :
//   Header(40B) + NkAssetMetadata + Payload(NkTextureAsset serialise)
//
// Type asset : NkAssetType::Texture2D (deja declare).
// =============================================================================
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKSerialization/NkISerializable.h"
#include "NKSerialization/Asset/NkAssetMetadata.h"
#include "NKRHI/Core/NkTypes.h"  // NkGPUFormat

#ifdef GetObject
#undef GetObject
#endif

namespace nkentseu {
    namespace renderer {

        // =====================================================================
        // NkTextureAsset — Payload .nkasset pour une texture 2D
        // =====================================================================
        struct NkTextureAsset : public NkISerializable {
            // Source : chemin disque relatif au CWD (typique Resources/...)
            // Le loader read ce fichier via NkImage::Load() qui auto-detecte.
            NkString    sourceFilePath;

            // Format GPU cible. NK_RGBA8_UNORM pour albedo/normal/orm classiques,
            // NK_RGBA16_FLOAT pour HDR/specular/emission, NK_BC* compressé pour
            // optimisation (compression a faire lors de l'import si supporte).
            NkGPUFormat targetFormat = NkGPUFormat::NK_RGBA8_UNORM;

            // Generation auto des mipmaps (recommande pour samplers minLOD).
            bool        generateMips = true;

            // Espace colorimetrique sRGB : true pour albedo, false pour normal/data.
            bool        sRGB         = true;

            // Sampler par defaut : LinearRepeat / Aniso16 / Clamp.
            // 0=Linear, 1=Nearest, 2=Anisotropic16, 3=ClampLinear
            int32       samplerPreset = 0;

            // Mode alpha : 0=Opaque, 1=AlphaBlend, 2=AlphaTest (cutoff)
            int32       alphaMode    = 0;
            float32     alphaCutoff  = 0.5f; // si alphaMode == 2

            [[nodiscard]] const char* GetTypeName() const noexcept override {
                return "NkTextureAsset";
            }

            [[nodiscard]] nk_bool Serialize(NkArchive& ar) const override {
                ar.SetString("sourceFilePath", sourceFilePath.View());
                ar.SetUInt32("targetFormat",   static_cast<nk_uint32>(targetFormat));
                ar.SetBool  ("generateMips",   generateMips);
                ar.SetBool  ("sRGB",           sRGB);
                ar.SetInt32 ("samplerPreset",  samplerPreset);
                ar.SetInt32 ("alphaMode",      alphaMode);
                ar.SetFloat32("alphaCutoff",   alphaCutoff);
                return true;
            }

            [[nodiscard]] nk_bool Deserialize(const NkArchive& ar) override {
                (void)ar.GetString("sourceFilePath", sourceFilePath);
                nk_uint32 fmt = 0;
                (void)ar.GetUInt32("targetFormat",   fmt); targetFormat = static_cast<NkGPUFormat>(fmt);
                (void)ar.GetBool  ("generateMips",   generateMips);
                (void)ar.GetBool  ("sRGB",           sRGB);
                (void)ar.GetInt32 ("samplerPreset",  samplerPreset);
                (void)ar.GetInt32 ("alphaMode",      alphaMode);
                (void)ar.GetFloat32("alphaCutoff",   alphaCutoff);
                return true;
            }
        };

        // =====================================================================
        // NkTextureAssetIO — Helpers Save / Load .nkasset Texture2D
        // =====================================================================
        // Approche minimaliste sans creer un sous-systeme dedie : free functions
        // qui orchestrent NkAssetIO + NkTextureLibrary. Une integration plus
        // poussee (cache ref-counted comme NkMaterialLibrary, hot-reload native
        // events) viendra en Phase H v1.
        class NkTextureAssetIO {
            public:
                // Save un NkTextureAsset comme .nkasset. AssetId derive du
                // chemin logique pour stabilite cross-session.
                static bool Save(const NkTextureAsset& asset,
                                 const NkString& outDiskPath,
                                 const NkString& logicalPath,   // ex "/Textures/Wood"
                                 NkAssetId* outId = nullptr) noexcept;

                // Load un .nkasset Texture2D : lit metadata + payload, parse
                // NkTextureAsset, charge l'image via NkTextureLibrary::Load
                // (chemin source). Retourne le NkTexHandle. Echec -> {}.
                static NkTexHandle Load(const NkString& diskPath,
                                        NkTextureLibrary* texLib) noexcept;

                // Variante : load par AssetId via NkAssetRegistry.
                static NkTexHandle LoadById(const NkAssetId& id,
                                            NkTextureLibrary* texLib) noexcept;
        };

    } // namespace renderer
} // namespace nkentseu
