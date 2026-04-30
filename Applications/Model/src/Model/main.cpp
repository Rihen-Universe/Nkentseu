// =============================================================================
// main.cpp — Application « Triangle »
// Démo pédagogique : rendu d'un triangle coloré multi-backend (RHI Nkentseu).
//
// OBJECTIF DE L'EXERCICE
// ──────────────────────
// Ce fichier est le point de départ de votre TP. Il dessine un triangle en
// NDC (Normalized Device Coordinates) avec 3 couleurs différentes par sommet.
//
// ARCHITECTURE RHI (Rendering Hardware Interface)
// ───────────────────────────────────────────────
// La RHI est une couche d'abstraction au-dessus des APIs graphiques (OpenGL,
// Vulkan, DirectX). Elle expose toujours les mêmes objets :
//
//   NkIDevice         → le GPU logique (crée toutes les ressources)
//   NkShaderHandle    → programme de shader compilé
//   NkBufferHandle    → tampon de données GPU (vertices, indices, uniforms…)
//   NkPipelineHandle  → état fixe du pipeline (shader + layout + topologie…)
//   NkICommandBuffer  → liste d'opérations à envoyer au GPU
//
// PIPELINE D'INITIALISATION
// ─────────────────────────
//   1. Fenêtre (NkWindow)
//   2. Device GPU  (NkDeviceFactory::Create)
//   3. Render pass (device->GetSwapchainRenderPass)
//   4. Shaders     (device->CreateShader)
//   5. Vertex buffer (device->CreateBuffer)
//   6. Pipeline graphique (device->CreateGraphicsPipeline)
//   7. Boucle de rendu
//
// USAGE (ligne de commande)
//   --backend=opengl  (défaut)
//   --backend=vulkan
//   --backend=dx11
//   --backend=dx12
// =============================================================================

// ── En-têtes NKEngine ─────────────────────────────────────────────────────────
#include "NKWindow/NkWindow.h"           // Fenêtre native multi-plateforme
#include "NKWindow/Core/NkMain.h"        // Macro nkmain() + NkEntryState
#include "NKEvent/NkWindowEvent.h"       // NkWindowCloseEvent, NkWindowResizeEvent

#include "NKRHI/Core/NkDeviceFactory.h"  // Fabrique de devices GPU
// #include "NKRHI/Core/NkSkSL.h"  // Fabrique de devices GPU
// #include "NKRHI/SL/NkSLIntegration.h"
#include "NKRHI/SL/NkSWShaderBridge.h"
#include "NKRHI/Commands/NkCommandPool.h"// Pool de command buffers (réutilisation)
#include "NKRHI/ShaderConvert/NkShaderConvert.h" // Chargement / compilation shaders

#include "NKMath/NKMath.h"               // NkVec2f, NkVec3f, NkMat4f…
#include "NKLogger/NkLog.h"              // logger.Info / logger.Error
#include "NKTime/NkChrono.h"             // Chronomètre pour limiter le FPS

using namespace nkentseu;
using namespace nkentseu::math;

// =============================================================================
// État global de l'application
// =============================================================================
// TODO (étudiant) : vous pouvez ajouter ici d'autres variables partagées
//                   (ex. couleur de fond, position du triangle, angle de rotation)
struct AppState {
    bool      running = true;              // Flag de la boucle principale
    NkVec2i   size    = {1280, 720};       // Dimensions initiales de la fenêtre
    NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_OPENGL; // Backend choisi au démarrage
} appState;

// Macro obligatoire — déclare les métadonnées de l'application (nom, version…)
NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "Triangle";
    d.appVersion = "1.0.0";
    return d;
})());

// =========================================================================
// ÉTAPE 6 : Définition de la géométrie (vertex buffer)
// =========================================================================
// Un vertex = un sommet du triangle avec ses attributs (position, couleur…).
// Les coordonnées sont en NDC (Normalized Device Coordinates) :
//   X : -1 (gauche) → +1 (droite)
//   Y : -1 (bas)    → +1 (haut)       (convention OpenGL/Vulkan flippé Y)
//   Z :  0 (plan proche) — non utilisé ici
//

struct Vertex {
    NkVec2f position; // 2 floats (x, y) — attribut location 0
    NkVec3f color;    // 3 floats (r, g, b) — attribut location 1
};

// =============================================================================
// ParseBackend — lit l'argument --backend=xxx de la ligne de commande
// =============================================================================
// TODO (étudiant) : ajoutez la valeur "--backend=sw" pour tester le renderer
//                   software (CPU), utile pour débogage sans carte graphique.
static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (size_t i = 1; i < args.Size(); ++i) {
        const NkString& arg = args[i];
        if (arg == "--backend=vulkan" || arg == "-bvk")   return NkGraphicsApi::NK_GFX_API_VULKAN;
        if (arg == "--backend=dx11"   || arg == "-bdx11") return NkGraphicsApi::NK_GFX_API_D3D11;
        if (arg == "--backend=dx12"   || arg == "-bdx12") return NkGraphicsApi::NK_GFX_API_D3D12;
        if (arg == "--backend=sw"     || arg == "-bsw")   return NkGraphicsApi::NK_GFX_API_SOFTWARE;
        if (arg == "--backend=opengl" || arg == "-bgl")   return NkGraphicsApi::NK_GFX_API_OPENGL;
    }
    return NkGraphicsApi::NK_GFX_API_OPENGL; // valeur par défaut
}

// =============================================================================
// LoadShadersForAPI — charge les sources de shader selon l'API cible
// =============================================================================
// Chaque API graphique utilise son propre langage de shader :
//   • OpenGL  → GLSL  (fichiers .gl.glsl)
//   • Vulkan  → SPIR-V binaire compilé depuis .vk.glsl via NkShaderConverter
//   • DX11    → HLSL  (fichiers .dx11.hlsl)
//   • DX12    → HLSL  (fichiers .dx12.hlsl)
//
// TODO (étudiant) :
//   1. Ouvrez les fichiers dans Resources/Shaders/Model/ et lisez-les.
//   2. Modifiez le vertex shader pour que le triangle tourne (ajoutez un UBO).
//   3. Modifiez le fragment shader pour interpoler une couleur arc-en-ciel.
static NkShaderHandle LoadShadersForAPI(NkIDevice* device, NkGraphicsApi api) {
    NkShaderDesc shaderDesc;
    shaderDesc.debugName = "TriangleShader";

    // ⚠️ PIÈGE MÉMOIRE — NkShaderStageDesc stocke des const char* RAW (pointeurs,
    //    pas de copie). Les résultats vert/frag DOIVENT rester en vie jusqu'à la fin
    //    de device->CreateShader(). On les déclare donc ICI, au scope de la fonction,
    //    pas à l'intérieur d'un bloc case { } qui se terminerait avant CreateShader.
    NkShaderConvertResult vert, frag;   // sources texte (GLSL/HLSL)
    NkShaderConvertResult vertSrc, fragSrc; // sources Vulkan (texte intermédiaire)

    switch (api) {
        // ── OpenGL et Software (CPU rasterizer) ──────────────────────────────
        // Le backend Software utilise les mêmes sources GLSL pour la description
        // du layout, mais exécute les shaders en C++ (callbacks CPU).
        case NkGraphicsApi::NK_GFX_API_OPENGL:
            vert = NkShaderConverter::LoadFile("Resources/Shaders/Model/triangle.vert.gl.glsl");
            frag = NkShaderConverter::LoadFile("Resources/Shaders/Model/triangle.frag.gl.glsl");
            if (!vert.success || !frag.success) {
                logger.Error("[Shader] OpenGL : échec chargement des fichiers GLSL");
                return {};
            }
            shaderDesc.AddGLSL(NkShaderStage::NK_VERTEX,   vert.source.CStr(), "main");
            shaderDesc.AddGLSL(NkShaderStage::NK_FRAGMENT, frag.source.CStr(), "main");
            break;
        // ── Vulkan ────────────────────────────────────────────────────────────
        // Vulkan n'accepte que du SPIR-V binaire. NkShaderConverter compile les
        // sources GLSL à la volée (via glslang) et retourne des uint32[].
        //
        // ⚠️ PIÈGE : LoadAsSpirv déduit le stage depuis l'avant-dernière extension.
        //    "triangle.vert.vk.glsl" → StageExt = "vk" → stage inconnu → vertex !
        //    Il faut donc charger le source puis appeler GlslToSpirv avec le stage
        //    explicite : NkSLStage::NK_VERTEX et NkSLStage::NK_FRAGMENT.
        case NkGraphicsApi::NK_GFX_API_VULKAN:
            vertSrc = NkShaderConverter::LoadFile("Resources/Shaders/Model/triangle.vert.vk.glsl");
            fragSrc = NkShaderConverter::LoadFile("Resources/Shaders/Model/triangle.frag.vk.glsl");
            if (!vertSrc.success || !fragSrc.success) {
                logger.Error("[Shader] Vulkan : échec chargement des sources GLSL");
                return {};
            }
            // Compilation explicite avec le stage correct
            vert = NkShaderConverter::GlslToSpirv(vertSrc.source, NkSLStage::NK_VERTEX,   "triangle.vert");
            frag = NkShaderConverter::GlslToSpirv(fragSrc.source, NkSLStage::NK_FRAGMENT, "triangle.frag");
            if (!vert.success || !frag.success) {
                logger.Error("[Shader] Vulkan : échec compilation GLSL → SPIR-V");
                logger.Error("[Shader] Vert errors: {0}", vert.errors.CStr());
                logger.Error("[Shader] Frag errors: {0}", frag.errors.CStr());
                return {};
            }
            // AddSPIRV copie le binaire → pas de problème de durée de vie
            shaderDesc.AddSPIRV(NkShaderStage::NK_VERTEX,   vert.binary.Data(), vert.binary.Size());
            shaderDesc.AddSPIRV(NkShaderStage::NK_FRAGMENT, frag.binary.Data(), frag.binary.Size());
            break;

        // ── DirectX 11 ────────────────────────────────────────────────────────
        // HLSL compilé par D3DCompile au moment de CreateShader().
        // Les entry points sont "VSMain" (vertex) et "PSMain" (pixel/fragment).
        // ⚠️ AddHLSL stocke un pointeur RAW → vert/frag doivent rester vivants
        //    jusqu'à CreateShader (c'est garanti ici car ils sont en scope fonction).
        case NkGraphicsApi::NK_GFX_API_D3D11:
            vert = NkShaderConverter::LoadFile("Resources/Shaders/Model/triangle.vert.dx11.hlsl");
            frag = NkShaderConverter::LoadFile("Resources/Shaders/Model/triangle.frag.dx11.hlsl");
            if (!vert.success || !frag.success) {
                logger.Error("[Shader] DX11 : échec chargement des fichiers HLSL");
                return {};
            }
            shaderDesc.AddHLSL(NkShaderStage::NK_VERTEX,   vert.source.CStr(), "VSMain");
            shaderDesc.AddHLSL(NkShaderStage::NK_FRAGMENT, frag.source.CStr(), "PSMain");
            break;

        // ── DirectX 12 ────────────────────────────────────────────────────────
        case NkGraphicsApi::NK_GFX_API_D3D12:
            vert = NkShaderConverter::LoadFile("Resources/Shaders/Model/triangle.vert.dx12.hlsl");
            frag = NkShaderConverter::LoadFile("Resources/Shaders/Model/triangle.frag.dx12.hlsl");
            if (!vert.success || !frag.success) {
                logger.Error("[Shader] DX12 : échec chargement des fichiers HLSL");
                return {};
            }
            shaderDesc.AddHLSL(NkShaderStage::NK_VERTEX,   vert.source.CStr(), "VSMain");
            shaderDesc.AddHLSL(NkShaderStage::NK_FRAGMENT, frag.source.CStr(), "PSMain");
            break;

        case NkGraphicsApi::NK_GFX_API_SOFTWARE: {
            // auto* vsFn = new NkVertexShaderSoftware([](const void* vdata, uint32 idx, const void* udata) -> NkVertexSoftware {
            //     const Vertex* v = static_cast<const Vertex*>(vdata) + idx;
                
            //     NkVertexSoftware out;
            //     out.position = math::NkVec4(v->position.x, v->position.y, 0, 1.0f);
            //     out.color = math::NkVec4(v->color.r, v->color.g, v->color.b, 1.0f);

            //     return out;
            // });

            // auto* fsFn = new NkPixelShaderSoftware([](const NkVertexSoftware& frag, const void* udata, const void*) -> math::NkVec4f {
            //     return {frag.color.r, frag.color.g, frag.color.b, 1.f};
            // });

            // shaderDesc.AddSWFn(NkShaderStage::NK_VERTEX, static_cast<const void*>(vsFn));
            // shaderDesc.AddSWFn(NkShaderStage::NK_FRAGMENT, static_cast<const void*>(fsFn));

            // sksl::NkSkSLBinary bin;
            // bool loaded = false;

            // if (!loaded) {
            //     // Fallback 1 : fichier .nsksl binaire
            //     loaded = sksl::NkSkSLBinary::DeserializeFile("Resources/Shaders/Model/triangle.sw.sksl", bin);
            // }
            // if (!loaded) {
            //     // Fallback 2 : fichier .sksl source
            //     loaded = sksl::NkSkSLLoader::LoadFromFile("Resources/Shaders/Model/triangle.sw.sksl", bin);
            // }
            // if (!loaded) {
            //     // Fallback 3 : source inline (toujours disponible)
            //     const char* fallback = R"sksl(
            // // @stage vertex
            // layout(location=0) in vec2 aPosition;
            // layout(location=1) in vec3 aColor;

            // out vec3 vColor;

            // void main() {
            //     vColor = aColor;
            //     gl_Position = vec4(aPosition, 0.0, 1.0);
            // }

            // // @stage fragment
            // in  vec3 vColor;

            // out vec4 FragColor;

            // void main() {
            //     FragColor = vec4(vColor, 1.0);
            // }
            // )sksl";
            //     loaded = sksl::NkSkSLLoader::LoadFromString(fallback, strlen(fallback), bin);
            // }

            // auto compiled = sksl::NkSkSLCompiler::Compile(bin);
            // if (!compiled.success) {
            //     logger.Errorf("[Shader] Software SkSL error: {0}", compiled.errors.CStr());
            //     return {};
            // }

            // // Créer les pointeurs heap — NkSoftwareDevice::CreateShader() fait delete
            // auto* vsFn = new NkVertexShaderSoftware(traits::NkMove(compiled.vertFn));
            // auto* psFn = new NkPixelShaderSoftware(traits::NkMove(compiled.fragFn));
            // shaderDesc.AddSWFn(NkShaderStage::NK_VERTEX,   static_cast<const void*>(vsFn));
            // shaderDesc.AddSWFn(NkShaderStage::NK_FRAGMENT,  static_cast<const void*>(psFn));

            // Stride fixe NkVertex2D : float x,y + float u,v + uint8 r,g,b,a = 20 bytes
//             constexpr uint32 kNkVertex2DStride = 20;

//             // Chemin 1 : fichier .sw.sksl dédié au backend software
//             swbridge::NkSWBridgeResult compiled;
//             bool loaded = false;

//             // Essayer le fichier .sw.sksl spécifique au backend software
//             {
//                 FILE* f = fopen("Resources/Shaders/Model/triangle.sw.sksl", "rb");
//                 if (f) {
//                     fclose(f);
//                     compiled = swbridge::NkCompileFile("Resources/Shaders/Model/triangle.sw.sksl", kNkVertex2DStride);
//                     loaded = compiled.success;
//                 }
//             }

//             // Chemin 2 : fallback inline (toujours disponible, ne dépend d'aucun fichier)
//             if (!loaded) {
//                 logger.Warnf("[Shader] Software: fichier .sw.sksl non trouve, utilisation du fallback inline");

//                 constexpr const char* kFallback = R"nksl(
// // triangle.sw.sksl
// // Source NkSL compilée par NkSLCompiler vers C++ (NK_CPLUSPLUS target)
// // Compatible avec NkSoftwareDevice via NkSWVertexShader / NkSWPixelShader
// //
// // Layout NkVertex2D (stride=20) :
// //   offset 0  : float x, y    (position écran en pixels)
// //   offset 8  : float u, v    (UV [0,1])
// //   offset 16 : uint8 r,g,b,a (couleur — converti float [0,1] dans le vertex shader)
// //
// // Push constant (uniform block) :
// //   mat4 proj  — matrice ortho NkView2D::ToProjectionMatrix() column-major

// @location(0) in vec2 aPosition;
// @location(1) in vec3 aColor;

// @location(0) out vec4 vColor;

// @stage(vertex)
// @entry
// void vert_main() {
//     vColor = aColor;
//     gl_Position = vec4(aPosition, 0.0, 1.0);
// }

// @location(0) in vec3 vColor;
// @location(0) out vec4 FragColor;

// @stage(fragment)
// @entry
// void frag_main() {
//     FragColor = vec4(vColor, 1.0);
// }
// )nksl";

//                 compiled = swbridge::NkCompile(NkString(kFallback), "inline_fallback.sw.sksl", kNkVertex2DStride);
//                 loaded = compiled.success;
//             }

//             if (!loaded) {
//                 logger.Errorf("[Shader] Software: echec compilation NkSL: {0}", compiled.error.CStr());
//                 return {};
//             }

//             // Passer les lambdas au device via AddSWFn
//             // Pattern heap-allocate : NkSoftwareDevice::CreateShader() fait delete
//             auto* vsFn = new NkVertexShaderSoftware(traits::NkMove(compiled.vertFn));
//             auto* psFn = new NkPixelShaderSoftware (traits::NkMove(compiled.fragFn));

//             shaderDesc.AddSWFn(NkShaderStage::NK_VERTEX, static_cast<const void*>(vsFn));
//             shaderDesc.AddSWFn(NkShaderStage::NK_FRAGMENT, static_cast<const void*>(psFn));
// Stride exact = sizeof(Vertex) = sizeof(NkVec2f) + sizeof(NkVec3f) = 20
            swbridge::NkSWBridgeResult compiled;
            bool loaded = false;
        
            // ── Chemin 1 : deux fichiers séparés (RECOMMANDÉ) ─────────────────────────
            {
                const char* vertPath = "Resources/Shaders/Model/triangle.vert.sw.sksl";
                const char* fragPath = "Resources/Shaders/Model/triangle.frag.sw.sksl";
                FILE* fv = fopen(vertPath, "rb");
                FILE* ff = fopen(fragPath, "rb");
                if (fv && ff) {
                    fclose(fv); fclose(ff);
                    compiled = swbridge::NkCompileFiles(vertPath, fragPath);
                    loaded = compiled.success;
                } else {
                    if (fv) fclose(fv);
                    if (ff) fclose(ff);
                }
            }
        
            // ── Chemin 2 : fallback inline avec deux stages explicites ────────────────
            if (!loaded) {
                logger.Warnf("[Shader] Software: fichiers .sw.sksl absents, fallback inline");
        
                // VERTEX — seulement les déclarations du stage vertex
                constexpr const char* kVertSrc = R"(
        @location(0) in vec2 aPosition;
        @location(1) in vec3 aColor;
        
        @location(0) out vec3 vColor;
        
        @stage(vertex)
        @entry
        void vert_main() {
            vColor = aColor;
            gl_Position = vec4(aPosition, 0.0, 1.0);
        }
        )";
                // FRAGMENT — seulement les déclarations du stage fragment
                constexpr const char* kFragSrc = R"(
        @location(0) in  vec3 vColor;
        @location(0) out vec4 FragColor;
        
        @stage(fragment)
        @entry
        void frag_main() {
            FragColor = vec4(vColor, 1.0);
        }
        )";
                compiled = swbridge::NkCompileSources(
                    NkString(kVertSrc), NkString(kFragSrc),
                    "inline.vert.sw.sksl", "inline.frag.sw.sksl");
                loaded = compiled.success;
            }
        
            if (!loaded) {
                logger.Errorf("[Shader] Software: echec NkSL: {0}", compiled.error.CStr());
                return {};
            }
        
            auto* vsFn = new NkVertexShaderSoftware(traits::NkMove(compiled.vertFn));
            auto* psFn = new NkPixelShaderSoftware (traits::NkMove(compiled.fragFn));
            shaderDesc.AddSWFn(NkShaderStage::NK_VERTEX,   static_cast<const void*>(vsFn));
            shaderDesc.AddSWFn(NkShaderStage::NK_FRAGMENT, static_cast<const void*>(psFn));
            break;
        }

        default:
            logger.Error("[Shader] API non supportée");
            return {};
    }

    NkShaderHandle shader = device->CreateShader(shaderDesc);

    if (!shader.IsValid()) {
        logger.Error("[Shader] Échec création du shader GPU");
    }
    
    return shader;
}

// =============================================================================
// nkmain — point d'entrée de l'application Nkentseu
// =============================================================================
// Remplace main() / WinMain(). NkEntryState contient les arguments de la ligne
// de commande, le nom de l'application et d'autres infos de démarrage.
int nkmain(const NkEntryState& state) {

    // =========================================================================
    // ÉTAPE 1 : Sélection du backend graphique
    // =========================================================================
    // On lit l'argument --backend=xxx. Si absent, on utilise OpenGL par défaut.
    appState.api = ParseBackend(state.GetArgs());
    logger.Info("[Main] Backend sélectionné : {0}", NkGraphicsApiName(appState.api));
    nkentseu::nksl::InitCompiler("./shader_cache");

    // =========================================================================
    // ÉTAPE 2 : Création de la fenêtre
    // =========================================================================
    // NkWindowConfig rassemble toutes les propriétés de la fenêtre.
    // TODO (étudiant) : essayez de changer le titre, la taille ou de désactiver
    //                   le redimensionnement (resizable = false).
    NkWindowConfig cfg;
    cfg.title       = state.appName;          // titre dans la barre de titre
    cfg.width       = appState.size.width;
    cfg.height      = appState.size.height;
    cfg.centered    = true;                   // centrer sur l'écran
    cfg.resizable   = true;                   // permet le redimensionnement

    NkWindow window;
    if (!window.Create(cfg)) {
        logger.Error("[Window] Impossible de créer la fenêtre");
        return -2;
    }

    // =========================================================================
    // ÉTAPE 3 : Création du device GPU (NkIDevice)
    // =========================================================================
    // NkDeviceInitInfo regroupe tout ce dont le driver a besoin pour s'initialiser :
    //   - l'API cible  (OpenGL, Vulkan…)
    //   - la surface   (handle natif de la fenêtre)
    //   - les dimensions initiales
    //   - les paramètres de contexte spécifiques à l'API (version, debug mode…)
    NkDeviceInitInfo initInfo;
    initInfo.api     = appState.api;
    initInfo.surface = window.GetSurfaceDesc(); // handle natif Win32/X11/…
    initInfo.width   = appState.size.width;
    initInfo.height  = appState.size.height;

    // Paramètres spécifiques à chaque API
    // TODO (étudiant) : pour Vulkan, "engineName" s'affiche dans les outils
    //                   de diagnostic (RenderDoc, Nsight…).
    switch (appState.api) {
        case NkGraphicsApi::NK_GFX_API_OPENGL:
            initInfo.context = NkContextDesc::MakeOpenGL(4, 6, true); // version 4.6, debug=true
            break;
        case NkGraphicsApi::NK_GFX_API_VULKAN:
            initInfo.context = NkContextDesc::MakeVulkan(true);       // validation layers activées
            initInfo.context.vulkan.appName    = "Triangle";
            initInfo.context.vulkan.engineName = "Nkentseu";
            break;
        case NkGraphicsApi::NK_GFX_API_D3D11:
            initInfo.context = NkContextDesc::MakeDirectX11(true);    // debug layer activée
            break;
        case NkGraphicsApi::NK_GFX_API_D3D12:
            initInfo.context = NkContextDesc::MakeDirectX12(true);
            break;
        default:
            initInfo.context = NkContextDesc::MakeOpenGL(4, 6, true);
            break;
    }

    NkIDevice* device = NkDeviceFactory::Create(initInfo);
    if (!device || !device->IsValid()) {
        // Fallback automatique vers le renderer Software (CPU) si le GPU échoue
        logger.Error("[Device] Échec de création du device principal, tentative Software…");
        initInfo.api = NkGraphicsApi::NK_GFX_API_SOFTWARE;
        device = NkDeviceFactory::Create(initInfo);
        if (!device || !device->IsValid()) {
            logger.Error("[Device] Aucun device valide disponible");
            window.Close();
            return -3;
        }
        appState.api = NkGraphicsApi::NK_GFX_API_SOFTWARE;
    }
    logger.Info("[Device] Device initialisé : {0}", NkGraphicsApiName(appState.api));

    // =========================================================================
    // ÉTAPE 4 : Récupération du render pass de la swapchain
    // =========================================================================
    // Un render pass décrit les attachements de rendu (color, depth, stencil)
    // ainsi que les opérations de chargement/stockage (clear, keep, …).
    //
    // IMPORTANT : GetSwapchainRenderPass() retourne le render pass INTERNE de
    // la swapchain. Ne pas le détruire manuellement ; il est géré par le device.
    //
    // ⚠️ PIÈGE CLASSIQUE : après un redimensionnement (OnResize), la swapchain
    // est recréée et l'ID du render pass peut changer. Il faut alors re-créer
    // le pipeline graphique. Voir la gestion de resize dans la boucle principale.
    //
    // TODO (étudiant) : pour un render pass personnalisé (clear de couleur
    //                   différente, attachement depth…), utilisez
    //                   device->CreateRenderPass(NkRenderPassDesc).
    NkRenderPassHandle renderPass = device->GetSwapchainRenderPass();
    if (!renderPass.IsValid()) {
        logger.Error("[RenderPass] Render pass swapchain invalide");
        NkDeviceFactory::Destroy(device);
        window.Close();
        return -4;
    }

    // =========================================================================
    // ÉTAPE 5 : Chargement des shaders
    // =========================================================================
    // LoadShadersForAPI crée un NkShaderHandle qui contient les stages vertex
    // et fragment compilés pour l'API courante.
    NkShaderHandle gpuShader = LoadShadersForAPI(device, appState.api);
    if (!gpuShader.IsValid()) {
        logger.Error("[Shader] Impossible de charger les shaders");
        NkDeviceFactory::Destroy(device);
        window.Close();
        return -5;
    }

    // =========================================================================
    // ÉTAPE 6 : Définition de la géométrie (vertex buffer)
    // =========================================================================
    // Un vertex = un sommet du triangle avec ses attributs (position, couleur…).
    // Les coordonnées sont en NDC (Normalized Device Coordinates) :
    //   X : -1 (gauche) → +1 (droite)
    //   Y : -1 (bas)    → +1 (haut)       (convention OpenGL/Vulkan flippé Y)
    //   Z :  0 (plan proche) — non utilisé ici
    //

    // TODO (étudiant) :
    //   1. Modifiez les positions pour changer la forme du triangle.
    //   2. Ajoutez un 4ème sommet et un index buffer pour dessiner un carré.
    //   3. Essayez de rendre le triangle plus grand ou plus petit.
    // Vertex vertices[] = {
    //     { { 0.0f,  0.5f }, { 1.0f, 0.0f, 0.0f } }, // sommet haut   → rouge
    //     { { 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } }, // sommet droit  → vert
    //     { {-0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f } }  // sommet gauche → bleu
    // };
    Vertex vertices[] = {
        { { 0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f } }, // sommet haut   → rouge
        { { 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } }, // sommet droit  → vert
        { {-0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f } },  // sommet gauche → bleu
        { {-0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f } },  // sommet gauche → bleu
        { {-0.5f,  0.5f }, { 0.0f, 0.0f, 0.0f } }, // sommet droit  → vert
        { { 0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f } }, // sommet haut   → rouge
    };

    // NkBufferDesc::Vertex crée un descriptor pour un vertex buffer avec upload
    // immédiat des données (vertices) vers la mémoire GPU.
    NkBufferDesc vboDesc = NkBufferDesc::Vertex(sizeof(vertices), vertices);
    vboDesc.debugName = "TriangleVBO"; // nom affiché dans les outils de debug
    NkBufferHandle vertexBuffer = device->CreateBuffer(vboDesc);
    if (!vertexBuffer.IsValid()) {
        logger.Error("[Buffer] Impossible de créer le vertex buffer");
        device->DestroyShader(gpuShader);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return -6;
    }

    // =========================================================================
    // ÉTAPE 7 : Description du layout des vertices (NkVertexLayout)
    // =========================================================================
    // Le vertex layout indique au pipeline comment interpréter les octets du
    // vertex buffer :
    //   AddAttribute(location, binding, format, offset)
    //     location = numéro de layout dans le shader (layout(location = N))
    //     binding  = quel vertex buffer (0 = premier buffer lié)
    //     format   = type de données (NK_RG32_FLOAT = 2 floats, …)
    //     offset   = décalage en octets dans la structure Vertex
    //
    //   AddBinding(binding, stride)
    //     stride = taille totale d'un vertex en octets (sizeof(Vertex))
    //
    // TODO (étudiant) : si vous ajoutez un attribut UV (texture), ajoutez
    //   .AddAttribute(2, 0, NkGPUFormat::NK_RG32_FLOAT, offsetof(Vertex, uv))
    // ⚠️ Les noms sémantiques ("POSITION", "COLOR") sont obligatoires pour DX11/DX12 :
    //    ils correspondent aux attributs HLSL ( float2 pos : POSITION; float3 col : COLOR; ).
    //    Pour OpenGL/Vulkan, ils sont ignorés mais ne causent pas d'erreur.
    NkVertexLayout vertexLayout;
    vertexLayout
        .AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT,  offsetof(Vertex, position), "POSITION", 0) // position (vec2)
        .AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT, offsetof(Vertex, color),    "COLOR",    0) // couleur  (vec3)
        .AddBinding(0, sizeof(Vertex));                                                              // stride = 20 octets

    // =========================================================================
    // ÉTAPE 8 : Création du pipeline graphique
    // =========================================================================
    // Un pipeline graphique est un objet immuable qui encode l'état complet
    // du rendu : shader, layout, topologie, rasterizer, depth, blend, renderpass.
    //
    // ⚠️ Le pipeline est lié à un render pass PRÉCIS. Si le render pass change
    //    (après resize), le pipeline doit être recréé. Voir la boucle principale.
    //
    // TODO (étudiant) : essayez NkRasterizerDesc::Wireframe() pour afficher
    //                   uniquement les arêtes du triangle.
    //
    // ⚠️ FACE CULLING : NkRasterizerDesc::Default() active le back-face culling
    //    avec frontFace=NK_CCW (anti-horaire vu de face). Si vos vertices sont en
    //    ordre horaire (CW), le triangle est invisible ! Deux solutions :
    //      1. NkRasterizerDesc::NoCull()   → désactiver le culling (simple)
    //      2. Réordonner les vertices en CCW (gauche→droite→haut)
    //    Ici on choisit NoCull pour garder la liberté de l'ordre des vertices.
    NkGraphicsPipelineDesc pipelineDesc;
    pipelineDesc.shader       = gpuShader;
    pipelineDesc.vertexLayout = vertexLayout;
    pipelineDesc.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;  // 3 vertices = 1 triangle
    pipelineDesc.rasterizer   = NkRasterizerDesc::NoCull();              // pas de culling → triangle toujours visible
    pipelineDesc.depthStencil = NkDepthStencilDesc::NoDepth();           // pas de test de profondeur
    pipelineDesc.blend        = NkBlendDesc::Opaque();                   // pas de transparence
    pipelineDesc.renderPass   = renderPass;                              // render pass cible
    pipelineDesc.debugName    = "TrianglePipeline";

    NkPipelineHandle graphicsPipeline = device->CreateGraphicsPipeline(pipelineDesc);
    if (!graphicsPipeline.IsValid()) {
        logger.Error("[Pipeline] Impossible de créer le pipeline graphique");
        device->DestroyBuffer(vertexBuffer);
        device->DestroyShader(gpuShader);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return -7;
    }

    // =========================================================================
    // ÉTAPE 9 : Création du pool de command buffers
    // =========================================================================
    // Un command buffer enregistre une liste de commandes GPU (draw, bind, …)
    // à soumettre en une seule fois. Le pool évite d'en créer/détruire un
    // à chaque frame (coûteux). Acquire() retourne un CB libre ou en crée un.
    //
    // TODO (étudiant) : si vous avez plusieurs passes de rendu (shadow + main),
    //                   acquérez deux command buffers distincts.
    NkCommandPool cmdPool(device, NkCommandBufferType::NK_GRAPHICS);

    // =========================================================================
    // ÉTAPE 10 : Enregistrement des callbacks d'événements
    // =========================================================================
    // Le système d'événements est basé sur des callbacks. On les enregistre une
    // seule fois ici. Chaque fois que PollEvents() est appelé dans la boucle,
    // les événements en attente sont distribués aux callbacks correspondants.
    //
    // TODO (étudiant) : ajoutez un callback NkKeyPressEvent pour quitter avec
    //                   la touche Échap, ou pour changer de couleur de fond.
    auto& eventSystem = NkEvents();
    eventSystem.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) {
        appState.running = false; // fermeture de fenêtre → arrêt de la boucle
    });
    eventSystem.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        // Mise à jour de la taille mémorisée (la recreation de la swapchain
        // se fait dans la boucle principale au début de chaque frame).
        appState.size.width  = e->GetWidth();
        appState.size.height = e->GetHeight();
    });

    // =========================================================================
    // ÉTAPE 11 : Boucle principale de rendu
    // =========================================================================
    // Chaque itération = une frame. L'ordre est toujours :
    //   1. PollEvents()                  → traiter les entrées clavier/souris/…
    //   2. OnResize() si nécessaire      → recréer swapchain + pipeline
    //   3. BeginFrame()                  → acquérir l'image swapchain courante
    //   4. Enregistrer les commandes GPU → viewport, renderpass, draw…
    //   5. SubmitAndPresent()            → envoyer au GPU et afficher
    //   6. EndFrame()                    → libérer les ressources de la frame
    NkChrono chrono;
    NkElapsedTime elapsed;

    logger.Info("[Main] Boucle principale démarrée. Fermez la fenêtre pour quitter.");

    while (appState.running) {

        // ── 11.1 : Distribution des événements ──────────────────────────────
        // PollEvents() (pluriel, sans valeur de retour) dispatche tous les
        // événements en attente vers les callbacks enregistrés à l'étape 10.
        // Ou utiliser PollEvent() en boucle : c'est aussi un pattern
        //    supporté par ce système d'événements.
        eventSystem.PollEvents();
        if (!appState.running) break;

        // ── 11.2 : Gestion du redimensionnement ─────────────────────────────
        // Quand la fenêtre est redimensionnée, la swapchain doit être recréée.
        // OnResize() invalide le render pass actuel et en crée un nouveau avec
        // les nouvelles dimensions. On compare les IDs pour le détecter.
        NkVec2u winSize = window.GetSize();
        if (winSize.x != (uint32)appState.size.width || winSize.y != (uint32)appState.size.height) {
            appState.size = { (int)winSize.x, (int)winSize.y };
        }

        if (appState.size.width  != (int)device->GetSwapchainWidth() || appState.size.height != (int)device->GetSwapchainHeight()) {
            device->OnResize(appState.size.width, appState.size.height);
        }

        // ── 11.3 : Début de frame ────────────────────────────────────────────
        // BeginFrame() acquiert l'image courante de la swapchain et retourne
        // false si la swapchain est invalide (ex. fenêtre minimisée → skip).
        NkFrameContext frame;
        if (!device->BeginFrame(frame)) {
            NkChrono::Sleep(8.0f); // attendre si fenêtre minimisée
            continue;
        }

        // Dimensions effectives après BeginFrame (peuvent différer si resize async)
        uint32 w = device->GetSwapchainWidth();
        uint32 h = device->GetSwapchainHeight();
        if (w == 0 || h == 0) {
            device->EndFrame(frame);
            continue;
        }

        // ── 11.4 : Recréation du pipeline si le render pass a changé ─────────
        // Après un OnResize(), GetSwapchainRenderPass() retourne un handle
        // potentiellement différent (nouvel ID). Le pipeline étant lié à un
        // render pass précis, il doit être recréé dans ce cas.
        //
        // ⚠️ C'EST LE PIÈGE LE PLUS FRÉQUENT : oublier cette étape empêche
        //    tout dessin après le premier redimensionnement de la fenêtre.
        NkRenderPassHandle currentRP = device->GetSwapchainRenderPass();
        if (currentRP.IsValid() && currentRP.id != renderPass.id) {
            // Le render pass a changé : on recrée le pipeline avec le nouveau
            renderPass = currentRP;
            pipelineDesc.renderPass = renderPass;
            if (graphicsPipeline.IsValid()) device->DestroyPipeline(graphicsPipeline);
            graphicsPipeline = device->CreateGraphicsPipeline(pipelineDesc);
            if (!graphicsPipeline.IsValid()) {
                logger.Error("[Pipeline] Recréation du pipeline après resize échouée");
                device->EndFrame(frame);
                continue;
            }
        }

        // ── 11.5 : Framebuffer swapchain ─────────────────────────────────────
        // Le framebuffer est l'image destination du rendu pour cette frame.
        // Il faut l'obtenir APRÈS BeginFrame() car il change à chaque frame.
        NkFramebufferHandle swapchainFb = device->GetSwapchainFramebuffer();
        if (!swapchainFb.IsValid()) {
            logger.Error("[Frame] Framebuffer swapchain invalide");
            device->EndFrame(frame);
            continue;
        }

        // ── 11.6 : Acquisition et enregistrement du command buffer ───────────
        // Acquire() retourne un CB vide (déjà Reset). Begin() ouvre
        // l'enregistrement. Toutes les commandes entre Begin() et End()
        // sont mémorisées dans la liste de commandes, pas exécutées.
        NkICommandBuffer* cmd = cmdPool.Acquire();
        if (!cmd->Begin()) {
            logger.Error("[Cmd] Impossible d'ouvrir le command buffer");
            cmdPool.Release(cmd);
            device->EndFrame(frame);
            continue;
        }

        // ── 11.7 : Couleur de fond dynamique (SetClearColor / SetClearDepth) ──
        // Ces appels surchargent la couleur de clear du NkRenderPassDesc.
        // Analogue à glClearColor() + glClear() en OpenGL.
        // À appeler AVANT BeginRenderPass.
        //
        // TODO (étudiant) : changez cette couleur (ex: 0.1f, 0.3f, 0.6f pour du bleu)
        //                   ou calculez-la depuis le temps pour un fond animé.
        cmd->SetClearColor(math::NkRand.NextColor()); // fond gris-bleuté
        cmd->SetClearDepth(1.0f);                 // depth buffer remis à 1.0 (plan lointain)

        // ── 11.8 : Ouverture du render pass ──────────────────────────────────
        // BeginRenderPass définit la zone de rendu (area) et exécute les
        // opérations de chargement (clear color / depth si configuré).
        //
        // ⚠️ Le viewport et scissor doivent être définis APRÈS BeginRenderPass.
        //    Certains backends (notamment OpenGL) réinitialisent l'état lors du
        //    binding du framebuffer. Définir le viewport avant BeginRenderPass
        //    risque de n'avoir aucun effet.
        NkRect2D area{{0, 0}, {(int)w, (int)h}};
        if (!cmd->BeginRenderPass(renderPass, swapchainFb, area)) {
            logger.Error("[Cmd] BeginRenderPass échoué");
            // Pour Vulkan : un échec de BeginRenderPass indique souvent que la
            // swapchain est périmée — on force un resize pour la recréer.
            if (appState.api == NkGraphicsApi::NK_GFX_API_VULKAN)
                device->OnResize(w, h);
            cmd->End();
            cmdPool.Release(cmd);
            device->EndFrame(frame);
            continue;
        }

        // ── 11.9 : Viewport et scissor (à l'intérieur du render pass) ────────
        // Viewport : zone de l'écran où le NDC est projeté (souvent = fenêtre).
        //   flipY : NkViewport::flipY vaut true par défaut — il inverse l'axe Y
        //   pour que la convention OpenGL (Y+ vers le haut) soit respectée sur
        //   tous les backends. Ne pas modifier sauf cas particulier (shadow map).
        //
        // TODO (étudiant) : essayez un viewport de taille réduite pour n'afficher
        //                   le triangle que dans un coin de la fenêtre.
        NkViewport vp{0.f, 0.f, (float)w, (float)h, 0.f, 1.f};
        // flipY non défini → valeur par défaut = true (identique à NkRHIDemoFull)
        cmd->SetViewport(vp);
        cmd->SetScissor(area);

        // ── 11.10 : Dessin du triangle ─────────────────────────────────────────
        // L'ordre de binding est important :
        //   1. BindGraphicsPipeline → active le shader + l'état de rendu
        //   2. BindVertexBuffer     → connecte le buffer de données au slot 0
        //   3. Draw(vertexCount, instanceCount, firstVertex, firstInstance)
        //        → 3 vertices, 1 instance, à partir de l'indice 0
        //
        // TODO (étudiant) :
        //   - Essayez Draw(3, 3, 0, 0) pour voir l'instancing (3 triangles)
        //     si votre shader utilise gl_InstanceIndex / SV_InstanceID.
        //   - Pour dessiner avec des indices : ajoutez un NkBufferDesc::Index
        //     et appelez DrawIndexed() à la place de Draw().
        cmd->BindGraphicsPipeline(graphicsPipeline);
        cmd->BindVertexBuffer(0, vertexBuffer, 0);
        cmd->Draw(6, 2, 0, 0); // 3 vertices, 1 instance

        cmd->EndRenderPass();
        cmd->End();

        // ── 11.11 : Soumission et présentation ────────────────────────────────
        // SubmitAndPresent envoie les commandes enregistrées au GPU puis
        // présente l'image rendue à l'écran (swap des buffers).
        device->SubmitAndPresent(cmd);

        cmdPool.Release(cmd);
        device->EndFrame(frame);

        // ── 11.12 : Limitation à ~60 FPS ─────────────────────────────────────
        // On mesure le temps écoulé et on dort le temps restant pour atteindre
        // 16 ms par frame (≈ 60 images/seconde).
        //
        // TODO (étudiant) : remplacez "16" par une valeur calculée depuis
        //                   un NkClock pour un contrôle plus précis du delta time.
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
        else
            NkChrono::YieldThread();
    }

    // =========================================================================
    // ÉTAPE 12 : Nettoyage des ressources
    // =========================================================================
    // Ordre impératif :
    //   1. WaitIdle()       → attendre que le GPU ait fini toutes ses tâches
    //   2. Libérer les ressources GPU dans l'ordre inverse de création
    //   3. Destroy(device)  → détruit le device et la swapchain
    //   4. window.Close()   → ferme la fenêtre et libère les handles natifs
    //
    // ⚠️ NE PAS appeler device->DestroyRenderPass(renderPass) ici :
    //    le render pass swapchain est géré INTERNELLEMENT par le device.
    //    Seuls les render pass créés avec device->CreateRenderPass() doivent
    //    être détruits manuellement.
    device->WaitIdle();
    cmdPool.Reset();                           // détruit tous les command buffers
    device->DestroyPipeline(graphicsPipeline); // pipeline graphique
    device->DestroyBuffer(vertexBuffer);       // vertex buffer
    device->DestroyShader(gpuShader);          // shader programme
    // renderPass : PAS de DestroyRenderPass → géré par le device (swapchain RP)
    NkDeviceFactory::Destroy(device);          // device + swapchain
    window.Close();                            // fenêtre native
    
    nkentseu::nksl::ShutdownCompiler();
    logger.Info("[Main] Application terminée normalement.");
    return 0;
}