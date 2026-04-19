// =============================================================================
// Example_ShaderBackends.cpp
// Exemples d'utilisation multi-backend : OpenGL, Vulkan, DX11, DX12, Metal
// + Sous-meshes complets
// =============================================================================
#include "../NkRendererModule.h"
#include "../Shader/NkShaderCompiler.h"
#include "../Mesh/NkMeshSystem.h"
using namespace nkentseu;

// =============================================================================
// EXEMPLE 1 : Utilisation multi-backend transparente (auto-detect)
// =============================================================================
void Example_MultiBackendAutomatic(NkIDevice* dev, uint32 w, uint32 h) {
    NkRendererConfig cfg = NkRendererConfig::ForGame(dev->GetApi(), w, h);
    NkRenderer* rend     = NkRenderer::Create(dev, cfg);
    NkShaderCompiler compiler(dev);
    compiler.Initialize("Build/ShaderCache");

    // ── NkShaderBackendSources : toutes les versions ──────────────────────────
    NkShaderBackendSources lavasrc;
    lavasrc.name = "LavaMaterial";

    // OpenGL GLSL (binding sans set, sampler2D global)
    lavasrc.glslVertSrc = R"GLSL(
#version 460 core
layout(location=0) in vec3 aPos;
layout(binding=0, std140) uniform FrameUBO { mat4 nk_VP; vec4 nk_Time; };
layout(binding=1, std140) uniform ObjectUBO { mat4 model; };
out vec2 vUV;
void main() { vUV=aPos.xy*0.5+0.5; gl_Position=nk_VP*model*vec4(aPos,1.0); }
)GLSL";
    lavasrc.glslFragSrc = R"GLSL(
#version 460 core
in vec2 vUV;
layout(location=0) out vec4 fragColor;
layout(binding=0, std140) uniform FrameUBO { mat4 nk_VP; vec4 nk_Time; };
// OpenGL: uniform sampler sans set
layout(binding=2) uniform sampler2D tNoise;
float fbm(vec2 p){float v=0.;float a=0.5;for(int i=0;i<4;i++){v+=a*fract(sin(dot(p,vec2(127.1,311.7)))*43758.5);p*=2.1;a*=0.5;}return v;}
void main() {
    float n=fbm(vUV*4.0+nk_Time.x*0.2);
    vec3 hot=vec3(1.,.3,.0), cold=vec3(.05,.01,.0);
    fragColor=vec4(mix(cold,hot,n)+hot*n*3.,1.);
}
)GLSL";

    // Vulkan GLSL (set + binding, push_constant)
    lavasrc.vkVertSrc = R"GLSL(
#version 460
layout(location=0) in vec3 aPos;
layout(set=0,binding=0,std140) uniform FrameUBO { mat4 nk_VP; vec4 nk_Time; };
layout(push_constant) uniform PC { mat4 model; vec4 color; uint matId; uint p[3]; };
layout(location=0) out vec2 vUV;
void main() { vUV=aPos.xy*0.5+0.5; gl_Position=nk_VP*model*vec4(aPos,1.0); }
)GLSL";
    lavasrc.vkFragSrc = R"GLSL(
#version 460
layout(location=0) in vec2 vUV;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0,std140) uniform FrameUBO { mat4 nk_VP; vec4 nk_Time; };
layout(set=1,binding=0) uniform sampler2D tNoise;
float fbm(vec2 p){float v=0.;float a=0.5;for(int i=0;i<4;i++){v+=a*fract(sin(dot(p,vec2(127.1,311.7)))*43758.5);p*=2.1;a*=0.5;}return v;}
void main() {
    float n=fbm(vUV*4.0+nk_Time.x*0.2);
    fragColor=vec4(mix(vec3(.05,.01,.0),vec3(1.,.3,.0),n)+vec3(1.,.3,.0)*n*3.,1.);
}
)GLSL";

    // HLSL DX11
    lavasrc.dx11VertSrc = R"HLSL(
cbuffer FrameUBO : register(b1) { float4x4 nk_VP; float4 nk_Time; };
cbuffer ObjectUBO : register(b0) { float4x4 model; float4 color; uint matId; uint3 p; };
struct VSIn { float3 pos:POSITION; };
struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; };
VSOut VSMain(VSIn v) { VSOut o; o.uv=v.pos.xy*0.5+0.5; o.pos=mul(float4(v.pos,1.0),mul(model,nk_VP)); return o; }
)HLSL";
    lavasrc.dx11FragSrc = R"HLSL(
cbuffer FrameUBO : register(b1) { float4x4 nk_VP; float4 nk_Time; };
Texture2D tNoise : register(t0); SamplerState s : register(s0);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; };
float fbm(float2 p){float v=0;float a=0.5;for(int i=0;i<4;i++){v+=a*frac(sin(dot(p,float2(127.1,311.7)))*43758.5);p*=2.1;a*=0.5;}return v;}
float4 PSMain(PSIn i) : SV_Target0 {
    float n=fbm(i.uv*4.0+nk_Time.x*0.2);
    return float4(lerp(float3(0.05,0.01,0.0),float3(1.0,0.3,0.0),n)+float3(1.0,0.3,0.0)*n*3.0,1.0);
}
)HLSL";

    // MSL (Metal)
    lavasrc.mslFile    = "Assets/Shaders/lava.metal";
    lavasrc.mslVertEntry = "lavaVert";
    lavasrc.mslFragEntry = "lavaFrag";

    // ── Compilation auto-detect ────────────────────────────────────────────────
    auto result = compiler.Compile(lavasrc);
    if (!result.IsValid()) {
        logger.Errorf("[MultiBackend] Shader compile failed: %s\n", result.errors.CStr());
        logger.Infof("[MultiBackend] Backend: %s\n", NkShaderBackendName(compiler.GetCurrentBackend()));
    } else {
        logger.Infof("[MultiBackend] Shader OK for %s\n",
                     NkShaderBackendName(compiler.GetCurrentBackend()));
    }

    NkRenderer::Destroy(rend);
}

// =============================================================================
// EXEMPLE 2 : Sous-meshes (personnage 3D avec body + eyes + clothes)
// =============================================================================
void Example_SubMeshes(NkIDevice* dev, uint32 w, uint32 h) {
    NkRendererConfig cfg = NkRendererConfig::ForGame(dev->GetApi(), w, h);
    NkRenderer* rend     = NkRenderer::Create(dev, cfg);
    NkMeshSystem meshSys(dev, rend->GetMaterials(), rend->GetTextures());
    meshSys.Initialize();

    auto* mats = rend->GetMaterials();

    // ── Construire un mesh avec 4 sous-meshes ─────────────────────────────────
    auto layout = NkMeshSystem::GetStandardLayout();
    NkMeshBuilder mb(layout);

    // Sous-mesh 0 : Corps (opaque PBR)
    mb.BeginSubMesh("Body", NK_INVALID_MAT);
    {
        // Vertices du corps (simplifié — en réalité chargé depuis GLTF)
        for(uint32 i=0;i<32;i++){
            float a=i/32.f*6.2831f;
            mb.BeginVertex();
            mb.Position(cosf(a)*0.5f, -0.5f+i/31.f, sinf(a)*0.5f);
            mb.Normal(cosf(a),0,sinf(a));
            mb.UV(i/31.f, 0.5f);
            mb.EndVertex();
        }
        for(uint32 i=0;i<30;i++) mb.Triangle(i,i+1,i+2);
    }
    mb.EndSubMesh();

    // Sous-mesh 1 : Yeux (unlit, petits)
    mb.BeginSubMesh("Eyes", NK_INVALID_MAT);
    {
        uint32 base=mb.GetVertexCount();
        float ey[][3]={{-0.15f,0.3f,0.48f},{0.15f,0.3f,0.48f}};
        for(int ei=0;ei<2;ei++){
            for(uint32 i=0;i<8;i++){
                float a=i/8.f*6.2831f;
                mb.BeginVertex();
                mb.Position(ey[ei][0]+cosf(a)*0.06f, ey[ei][1]+sinf(a)*0.06f, ey[ei][2]);
                mb.Normal(0,0,1);
                mb.UV(cosf(a)*0.5f+0.5f, sinf(a)*0.5f+0.5f);
                mb.EndVertex();
            }
        }
        // Remplir les cercles des yeux
        for(uint32 i=0;i<6;i++) mb.Triangle(base,base+i+1,base+i+2);
        for(uint32 i=0;i<6;i++) mb.Triangle(base+8,base+8+i+1,base+8+i+2);
    }
    mb.EndSubMesh();

    // Sous-mesh 2 : Vêtements (alpha blend)
    mb.BeginSubMesh("Clothes", NK_INVALID_MAT);
    {
        uint32 base=mb.GetVertexCount();
        mb.BeginVertex(); mb.Position(-0.5f,-0.5f,0.f); mb.Normal(0,0,1); mb.UV(0,0); mb.EndVertex();
        mb.BeginVertex(); mb.Position( 0.5f,-0.5f,0.f); mb.Normal(0,0,1); mb.UV(1,0); mb.EndVertex();
        mb.BeginVertex(); mb.Position( 0.5f, 0.5f,0.f); mb.Normal(0,0,1); mb.UV(1,1); mb.EndVertex();
        mb.BeginVertex(); mb.Position(-0.5f, 0.5f,0.f); mb.Normal(0,0,1); mb.UV(0,1); mb.EndVertex();
        mb.Quad(base,base+1,base+2,base+3);
    }
    mb.EndSubMesh();

    // Sous-mesh 3 : Cheveux (double-sided, transparent)
    mb.BeginSubMesh("Hair", NK_INVALID_MAT);
    { /* ... */ }
    mb.EndSubMesh();

    mb.ComputeNormals();

    // ── Upload et assigner des matériaux différents par sous-mesh ─────────────
    NkMeshId charMesh = meshSys.Upload(mb, "Character");

    // Matériaux par sous-mesh
    auto* tex = rend->GetTextures();
    NkMatId matBody    = mats->CreatePBR(tex->Load("Assets/char_body.png"));
    NkMatId matEyes    = mats->CreateUnlit({0.05f,0.05f,0.1f,1.f});
    NkMatId matClothes = mats->CreatePBR(tex->Load("Assets/char_clothes.png"));
    auto* mc = mats->Get(matClothes);
    mc->alphaBlend = true;
    mc->pbr.roughness = 0.7f;
    mats->MarkDirty(matClothes);
    NkMatId matHair = mats->CreatePBR(tex->Load("Assets/char_hair.png"));

    // Assigner matériaux aux sous-meshes
    meshSys.SetSubMeshMaterial(charMesh, 0, matBody);
    meshSys.SetSubMeshMaterial(charMesh, 1, matEyes);
    meshSys.SetSubMeshMaterial(charMesh, 2, matClothes);
    meshSys.SetSubMeshMaterial(charMesh, 3, matHair);

    // ── Rendu avec sous-meshes ────────────────────────────────────────────────
    rend->BeginFrame();
    {
        auto* cmd = rend->GetCommandBuffer();
        meshSys.BindMesh(cmd, charMesh);

        uint32 subCount = meshSys.GetSubMeshCount(charMesh);
        for(uint32 s=0; s<subCount; ++s) {
            NkSubMesh* sub = meshSys.GetSubMesh(charMesh, s);
            if(!sub || !sub->visible) continue;

            // Binder le matériau du sous-mesh
            if(sub->material != NK_INVALID_MAT)
                mats->Bind(cmd, sub->material);

            // Dessiner ce sous-mesh
            meshSys.DrawSubMesh(cmd, charMesh, s);
        }
    }
    rend->EndFrame();

    // ── LOD par sous-mesh (ex: les cheveux ont 3 LODs) ────────────────────────
    uint32 hairLOD1[] = { 0,1,2 }; // version simplifiée 3 triangles
    meshSys.AddSubMeshLOD(charMesh, 3, hairLOD1, 3, 0.1f); // LOD si < 10% écran

    // ── Exemple : cacher dynamiquement les vêtements ──────────────────────────
    meshSys.SetSubMeshVisible(charMesh, 2, false);  // cacher les vêtements
    meshSys.SetSubMeshVisible(charMesh, 2, true);   // réafficher

    // ── Import GLTF avec sous-meshes automatiques ─────────────────────────────
    NkImportOptions opts;
    opts.importMaterials = true;
    opts.mergeSubmeshes  = false;  // NE PAS fusionner → conserver les sous-meshes
    NkMeshLoadResult carResult = meshSys.LoadGLTF("Assets/Models/car.glb", opts);
    if(carResult.success) {
        for(uint32 mi=0; mi<(uint32)carResult.meshIds.Size(); ++mi) {
            NkMeshId id = carResult.meshIds[mi];
            logger.Infof("[SubMesh] Mesh '%s': %u sous-meshes\n",
                         carResult.meshNames[mi].CStr(),
                         meshSys.GetSubMeshCount(id));
            // Accès à chaque sous-mesh (ex: rendre les vitres en dernier)
            for(uint32 s=0; s<meshSys.GetSubMeshCount(id); ++s) {
                NkSubMesh* sm = meshSys.GetSubMesh(id,s);
                logger.Infof("  SubMesh[%u] '%s' idx=%u count=%u\n",
                             s,sm->name.CStr(),sm->firstIndex,sm->indexCount);
            }
        }
    }

    NkRenderer::Destroy(rend);
}

// =============================================================================
// EXEMPLE 3 : Layout custom avec génération GLSL/HLSL/MSL automatique
// =============================================================================
void Example_CustomLayoutCodegen(NkIDevice* dev) {
    // Définir un layout très custom
    NkVertexLayoutBuilder lb;
    lb.AddPosition3F()           // location=0 : vec3 position
      .AddNormal3F()             // location=1 : vec3 normal
      .AddUV2F(0)                // location=2 : vec2 uv0
      .AddColor4UB(0)            // location=3 : uvec4/vec4 color (RGBA8)
      .AddFloat("instanceId")    // location=4 : float instanceId
      .AddFloat4("customData");  // location=5 : vec4 customData

    // Générer le code shader pour chaque backend
    logger.Infof("=== OpenGL GLSL ===\n%s\n",   lb.GenerateGLSLInputs(460).CStr());
    logger.Infof("=== Vulkan GLSL ===\n%s\n",   lb.GenerateVkGLSLInputs(460).CStr());
    logger.Infof("=== HLSL Struct ===\n%s\n",   lb.GenerateHLSLStruct(true).CStr());
    logger.Infof("=== MSL Struct ===\n%s\n",    lb.GenerateMSLStruct(true).CStr());

    // Générer un vertex shader passthrough minimal pour ce layout
    NkShaderBackend backend = ApiToBackend(dev->GetApi());
    NkString passthroughVert = lb.GeneratePassthroughVert(backend);
    logger.Infof("=== Passthrough Vertex (%s) ===\n%s\n",
                 NkShaderBackendName(backend), passthroughVert.CStr());
}

// =============================================================================
// EXEMPLE 4 : Conversion cross-backend (GLSL → HLSL → MSL)
// =============================================================================
void Example_CrossBackendConvert(NkIDevice* dev) {
    NkShaderCompiler compiler(dev);
    compiler.Initialize();

    // Source GLSL de base
    const char* glslVert = R"GLSL(
#version 460
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
layout(set=0,binding=0,std140) uniform Frame { mat4 VP; };
layout(push_constant) uniform PC { mat4 model; };
layout(location=0) out vec2 vUV;
void main() { vUV=aUV; gl_Position=VP*model*vec4(aPos,1.0); }
)GLSL";

    const char* glslFrag = R"GLSL(
#version 460
layout(location=0) in vec2 vUV;
layout(location=0) out vec4 fragColor;
layout(set=1,binding=0) uniform sampler2D tTex;
void main() { fragColor=texture(tTex,vUV); }
)GLSL";

    // Convertir vers HLSL (via SPIRV-Cross)
    NkString hlslVert = compiler.GLSLToHLSL(glslVert, NkShaderStage::NK_VERTEX,  50);
    NkString hlslFrag = compiler.GLSLToHLSL(glslFrag, NkShaderStage::NK_FRAGMENT, 50);
    logger.Infof("=== HLSL Vertex ===\n%s\n",   hlslVert.CStr());
    logger.Infof("=== HLSL Pixel  ===\n%s\n",   hlslFrag.CStr());

    // Convertir vers MSL
    NkString mslVert = compiler.GLSLToMSL(glslVert, NkShaderStage::NK_VERTEX);
    NkString mslFrag = compiler.GLSLToMSL(glslFrag, NkShaderStage::NK_FRAGMENT);
    logger.Infof("=== MSL Vertex ===\n%s\n",    mslVert.CStr());
    logger.Infof("=== MSL Fragment ===\n%s\n",  mslFrag.CStr());

    // Compiler pour le backend courant (auto-detect)
    NkShaderBackendSources src;
    src.name       = "SimpleTexture";
    src.vkVertSrc  = glslVert;
    src.vkFragSrc  = glslFrag;
    src.glslVertSrc= compiler.VkGLSLToOpenGLGLSL(glslVert, NkShaderStage::NK_VERTEX);
    src.glslFragSrc= compiler.VkGLSLToOpenGLGLSL(glslFrag, NkShaderStage::NK_FRAGMENT);

    auto result = compiler.Compile(src);
    logger.Infof("[CrossBackend] Compile %s: %s\n",
                 NkShaderBackendName(compiler.GetCurrentBackend()),
                 result.success ? "OK" : result.errors.CStr());
}
