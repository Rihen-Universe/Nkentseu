#pragma once
// =============================================================================
// NkMaterialAsset.h  — NKRenderer Phase G
//
// Format de serialisation pour les materiaux NKRenderer en fichiers .nkasset.
//
// Architecture :
//   .nkasset = NkAssetFileHeader(40B) + NkAssetMetadata(NkNative) + Payload
//   Payload = NkMaterialAsset serialise en NkArchive (clef/valeur).
//
// Cycle de vie typique :
//   - Editeur / outil : construit NkMaterialAsset, appelle NkAssetIO::Write().
//   - Runtime : NkMaterialLibrary scanne le dossier Materials/, lit metadata
//     legere (sans payload), enregistre dans NkAssetRegistry. Au premier
//     Load(path), lit le payload, deserialize, cree un NkMaterialInstance.
//   - Hot-reload : NkFileWatcher detecte le changement, on re-deserialize,
//     on patche l'instance existante (handle preserve).
//
// Convention chemin logique : "/Materials/<categorie>/<nom>".
//   ex: "/Materials/Metals/Gold", "/Materials/Plastics/RedRubber".
//
// Type asset : NkAssetType::Material (deja declare dans NkAssetMetadata.h).
//
// Dependances :
//   - Les textures referencees sont stockees comme NkAssetId (resolution
//     differee via NkAssetRegistry au moment du bind). Cela permet le
//     lazy-loading et le ref-counting (pattern Hazel Engine).
// =============================================================================
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKSerialization/NkISerializable.h"
#include "NKSerialization/Asset/NkAssetMetadata.h"

// Windows.h pollue le namespace global avec #define GetObject GetObjectA.
// Repete la garde apres tous les includes pour s'assurer que nos appels
// archive.GetObject(...) ne deviennent pas archive.GetObjectA(...).
#ifdef GetObject
#undef GetObject
#endif

namespace nkentseu {
    namespace renderer {

        // =====================================================================
        // NkMaterialTextureRef — Reference vers une texture asset
        // =====================================================================
        // Le slot identifie le binding fonctionnel ("albedo", "normal", "orm",
        // "emissive", "matcap", "shadow_ramp", etc.) — interprete par le
        // NkMaterialSystem::BindInstance() qui mappe slot -> binding GPU.
        //
        // assetId : reference forte vers une Texture2D .nkasset.
        // fallbackPath : chemin direct (Resources/Textures/...) utilise si
        //                assetId.IsNull() ou si l'asset n'est pas dans le
        //                NkAssetRegistry au moment du Load.
        // =====================================================================
        struct NkMaterialTextureRef {
            NkString    slot;
            NkAssetId   assetId;
            NkString    fallbackPath;

            nk_bool Serialize(NkArchive& ar) const noexcept {
                ar.SetString("slot",     slot.View());
                ar.SetString("assetId",  assetId.ToString().View());
                ar.SetString("fallback", fallbackPath.View());
                return true;
            }
            nk_bool Deserialize(const NkArchive& ar) noexcept {
                NkString idStr;
                (void)ar.GetString("slot",     slot);
                (void)ar.GetString("assetId",  idStr);
                (void)ar.GetString("fallback", fallbackPath);
                assetId = NkAssetId::FromString(idStr.View());
                return true;
            }
        };

        // =====================================================================
        // NkMaterialAsset — Payload .nkasset pour un materiau
        // =====================================================================
        // Sérialise l'ensemble des informations necessaires pour reconstruire
        // un NkMaterialInstance equivalent a celui qu'on aurait cree en code
        // via NkMaterial::Create() + setters.
        // =====================================================================
        struct NkMaterialAsset : public NkISerializable {
            // ── Identite ──────────────────────────────────────────────────────
            NkMaterialType  type        = NkMaterialType::NK_PBR_METALLIC;
            NkString        name;

            // ── Pipeline state ────────────────────────────────────────────────
            NkRenderQueue   queue       = NkRenderQueue::NK_OPAQUE;
            NkBlendMode     blendMode   = NkBlendMode::NK_OPAQUE;
            NkCullMode      cullMode    = NkCullMode::NK_BACK;
            NkFillMode      fillMode    = NkFillMode::NK_SOLID;
            bool            depthWrite  = true;
            bool            depthTest   = true;
            bool            doubleSided = false;

            // ── Parametres UBO selon type ─────────────────────────────────────
            // Le type material determine lequel des deux est utilise :
            //   - PBR/Skin/Hair/Glass/Cloth/CarPaint/Foliage/Water/Terrain
            //     -> NkPBRParams (albedo, metallic, roughness, ao, ...)
            //   - Toon/ToonInk/Anime/Watercolor/Sketch
            //     -> NkToonParams (albedoColor, shadowColor, outlineWidth, ...)
            //   - Unlit/Debug : utilise pbr.albedo seulement.
            NkPBRParams     pbr;
            NkToonParams    toon;

            // ── Textures liees ────────────────────────────────────────────────
            // Resolution differee : au Load() on garde les NkAssetId ; au bind
            // pour rendu, le NkMaterialLibrary les resout en NkTexHandle via
            // NkTextureLibrary (qui peut charger depuis disque a la demande).
            NkVector<NkMaterialTextureRef> textures;

            // ── Shader custom (si type == NK_CUSTOM) ──────────────────────────
            // customShaderAssetId : si valide, le shader vient d'un .nkasset
            //                       (chargement central via NkAssetRegistry).
            // customShaderDir : chemin direct (Resources/Shaders/<dir>/) en
            //                   fallback. Convention NkShaderLibrary actuelle.
            NkAssetId       customShaderAssetId;
            NkString        customShaderDir;

            // =================================================================
            // Serialisation NkArchive (utilise par NkAssetIO::Write/Read)
            // =================================================================
            [[nodiscard]] const char* GetTypeName() const noexcept override {
                return "NkMaterialAsset";
            }

            [[nodiscard]] nk_bool Serialize(NkArchive& archive) const override {
                // Identite + type
                archive.SetUInt32("type",      static_cast<nk_uint32>(type));
                archive.SetString("name",      name.View());
                archive.SetUInt32("queue",     static_cast<nk_uint32>(queue));
                archive.SetUInt32("blendMode", static_cast<nk_uint32>(blendMode));
                archive.SetUInt32("cullMode",  static_cast<nk_uint32>(cullMode));
                archive.SetUInt32("fillMode",  static_cast<nk_uint32>(fillMode));
                archive.SetBool("depthWrite",  depthWrite);
                archive.SetBool("depthTest",   depthTest);
                archive.SetBool("doubleSided", doubleSided);

                // PBR params (utilise selon type)
                {
                    NkArchive p;
                    p.SetFloat32("albedoR",          pbr.albedo.x);
                    p.SetFloat32("albedoG",          pbr.albedo.y);
                    p.SetFloat32("albedoB",          pbr.albedo.z);
                    p.SetFloat32("albedoA",          pbr.albedo.w);
                    p.SetFloat32("emissiveR",        pbr.emissive.x);
                    p.SetFloat32("emissiveG",        pbr.emissive.y);
                    p.SetFloat32("emissiveB",        pbr.emissive.z);
                    p.SetFloat32("emissiveA",        pbr.emissive.w);
                    p.SetFloat32("metallic",         pbr.metallic);
                    p.SetFloat32("roughness",        pbr.roughness);
                    p.SetFloat32("ao",               pbr.ao);
                    p.SetFloat32("emissiveStrength", pbr.emissiveStrength);
                    p.SetFloat32("normalStrength",   pbr.normalStrength);
                    p.SetFloat32("clearcoat",        pbr.clearcoat);
                    p.SetFloat32("clearcoatRough",   pbr.clearcoatRough);
                    p.SetFloat32("subsurface",       pbr.subsurface);
                    p.SetFloat32("subsurfaceR",      pbr.subsurfaceColor.x);
                    p.SetFloat32("subsurfaceG",      pbr.subsurfaceColor.y);
                    p.SetFloat32("subsurfaceB",      pbr.subsurfaceColor.z);
                    p.SetFloat32("subsurfaceA",      pbr.subsurfaceColor.w);
                    p.SetFloat32("anisotropy",       pbr.anisotropy);
                    p.SetFloat32("sheen",            pbr.sheen);
                    archive.SetObject("pbr", p);
                }

                // Toon params (utilise selon type)
                {
                    NkArchive t;
                    t.SetFloat32("albedoR",         toon.albedoColor.x);
                    t.SetFloat32("albedoG",         toon.albedoColor.y);
                    t.SetFloat32("albedoB",         toon.albedoColor.z);
                    t.SetFloat32("albedoA",         toon.albedoColor.w);
                    t.SetFloat32("shadowR",         toon.shadowColor.x);
                    t.SetFloat32("shadowG",         toon.shadowColor.y);
                    t.SetFloat32("shadowB",         toon.shadowColor.z);
                    t.SetFloat32("shadowA",         toon.shadowColor.w);
                    t.SetFloat32("shadowThreshold", toon.shadowThreshold);
                    t.SetFloat32("shadowSmooth",    toon.shadowSmooth);
                    t.SetFloat32("outlineWidth",    toon.outlineWidth);
                    t.SetFloat32("rimIntensity",    toon.rimIntensity);
                    t.SetFloat32("outlineR",        toon.outlineColor.x);
                    t.SetFloat32("outlineG",        toon.outlineColor.y);
                    t.SetFloat32("outlineB",        toon.outlineColor.z);
                    t.SetFloat32("outlineA",        toon.outlineColor.w);
                    t.SetFloat32("rimR",            toon.rimColor.x);
                    t.SetFloat32("rimG",            toon.rimColor.y);
                    t.SetFloat32("rimB",            toon.rimColor.z);
                    t.SetFloat32("rimA",            toon.rimColor.w);
                    t.SetFloat32("specHardness",    toon.specHardness);
                    t.SetFloat32("metallic",        toon.metallic);
                    t.SetFloat32("matcapStrength",  toon.matcapStrength);
                    archive.SetObject("toon", t);
                }

                // Textures (array)
                {
                    NkArchive arr;
                    arr.SetUInt32("count", static_cast<nk_uint32>(textures.Size()));
                    for (nk_size i = 0; i < textures.Size(); ++i) {
                        NkArchive sub;
                        textures[i].Serialize(sub);
                        NkString k = NkString::Fmtf("t_%llu", (unsigned long long)i);
                        arr.SetObject(k.View(), sub);
                    }
                    archive.SetObject("textures", arr);
                }

                // Shader custom
                archive.SetString("customShaderId",  customShaderAssetId.ToString().View());
                archive.SetString("customShaderDir", customShaderDir.View());
                return true;
            }

            [[nodiscard]] nk_bool Deserialize(const NkArchive& archive) override {
                // Identite + type
                nk_uint32 rawType = 0; (void)archive.GetUInt32("type", rawType);
                type = static_cast<NkMaterialType>(rawType);
                (void)archive.GetString("name", name);
                nk_uint32 rawQ=0, rawB=0, rawC=0, rawF=0;
                (void)archive.GetUInt32("queue",     rawQ); queue     = static_cast<NkRenderQueue>(rawQ);
                (void)archive.GetUInt32("blendMode", rawB); blendMode = static_cast<NkBlendMode>(rawB);
                (void)archive.GetUInt32("cullMode",  rawC); cullMode  = static_cast<NkCullMode>(rawC);
                (void)archive.GetUInt32("fillMode",  rawF); fillMode  = static_cast<NkFillMode>(rawF);
                (void)archive.GetBool("depthWrite",  depthWrite);
                (void)archive.GetBool("depthTest",   depthTest);
                (void)archive.GetBool("doubleSided", doubleSided);

                // PBR params
                NkArchive p;
                if (archive.GetObject("pbr", p)) {
                    (void)p.GetFloat32("albedoR",          pbr.albedo.x);
                    (void)p.GetFloat32("albedoG",          pbr.albedo.y);
                    (void)p.GetFloat32("albedoB",          pbr.albedo.z);
                    (void)p.GetFloat32("albedoA",          pbr.albedo.w);
                    (void)p.GetFloat32("emissiveR",        pbr.emissive.x);
                    (void)p.GetFloat32("emissiveG",        pbr.emissive.y);
                    (void)p.GetFloat32("emissiveB",        pbr.emissive.z);
                    (void)p.GetFloat32("emissiveA",        pbr.emissive.w);
                    (void)p.GetFloat32("metallic",         pbr.metallic);
                    (void)p.GetFloat32("roughness",        pbr.roughness);
                    (void)p.GetFloat32("ao",               pbr.ao);
                    (void)p.GetFloat32("emissiveStrength", pbr.emissiveStrength);
                    (void)p.GetFloat32("normalStrength",   pbr.normalStrength);
                    (void)p.GetFloat32("clearcoat",        pbr.clearcoat);
                    (void)p.GetFloat32("clearcoatRough",   pbr.clearcoatRough);
                    (void)p.GetFloat32("subsurface",       pbr.subsurface);
                    (void)p.GetFloat32("subsurfaceR",      pbr.subsurfaceColor.x);
                    (void)p.GetFloat32("subsurfaceG",      pbr.subsurfaceColor.y);
                    (void)p.GetFloat32("subsurfaceB",      pbr.subsurfaceColor.z);
                    (void)p.GetFloat32("subsurfaceA",      pbr.subsurfaceColor.w);
                    (void)p.GetFloat32("anisotropy",       pbr.anisotropy);
                    (void)p.GetFloat32("sheen",            pbr.sheen);
                }

                // Toon params
                NkArchive t;
                if (archive.GetObject("toon", t)) {
                    (void)t.GetFloat32("albedoR",         toon.albedoColor.x);
                    (void)t.GetFloat32("albedoG",         toon.albedoColor.y);
                    (void)t.GetFloat32("albedoB",         toon.albedoColor.z);
                    (void)t.GetFloat32("albedoA",         toon.albedoColor.w);
                    (void)t.GetFloat32("shadowR",         toon.shadowColor.x);
                    (void)t.GetFloat32("shadowG",         toon.shadowColor.y);
                    (void)t.GetFloat32("shadowB",         toon.shadowColor.z);
                    (void)t.GetFloat32("shadowA",         toon.shadowColor.w);
                    (void)t.GetFloat32("shadowThreshold", toon.shadowThreshold);
                    (void)t.GetFloat32("shadowSmooth",    toon.shadowSmooth);
                    (void)t.GetFloat32("outlineWidth",    toon.outlineWidth);
                    (void)t.GetFloat32("rimIntensity",    toon.rimIntensity);
                    (void)t.GetFloat32("outlineR",        toon.outlineColor.x);
                    (void)t.GetFloat32("outlineG",        toon.outlineColor.y);
                    (void)t.GetFloat32("outlineB",        toon.outlineColor.z);
                    (void)t.GetFloat32("outlineA",        toon.outlineColor.w);
                    (void)t.GetFloat32("rimR",            toon.rimColor.x);
                    (void)t.GetFloat32("rimG",            toon.rimColor.y);
                    (void)t.GetFloat32("rimB",            toon.rimColor.z);
                    (void)t.GetFloat32("rimA",            toon.rimColor.w);
                    (void)t.GetFloat32("specHardness",    toon.specHardness);
                    (void)t.GetFloat32("metallic",        toon.metallic);
                    (void)t.GetFloat32("matcapStrength",  toon.matcapStrength);
                }

                // Textures
                textures.Clear();
                NkArchive arr;
                if (archive.GetObject("textures", arr)) {
                    nk_uint32 count = 0; (void)arr.GetUInt32("count", count);
                    for (nk_uint32 i = 0; i < count; ++i) {
                        NkString k = NkString::Fmtf("t_%u", i);
                        NkArchive sub;
                        if (!arr.GetObject(k.View(), sub)) continue;
                        NkMaterialTextureRef ref;
                        ref.Deserialize(sub);
                        textures.PushBack(std::move(ref));
                    }
                }

                // Shader custom
                NkString idStr;
                (void)archive.GetString("customShaderId",  idStr);
                (void)archive.GetString("customShaderDir", customShaderDir);
                customShaderAssetId = NkAssetId::FromString(idStr.View());
                return true;
            }
        };

    } // namespace renderer
} // namespace nkentseu
