#pragma once
// =============================================================================
// NkShaderAsset.h
// Actifs shaders pour le NKRenderer.
// Gère la compilation, le cache, les variantes (permutations) et les macros.
//
// Concept central : un "shader asset" = une description + N variantes GPU.
// Les variantes sont sélectionnées automatiquement selon le matériau,
// la géométrie et les features actives.
// =============================================================================
#include "NkRenderTypes.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Defines / macros du shader (permutations)
        // Exemple : { "USE_NORMAL_MAP", "1" } active la normal map dans le shader.
        // =============================================================================
        struct NkShaderDefine {
            NkString name;
            NkString value = "1";
        };

        // =============================================================================
        // Description d'un shader asset
        // Contient toutes les sources (GLSL/HLSL/MSL/SPIRV) + les permutations.
        // =============================================================================
        struct NkShaderAssetDesc {
            NkString  name;

            // Sources multi-API
            NkString  glslVertSrc;
            NkString  glslFragSrc;
            NkString  glslGeomSrc;
            NkString  glslTCSrc;    // Tessellation Control
            NkString  glslTESrc;    // Tessellation Evaluation
            NkString  glslCompSrc;  // Compute
            NkString  hlslSrc;      // HLSL unifié (VS + PS dans le même fichier)
            NkString  hlslVSSrc;    // HLSL VS séparé
            NkString  hlslPSSrc;    // HLSL PS séparé
            NkString  mslSrc;       // MSL (Metal)

            // Chemins vers fichiers
            NkString  glslVertPath;
            NkString  glslFragPath;
            NkString  hlslPath;
            NkString  mslPath;

            // Points d'entrée
            NkString  vertEntry  = "VSMain";
            NkString  fragEntry  = "PSMain";
            NkString  geomEntry  = "GSMain";
            NkString  compEntry  = "CSMain";

            // Macros fixes (non variables selon le matériau)
            NkVector<NkShaderDefine> defines;

            NkShaderAssetDesc& AddDefine(const char* name, const char* value = "1") {
                defines.PushBack({NkString(name), NkString(value)});
                return *this;
            }
        };

        // =============================================================================
        // Variante de shader (combinaison de defines active)
        // Identifiée par un hash de ses defines
        // =============================================================================
        struct NkShaderVariantKey {
            NkVector<NkShaderDefine> defines;
            uint64 hash = 0;

            void ComputeHash();
            bool operator==(const NkShaderVariantKey& o) const { return hash == o.hash; }

            NkShaderVariantKey& Set(const char* name, const char* value = "1") {
                for (auto& d : defines) {
                    if (d.name == name) { d.value = value; return *this; }
                }
                defines.PushBack({NkString(name), NkString(value)});
                return *this;
            }

            NkShaderVariantKey& Unset(const char* name) {
                for (usize i = 0; i < defines.Size(); i++) {
                    if (defines[i].name == name) { defines.Erase(defines.Begin()+i); break; }
                }
                return *this;
            }

            bool Has(const char* name) const {
                for (const auto& d : defines)
                    if (d.name == name) return true;
                return false;
            }
        };

        // =============================================================================
        // NkShaderAsset — shader compilé prêt à l'emploi
        // =============================================================================
        class NkShaderAsset {
            public:
                NkShaderAsset() = default;
                ~NkShaderAsset() { Destroy(); }
                NkShaderAsset(const NkShaderAsset&) = delete;
                NkShaderAsset& operator=(const NkShaderAsset&) = delete;

                // Compiler depuis une description
                bool Compile(NkIDevice* device, const NkShaderAssetDesc& desc,
                            const NkShaderVariantKey& variant = {});

                // Obtenir/créer une variante
                NkShaderHandle GetVariant(NkIDevice* device,
                                        const NkShaderVariantKey& variant);

                // Variante par défaut
                NkShaderHandle GetDefault() const { return mDefaultShader; }

                void Destroy();
                bool IsValid() const { return mDefaultShader.IsValid(); }
                NkShaderAssetID GetID() const { return mID; }
                const NkString& GetName() const { return mName; }

            private:
                struct VariantEntry {
                    NkShaderHandle  shader;
                    NkShaderVariantKey key;
                };

                NkIDevice*             mDevice = nullptr;
                NkString               mName;
                NkShaderAssetDesc      mDesc;
                NkShaderHandle         mDefaultShader;
                NkVector<VariantEntry> mVariants;
                NkShaderAssetID        mID;
                static uint64          sIDCounter;

                NkShaderDesc BuildShaderDesc(const NkShaderVariantKey& variant, NkGraphicsApi api) const;
                NkString    InjectDefines(const NkString& src, const NkVector<NkShaderDefine>& defines) const;
        };

        // =============================================================================
        // Bibliothèque de shaders built-in du NKRenderer
        // =============================================================================
        namespace NkBuiltinShaders {
            // Jeux vidéo / temps réel
            const char* PBR_Metallic_Vert();   // Vertex PBR standard
            const char* PBR_Metallic_Frag();   // Fragment PBR Metallic/Roughness
            const char* PBR_SpecGloss_Frag();  // Fragment PBR Specular/Glossiness
            const char* Principled_Frag();     // Disney Principled BSDF
            const char* Unlit_Vert();
            const char* Unlit_Frag();
            const char* Toon_Frag();           // Cel shading
            const char* Subsurface_Frag();     // SSS approximé
            const char* Hair_Frag();           // Marschner
            const char* Cloth_Frag();          // Ashikhmin
            const char* Eye_Frag();            // Yeux
            const char* Water_Frag();          // Eau
            const char* Glass_Frag();          // Verre

            // Utilitaires
            const char* ShadowDepth_Vert();
            const char* ShadowDepth_Frag();
            const char* DepthOnly_Vert();
            const char* DepthOnly_Frag();
            const char* Skybox_Vert();
            const char* Skybox_Frag();
            const char* Wireframe_Geom();      // Geometry shader wireframe

            // Post-processing
            const char* Fullscreen_Vert();     // Quad plein écran
            const char* Tonemap_Frag();        // Tonemapping (ACES, Reinhard, Filmic)
            const char* Bloom_Frag();
            const char* FXAA_Frag();
            const char* SSAO_Frag();
            const char* SSR_Frag();            // Screen Space Reflections
            const char* DOF_Frag();            // Depth of Field
            const char* MotionBlur_Frag();
            const char* VignetteChromatic_Frag();
            const char* ColorGrade_Frag();     // LUT 3D color grading

            // 2D / UI
            const char* Sprite_Vert();
            const char* Sprite_Frag();
            const char* Text2D_Vert();
            const char* Text2D_Frag();
            const char* Shape2D_Vert();
            const char* Shape2D_Frag();

            // 3D Text
            const char* Text3D_Vert();
            const char* Text3D_Frag();

            // IBL
            const char* EquirectToCube_Vert();
            const char* EquirectToCube_Frag();
            const char* Irradiance_Vert();
            const char* Irradiance_Frag();
            const char* Prefilter_Vert();
            const char* Prefilter_Frag();
            const char* BRDF_LUT_Vert();
            const char* BRDF_LUT_Frag();

            // Particules / VFX
            const char* Particle_Vert();
            const char* Particle_Frag();
            const char* Trail_Vert();
            const char* Trail_Frag();
        }

        // =============================================================================
        // Sources GLSL des shaders built-in
        // =============================================================================

        // ── UBO commun à tous les shaders de scène ────────────────────────────────
        static constexpr const char* kGLSL_SceneUBO = R"GLSL(
        layout(std140, binding = 0) uniform SceneUBO {
            mat4  view;
            mat4  proj;
            mat4  viewProj;
            mat4  invView;
            mat4  invProj;
            vec4  cameraPos;       // xyz=position, w=nearPlane
            vec4  cameraDir;       // xyz=direction, w=farPlane
            vec4  screenSize;      // xy=size, zw=invSize
            vec4  time;            // x=time, y=deltaTime, z=frameCount
            vec4  ambientColor;    // xyz=couleur, w=intensité
            // Lumières (max 8 directionnelles pour ombre, puis les autres)
            vec4  lightCount;      // x=total, y=shadow casters
        } scene;
        )GLSL";

        // ── UBO par objet ─────────────────────────────────────────────────────────
        static constexpr const char* kGLSL_ObjectUBO = R"GLSL(
        layout(std140, binding = 1) uniform ObjectUBO {
            mat4  model;
            mat4  modelViewProj;
            mat4  normalMatrix;   // transpose(inverse(mat3(model)))
            vec4  objectID;       // x=id pour picking, y=lod, zw=custom
        } object;
        )GLSL";

        // ── UBO par matériau ──────────────────────────────────────────────────────
        static constexpr const char* kGLSL_MaterialUBO = R"GLSL(
        layout(std140, binding = 2) uniform MaterialUBO {
            // PBR params
            vec4  albedo;          // rgb=couleur, a=alpha
            vec4  pbrParams;       // x=metallic, y=roughness, z=ao, w=emissiveScale
            vec4  emissiveColor;   // rgb=emissive, a=unused
            vec4  uvTilingOffset;  // xy=tiling, zw=offset

            // Flags
            vec4  flags;           // x=hasAlbedoTex, y=hasNormalMap, z=hasORMTex, w=hasEmissiveTex
            vec4  flags2;          // x=hasHeightTex, y=heightScale, z=alphaCutoff, w=shadingModel

            // Subsurface
            vec4  subsurfaceColor; // rgb=couleur SSS, a=radius
            // Clear coat
            vec4  clearCoat;       // x=intensity, y=roughness, zw=unused
            // Specular
            vec4  specular;        // x=specular, y=specularTint, zw=anisotropy xy
            // Sheen (tissu)
            vec4  sheen;           // rgb=sheen color, a=tint
            // Transmission
            vec4  transmission;    // x=weight, y=IOR, z=thickness, w=absorptionDist
        } material;
        )GLSL";

        // ── PBR Fragment complet ──────────────────────────────────────────────────
        static constexpr const char* kGLSL_PBR_Frag = R"GLSL(
        #version 460 core

        // --- Inputs ---
        layout(location = 0) in vec3  vWorldPos;
        layout(location = 1) in vec3  vNormal;
        layout(location = 2) in vec3  vTangent;
        layout(location = 3) in vec3  vBitangent;
        layout(location = 4) in vec2  vUV0;
        layout(location = 5) in vec2  vUV1;
        layout(location = 6) in vec4  vColor;
        layout(location = 7) in vec4  vShadowCoord;

        layout(location = 0) out vec4 fragColor;

        // --- UBOs ---
        )" NKSTR(kGLSL_SceneUBO) R"GLSL(
        )" NKSTR(kGLSL_ObjectUBO) R"GLSL(
        )" NKSTR(kGLSL_MaterialUBO) R"GLSL(

        // --- Textures ---
        layout(binding = 3)  uniform sampler2D       uAlbedoMap;
        layout(binding = 4)  uniform sampler2D       uNormalMap;
        layout(binding = 5)  uniform sampler2D       uORMMap;       // Occlusion/Roughness/Metallic
        layout(binding = 6)  uniform sampler2D       uEmissiveMap;
        layout(binding = 7)  uniform sampler2D       uHeightMap;
        layout(binding = 8)  uniform sampler2DShadow uShadowMap;
        layout(binding = 9)  uniform samplerCube     uIrradianceMap; // IBL diffuse
        layout(binding = 10) uniform samplerCube     uPrefilterMap;  // IBL specular
        layout(binding = 11) uniform sampler2D       uBRDF_LUT;      // IBL BRDF LUT
        layout(binding = 12) uniform sampler3D       uColorLUT;      // Color grading LUT

        // =============================================================================
        // Constantes
        // =============================================================================
        const float PI = 3.14159265359;
        const float INV_PI = 0.31830988618;
        const float EPSILON = 1e-5;

        // =============================================================================
        // Distribution GGX (Trowbridge-Reitz)
        // =============================================================================
        float DistributionGGX(vec3 N, vec3 H, float roughness) {
            float a = roughness * roughness;
            float a2 = a * a;
            float NdotH = max(dot(N, H), 0.0);
            float NdotH2 = NdotH * NdotH;
            float denom = NdotH2 * (a2 - 1.0) + 1.0;
            return a2 / (PI * denom * denom);
        }

        // =============================================================================
        // Geometry Smith / Schlick-GGX
        // =============================================================================
        float GeometrySchlickGGX(float NdotV, float roughness) {
            float r = roughness + 1.0;
            float k = (r * r) / 8.0;
            return NdotV / (NdotV * (1.0 - k) + k);
        }

        float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
            return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness)
                * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
        }

        // =============================================================================
        // Fresnel Schlick
        // =============================================================================
        vec3 FresnelSchlick(float cosTheta, vec3 F0) {
            return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
        }

        vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
            return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
        }

        // =============================================================================
        // Normal Mapping
        // =============================================================================
        vec3 GetNormal(vec2 uv) {
        #ifdef USE_NORMAL_MAP
            vec3 tn = texture(uNormalMap, uv).rgb * 2.0 - 1.0;
            mat3 TBN = mat3(normalize(vTangent), normalize(vBitangent), normalize(vNormal));
            return normalize(TBN * tn);
        #else
            return normalize(vNormal);
        #endif
        }

        // =============================================================================
        // Parallax Occlusion Mapping
        // =============================================================================
        #ifdef USE_PARALLAX_MAP
        vec2 ParallaxMapping(vec2 uv, vec3 viewDir) {
            const float heightScale = material.flags2.y;
            const int   numLayers   = 32;
            float layerDepth = 1.0 / float(numLayers);
            float currentDepth = 0.0;
            vec2  deltaUV = viewDir.xy * heightScale / (viewDir.z * float(numLayers));
            vec2  currentUV = uv;
            float currentSample = texture(uHeightMap, currentUV).r;
            while (currentDepth < currentSample) {
                currentUV -= deltaUV;
                currentSample = texture(uHeightMap, currentUV).r;
                currentDepth += layerDepth;
            }
            vec2 prevUV = currentUV + deltaUV;
            float after  = currentSample - currentDepth;
            float before = texture(uHeightMap, prevUV).r - (currentDepth - layerDepth);
            return mix(currentUV, prevUV, after / (after - before));
        }
        #endif

        // =============================================================================
        // Shadow PCF 3x3
        // =============================================================================
        float ShadowPCF(vec4 shadowCoord, float bias) {
            vec3 proj = shadowCoord.xyz / shadowCoord.w;
            proj.xy = proj.xy * 0.5 + 0.5;
            if (any(lessThan(proj.xy, vec2(0))) || any(greaterThan(proj.xy, vec2(1))) || proj.z > 1.0)
                return 1.0;
            proj.z -= bias;
            vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
            float shadow = 0.0;
            for (int x = -1; x <= 1; x++)
                for (int y = -1; y <= 1; y++)
                    shadow += texture(uShadowMap, vec3(proj.xy + vec2(x,y)*texel, proj.z));
            return shadow / 9.0;
        }

        // =============================================================================
        // IBL (Image Based Lighting)
        // =============================================================================
        vec3 IBL_Diffuse(vec3 N, vec3 albedo, float ao) {
            vec3 irradiance = texture(uIrradianceMap, N).rgb;
            return irradiance * albedo * ao;
        }

        vec3 IBL_Specular(vec3 N, vec3 V, vec3 F0, float roughness, float ao) {
            vec3 R = reflect(-V, N);
            const float MAX_REFLECTION_LOD = 4.0;
            vec3 prefilteredColor = textureLod(uPrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
            vec2 brdf = texture(uBRDF_LUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
            vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
            return prefilteredColor * (F * brdf.x + brdf.y) * ao;
        }

        // =============================================================================
        // Main PBR
        // =============================================================================
        void main() {
            // UV avec tiling/offset
            vec2 uv = vUV0 * material.uvTilingOffset.xy + material.uvTilingOffset.zw;

        #ifdef USE_PARALLAX_MAP
            vec3 viewTangent = normalize(/* tbd */vec3(0));
            uv = ParallaxMapping(uv, viewTangent);
        #endif

            // Albedo
            vec4 albedoSample = (material.flags.x > 0.5)
                ? texture(uAlbedoMap, uv) : vec4(1.0);
            vec3 albedo = albedoSample.rgb * material.albedo.rgb * vColor.rgb;
            float alpha = albedoSample.a * material.albedo.a * vColor.a;

            // Alpha test
            float alphaCutoff = material.flags2.z;
            if (alpha < alphaCutoff) discard;

            // ORM — Occlusion, Roughness, Metallic
            vec3 orm = (material.flags.z > 0.5)
                ? texture(uORMMap, uv).rgb
                : vec3(material.pbrParams.z, material.pbrParams.y, material.pbrParams.x);
            float ao        = orm.r;
            float roughness = clamp(orm.g, 0.04, 1.0);
            float metallic  = clamp(orm.b, 0.0, 1.0);

            // Emissive
            vec3 emissive = vec3(0.0);
            if (material.flags.w > 0.5)
                emissive = texture(uEmissiveMap, uv).rgb * material.emissiveColor.rgb;
            else
                emissive = material.emissiveColor.rgb;
            emissive *= material.pbrParams.w;

            // Normal
            vec3 N = GetNormal(uv);
            vec3 V = normalize(scene.cameraPos.xyz - vWorldPos);
            float NdotV = max(dot(N, V), 0.0);

            // F0 (réflectivité à incidence zéro)
            vec3 F0 = mix(vec3(0.04), albedo, metallic);

            // --- IBL (éclairage ambiant global) ---
            vec3 ambient = IBL_Diffuse(N, albedo * (1.0 - metallic), ao)
                        + IBL_Specular(N, V, F0, roughness, ao);

            // --- Éclairage direct (lumières de scène) ---
            // Simplifié: 1 lumière directionnelle principale avec shadow
            vec3 Lo = vec3(0.0);
            // TODO: loop sur les lumières via SSBO
            {
                vec3 L = normalize(-scene.ambientColor.xyz); // direction from light
                vec3 H = normalize(V + L);
                vec3 radiance = vec3(1.0) * 3.0; // intensité

                float NDF = DistributionGGX(N, H, roughness);
                float G   = GeometrySmith(N, V, L, roughness);
                vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

                vec3  kD = (1.0 - F) * (1.0 - metallic);
                vec3  num = NDF * G * F;
                float den = 4.0 * NdotV * max(dot(N, L), 0.0) + EPSILON;
                vec3  specular = num / den;

                float NdotL = max(dot(N, L), 0.0);
                float shadow = ShadowPCF(vShadowCoord, mix(0.005, 0.0005, NdotL));
                Lo += (kD * albedo * INV_PI + specular) * radiance * NdotL * max(shadow, 0.3);
            }

            vec3 color = ambient * scene.ambientColor.w + Lo + emissive;

            // Color grading LUT
        #ifdef USE_COLOR_LUT
            vec3 lutUV = clamp(color, 0.0, 1.0);
            color = texture(uColorLUT, lutUV * (15.0/16.0) + 0.5/16.0).rgb;
        #endif

            fragColor = vec4(color, alpha);
        }
        )GLSL";

        // ── PBR Vertex ────────────────────────────────────────────────────────────
        static constexpr const char* kGLSL_PBR_Vert = R"GLSL(
        #version 460 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec3 aTangent;
        layout(location = 3) in vec3 aBitangent;
        layout(location = 4) in vec2 aUV0;
        layout(location = 5) in vec2 aUV1;
        layout(location = 6) in vec4 aColor;
        layout(location = 7) in vec4 aJoints;
        layout(location = 8) in vec4 aWeights;

        )" NKSTR(kGLSL_SceneUBO) R"GLSL(
        )" NKSTR(kGLSL_ObjectUBO) R"GLSL(

        layout(location = 0) out vec3 vWorldPos;
        layout(location = 1) out vec3 vNormal;
        layout(location = 2) out vec3 vTangent;
        layout(location = 3) out vec3 vBitangent;
        layout(location = 4) out vec2 vUV0;
        layout(location = 5) out vec2 vUV1;
        layout(location = 6) out vec4 vColor;
        layout(location = 7) out vec4 vShadowCoord;

        #ifdef USE_SKINNING
        layout(std430, binding = 13) readonly buffer SkinMatrices {
            mat4 bones[];
        };
        #endif

        void main() {
            vec4 localPos = vec4(aPos, 1.0);

        #ifdef USE_SKINNING
            mat4 skinMatrix =
                aWeights.x * bones[int(aJoints.x)] +
                aWeights.y * bones[int(aJoints.y)] +
                aWeights.z * bones[int(aJoints.z)] +
                aWeights.w * bones[int(aJoints.w)];
            localPos = skinMatrix * localPos;
            vec3 skinNormal = mat3(skinMatrix) * aNormal;
        #else
            vec3 skinNormal = aNormal;
        #endif

            vec4 worldPos = object.model * localPos;
            vWorldPos  = worldPos.xyz;
            vNormal    = normalize(mat3(object.normalMatrix) * skinNormal);
            vTangent   = normalize(mat3(object.model) * aTangent);
            vBitangent = normalize(mat3(object.model) * aBitangent);
            vUV0       = aUV0;
            vUV1       = aUV1;
            vColor     = aColor;

            // Shadow coord (calculé par la passe shadow dans un UBO séparé)
            vShadowCoord = vec4(0.0);

            gl_Position = object.modelViewProj * localPos;
        }
        )GLSL";

        // ── Unlit Vertex / Fragment ───────────────────────────────────────────────
        static constexpr const char* kGLSL_Unlit_Vert = R"GLSL(
        #version 460 core
        layout(location = 0) in vec3  aPos;
        layout(location = 4) in vec2  aUV0;
        layout(location = 6) in vec4  aColor;
        )" NKSTR(kGLSL_ObjectUBO) R"GLSL(
        layout(location = 0) out vec2 vUV;
        layout(location = 1) out vec4 vColor;
        void main() {
            vUV = aUV0;
            vColor = aColor;
            gl_Position = object.modelViewProj * vec4(aPos, 1.0);
        }
        )GLSL";

        static constexpr const char* kGLSL_Unlit_Frag = R"GLSL(
        #version 460 core
        layout(location = 0) in vec2  vUV;
        layout(location = 1) in vec4  vColor;
        layout(binding = 3) uniform sampler2D uAlbedoMap;
        )" NKSTR(kGLSL_MaterialUBO) R"GLSL(
        layout(location = 0) out vec4 fragColor;
        void main() {
            vec4 tex = (material.flags.x > 0.5) ? texture(uAlbedoMap, vUV) : vec4(1.0);
            vec4 col = tex * material.albedo * vColor;
            if (col.a < material.flags2.z) discard;
            fragColor = col;
        }
        )GLSL";

        // ── Sprite 2D ─────────────────────────────────────────────────────────────
        static constexpr const char* kGLSL_Sprite_Vert = R"GLSL(
        #version 460 core
        layout(location = 0) in vec2  aPos;
        layout(location = 1) in vec2  aUV;
        layout(location = 2) in vec4  aColor;
        layout(std140, binding = 0) uniform SpriteUBO {
            mat4 viewProj;
        } ubo;
        layout(location = 0) out vec2 vUV;
        layout(location = 1) out vec4 vColor;
        void main() {
            vUV = aUV;
            vColor = aColor;
            gl_Position = ubo.viewProj * vec4(aPos, 0.0, 1.0);
        }
        )GLSL";

        static constexpr const char* kGLSL_Sprite_Frag = R"GLSL(
        #version 460 core
        layout(location = 0) in vec2 vUV;
        layout(location = 1) in vec4 vColor;
        layout(binding = 1) uniform sampler2D uSpriteTex;
        layout(location = 0) out vec4 fragColor;
        void main() {
            vec4 tex = texture(uSpriteTex, vUV);
            fragColor = tex * vColor;
            if (fragColor.a < 0.001) discard;
        }
        )GLSL";

        // ── Skybox ────────────────────────────────────────────────────────────────
        static constexpr const char* kGLSL_Skybox_Vert = R"GLSL(
        #version 460 core
        layout(location = 0) in vec3 aPos;
        layout(std140, binding = 0) uniform SkyUBO {
            mat4 viewProj; // sans translation
        } ubo;
        layout(location = 0) out vec3 vTexCoord;
        void main() {
            vTexCoord = aPos;
            vec4 pos = ubo.viewProj * vec4(aPos, 1.0);
            gl_Position = pos.xyww; // force depth=1.0
        }
        )GLSL";

        static constexpr const char* kGLSL_Skybox_Frag = R"GLSL(
        #version 460 core
        layout(location = 0) in vec3 vTexCoord;
        layout(binding = 1) uniform samplerCube uSkybox;
        layout(location = 0) out vec4 fragColor;
        uniform float uExposure;
        void main() {
            vec3 color = texture(uSkybox, vTexCoord).rgb;
            // Tonemapping HDR simple
            color = color / (color + vec3(1.0)) * uExposure;
            fragColor = vec4(color, 1.0);
        }
        )GLSL";

        // ── Tonemap fullscreen ─────────────────────────────────────────────────────
        static constexpr const char* kGLSL_Fullscreen_Vert = R"GLSL(
        #version 460 core
        // Génère un triangle plein écran sans VBO
        void main() {
            vec2 pos = vec2((gl_VertexID & 1) * 4.0 - 1.0,
                            (gl_VertexID & 2) * 2.0 - 1.0);
            gl_Position = vec4(pos, 0.0, 1.0);
        }
        )GLSL";

        static constexpr const char* kGLSL_Tonemap_Frag = R"GLSL(
        #version 460 core
        layout(binding = 0) uniform sampler2D uHDRBuffer;
        layout(std140, binding = 0) uniform TonemapUBO {
            float exposure;
            float gamma;
            int   mode;   // 0=Reinhard, 1=ACES, 2=Uncharted2, 3=Filmic
            float pad;
        } ubo;
        layout(location = 0) out vec4 fragColor;

        vec3 ACES(vec3 x) {
            const float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14;
            return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0);
        }

        vec3 Uncharted2(vec3 x) {
            const float A=0.15,B=0.50,C=0.10,D=0.20,E=0.02,F=0.30,W=11.2;
            return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
        }

        void main() {
            vec2 uv = gl_FragCoord.xy / vec2(textureSize(uHDRBuffer,0));
            vec3 hdr = texture(uHDRBuffer, uv).rgb * ubo.exposure;
            vec3 ldr;
            if (ubo.mode == 1)      ldr = ACES(hdr);
            else if (ubo.mode == 2) ldr = Uncharted2(hdr) / Uncharted2(vec3(11.2));
            else                    ldr = hdr / (hdr + vec3(1.0)); // Reinhard
            ldr = pow(ldr, vec3(1.0 / ubo.gamma));
            fragColor = vec4(ldr, 1.0);
        }
        )GLSL";

        // ── Macros helper ─────────────────────────────────────────────────────────
        // NKSTR ne peut pas être utilisé tel quel dans les raw strings
        // Ces constantes sont directement utilisables dans MakeShaderAssetDesc

    } // namespace renderer
} // namespace nkentseu
