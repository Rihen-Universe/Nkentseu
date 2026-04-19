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
            
            NkString  glslVkVertSrc;
            NkString  glslVkFragSrc;
            NkString  glslVkGeomSrc;
            NkString  glslVkTCSrc;    // Tessellation Control
            NkString  glslVkTESrc;    // Tessellation Evaluation
            NkString  glslVkCompSrc;  // Compute

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
                mutable NkVector<NkString> mBuildSourceCache; // Stable storage for C-string pointers passed to NkShaderDesc
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
            const char* PBRMetallicVert();   // Vertex PBR standard
            const char* PBRMetallicFrag();   // Fragment PBR Metallic/Roughness
            const char* PBRSpecGlossFrag();  // Fragment PBR Specular/Glossiness
            const char* PrincipledFrag();     // Disney Principled BSDF
            const char* UnlitVert();
            const char* UnlitFrag();
            const char* ToonFrag();           // Cel shading
            const char* SubsurfaceFrag();     // SSS approximé
            const char* HairFrag();           // Marschner
            const char* ClothFrag();          // Ashikhmin
            const char* EyeFrag();            // Yeux
            const char* WaterFrag();          // Eau
            const char* GlassFrag();          // Verre

            // Utilitaires
            const char* ShadowDepthVert();
            const char* ShadowDepthFrag();
            const char* DepthOnlyVert();
            const char* DepthOnlyFrag();
            const char* Basic3DVert();       // Shader 3D compatible layout Scene/Object minimal
            const char* Basic3DFrag();
            const char* Basic3DVertVK();     // Variante Vulkan (set/binding explicites)
            const char* Basic3DFragVK();
            const char* Basic3DVertHLSL();
            const char* Basic3DFragHLSL();
            const char* Basic3DVertMSL();
            const char* Basic3DFragMSL();
            const char* SkyboxVert();
            const char* SkyboxFrag();
            const char* WireframeGeom();      // Geometry shader wireframe

            // Post-processing
            const char* FullscreenVert();     // Quad plein écran
            const char* TonemapFrag();        // Tonemapping (ACES, Reinhard, Filmic)
            const char* BloomFrag();
            const char* FXAAFrag();
            const char* SSAOFrag();
            const char* SSRFrag();            // Screen Space Reflections
            const char* DOFFrag();            // Depth of Field
            const char* MotionBlurFrag();
            const char* VignetteChromaticFrag();
            const char* ColorGradeFrag();     // LUT 3D color grading

            // 2D / UI
            const char* SpriteVert();
            const char* SpriteFrag();
            const char* SpriteVertVK();      // Variante GLSL orientee Vulkan
            const char* SpriteFragVK();      // Variante GLSL orientee Vulkan
            const char* SpriteVertHLSL();
            const char* SpriteFragHLSL();
            const char* SpriteVertMSL();
            const char* SpriteFragMSL();
            const char* Text2DVert();
            const char* Text2DFrag();
            const char* Shape2DVert();
            const char* Shape2DFrag();
            const char* Shape2DVertVK();     // Variante GLSL orientee Vulkan
            const char* Shape2DFragVK();     // Variante GLSL orientee Vulkan
            const char* Shape2DVertHLSL();
            const char* Shape2DFragHLSL();
            const char* Shape2DVertMSL();
            const char* Shape2DFragMSL();

            // 3D Text
            const char* Text3DVert();
            const char* Text3DFrag();

            // IBL
            const char* EquirectToCubeVert();
            const char* EquirectToCubeFrag();
            const char* IrradianceVert();
            const char* IrradianceFrag();
            const char* PrefilterVert();
            const char* PrefilterFrag();
            const char* BRDFLUTVert();
            const char* BRDFLUTFrag();

            // Particules / VFX
            const char* ParticleVert();
            const char* ParticleFrag();
            const char* TrailVert();
            const char* TrailFrag();
        }

        // =============================================================================
        // Sources GLSL des shaders built-in
        // =============================================================================

        // ── UBO commun à tous les shaders de scène ────────────────────────────────
        static constexpr const char* kGLSLSceneUBO = R"GLSL(
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
        static constexpr const char* kGLSLObjectUBO = R"GLSL(
        layout(std140, binding = 1) uniform ObjectUBO {
            mat4  model;
            mat4  modelViewProj;
            mat4  normalMatrix;   // transpose(inverse(mat3(model)))
            vec4  objectID;       // x=id pour picking, y=lod, zw=custom
        } object;
        )GLSL";

        // ── UBO par matériau ──────────────────────────────────────────────────────
        static constexpr const char* kGLSLMaterialUBO = R"GLSL(
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
        static constexpr const char* kGLSLPBRFrag = R"GLSL(
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
        )" NKSTR(kGLSLSceneUBO) R"GLSL(
        )" NKSTR(kGLSLObjectUBO) R"GLSL(
        )" NKSTR(kGLSLMaterialUBO) R"GLSL(

        // --- Textures ---
        layout(binding = 3)  uniform sampler2D       uAlbedoMap;
        layout(binding = 4)  uniform sampler2D       uNormalMap;
        layout(binding = 5)  uniform sampler2D       uORMMap;       // Occlusion/Roughness/Metallic
        layout(binding = 6)  uniform sampler2D       uEmissiveMap;
        layout(binding = 7)  uniform sampler2D       uHeightMap;
        layout(binding = 8)  uniform sampler2DShadow uShadowMap;
        layout(binding = 9)  uniform samplerCube     uIrradianceMap; // IBL diffuse
        layout(binding = 10) uniform samplerCube     uPrefilterMap;  // IBL specular
        layout(binding = 11) uniform sampler2D       uBRDFLUT;      // IBL BRDF LUT
        layout(binding = 12) uniform sampler3D       uColorLUT;      // Color grading LUT

        // =============================================================================
        // Constantes
        // =============================================================================
        const float PI = 3.14159265359;
        const float INVPI = 0.31830988618;
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
        #ifdef USENORMALMAP
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
        #ifdef USEPARALLAXMAP
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
        vec3 IBLDiffuse(vec3 N, vec3 albedo, float ao) {
            vec3 irradiance = texture(uIrradianceMap, N).rgb;
            return irradiance * albedo * ao;
        }

        vec3 IBLSpecular(vec3 N, vec3 V, vec3 F0, float roughness, float ao) {
            vec3 R = reflect(-V, N);
            const float MAXREFLECTIONLOD = 4.0;
            vec3 prefilteredColor = textureLod(uPrefilterMap, R, roughness * MAXREFLECTIONLOD).rgb;
            vec2 brdf = texture(uBRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
            vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
            return prefilteredColor * (F * brdf.x + brdf.y) * ao;
        }

        // =============================================================================
        // Main PBR
        // =============================================================================
        void main() {
            // UV avec tiling/offset
            vec2 uv = vUV0 * material.uvTilingOffset.xy + material.uvTilingOffset.zw;

        #ifdef USEPARALLAXMAP
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
            vec3 ambient = IBLDiffuse(N, albedo * (1.0 - metallic), ao)
                        + IBLSpecular(N, V, F0, roughness, ao);

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
                Lo += (kD * albedo * INVPI + specular) * radiance * NdotL * max(shadow, 0.3);
            }

            vec3 color = ambient * scene.ambientColor.w + Lo + emissive;

            // Color grading LUT
        #ifdef USECOLORLUT
            vec3 lutUV = clamp(color, 0.0, 1.0);
            color = texture(uColorLUT, lutUV * (15.0/16.0) + 0.5/16.0).rgb;
        #endif

            fragColor = vec4(color, alpha);
        }
        )GLSL";

        // ── PBR Vertex ────────────────────────────────────────────────────────────
        static constexpr const char* kGLSLPBRVert = R"GLSL(
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

        )" NKSTR(kGLSLSceneUBO) R"GLSL(
        )" NKSTR(kGLSLObjectUBO) R"GLSL(

        layout(location = 0) out vec3 vWorldPos;
        layout(location = 1) out vec3 vNormal;
        layout(location = 2) out vec3 vTangent;
        layout(location = 3) out vec3 vBitangent;
        layout(location = 4) out vec2 vUV0;
        layout(location = 5) out vec2 vUV1;
        layout(location = 6) out vec4 vColor;
        layout(location = 7) out vec4 vShadowCoord;

        #ifdef USESKINNING
        layout(std430, binding = 13) readonly buffer SkinMatrices {
            mat4 bones[];
        };
        #endif

        void main() {
            vec4 localPos = vec4(aPos, 1.0);

        #ifdef USESKINNING
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
        static constexpr const char* kGLSLUnlitVert = R"GLSL(
        #version 460 core
        layout(location = 0) in vec3  aPos;
        layout(location = 4) in vec2  aUV0;
        layout(location = 6) in vec4  aColor;
        )" NKSTR(kGLSLObjectUBO) R"GLSL(
        layout(location = 0) out vec2 vUV;
        layout(location = 1) out vec4 vColor;
        void main() {
            vUV = aUV0;
            vColor = aColor;
            gl_Position = object.modelViewProj * vec4(aPos, 1.0);
        }
        )GLSL";

        static constexpr const char* kGLSLUnlitFrag = R"GLSL(
        #version 460 core
        layout(location = 0) in vec2  vUV;
        layout(location = 1) in vec4  vColor;
        layout(binding = 3) uniform sampler2D uAlbedoMap;
        )" NKSTR(kGLSLMaterialUBO) R"GLSL(
        layout(location = 0) out vec4 fragColor;
        void main() {
            vec4 tex = (material.flags.x > 0.5) ? texture(uAlbedoMap, vUV) : vec4(1.0);
            vec4 col = tex * material.albedo * vColor;
            if (col.a < material.flags2.z) discard;
            fragColor = col;
        }
        )GLSL";

        // —— Basic 3D (layout scene/object minimal) ————————————————————————————————
        static constexpr const char* kGLSLBasic3DVert = R"GLSL(
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

        layout(std140, binding = 0) uniform SceneUBO {
            mat4 view;
            mat4 proj;
            mat4 viewProj;
            vec4 cameraPos;
            vec4 ambient;
        } scene;

        layout(std140, binding = 1) uniform ObjectUBO {
            mat4 model;
        } object;

        layout(location = 0) out vec3 vNormal;
        layout(location = 1) out vec4 vColor;

        void main() {
            vec4 world = object.model * vec4(aPos, 1.0);
            gl_Position = scene.viewProj * world;
            vNormal = normalize((object.model * vec4(aNormal, 0.0)).xyz);
            vColor = aColor;
        }
        )GLSL";

        static constexpr const char* kGLSLBasic3DFrag = R"GLSL(
        #version 460 core
        layout(std140, binding = 0) uniform SceneUBO {
            mat4 view;
            mat4 proj;
            mat4 viewProj;
            vec4 cameraPos;
            vec4 ambient;
        } scene;

        layout(location = 0) in vec3 vNormal;
        layout(location = 1) in vec4 vColor;
        layout(location = 0) out vec4 fragColor;

        void main() {
            vec3 n = normalize(vNormal);
            float ndl = max(dot(n, normalize(vec3(0.35, 0.8, 0.2))), 0.15);
            vec3 amb = scene.ambient.rgb * scene.ambient.a;
            vec3 lit = amb + vec3(ndl);
            fragColor = vec4(vColor.rgb * lit, vColor.a);
        }
        )GLSL";

        static constexpr const char* kGLSLVulkanBasic3DVert = R"GLSL(
        #version 450
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec3 aTangent;
        layout(location = 3) in vec3 aBitangent;
        layout(location = 4) in vec2 aUV0;
        layout(location = 5) in vec2 aUV1;
        layout(location = 6) in vec4 aColor;
        layout(location = 7) in vec4 aJoints;
        layout(location = 8) in vec4 aWeights;

        layout(set = 0, binding = 0, std140) uniform SceneUBO {
            mat4 view;
            mat4 proj;
            mat4 viewProj;
            vec4 cameraPos;
            vec4 ambient;
        } scene;

        layout(set = 0, binding = 1, std140) uniform ObjectUBO {
            mat4 model;
        } object;

        layout(location = 0) out vec3 vNormal;
        layout(location = 1) out vec4 vColor;

        void main() {
            vec4 world = object.model * vec4(aPos, 1.0);
            gl_Position = scene.viewProj * world;
            vNormal = normalize((object.model * vec4(aNormal, 0.0)).xyz);
            vColor = aColor;
        }
        )GLSL";

        static constexpr const char* kGLSLVulkanBasic3DFrag = R"GLSL(
        #version 450
        layout(set = 0, binding = 0, std140) uniform SceneUBO {
            mat4 view;
            mat4 proj;
            mat4 viewProj;
            vec4 cameraPos;
            vec4 ambient;
        } scene;

        layout(location = 0) in vec3 vNormal;
        layout(location = 1) in vec4 vColor;
        layout(location = 0) out vec4 fragColor;

        void main() {
            vec3 n = normalize(vNormal);
            float ndl = max(dot(n, normalize(vec3(0.35, 0.8, 0.2))), 0.15);
            vec3 amb = scene.ambient.rgb * scene.ambient.a;
            vec3 lit = amb + vec3(ndl);
            fragColor = vec4(vColor.rgb * lit, vColor.a);
        }
        )GLSL";

        static constexpr const char* kHLSLBasic3DVert = R"HLSL(
cbuffer SceneUBO : register(b0) {
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float4 cameraPos;
    float4 ambient;
};
cbuffer ObjectUBO : register(b1) {
    float4x4 model;
};

struct VSIn {
    float3 pos   : POSITION;
    float3 normal: NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float2 uv0   : TEXCOORD0;
    float2 uv1   : TEXCOORD1;
    float4 color : COLOR0;
    float4 joints: BLENDINDICES;
    float4 weights: BLENDWEIGHT;
};

struct VSOut {
    float4 pos   : SV_POSITION;
    float3 normal: TEXCOORD0;
    float4 color : COLOR0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    float4 world = mul(float4(i.pos, 1.0), model);
    o.pos = mul(world, viewProj);
    o.normal = normalize(mul(float4(i.normal, 0.0), model).xyz);
    o.color = i.color;
    return o;
}
)HLSL";

        static constexpr const char* kHLSLBasic3DFrag = R"HLSL(
cbuffer SceneUBO : register(b0) {
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float4 cameraPos;
    float4 ambient;
};

struct PSIn {
    float4 pos   : SV_POSITION;
    float3 normal: TEXCOORD0;
    float4 color : COLOR0;
};

float4 PSMain(PSIn i) : SV_TARGET {
    float3 n = normalize(i.normal);
    float ndl = max(dot(n, normalize(float3(0.35, 0.8, 0.2))), 0.15);
    float3 amb = ambient.rgb * ambient.a;
    float3 lit = amb + ndl.xxx;
    return float4(i.color.rgb * lit, i.color.a);
}
)HLSL";

        static constexpr const char* kMSLBasic3DVert = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct VSIn {
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 tangent [[attribute(2)]];
    float3 bitangent [[attribute(3)]];
    float2 uv0 [[attribute(4)]];
    float2 uv1 [[attribute(5)]];
    float4 color [[attribute(6)]];
    float4 joints [[attribute(7)]];
    float4 weights [[attribute(8)]];
};

struct SceneUBO {
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float4 cameraPos;
    float4 ambient;
};

struct ObjectUBO {
    float4x4 model;
};

struct VSOut {
    float4 pos [[position]];
    float3 normal;
    float4 color;
};

vertex VSOut VSMain(VSIn in [[stage_in]],
                    constant SceneUBO& scene [[buffer(0)]],
                    constant ObjectUBO& object [[buffer(1)]]) {
    VSOut out;
    float4 world = object.model * float4(in.pos, 1.0);
    out.pos = scene.viewProj * world;
    out.normal = normalize((object.model * float4(in.normal, 0.0)).xyz);
    out.color = in.color;
    return out;
}
)MSL";

        static constexpr const char* kMSLBasic3DFrag = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct SceneUBO {
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float4 cameraPos;
    float4 ambient;
};

struct VSOut {
    float4 pos [[position]];
    float3 normal;
    float4 color;
};

fragment float4 PSMain(VSOut in [[stage_in]],
                       constant SceneUBO& scene [[buffer(0)]]) {
    float3 n = normalize(in.normal);
    float ndl = max(dot(n, normalize(float3(0.35, 0.8, 0.2))), 0.15);
    float3 amb = scene.ambient.rgb * scene.ambient.a;
    float3 lit = amb + float3(ndl);
    return float4(in.color.rgb * lit, in.color.a);
}
)MSL";

        static constexpr const char* kHLSLSpriteVert = R"HLSL(
cbuffer SpriteUBO : register(b0) {
    float4x4 viewProj;
};

struct VSIn {
    float2 pos   : POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

struct VSOut {
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.pos = mul(float4(i.pos, 0.0, 1.0), viewProj);
    o.uv = i.uv;
    o.color = i.color;
    return o;
}
)HLSL";

        static constexpr const char* kHLSLSpriteFrag = R"HLSL(
Texture2D    uSpriteTex : register(t1);
SamplerState uSpriteSamp : register(s1);

struct PSIn {
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

float4 PSMain(PSIn i) : SV_TARGET {
    float4 c = uSpriteTex.Sample(uSpriteSamp, i.uv) * i.color;
    clip(c.a - 0.001f);
    return c;
}
)HLSL";

        static constexpr const char* kMSLSpriteVert = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct VSIn {
    float2 pos   [[attribute(0)]];
    float2 uv    [[attribute(1)]];
    float4 color [[attribute(2)]];
};

struct SpriteUBO {
    float4x4 viewProj;
};

struct VSOut {
    float4 pos [[position]];
    float2 uv;
    float4 color;
};

vertex VSOut VSMain(VSIn in [[stage_in]], constant SpriteUBO& ubo [[buffer(0)]]) {
    VSOut out;
    out.pos = ubo.viewProj * float4(in.pos, 0.0, 1.0);
    out.uv = in.uv;
    out.color = in.color;
    return out;
}
)MSL";

        static constexpr const char* kMSLSpriteFrag = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct VSOut {
    float4 pos [[position]];
    float2 uv;
    float4 color;
};

fragment float4 PSMain(VSOut in [[stage_in]],
                       texture2d<float> tex [[texture(1)]],
                       sampler samp [[sampler(1)]]) {
    float4 c = tex.sample(samp, in.uv) * in.color;
    if (c.a < 0.001) {
        discard_fragment();
    }
    return c;
}
)MSL";

        // ── Sprite 2D ─────────────────────────────────────────────────────────────
        static constexpr const char* kGLSLSpriteVert = R"GLSL(
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

        static constexpr const char* kGLSLSpriteFrag = R"GLSL(
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

        // Variante GLSL Vulkan (set/binding explicites, #version 450)
        static constexpr const char* kGLSLVulkanSpriteVert = R"GLSL(
        #version 450
        layout(location = 0) in vec2  aPos;
        layout(location = 1) in vec2  aUV;
        layout(location = 2) in vec4  aColor;
        layout(set = 0, binding = 0, std140) uniform SpriteUBO {
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

        static constexpr const char* kGLSLVulkanSpriteFrag = R"GLSL(
        #version 450
        layout(location = 0) in vec2 vUV;
        layout(location = 1) in vec4 vColor;
        layout(set = 0, binding = 1) uniform sampler2D uSpriteTex;
        layout(location = 0) out vec4 fragColor;
        void main() {
            vec4 tex = texture(uSpriteTex, vUV);
            fragColor = tex * vColor;
            if (fragColor.a < 0.001) discard;
        }
        )GLSL";

        // ── Skybox ────────────────────────────────────────────────────────────────
        static constexpr const char* kGLSLSkyboxVert = R"GLSL(
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

        static constexpr const char* kGLSLSkyboxFrag = R"GLSL(
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
        static constexpr const char* kGLSLFullscreenVert = R"GLSL(
        #version 460 core
        // Génère un triangle plein écran sans VBO
        void main() {
            vec2 pos = vec2((gl_VertexID & 1) * 4.0 - 1.0,
                            (gl_VertexID & 2) * 2.0 - 1.0);
            gl_Position = vec4(pos, 0.0, 1.0);
        }
        )GLSL";

        static constexpr const char* kGLSLTonemapFrag = R"GLSL(
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
