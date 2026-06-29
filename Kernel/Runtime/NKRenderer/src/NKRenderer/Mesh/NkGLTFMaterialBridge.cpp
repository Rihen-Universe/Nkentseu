// =============================================================================
// NkGLTFMaterialBridge.cpp  — NKRenderer v5.0
//
// Voir NkGLTFMaterialBridge.h. Upload images + cree NkMaterialInstance PBR.
// =============================================================================
#include "NkGLTFMaterialBridge.h"

#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace renderer {

        namespace {

            // Upload une image glTF decodee (RGBA8 CPU) vers une NkTexHandle.
            // srgb=true pour les albedo/emissive (encodage gamma), false pour
            // normal / metallic-roughness / occlusion (donnees lineaires).
            NkTexHandle UploadImage(NkTextureLibrary* texLib,
                                    const NkGLTFImage& img, bool srgb,
                                    const char* dbg) {
                if (!texLib || !img.valid || !img.decoded.IsValid())
                    return NkTexHandle::Null();

                NkTextureCreateDesc d;
                d.pixels    = img.decoded.Pixels();
                d.width     = (uint32)img.decoded.Width();
                d.height    = (uint32)img.decoded.Height();
                d.depth     = 1;
                d.mipLevels = 1;
                d.genMips   = true;
                d.srgb      = srgb;
                d.format    = NkGPUFormat::NK_RGBA8_UNORM;
                d.debugName = dbg;
                // DIAG opt-in (NK_GLTF_TEXDIAG) : log la taille + la moyenne
                // RGBA des pixels decodes + quelques echantillons. Sert a
                // confirmer que le decodeur NKImage (PNG/JPEG) sort des couleurs
                // correctes (et pas du noir/grisatre/teinte). Aucune capture pixel
                // requise : la moyenne d'un atlas non-noir est franchement >0.
                {
                    static int diag = -1;
                    if (diag == -1) { const char* v = getenv("NK_GLTF_TEXDIAG"); diag = (v && v[0] && v[0]!='0') ? 1 : 0; }
                    if (diag) {
                        const uint8* px = (const uint8*)img.decoded.Pixels();
                        const uint32 W = (uint32)img.decoded.Width();
                        const uint32 H = (uint32)img.decoded.Height();
                        if (px && W && H) {
                            uint64 sr=0,sg=0,sb=0,sa=0; const uint64 n=(uint64)W*H;
                            for (uint64 i=0;i<n;i++){ sr+=px[i*4+0]; sg+=px[i*4+1]; sb+=px[i*4+2]; sa+=px[i*4+3]; }
                            // 4 echantillons (coins + centre)
                            auto P=[&](uint32 x,uint32 y,const char* tag){
                                uint64 o=((uint64)y*W+x)*4;
                                logger.Info("[GLTF_TEXDIAG]   px[{0}]=({1},{2},{3},{4})\n",
                                            tag, (uint32)px[o],(uint32)px[o+1],(uint32)px[o+2],(uint32)px[o+3]);
                            };
                            logger.Info("[GLTF_TEXDIAG] '{0}' {1}x{2} srgb={3} moyenne RGBA=({4},{5},{6},{7})\n",
                                        dbg?dbg:"?", W, H, srgb?1:0,
                                        (uint32)(sr/n),(uint32)(sg/n),(uint32)(sb/n),(uint32)(sa/n));
                            P(0,0,"haut-gauche"); P(W/2,H/2,"centre"); P(W-1,H-1,"bas-droite");
                        }
                    }
                }

                NkTexHandle t = texLib->Create(d);
                if (!t.IsValid() || t.id == texLib->GetError().id)
                    return NkTexHandle::Null();
                return t;
            }

        } // namespace anonyme

        bool BuildGLTFMaterials(const NkGLTFMeshData& data,
                                NkMaterialSystem* matSys,
                                NkTextureLibrary* texLib,
                                NkGLTFMaterialSet& out) {
            out.instances.Clear();
            out.textures.Clear();

            if (!matSys || !texLib) {
                logger.Warnf("[NkGLTFMaterialBridge] matSys/texLib nul\n");
                return false;
            }
            if (data.materials.Empty()) {
                logger.Info("[NkGLTFMaterialBridge] aucun materiau glTF (PBR par-drawcall)\n");
                return false;
            }

            // 1) Upload toutes les images. On determine le srgb par usage au
            //    moment de la creation du materiau (une meme image peut servir
            //    a plusieurs slots, mais en pratique glTF dedie un slot par
            //    image). On uploade ici en sRGB pour le baseColor par defaut ;
            //    les normal/orm seront re-uploadees en lineaire si besoin.
            //    Pour rester simple et eviter les doublons, on uploade chaque
            //    image une seule fois et on choisit srgb selon le 1er usage
            //    rencontre dans les materiaux.
            const uint32 imgCount = (uint32)data.images.Size();
            out.textures.Resize(imgCount);
            NkVector<int8> imgSrgb;          // -1 inconnu, 0 lineaire, 1 srgb
            imgSrgb.Resize(imgCount);
            for (uint32 i = 0; i < imgCount; ++i) imgSrgb[i] = -1;

            // Pre-determine l'espace colorimetrique par image selon les usages.
            auto MarkSrgb = [&](int32 idx, bool srgb) {
                if (idx < 0 || (uint32)idx >= imgCount) return;
                // sRGB l'emporte si au moins un usage couleur (baseColor/emissive).
                if (srgb) imgSrgb[(uint32)idx] = 1;
                else if (imgSrgb[(uint32)idx] == -1) imgSrgb[(uint32)idx] = 0;
            };
            for (uint32 m = 0; m < (uint32)data.materials.Size(); ++m) {
                const NkGLTFMaterial& gm = data.materials[m];
                MarkSrgb(gm.baseColorImage,         true);
                MarkSrgb(gm.emissiveImage,          true);
                MarkSrgb(gm.normalImage,            false);
                MarkSrgb(gm.metallicRoughnessImage, false);
                MarkSrgb(gm.occlusionImage,         false);
            }
            for (uint32 i = 0; i < imgCount; ++i) {
                bool srgb = (imgSrgb[i] == 1);
                out.textures[i] = UploadImage(texLib, data.images[i], srgb,
                                              "gltf_image");
            }

            auto TexFor = [&](int32 imgIdx) -> NkTexHandle {
                if (imgIdx < 0 || (uint32)imgIdx >= out.textures.Size())
                    return NkTexHandle::Null();
                return out.textures[(uint32)imgIdx];
            };

            // 2) Cree une NkMaterialInstance PBR par materiau glTF.
            NkMatHandle pbrTmpl = matSys->DefaultPBR();

            // Template PBR a culling desactive pour les materiaux glTF
            // doubleSided (sinon : trous / faces internes manquantes en
            // back-face culling). Enregistre au PLUS une fois par chargement,
            // a la demande (replique le template PBR : meme type + meme hint de
            // shaderDir "PBR" via vertSrcGL, mais cullMode = NK_NONE).
            NkMatHandle pbrDoubleSidedTmpl = NkMatHandle::Null();
            auto DoubleSidedTemplate = [&]() -> NkMatHandle {
                if (pbrDoubleSidedTmpl.IsValid()) return pbrDoubleSidedTmpl;
                NkMaterialTemplateDesc d;
                d.type        = NkMaterialType::NK_PBR_METALLIC;
                d.cullMode    = NkCullMode::NK_NONE;
                d.doubleSided = true;
                d.name        = NkString("PBR_DoubleSided");
                // Hint de shaderDir : meme convention que Default_PBR
                // (CompilePipeline lit vertSrcGL comme nom de dossier shader).
                d.vertSrcGL   = NkString("PBR");
                pbrDoubleSidedTmpl = matSys->RegisterTemplate(d);
                if (!pbrDoubleSidedTmpl.IsValid()) {
                    logger.Warnf("[NkGLTFMaterialBridge] RegisterTemplate "
                                 "(PBR_DoubleSided) echoue -> fallback DefaultPBR\n");
                    pbrDoubleSidedTmpl = matSys->DefaultPBR();
                }
                return pbrDoubleSidedTmpl;
            };

            uint32 created = 0;
            for (uint32 m = 0; m < (uint32)data.materials.Size(); ++m) {
                const NkGLTFMaterial& gm = data.materials[m];
                // doubleSided -> template cull NONE, sinon PBR standard (cull back).
                NkMatHandle tmpl = gm.doubleSided ? DoubleSidedTemplate() : pbrTmpl;
                NkMaterialInstance* inst = matSys->CreateInstance(tmpl);
                if (!inst) {
                    out.instances.PushBack(NkMatInstHandle::Null());
                    logger.Warnf("[NkGLTFMaterialBridge] CreateInstance echoue (mat %u)\n", m);
                    continue;
                }

                // Facteurs PBR.
                NkVec3f albedo = { gm.baseColorFactor.x,
                                   gm.baseColorFactor.y,
                                   gm.baseColorFactor.z };
                inst->SetAlbedo(albedo, gm.baseColorFactor.w)
                    ->SetMetallic(gm.metallicFactor)
                    ->SetRoughness(gm.roughnessFactor);

                // Emissive (facteur + strength KHR).
                if (gm.emissiveFactor.x > 0.f || gm.emissiveFactor.y > 0.f ||
                    gm.emissiveFactor.z > 0.f) {
                    inst->SetEmissive(gm.emissiveFactor, gm.emissiveStrength);
                }

                // Maps.
                NkTexHandle baseColor = TexFor(gm.baseColorImage);
                if (baseColor.IsValid()) inst->SetAlbedoMap(baseColor);

                NkTexHandle normal = TexFor(gm.normalImage);
                if (normal.IsValid()) inst->SetNormalMap(normal, gm.normalScale);

                // glTF metallicRoughnessTexture : G=roughness, B=metallic. Le
                // PBR du renderer attend une ORM (R=AO,G=rough,B=metal) : c'est
                // compatible pour les canaux rough/metal. Si une occlusion
                // separee existe, on prefere l'ORM dediee ; sinon la MR sert
                // d'ORM (AO=1 par defaut cote shader).
                NkTexHandle orm = TexFor(gm.metallicRoughnessImage);
                if (orm.IsValid()) inst->SetORMMap(orm);

                NkTexHandle ao = TexFor(gm.occlusionImage);
                if (ao.IsValid()) inst->SetAOMap(ao);

                NkTexHandle emissiveMap = TexFor(gm.emissiveImage);
                if (emissiveMap.IsValid()) inst->SetEmissiveMap(emissiveMap);

                out.instances.PushBack(inst->GetHandle());
                ++created;
            }

            logger.Info("[NkGLTFMaterialBridge] {0} materiaux crees, {1} images uploadees\n",
                        created, imgCount);
            return created > 0;
        }

    } // namespace renderer
} // namespace nkentseu
