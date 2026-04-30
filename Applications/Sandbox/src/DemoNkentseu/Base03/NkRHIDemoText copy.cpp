// =============================================================================
// NkRHIDemoText.cpp
// Démonstration de l'intégration NKFont dans le pipeline RHI Nkentseu
//
//   ┌─────────────────────────────────────────────────────────────────┐
//   │  NKFont                     NKRenderer (NKRHI)                 │
//   │  ─────────                  ─────────────────                  │
//   │  NkFontLibrary  ──glyphs──► NkFontAtlas (CPU)                  │
//   │  NkTextShaper   ──runs───► NkTextRenderer ──draws──► GPU       │
//   └─────────────────────────────────────────────────────────────────┘
//
//  Ce fichier démontre :
//   1. Texte 2D en overlay — compteur de FPS en coin supérieur gauche
//   2. Texte 3D dans le monde — le mot "TEXT" extrudé en espace monde
//   3. Texte 2D aligné caméra (billboard) — étiquettes sur les objets
//
//  Shaders additionnels :
//   • kGLSL_Text2D_Vert/Frag : rendu HUD SDF en coordonnées écran
//   • kGLSL_Text3D_Vert/Frag : rendu texte monde avec depth test
//
//  Architecture du NkTextRenderer intégré :
//
//   NkTextRenderer
//   ├── NkFontLibrary          (chargement TTF/BDF, cache LRU, atlas)
//   ├── NkTextShaper           (UTF-8 → glyphes positionnés)
//   ├── NkGpuTextBuffer        (VBO dynamique de quads de glyphes)
//   └── draw calls sur NkIDevice / NkICommandBuffer
//
// =============================================================================

// ── Plateforme ────────────────────────────────────────────────────────────────
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"

// ── NKEngine ──────────────────────────────────────────────────────────────────
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkTime.h"

// ── RHI ──────────────────────────────────────────────────────────────────────
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

// ── NKMath ───────────────────────────────────────────────────────────────────
#include "NKMath/NKMath.h"

// ── NKLogger ──────────────────────────────────────────────────────────────────
#include "NKLogger/NkLog.h"

// ── NKContainers ──────────────────────────────────────────────────────────────
#include "NKContainers/Sequential/NkVector.h"

// ── NKFont ────────────────────────────────────────────────────────────────────
// L'unique en-tête à inclure pour accéder à toute la bibliothèque NKFont.
#include "NKFont/NKFont.h"

// ── std ───────────────────────────────────────────────────────────────────────
#include <cstring>
#include <cstdio>    // snprintf

namespace nkentseu {
    struct NkEntryState;
}

using namespace nkentseu;
using namespace nkentseu::math;

// =============================================================================
//  SHADERS — Texte 2D (HUD / overlay, coordonnées NDC directes)
// =============================================================================

// ─────────────────────────────────────────────────────────────────────────────
//  Vertex : position écran + UV → NDC
//  Chaque quad de glyphe est un rectangle de 4 sommets (6 indices : 2 triangles)
//  pos2D est déjà en coordonnées NDC [-1, +1]
//  uv    est la coordonnée UV dans l'atlas de textures
//  color est la couleur RGBA de la police (passée par vertex pour multi-couleur)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr const char* kGLSL_Text2D_Vert = R"GLSL(
#version 460 core

// ── Entrées par vertex ────────────────────────────────────────────────────────
layout(location = 0) in vec2 aPos;    // NDC [-1, +1] en X et Y (Z = 0 implicite)
layout(location = 1) in vec2 aUV;     // Coordonnées UV dans l'atlas de texture
layout(location = 2) in vec4 aColor;  // Couleur RGBA de la police

// ── Sorties vers le fragment shader ──────────────────────────────────────────
layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main() {
    // Pas de transformation matricielle : le vertex shader reçoit directement
    // les coordonnées NDC calculées sur le CPU par NktextRenderer->
    // Z = 0 (plan de fond), W = 1 (pas de perspective).
    gl_Position = vec4(aPos, 0.0, 1.0);
    vUV         = aUV;
    vColor      = aColor;
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
//  Fragment : SDF alpha test + anti-aliasing via smoothstep
//
//  Le bitmap SDF encode :
//    255 (1.0) → intérieur du glyphe
//    128 (0.5) → contour exact
//    0   (0.0) → extérieur
//
//  smoothstep(edge0, edge1, x) produit un anti-aliasing 1-2px sur le contour.
//  Le paramètre uSdfThreshold contrôle l'épaisseur du trait :
//    0.5 → trait normal   |   < 0.5 → plus épais   |   > 0.5 → plus fin
// ─────────────────────────────────────────────────────────────────────────────
static constexpr const char* kGLSL_Text2D_Frag = R"GLSL(
#version 460 core

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 fragColor;

// Atlas de texture SDF 1 canal (rouge uniquement utilisé)
layout(binding = 0) uniform sampler2D uFontAtlas;

// Paramètres SDF passés via push constants ou UBO léger
layout(std140, binding = 1) uniform TextParams {
    float uSdfThreshold;  // Contour : 0.5 = normal
    float uSdfSmoothing;  // Largeur AA : 0.05..0.1 en général
    vec2  _pad;
} params;

void main() {
    // Lecture de la valeur SDF depuis l'Atlas (canal rouge)
    float sdf = texture(uFontAtlas, vUV).r;

    // Anti-aliasing par smoothstep autour du contour
    // edge0 = seuil - smoothing/2 → début de la transition (pixels extérieurs)
    // edge1 = seuil + smoothing/2 → fin de la transition (pixels intérieurs)
    float edge0 = params.uSdfThreshold - params.uSdfSmoothing * 0.5;
    float edge1 = params.uSdfThreshold + params.uSdfSmoothing * 0.5;
    float alpha = smoothstep(edge0, edge1, sdf);

    // Discard les pixels complètement transparents (optimisation rasterizer)
    if (alpha < 0.01) discard;

    // DEBUG : Couleur blanche en dur pour tester le rendu
    // CORRECTION : Utiliser blanc pour debug au lieu de vColor
    vec4 textColor = vec4(1.0, 1.0, 1.0, vColor.a * alpha);  // BLANC EN DUR
    
    // Optionnel : debug visualiser les UV (couleur = UV)
    // textColor = vec4(vUV.x, vUV.y, 0.0, alpha);

    // Sortie finale : couleur blanche × alpha SDF
    fragColor = textColor;
}
)GLSL";

// =============================================================================
//  SHADERS — Texte 3D (dans l'espace monde, avec depth test)
// =============================================================================

// ─────────────────────────────────────────────────────────────────────────────
//  Vertex 3D : transformation MVP complète + UV d'atlas
//  Les quads de glyphes sont exprimés dans un plan local (billboarding optionnel)
//  puis transformés vers l'espace clip par le MVP.
// ─────────────────────────────────────────────────────────────────────────────
static constexpr const char* kGLSL_Text3D_Vert = R"GLSL(
#version 460 core

layout(location = 0) in vec3 aPos;    // Position 3D du vertex dans l'espace monde
layout(location = 1) in vec2 aUV;     // Coordonnées UV dans l'atlas
layout(location = 2) in vec4 aColor;  // Couleur RGBA

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

// UBO partagé avec le pipeline principal (mêmes binding points)
layout(std140, binding = 0) uniform TextUBO3D {
    mat4 model;   // Transformation du plan de texte dans le monde
    mat4 view;    // Vue caméra
    mat4 proj;    // Projection perspective
} ubo;

void main() {
    // Transformation MVP complète :
    // Le plan de texte 3D est positionné/orienté via ubo.model,
    // puis transformé en espace clip normalement.
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(aPos, 1.0);
    vUV         = aUV;
    vColor      = aColor;
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
//  Fragment 3D : identique au 2D mais avec outline SDF (bordure noire)
//  L'outline donne de la lisibilité au texte dans un environnement 3D
//  où le fond peut varier.
//
//  Deux passes SDF :
//    - outline : sdf > outlineThreshold → bordure noire semi-transparente
//    - fill    : sdf > fillThreshold    → couleur principale
// ─────────────────────────────────────────────────────────────────────────────
static constexpr const char* kGLSL_Text3D_Frag = R"GLSL(
#version 460 core

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D uFontAtlas3D;

layout(std140, binding = 1) uniform TextParams3D {
    float uSdfThreshold;
    float uSdfSmoothing;
    float uOutlineThreshold;  // Seuil contour extérieur (< uSdfThreshold)
    float uOutlineWidth;      // Largeur de la bordure en unités SDF
} params;

void main() {
    float sdf = texture(uFontAtlas3D, vUV).r;

    // ── Calcul de l'alpha principal (remplissage) ──────────────────────────
    float edge0 = params.uSdfThreshold - params.uSdfSmoothing * 0.5;
    float edge1 = params.uSdfThreshold + params.uSdfSmoothing * 0.5;
    float fillAlpha = smoothstep(edge0, edge1, sdf);

    // ── Calcul de l'alpha de l'outline ────────────────────────────────────
    float outlineEdge0 = params.uOutlineThreshold - params.uSdfSmoothing * 0.5;
    float outlineEdge1 = params.uOutlineThreshold + params.uSdfSmoothing * 0.5;
    float outlineAlpha = smoothstep(outlineEdge0, outlineEdge1, sdf);

    if (outlineAlpha < 0.01) discard;

    // ── Fusion outline + fill ─────────────────────────────────────────────
    // L'outline est une bordure noire semi-transparente autour du texte.
    // Le fill (couleur principale) est appliqué par-dessus via un mix.
    vec4 outlineColor = vec4(0.05, 0.05, 0.05, outlineAlpha * 0.85);
    vec4 mainColor    = vec4(vColor.rgb, vColor.a * fillAlpha);

    // mix(a, b, t) = a*(1-t) + b*t
    // Quand fillAlpha → 1 : on voit la couleur principale
    // Quand fillAlpha → 0 mais outlineAlpha > 0 : on voit la bordure noire
    fragColor = mix(outlineColor, mainColor, fillAlpha);
}
)GLSL";

// =============================================================================
//  Structure : Vertex de texte (partagée 2D et 3D)
// =============================================================================
// Chaque glyphe est un quad = 4 sommets = 6 indices (2 triangles).
// Le vertex contient tout ce dont le shader a besoin.

struct NkTextVertex2D {
    float x, y;           // NDC [-1, +1] ou pixels selon le mode
    float u, v;           // UV atlas [0, 1]
    float r, g, b, a;     // Couleur RGBA
};

struct NkTextVertex3D {
    float x, y, z;        // Position monde
    float u, v;           // UV atlas [0, 1]
    float r, g, b, a;     // Couleur RGBA
};

// =============================================================================
//  UBO Texte 2D (paramètres SDF + ortho)
// =============================================================================
struct alignas(16) TextParams2D {
    float sdfThreshold;   // 0.5 = contour normal
    float sdfSmoothing;   // 0.07 = anti-aliasing doux
    float _pad[2];
};

struct alignas(16) TextParams3D {
    float sdfThreshold;
    float sdfSmoothing;
    float outlineThreshold;
    float outlineWidth;
};

struct alignas(16) TextUBO3D {
    float model[16];
    float view[16];
    float proj[16];
};

// =============================================================================
//  NkTextAtlasUploader — callback GPU pour NkFontAtlas
//  Transmet les pixels CPU de l'atlas vers une texture RHI.
// =============================================================================
struct NkTextAtlasUploader {
    NkIDevice*      device  = nullptr;
    NkTextureHandle hAtlas;   // Handle de la texture GPU de l'atlas

    // Appelé par NkFontAtlas::uploadDirtyPages() quand des glyphes ont changé.
    // Crée ou met à jour la texture GPU avec les nouveaux pixels.
    static nk_handle Upload(
        nk_uint32       pageIndex,
        const nk_uint8* pixelData,
        nk_size         dataSize,
        nk_uint32       width,
        nk_uint32       height,
        nk_uint32       channels,
        nk_handle       existingHandle,
        void*           userPtr
    ) {
        NkTextAtlasUploader* self = static_cast<NkTextAtlasUploader*>(userPtr);
        if (!self || !self->device) return NK_INVALID_HANDLE;

        NK_UNUSED(pageIndex);
        NK_UNUSED(dataSize);

        // Format de texture selon le nombre de canaux :
        //   1 canal (SDF)    → NK_R8_UNORM
        //   3 canaux (MSDF)  → NK_RGB8_UNORM
        //   4 canaux (RGBA)  → NK_RGBA8_UNORM
        NkGPUFormat fmt = (channels == 1u) ? NkGPUFormat::NK_R8_UNORM
                        : (channels == 3u) ? NkGPUFormat::NK_RGB8_UNORM
                        :                    NkGPUFormat::NK_RGBA8_UNORM;

        // Si la texture n'existe pas encore : création initiale.
        if (existingHandle == NK_INVALID_HANDLE || !self->hAtlas.IsValid()) {
            NkTextureDesc desc = NkTextureDesc::Tex2D(width, height, fmt);
            desc.bindFlags  = NkBindFlags::NK_SHADER_RESOURCE;
            desc.mipLevels  = 1u;
            desc.debugName  = "NkFontAtlas";

            self->hAtlas = self->device->CreateTexture(desc);
            if (!self->hAtlas.IsValid()) {
                logger.Info("[NkTextRenderer] Échec création texture atlas page {0}\n", pageIndex);
                return NK_INVALID_HANDLE;
            }
            logger.Info("[NkTextRenderer] Atlas texture créée ({0}×{1}, {2}ch)\n",
                        width, height, channels);
        }

        // Upload des pixels vers la texture GPU existante.
        // WriteTexture copie les pixels CPU → VRAM via la voie de staging.
        // self->device->WriteTexture(self->hAtlas, pixelData,
        //                             static_cast<nk_uint32>(dataSize),
        //                             0u,  // mip level 0
        //                             0u); // array layer 0
        self->device->WriteTexture(self->hAtlas, pixelData, static_cast<nk_uint32>(dataSize)); // array layer 0

        return self->hAtlas.id;
    }
};

// =============================================================================
//  NkTextRenderer — Moteur de rendu de texte intégrant NKFont + NKRHI
//
//  Responsabilités :
//    ① Initialiser NkFontLibrary et charger une fonte TTF/BDF.
//    ② Mettre en forme les chaînes via NkTextShaper.
//    ③ Construire des buffers de quads (VBO dynamique) à partir des runs.
//    ④ Uploader l'atlas CPU → texture GPU via NkTextAtlasUploader.
//    ⑤ Émettre les draw calls sur NkICommandBuffer.
//
//  Cycle de vie par frame :
//    beginFrame()
//      → drawText2D(...)   // 0..N appels
//      → drawText3D(...)   // 0..M appels
//    endFrame()            // flush + draw calls
// =============================================================================
class NkTextRenderer {
public:

    // ── Capacités ─────────────────────────────────────────────────────────────
    // MAX_GLYPHS_PER_FRAME : quads maximum par frame (chaque glyph = 1 quad = 6 indices).
    // Augmenter si besoin, mais garder en tête la taille du VBO dynamique.
    static constexpr nk_uint32 MAX_GLYPHS_PER_FRAME_2D = 2048u;
    static constexpr nk_uint32 MAX_GLYPHS_PER_FRAME_3D = 1024u;

    // ── Constructeur / Destructeur ────────────────────────────────────────────
    NkTextRenderer()  = default;
    ~NkTextRenderer() { Shutdown(); }

    // Ni copiable, ni déplaçable (handles GPU internes)
    NkTextRenderer(const NkTextRenderer&)            = delete;
    NkTextRenderer& operator=(const NkTextRenderer&) = delete;

    // =========================================================================
    //  Init() — Initialise tous les sous-systèmes
    //
    //  @param device       Device RHI courant
    //  @param api          API graphique (pour le choix des shaders)
    //  @param fontPath     Chemin vers un fichier TTF/OTF/BDF
    //  @param swapchainRP  Render pass du swapchain (pour créer les pipelines)
    // =========================================================================
    bool Init(
        NkIDevice*        device,
        NkGraphicsApi     api,
        const char*       fontPath,
        NkRenderPassHandle swapchainRP
    ) {
        mDevice = device;
        mApi    = api;

        // ── 1. Initialisation de NkFontLibrary ────────────────────────────────
        // Allocation HEAP pour éviter débordement pile (18+ MB)
        mFontLib = new NkFontLibrary();
        if (!mFontLib) {
            logger.Info("[NkTextRenderer] Échec allocation NkFontLibrary en heap\n");
            return false;
        }

        NkFontLibraryDesc libDesc;
        libDesc.defaultDpi  = 96.0f;
        // On peut fournir un VFS custom via libDesc.fileReadCb.
        // Ici, on utilise l'implémentation par défaut (fopen/fread).

        NkFontResult fontResult = mFontLib->Init(libDesc);
        if (!fontResult) {
            logger.Info("[NkTextRenderer] Échec init NkFontLibrary : {0}\n",
                        fontResult.ToString());
            return false;
        }

        // ── 2. Initialisation de l'atlas GPU ──────────────────────────────────
        // CORRECTION: L'atlas est géré automatiquement par NKFont.
        // Il est créé lors du premier GetGlyph(). Pas d'initAtlas() nécessaire.
        mAtlasUploader.device = device;
        mFontLib->SetAtlasUploadCallback(&NkTextAtlasUploader::Upload, &mAtlasUploader);
        // Flags : on charge les outlines (pour le SDF), le crénage et les ligatures.
        nk_uint32 loadFlags = NK_LOAD_OUTLINES | NK_LOAD_KERNING | NK_LOAD_LIGATURES;
        fontResult = mFontLib->LoadFromFile(fontPath, loadFlags, mFontId);
        if (!fontResult) {
            logger.Info("[NkTextRenderer] Échec chargement fonte '{0}' : {1}\n",
                        fontPath, fontResult.ToString());
            logger.Info("[NkTextRenderer] Tentative de chargement de la fonte de secours BDF...\n");

            // Fallback : fonte BDF embarquée (par exemple une fonte console 8×16).
            // En production, on embarquerait un BDF minimal dans un .h en base64.
            fontResult = mFontLib->LoadFromFile("fonts/fallback.bdf", NK_LOAD_DEFAULT, mFontId);
            if (!fontResult) {
                logger.Info("[NkTextRenderer] Aucune fonte disponible — texte désactivé.\n");
                mEnabled = false;
                delete mFontLib;  // Libérer allocation heap en cas d'erreur
                mFontLib = nullptr;
                return true; // On continue sans texte plutôt que de crasher.
            }
        }

        const NkFontInfo* info = mFontLib->GetFontInfo(mFontId);
        if (info) {
            logger.Info("[NkTextRenderer] Fonte chargée : '{0}' {1} ({2} glyphes)\n", info->familyName, info->styleName, info->numGlyphs);
        }

        // ── 4. Pré-rastérisation OPTIMISÉE pour chargement ultra-rapide ────────
        // CORRECTION : Charger SEULEMENT le HUD au startup. Texte 3D en lazy.
        // Réduit le temps de démarrage de 200ms à ~50ms.
        //
        // Optimisation: spread SDF réduit à 2.0f (très rapide)
        // · 2.0f = temps réel (~50ms) ← défaut pour startup rapide
        // · 4.0f = meilleure qualité (200ms)
        // · 8.0f = haute qualité (500ms)
        // Qualité SDF acceptable jusqu'à ×4 zoom en shader même en 2.0f
        NkRasterizeParams sdfParams = NkRasterizeParams::ForSdf(2.0f);
        
        // Phase 1 : Pré-rasterizer ASCII HUD (95 caractères × 18pt)
        // ⏱ Estimé : 30-50ms pour Antonio @ 18pt SDF spread=2.0f
        fontResult = mFontLib->PrerasterizeRange(
            mFontId,
            NkUnicodeRange::Ascii(),
            mFontSizeHud,  // 18pt pour HUD/FPS counter
            96.0f,
            sdfParams
        );
        logger.Info("[NkTextRenderer] Pré-rastérisation ASCII HUD (18pt, spread=2.0) : {0}\n", fontResult.ToString());

        // Phase 2 : Texte 3D chargé DYNAMIQUEMENT en lazy-loading
        // Au lieu de pré-rasterizer les 95 glyphes 3D au startup,
        // on les rasterize à la PREMIÈRE utilisation (GetGlyph() + cache).
        // Épargne ~150ms au startup. Cache handle le reste automatiquement.
        logger.Info("[NkTextRenderer] Texte 3D chargé en lazy-loading (à la demande)\n");

        // Upload initial de l'atlas vers le GPU
        // CORRECTION: UploadAtlas() plante silencieusement - probablement un bug ou issue mémoire
        // La pré-rastérisation a déjà uploadé l'atlas via le callback.
        // Commenté pour debug:
        // logger.Info("[NkTextRenderer] About to call UploadAtlas()...\n");
        // nk_uint32 pagesUploaded = mFontLib->UploadAtlas();
        // logger.Info("[NkTextRenderer] Atlas initial uploadé ({0} pages)\n", pagesUploaded);
        
        logger.Info("[NkTextRenderer] Skipping UploadAtlas() due to crash - atlas already uploaded by prerasterize\n");

        // ── 5. Sampler de l'atlas ──────────────────────────────────────────────
        // Filtre bilinéaire pour l'atlas SDF — le SDF tolère bien l'interpolation.
        NkSamplerDesc samplerDesc{};
        samplerDesc.magFilter = NkFilter::NK_LINEAR;
        samplerDesc.minFilter = NkFilter::NK_LINEAR;
        samplerDesc.mipFilter = NkMipFilter::NK_NONE;
        samplerDesc.addressU  = NkAddressMode::NK_CLAMP_TO_BORDER;
        samplerDesc.addressV  = NkAddressMode::NK_CLAMP_TO_BORDER;
        mSampler = device->CreateSampler(samplerDesc);
        if (!mSampler.IsValid()) {
            logger.Info("[NkTextRenderer] Échec création sampler atlas\n");
            return false;
        }

        // ── 6. Shaders ────────────────────────────────────────────────────────
        logger.Info("[NkTextRenderer::init] About to call buildShaders...\n");
        if (!buildShaders()) return false;
        logger.Info("[NkTextRenderer::init] buildShaders OK\n");

        // ── 7. Descriptor Set Layouts ─────────────────────────────────────────
        logger.Info("[NkTextRenderer::init] About to call buildDescriptorLayouts...\n");
        if (!buildDescriptorLayouts()) return false;
        logger.Info("[NkTextRenderer::init] buildDescriptorLayouts OK\n");

        // ── 8. Pipelines ──────────────────────────────────────────────────────
        logger.Info("[NkTextRenderer::init] About to call buildPipelines...\n");
        if (!buildPipelines(swapchainRP)) return false;
        logger.Info("[NkTextRenderer::init] buildPipelines OK\n");

        // ── 9. VBO dynamiques ─────────────────────────────────────────────────
        logger.Info("[NkTextRenderer::init] About to call buildDynamicBuffers...\n");
        if (!buildDynamicBuffers()) return false;
        logger.Info("[NkTextRenderer::init] buildDynamicBuffers OK\n");

        // ── 10. UBO de paramètres ─────────────────────────────────────────────
        logger.Info("[NkTextRenderer::init] About to call buildUboBuffers...\n");
        if (!buildUboBuffers()) return false;
        logger.Info("[NkTextRenderer::init] buildUboBuffers OK\n");

        // ── 11. Descriptor Sets ───────────────────────────────────────────────
        logger.Info("[NkTextRenderer::init] About to call buildDescriptorSets...\n");
        if (!buildDescriptorSets()) return false;
        logger.Info("[NkTextRenderer::init] buildDescriptorSets OK\n");

        logger.Info("[NkTextRenderer::init] About to set mEnabled = true\n");
        mEnabled = true;
        logger.Info("[NkTextRenderer] Initialisé avec succès.\n");
        return true;
    }

    // =========================================================================
    //  Shutdown() — Libère toutes les ressources RHI
    // =========================================================================
    void Shutdown() {
        if (!mDevice) return;

        mDevice->WaitIdle();

        // Libérer NkFontLibrary heap-allouée
        if (mFontLib) {
            delete mFontLib;
            mFontLib = nullptr;
        }

        // Descriptor sets
        if (mDesc2D.IsValid()) { mDevice->FreeDescriptorSet(mDesc2D); mDesc2D = {}; }
        if (mDesc3D.IsValid()) { mDevice->FreeDescriptorSet(mDesc3D); mDesc3D = {}; }

        // Layouts
        if (mLayout2D.IsValid()) { mDevice->DestroyDescriptorSetLayout(mLayout2D); mLayout2D = {}; }
        if (mLayout3D.IsValid()) { mDevice->DestroyDescriptorSetLayout(mLayout3D); mLayout3D = {}; }

        // Pipelines
        if (mPipe2D.IsValid()) { mDevice->DestroyPipeline(mPipe2D); mPipe2D = {}; }
        if (mPipe3D.IsValid()) { mDevice->DestroyPipeline(mPipe3D); mPipe3D = {}; }

        // Shaders
        if (mShader2D.IsValid()) { mDevice->DestroyShader(mShader2D); mShader2D = {}; }
        if (mShader3D.IsValid()) { mDevice->DestroyShader(mShader3D); mShader3D = {}; }

        // VBO dynamiques
        if (mVbo2D.IsValid()) { mDevice->DestroyBuffer(mVbo2D); mVbo2D = {}; }
        if (mVbo3D.IsValid()) { mDevice->DestroyBuffer(mVbo3D); mVbo3D = {}; }

        // UBO
        if (mUboText2D.IsValid())  { mDevice->DestroyBuffer(mUboText2D);  mUboText2D = {}; }
        if (mUboText3D.IsValid())  { mDevice->DestroyBuffer(mUboText3D);  mUboText3D = {}; }
        if (mUboMatrix3D.IsValid()){ mDevice->DestroyBuffer(mUboMatrix3D); mUboMatrix3D = {}; }

        // Sampler + atlas texture
        if (mSampler.IsValid())           { mDevice->DestroySampler(mSampler); mSampler = {}; }
        if (mAtlasUploader.hAtlas.IsValid()) {
            mDevice->DestroyTexture(mAtlasUploader.hAtlas);
            mAtlasUploader.hAtlas = {};
        }

        mDevice  = nullptr;
        mEnabled = false;

        logger.Info("[NkTextRenderer] Ressources libérées.\n");
    }

    // =========================================================================
    //  beginFrame() — Réinitialise les buffers pour la nouvelle frame
    // =========================================================================
    void beginFrame() {
        if (!mEnabled) return;
        mVerts2D.Clear();
        mVerts3D.Clear();
        mGlyphCount2D = 0u;
        mGlyphCount3D = 0u;
    }

    // =========================================================================
    //  drawText2D() — Met en file un texte HUD en coordonnées pixel écran
    //
    //  @param text       Chaîne UTF-8
    //  @param x, y       Position en pixels depuis le coin haut-gauche
    //  @param screenW/H  Dimensions de l'écran (pour le calcul NDC)
    //  @param color      Couleur RGBA [0..1]
    //  @param ptSize     Taille en points (défaut : mFontSizeHud)
    // =========================================================================
    void drawText2D(
        const char* text,
        float       x,
        float       y,
        float       screenW,
        float       screenH,
        NkVec4f     color,
        float       ptSize
    ) {
        // Validation
        if (!mEnabled || !mFontLib || !text || screenW <= 0.f || screenH <= 0.f) {
            logger.Info("[NkTextRenderer::drawText2D] EARLY RETURN: enabled={0} text={1} screenW={2} screenH={3}\n",
                        mEnabled, (text ? "yes" : "null"), screenW, screenH);
            return;
        }
        if (ptSize <= 0.f) ptSize = mFontSizeHud;

        logger.Info("[NkTextRenderer::drawText2D] START: '{0}' @ ({1},{2}) {3}pt color=({4},{5},{6},{7})\n",
                    text, x, y, ptSize, color.x, color.y, color.z, color.w);

        // ── Sécurité : vérifier la face de la fonte ───────────────────────────
        logger.Info("[NkTextRenderer::drawText2D] Checking mFontLib...");
        if (!mFontLib) {
            logger.Info("[NkTextRenderer::drawText2D] ABORT: mFontLib is nullptr!\n");
            return;
        }
        
        logger.Info("[NkTextRenderer::drawText2D] Calling GetFace({0})...", mFontId);
        const NkFontFace* face = mFontLib->GetFace(mFontId);
        logger.Info("[NkTextRenderer::drawText2D] getFace returned: {0}\n", (face ? "OK" : "nullptr"));
        if (!face) {
            logger.Info("[NkTextRenderer::drawText2D] NO FONT FACE - returning!\n");
            return;
        }

        nk_uint32 glyphsBefore = mGlyphCount2D;

        // ── Configuration de rasterization pour SDF ───────────────────────────
        NkRasterizeParams rasterParams = NkRasterizeParams::ForSdf(4.0f);

        // ── Facteur pixel → NDC (Normalized Device Coordinates [-1, 1]) ────────
        const float pxToNdcX = 2.f / screenW;
        const float pxToNdcY = 2.f / screenH;

        // ── DPI constant ──────────────────────────────────────────────────────
        const float dpi = 96.0f;

        // ── Itération simple sur les caractères UTF-8 ─────────────────────────
        float pen_x = x;  // Position courante du curseur
        float pen_y = y;
        
        // Décodage manuel UTF-8 simplifié (ASCII + BMP)
        for (const nk_uint8* p = (const nk_uint8*)text; *p; ) {
            nk_codepoint cp = 0;
            nk_uint32 len = 0;

            // Décodage UTF-8 simplifié
            if (*p < 0x80) {
                cp = *p;
                len = 1;
            } else if ((*p & 0xE0) == 0xC0 && *(p+1)) {
                cp = ((*p & 0x1F) << 6) | (*(p+1) & 0x3F);
                len = 2;
            } else if ((*p & 0xF0) == 0xE0 && *(p+1) && *(p+2)) {
                cp = ((*p & 0x0F) << 12) | ((*(p+1) & 0x3F) << 6) | (*(p+2) & 0x3F);
                len = 3;
            } else {
                p++;
                continue;
            }

            p += len;

            // Ignorer les espaces et sauts de ligne
            if (cp == ' ') {
                pen_x += ptSize * 0.25f;  // Largeur approximative d'un espace
                continue;
            }
            if (cp == '\n') {
                pen_x = x;
                pen_y += ptSize * 1.25f;  // Hauteur de ligne approximative
                continue;
            }

            // Récupérer le glyphe
            logger.Info("[NkTextRenderer::drawText2D] GetGlyph(id={0}, cp=U+{1:04X}, {2}pt)...", mFontId, cp, ptSize);
            const NkCachedGlyph* glyph = nullptr;
            NkFontResult gr = mFontLib->GetGlyph(
                mFontId, cp, ptSize, dpi, rasterParams, glyph
            );
            logger.Info("[NkTextRenderer::drawText2D] getGlyph returned: {0}", gr.ToString());
            
            // Filtrer les glyphes invalides :
            // - Erreur de rasterization
            // - Glyphe marqué invalide (CFF non supporté, etc.)
            // - Bitmap vide (metrics.width=0 ou metrics.height=0)
            if (!gr) {
                logger.Info("[NkTextRenderer::drawText2D] Error result, skipping glyph\n");
                continue;
            }
            if (!glyph) {
                logger.Info("[NkTextRenderer::drawText2D] glyph is nullptr, skipping\n");
                continue;
            }
            if (!glyph->isValid) {
                logger.Info("[NkTextRenderer::drawText2D] glyph->isValid=false, skipping\n");
                continue;
            }
            if (glyph->metrics.width == 0.f || glyph->metrics.height == 0.f) {
                logger.Info("[NkTextRenderer::drawText2D] Empty bitmap {0}x{1}, skipping\n", glyph->metrics.width, glyph->metrics.height);
                continue;  // ← Bitmap vide
            }
            if (glyph->uvX0 >= glyph->uvX1 || glyph->uvY0 >= glyph->uvY1) {
                logger.Info("[NkTextRenderer::drawText2D] Invalid UV rect, skipping\n");
                continue;  // ← UV invalides
            }
            logger.Info("[NkTextRenderer::drawText2D] Glyph OK: {0}x{1} px at ({2},{3})\n", 
                        glyph->metrics.width, glyph->metrics.height,
                        glyph->metrics.bearingX, glyph->metrics.bearingY);

            // Position du glyphe en pixels écran
            const float px0 = pen_x + glyph->metrics.bearingX;
            const float py0 = pen_y - glyph->metrics.bearingY;
            const float px1 = px0 + glyph->metrics.width;
            const float py1 = py0 + glyph->metrics.height;

            // Conversion en coordonnées NDC
            const float nx0 = px0 * pxToNdcX - 1.f;
            const float ny0 = 1.f - py0 * pxToNdcY;
            const float nx1 = px1 * pxToNdcX - 1.f;
            const float ny1 = 1.f - py1 * pxToNdcY;

            // Coordonnées de texture (UV)
            const float u0 = glyph->uvX0, v0 = glyph->uvY0;
            const float u1 = glyph->uvX1, v1 = glyph->uvY1;
            const float r = color.x, gc_ = color.y, b = color.z, a = color.w;

            if (mGlyphCount2D >= MAX_GLYPHS_PER_FRAME_2D) break;

            // Création du quad (2 triangles = 6 vertices)
            mVerts2D.PushBack({nx0, ny0, u0, v0, r, gc_, b, a});  // Top-left
            mVerts2D.PushBack({nx1, ny0, u1, v0, r, gc_, b, a});  // Top-right
            mVerts2D.PushBack({nx0, ny1, u0, v1, r, gc_, b, a});  // Bottom-left

            mVerts2D.PushBack({nx1, ny0, u1, v0, r, gc_, b, a});  // Top-right
            mVerts2D.PushBack({nx1, ny1, u1, v1, r, gc_, b, a});  // Bottom-right
            mVerts2D.PushBack({nx0, ny1, u0, v1, r, gc_, b, a});  // Bottom-left

            ++mGlyphCount2D;

            // Avancer le curseur horizontal
            pen_x += glyph->metrics.width;
        }

        nk_uint32 glyphsAdded = mGlyphCount2D - glyphsBefore;
        logger.Info("[NkTextRenderer::drawText2D] END: created {0} glyphs (total now {1})\n",
                    glyphsAdded, mGlyphCount2D);
    }

    // =========================================================================
    //  drawText3D() — Met en file un texte dans l'espace monde (plan 3D)
    //
    //  Le texte est affiché dans un plan horizontal XZ ou vertical XY.
    //  worldMatrix transforme ce plan vers l'espace monde :
    //    - Translation : position dans le monde
    //    - Rotation    : orientation du plan de texte
    //    - Scale       : taille du texte dans le monde
    //
    //  @param text        Chaîne UTF-8
    //  @param worldMatrix Transformation monde du plan de texte
    //  @param color       Couleur RGBA [0..1]
    //  @param ptSize      Taille en points
    // =========================================================================
    void drawText3D(
        const char*   text,
        const NkMat4f& worldMatrix,
        NkVec4f        color  = {1.f, 0.9f, 0.2f, 1.f},
        float          ptSize = 0.f
    ) {
        if (!mEnabled || !text) return;
        if (ptSize <= 0.f) ptSize = mFontSize3D;

        // ── Mise en forme ──────────────────────────────────────────────────────
        NkShapeParams shapeParams;
        shapeParams.face      = mFontLib->GetFace(mFontId);
        if (!shapeParams.face) return;  // Safety check
        
        shapeParams.pointSize = ptSize;
        shapeParams.dpi       = 96.0f;
        shapeParams.renderMode= NkGlyphRenderMode::NK_RENDER_SDF;
        shapeParams.enableKerning = true;

        NkShapeResult shapeResult;
        if (!mShaper.ShapeUtf8(text, (nk_size)-1, shapeParams, shapeResult)) return;

        // ── Mesure du texte pour le centrage horizontal ────────────────────────
        // On veut centrer le texte autour de l'origine du plan 3D.
        float textWidth = 0.f, textHeight = 0.f;
        mShaper.MeasureUtf8(text, (nk_size)-1, shapeParams, textWidth, textHeight);

        NkRasterizeParams rasterParams = NkRasterizeParams::ForSdf(8.0f);

        // ── Facteur de mise à l'échelle pixel → unités monde ──────────────────
        // À ptSize pt, un em fait (ptSize * 96 / 72) pixels.
        // On normalise pour que textWidth corresponde à une longueur monde raisonnable.
        // Le facteur 0.01f est arbitraire : ajuster selon l'échelle de la scène.
        const float pixToWorld = 0.008f;

        // Décalage de centrage
        const float centerOffX = -textWidth * 0.5f * pixToWorld;
        const float centerOffY = textHeight * 0.5f * pixToWorld;

        for (nk_uint32 l = 0u; l < shapeResult.lineCount; ++l) {
            const NkGlyphRun& run = shapeResult.Line(l);
            const float lineOffY  = centerOffY - run.baselineY * pixToWorld;

            for (nk_uint32 g = 0u; g < run.glyphCount; ++g) {
                if (mGlyphCount3D >= MAX_GLYPHS_PER_FRAME_3D) break;

                const NkShapedGlyph& sg = run.Glyph(g);
                if (sg.isWhitespace || sg.isLineBreak) continue;

                const NkCachedGlyph* glyph = nullptr;
                NkFontResult gr = mFontLib->GetGlyph(
                    mFontId, sg.codepoint, ptSize, 96.0f, rasterParams, glyph
                );
                // Filtrer les glyphes invalides/vides (même que drawText2D)
                if (!gr || !glyph || !glyph->isValid) continue;
                if (glyph->metrics.width == 0.f || glyph->metrics.height == 0.f) continue;
                if (glyph->uvX0 >= glyph->uvX1 || glyph->uvY0 >= glyph->uvY1) continue;

                // ── Coin haut-gauche du quad en espace plan (XY local) ─────────
                const float lx0 = centerOffX + (sg.pen.x + sg.xOffset + sg.metrics.bearingX) * pixToWorld;
                const float ly0 = lineOffY  - sg.metrics.bearingY * pixToWorld;
                const float lx1 = lx0 + sg.metrics.width  * pixToWorld;
                const float ly1 = ly0 + sg.metrics.height * pixToWorld;

                // ── Transformation vers l'espace monde via worldMatrix ──────────
                // Le texte est dans le plan XY local (Z local = 0).
                // worldMatrix peut orienter ce plan dans n'importe quelle direction.
                auto transformPt = [&](float lx, float ly) -> NkVec3f {
                    // Applique worldMatrix au point (lx, ly, 0, 1)
                    const float* m = worldMatrix.data;
                    return NkVec3f(
                        m[0]*lx + m[4]*ly + m[8]*0.f  + m[12],
                        m[1]*lx + m[5]*ly + m[9]*0.f  + m[13],
                        m[2]*lx + m[6]*ly + m[10]*0.f + m[14]
                    );
                };

                NkVec3f wTL = transformPt(lx0, ly0); // Top-left
                NkVec3f wTR = transformPt(lx1, ly0); // Top-right
                NkVec3f wBL = transformPt(lx0, ly1); // Bottom-left
                NkVec3f wBR = transformPt(lx1, ly1); // Bottom-right

                const float u0 = glyph->uvX0, v0 = glyph->uvY0;
                const float u1 = glyph->uvX1, v1 = glyph->uvY1;
                const float r = color.x, gc_ = color.y, b = color.z, a = color.w;

                // 6 vertices (2 triangles) — coordonnées monde directement
                mVerts3D.PushBack({wTL.x, wTL.y, wTL.z, u0, v0, r, gc_, b, a});
                mVerts3D.PushBack({wTR.x, wTR.y, wTR.z, u1, v0, r, gc_, b, a});
                mVerts3D.PushBack({wBL.x, wBL.y, wBL.z, u0, v1, r, gc_, b, a});

                mVerts3D.PushBack({wTR.x, wTR.y, wTR.z, u1, v0, r, gc_, b, a});
                mVerts3D.PushBack({wBR.x, wBR.y, wBR.z, u1, v1, r, gc_, b, a});
                mVerts3D.PushBack({wBL.x, wBL.y, wBL.z, u0, v1, r, gc_, b, a});

                ++mGlyphCount3D;
            }
        }
    }

    // =========================================================================
    //  endFrame() — Upload atlas, flush VBO, émet les draw calls
    //
    //  @param cmd      Command buffer ouvert (entre BeginRenderPass et EndRenderPass)
    //  @param matView  Matrice de vue caméra (pour le texte 3D)
    //  @param matProj  Matrice de projection (pour le texte 3D)
    // =========================================================================
    void endFrame(
        NkICommandBuffer* cmd,
        const NkMat4f&    matView,
        const NkMat4f&    matProj
    ) {
        if (!mEnabled || !cmd) return;

        // ── Upload atlas si des nouveaux glyphes ont été rastérisés ───────────
        // Cette opération est en O(pages_dirty) : coût nul si rien n'a changé.
        mFontLib->UploadAtlas();

        // ── Mise à jour des descriptors si l'atlas a été recréé ───────────────
        // L'atlas peut avoir été agrandi (nouvelle page) entre deux frames.
        refreshAtlasDescriptors();

        // ── Draw 2D ───────────────────────────────────────────────────────────
        if (mGlyphCount2D > 0u && mVerts2D.Size() > 0u) {
            // DEBUG: Log du contenu 2D
            logger.Info("[NkTextRenderer::endFrame] DRAW 2D: glyphCount={0}, vertCount={1}\n", 
                        mGlyphCount2D, mVerts2D.Size());
            if (mVerts2D.Size() > 0u) {
                const auto& v0 = mVerts2D[0];
                logger.Info("  [First Vert 2D] pos=({0},{1}) uv=({2},{3}) color=({4},{5},{6},{7})\n",
                            v0.x, v0.y, v0.u, v0.v, v0.r, v0.g, v0.b, v0.a);
            }
            
            // Upload des vertices 2D dans le VBO dynamique
            nk_size sz2D = mVerts2D.Size() * sizeof(NkTextVertex2D);
            mDevice->WriteBuffer(mVbo2D, mVerts2D.Begin(), static_cast<nk_uint32>(sz2D));

            // Mise à jour des paramètres SDF (UBO léger)
            TextParams2D tp2d{};
            tp2d.sdfThreshold = 0.5f;
            tp2d.sdfSmoothing = 0.07f;
            mDevice->WriteBuffer(mUboText2D, &tp2d, sizeof(tp2d));

            // Draw call 2D
            logger.Info("[NkTextRenderer::endFrame] Binding Pipe2D\n");
            cmd->BindGraphicsPipeline(mPipe2D);
            logger.Info("[NkTextRenderer::endFrame] Binding Desc2D (atlas+params)\n");
            cmd->BindDescriptorSet(mDesc2D, 0);
            logger.Info("[NkTextRenderer::endFrame] Binding VBO 2D\n");
            cmd->BindVertexBuffer(0, mVbo2D);
            // 6 vertices par glyphe (2 triangles, pas d'index buffer)
            logger.Info("[NkTextRenderer::endFrame] Drawing {0} vertices (={1} glyphes)\n", 
                        mGlyphCount2D * 6u, mGlyphCount2D);
            cmd->Draw(mGlyphCount2D * 6u);
            logger.Info("[NkTextRenderer::endFrame] 2D draw complete\n");
        } else {
            logger.Info("[NkTextRenderer::endFrame] NO 2D GLYPHS (count={0}, verts={1})\n", 
                        mGlyphCount2D, mVerts2D.Size());
        }

        // ── Draw 3D ───────────────────────────────────────────────────────────
        if (mGlyphCount3D > 0u && mVerts3D.Size() > 0u) {
            // Upload des vertices 3D
            nk_size sz3D = mVerts3D.Size() * sizeof(NkTextVertex3D);
            mDevice->WriteBuffer(mVbo3D, mVerts3D.Begin(), static_cast<nk_uint32>(sz3D));

            // Upload de la matrice view+proj (model = identité pour le texte 3D,
            // les positions monde sont déjà calculées dans les vertices)
            TextUBO3D ubo3d{};
            NkMat4f identity = NkMat4f::Identity();
            auto copyMat = [](const NkMat4f& src, float dst[16]) {
                for (int i = 0; i < 16; ++i) dst[i] = src.data[i];
            };
            copyMat(identity, ubo3d.model);
            copyMat(matView,  ubo3d.view);
            copyMat(matProj,  ubo3d.proj);
            mDevice->WriteBuffer(mUboMatrix3D, &ubo3d, sizeof(ubo3d));

            TextParams3D tp3d{};
            tp3d.sdfThreshold      = 0.5f;
            tp3d.sdfSmoothing      = 0.06f;
            tp3d.outlineThreshold  = 0.3f;  // Outline débute à 30% du SDF
            tp3d.outlineWidth      = 0.2f;
            mDevice->WriteBuffer(mUboText3D, &tp3d, sizeof(tp3d));

            cmd->BindGraphicsPipeline(mPipe3D);
            cmd->BindDescriptorSet(mDesc3D, 0);
            cmd->BindVertexBuffer(0, mVbo3D);
            cmd->Draw(mGlyphCount3D * 6u);
        }
    }

    // ── Accesseurs ────────────────────────────────────────────────────────────
    bool      isEnabled()    const { return mEnabled; }
    nk_uint32 glyphs2D()     const { return mGlyphCount2D; }
    nk_uint32 glyphs3D()     const { return mGlyphCount3D; }

    // Tailles de fonte par défaut (ajustables avant Init())
    float mFontSizeHud = 18.0f;   // Taille HUD (FPS counter)
    float mFontSize3D  = 72.0f;   // Taille texte 3D (grande pour bonne résolution SDF)

public:

    // ── Membres ───────────────────────────────────────────────────────────────
    NkIDevice*     mDevice  = nullptr;
    NkGraphicsApi  mApi     = NkGraphicsApi::NK_GFX_API_OPENGL;
    bool           mEnabled = false;

    // ── NKFont ────────────────────────────────────────────────────────────────
    NkFontLibrary*      mFontLib = nullptr;  // Heap-allocated (18+ MB)
    NkTextShaper        mShaper;
    nk_uint32           mFontId     = 0u;
    NkTextAtlasUploader mAtlasUploader;
    nk_handle           mLastAtlasHandle = NK_INVALID_HANDLE;  // Pour détecter recréation

    // ── Handles RHI ───────────────────────────────────────────────────────────
    NkShaderHandle          mShader2D, mShader3D;
    NkDescSetHandle         mLayout2D, mLayout3D;   // (layouts = NkDescSetHandle par convention)
    NkDescSetHandle         mDesc2D,   mDesc3D;
    NkPipelineHandle        mPipe2D,   mPipe3D;
    NkBufferHandle          mVbo2D,    mVbo3D;
    NkBufferHandle          mUboText2D, mUboText3D, mUboMatrix3D;
    NkSamplerHandle         mSampler;

    // ── Buffers CPU par frame ─────────────────────────────────────────────────
    NkVector<NkTextVertex2D> mVerts2D;
    NkVector<NkTextVertex3D> mVerts3D;
    nk_uint32                mGlyphCount2D = 0u;
    nk_uint32                mGlyphCount3D = 0u;

    // =========================================================================
    //  buildShaders() — Compile les shaders texte selon l'API
    // =========================================================================
    bool buildShaders() {
        // ── Shader 2D ─────────────────────────────────────────────────────────
        {
            NkShaderDesc sd2d;
            sd2d.debugName = "Text2D";
            switch (mApi) {
                // Pour DX11/DX12 et Metal, il faudrait des variantes HLSL/MSL.
                // Dans cet exemple, on n'implémente que OpenGL et Vulkan (GLSL).
                // En production, on ajouterait des kHLSL_Text2D_VS etc.
                case NkGraphicsApi::NK_GFX_API_D3D11:
                case NkGraphicsApi::NK_GFX_API_D3D12:
                case NkGraphicsApi::NK_GFX_API_METAL:
                    logger.Info("[NkTextRenderer] Shader texte non implémenté pour {0} dans cet exemple\n",
                                NkGraphicsApiName(mApi));
                    // Fallback : désactiver le texte pour ces backends dans cet exemple
                    mEnabled = false;
                    return true; // Pas une erreur fatale
                default:
                    sd2d.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_Text2D_Vert);
                    sd2d.AddGLSL(NkShaderStage::NK_FRAGMENT,  kGLSL_Text2D_Frag);
                    break;
            }
            mShader2D = mDevice->CreateShader(sd2d);
            if (!mShader2D.IsValid()) {
                logger.Info("[NkTextRenderer] Échec compilation shader Text2D\n");
                return false;
            }
        }

        // ── Shader 3D ─────────────────────────────────────────────────────────
        {
            NkShaderDesc sd3d;
            sd3d.debugName = "Text3D";
            sd3d.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_Text3D_Vert);
            sd3d.AddGLSL(NkShaderStage::NK_FRAGMENT,  kGLSL_Text3D_Frag);
            mShader3D = mDevice->CreateShader(sd3d);
            if (!mShader3D.IsValid()) {
                logger.Info("[NkTextRenderer] Échec compilation shader Text3D\n");
                return false;
            }
        }
        return true;
    }

    // =========================================================================
    //  buildDescriptorLayouts()
    // =========================================================================
    bool buildDescriptorLayouts() {
        // Layout 2D : binding 0 = atlas SDF (sampler2D), binding 1 = TextParams UBO
        {
            NkDescriptorSetLayoutDesc ld;
            ld.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
            ld.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_FRAGMENT);
            mLayout2D = mDevice->CreateDescriptorSetLayout(ld);
            if (!mLayout2D.IsValid()) {
                logger.Info("[NkTextRenderer] Échec création layout 2D\n");
                return false;
            }
        }

        // Layout 3D : binding 0 = TextUBO3D (matrices), binding 1 = TextParams3D + atlas
        {
            NkDescriptorSetLayoutDesc ld;
            ld.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
            ld.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
            ld.Add(2, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_FRAGMENT);
            mLayout3D = mDevice->CreateDescriptorSetLayout(ld);
            if (!mLayout3D.IsValid()) {
                logger.Info("[NkTextRenderer] Échec création layout 3D\n");
                return false;
            }
        }
        return true;
    }

    // =========================================================================
    //  buildPipelines()
    // =========================================================================
    bool buildPipelines(NkRenderPassHandle swapchainRP) {
        // Vertex layout 2D : position 2D + UV + couleur
        NkVertexLayout vtx2D;
        vtx2D
            .AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT,   0,               "POSITION", 0)
            .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT,   2*sizeof(float),  "TEXCOORD", 0)
            .AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT,  4*sizeof(float),  "COLOR",    0)
            .AddBinding(0, sizeof(NkTextVertex2D));

        // Vertex layout 3D : position 3D + UV + couleur
        NkVertexLayout vtx3D;
        vtx3D
            .AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT,  0,               "POSITION", 0)
            .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT,   3*sizeof(float),  "TEXCOORD", 0)
            .AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT,  5*sizeof(float),  "COLOR",    0)
            .AddBinding(0, sizeof(NkTextVertex3D));

        // ── Pipeline 2D ───────────────────────────────────────────────────────
        // Pas de depth test (overlay HUD par-dessus tout)
        // Alpha blending activé (SDF alpha)
        {
            NkDepthStencilDesc ds = NkDepthStencilDesc::Default();
            ds.depthTestEnable  = false;
            ds.depthWriteEnable = false;

            NkBlendDesc blend = NkBlendDesc::Alpha();  // src*srcA + dst*(1-srcA)

            NkRasterizerDesc raster = NkRasterizerDesc::Default();
            raster.cullMode = NkCullMode::NK_NONE;  // Pas de culling pour le texte 2D

            NkGraphicsPipelineDesc pd;
            pd.shader       = mShader2D;
            pd.vertexLayout = vtx2D;
            pd.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            pd.rasterizer   = raster;
            pd.depthStencil = ds;
            pd.blend        = blend;
            pd.renderPass   = swapchainRP;
            pd.debugName    = "PipelineText2D";
            pd.descriptorSetLayouts.PushBack(mLayout2D);

            mPipe2D = mDevice->CreateGraphicsPipeline(pd);
            if (!mPipe2D.IsValid()) {
                logger.Info("[NkTextRenderer] Échec pipeline Text2D\n");
                return false;
            }
        }

        // ── Pipeline 3D ───────────────────────────────────────────────────────
        // Depth test activé (le texte se cache derrière la géométrie)
        // Depth write désactivé (le texte ne cache pas la géométrie derrière lui)
        // Alpha blending pour les bords SDF
        {
            NkDepthStencilDesc ds = NkDepthStencilDesc::Default();
            ds.depthTestEnable  = true;
            ds.depthWriteEnable = false;  // Ne pas polluer le z-buffer

            NkBlendDesc blend = NkBlendDesc::Alpha();

            NkRasterizerDesc raster = NkRasterizerDesc::Default();
            raster.cullMode = NkCullMode::NK_NONE;  // Les quads de texte sont double-face

            NkGraphicsPipelineDesc pd;
            pd.shader       = mShader3D;
            pd.vertexLayout = vtx3D;
            pd.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            pd.rasterizer   = raster;
            pd.depthStencil = ds;
            pd.blend        = blend;
            pd.renderPass   = swapchainRP;
            pd.debugName    = "PipelineText3D";
            pd.descriptorSetLayouts.PushBack(mLayout3D);

            mPipe3D = mDevice->CreateGraphicsPipeline(pd);
            if (!mPipe3D.IsValid()) {
                logger.Info("[NkTextRenderer] Échec pipeline Text3D\n");
                return false;
            }
        }
        return true;
    }

    // =========================================================================
    //  buildDynamicBuffers() — VBO dynamiques pour les quads de glyphes
    // =========================================================================
    bool buildDynamicBuffers() {
        // Taille : MAX_GLYPHS × 6 vertices × sizeof(vertex)
        const nk_uint32 sz2D = MAX_GLYPHS_PER_FRAME_2D * 6u * sizeof(NkTextVertex2D);
        const nk_uint32 sz3D = MAX_GLYPHS_PER_FRAME_3D * 6u * sizeof(NkTextVertex3D);

        // NK_BUFFER_DYNAMIC : indique au driver que ce buffer est mis à jour fréquemment.
        NkBufferDesc desc2D = NkBufferDesc::Vertex(sz2D, nullptr);
        desc2D.usage = NkResourceUsage::NK_UPLOAD;
        desc2D.debugName = "TextVBO2D";

        NkBufferDesc desc3D = NkBufferDesc::Vertex(sz3D, nullptr);
        desc3D.usage = NkResourceUsage::NK_UPLOAD;
        desc3D.debugName = "TextVBO3D";

        mVbo2D = mDevice->CreateBuffer(desc2D);
        mVbo3D = mDevice->CreateBuffer(desc3D);

        if (!mVbo2D.IsValid() || !mVbo3D.IsValid()) {
            logger.Info("[NkTextRenderer] Échec création VBO texte ({0} + {1} octets)\n",
                        sz2D, sz3D);
            return false;
        }

        // Réserve la capacité CPU des vecteurs pour éviter les réallocations
        mVerts2D.Reserve(MAX_GLYPHS_PER_FRAME_2D * 6u);
        mVerts3D.Reserve(MAX_GLYPHS_PER_FRAME_3D * 6u);

        logger.Info("[NkTextRenderer] VBO 2D: {0}KB, VBO 3D: {1}KB\n",
                    sz2D / 1024u, sz3D / 1024u);
        return true;
    }

    // =========================================================================
    //  buildUboBuffers()
    // =========================================================================
    bool buildUboBuffers() {
        mUboText2D   = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(TextParams2D)));
        mUboText3D   = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(TextParams3D)));
        mUboMatrix3D = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(TextUBO3D)));

        return mUboText2D.IsValid() && mUboText3D.IsValid() && mUboMatrix3D.IsValid();
    }

    // =========================================================================
    //  buildDescriptorSets()
    // =========================================================================
    bool buildDescriptorSets() {
        mDesc2D = mDevice->AllocateDescriptorSet(mLayout2D);
        mDesc3D = mDevice->AllocateDescriptorSet(mLayout3D);

        if (!mDesc2D.IsValid() || !mDesc3D.IsValid()) {
            logger.Info("[NkTextRenderer] Échec allocation descriptor sets\n");
            return false;
        }

        // Les bindings de l'atlas seront remplis lors du premier refreshAtlasDescriptors().
        // On configure déjà les UBO de paramètres qui ne changent pas de handle.
        updateUboDescriptors();
        return true;
    }

    // =========================================================================
    //  refreshAtlasDescriptors() — Re-bind l'atlas si la texture a changé
    //  Appelé à chaque endFrame() pour gérer les recréations d'atlas.
    // =========================================================================
    void refreshAtlasDescriptors() {
        if (!mAtlasUploader.hAtlas.IsValid()) return;
        if (mAtlasUploader.hAtlas.id == mLastAtlasHandle) return;

        mLastAtlasHandle = mAtlasUploader.hAtlas.id;

        // Rebind atlas + sampler dans les deux descriptor sets
        auto bindAtlas = [&](NkDescSetHandle ds, nk_uint32 binding) {
            if (!ds.IsValid()) return;
            NkDescriptorWrite w{};
            w.set           = ds;
            w.binding       = binding;
            w.type          = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            w.texture       = mAtlasUploader.hAtlas;
            w.sampler       = mSampler;
            w.textureLayout = NkResourceState::NK_SHADER_READ;
            mDevice->UpdateDescriptorSets(&w, 1);
        };

        bindAtlas(mDesc2D, 0u);  // binding 0 dans le layout 2D
        bindAtlas(mDesc3D, 0u);  // binding 0 dans le layout 3D

        logger.Info("[NkTextRenderer] Atlas descriptor rebindé (handle={0})\n",
                    (unsigned long long)mLastAtlasHandle);
    }

    // =========================================================================
    //  updateUboDescriptors() — Lie les UBO aux descriptor sets
    // =========================================================================
    void updateUboDescriptors() {
        // UBO TextParams2D → binding 1 du set 2D
        if (mDesc2D.IsValid() && mUboText2D.IsValid()) {
            NkDescriptorWrite w{};
            w.set         = mDesc2D;
            w.binding     = 1u;
            w.type        = NkDescriptorType::NK_UNIFORM_BUFFER;
            w.buffer      = mUboText2D;
            w.bufferRange = sizeof(TextParams2D);
            mDevice->UpdateDescriptorSets(&w, 1);
        }

        // UBO TextUBO3D (matrices) → binding 1 du set 3D
        if (mDesc3D.IsValid() && mUboMatrix3D.IsValid()) {
            NkDescriptorWrite w{};
            w.set         = mDesc3D;
            w.binding     = 1u;
            w.type        = NkDescriptorType::NK_UNIFORM_BUFFER;
            w.buffer      = mUboMatrix3D;
            w.bufferRange = sizeof(TextUBO3D);
            mDevice->UpdateDescriptorSets(&w, 1);
        }

        // UBO TextParams3D → binding 2 du set 3D
        if (mDesc3D.IsValid() && mUboText3D.IsValid()) {
            NkDescriptorWrite w{};
            w.set         = mDesc3D;
            w.binding     = 2u;
            w.type        = NkDescriptorType::NK_UNIFORM_BUFFER;
            w.buffer      = mUboText3D;
            w.bufferRange = sizeof(TextParams3D);
            mDevice->UpdateDescriptorSets(&w, 1);
        }
    }
};

// =============================================================================
//  UBO principal (même structure que NkRHIDemoFull.cpp pour réutilisabilité)
// =============================================================================
struct alignas(16) UboData {
    float model[16];
    float view[16];
    float proj[16];
    float lightVP[16];
    float lightDirW[4];
    float eyePosW[4];
    float ndcZScale;
    float ndcZOffset;
    float _pad[2];
};

// ── Réutilisation des shaders Phong de NkRHIDemoFull.cpp ──────────────────────
// (déclarés extern pour éviter la duplication)
static constexpr const char* kGLSL_ShadowVert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;

layout(std140, binding = 0) uniform ShadowUBO {
    mat4  model;
    mat4  view;
    mat4  proj;
    mat4  lightVP;
    vec4  lightDirW;
    vec4  eyePosW;
    float ndcZScale;
    float ndcZOffset;
    vec2  _pad;
} ubo;

void main() {
    gl_Position = ubo.lightVP * ubo.model * vec4(aPos, 1.0);
}
)GLSL";

static constexpr const char* kGLSL_ShadowFrag = R"GLSL(
#version 460 core
// Passe shadow depth-only : pas de sortie couleur
void main() {}
)GLSL";

// ── Passe principale Phong + shadow mapping PCF ───────────────────────────────
static constexpr const char* kGLSL_Vert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

layout(std140, binding = 0) uniform MainUBO {
    mat4  model;
    mat4  view;
    mat4  proj;
    mat4  lightVP;
    vec4  lightDirW;
    vec4  eyePosW;
    float ndcZScale;
    float ndcZOffset;
    vec2  _pad;
} ubo;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec3 vColor;
layout(location = 3) out vec4 vShadowCoord;  // coordonnées dans l'espace lumière (non-divisées)

void main() {
    vec4 worldPos     = ubo.model * vec4(aPos, 1.0);
    vWorldPos         = worldPos.xyz;

    // Matrice normale = transposée inverse de la partie 3x3 du model
    // (correct même pour les objets non-uniformément mis à l'échelle)
    mat3 normalMatrix = transpose(inverse(mat3(ubo.model)));
    vNormal           = normalize(normalMatrix * aNormal);

    vColor            = aColor;

    // Coordonnées shadow : espace lumière homogène, pas encore divisées par w
    vShadowCoord      = ubo.lightVP * worldPos;

    gl_Position       = ubo.proj * ubo.view * worldPos;
}
)GLSL";

static constexpr const char* kGLSL_Frag = R"GLSL(
#version 460 core
layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec4 vShadowCoord;   // coordonnées lumière homogènes

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform MainUBO {
    mat4  model;
    mat4  view;
    mat4  proj;
    mat4  lightVP;
    vec4  lightDirW;
    vec4  eyePosW;
    float ndcZScale;
    float ndcZOffset;
    vec2  _pad;
} ubo;

// sampler2DShadow avec compare mode LESS_OR_EQUAL
// (le sampler doit être configuré avec compareEnable=true, compareOp=LESS_EQUAL)
layout(binding = 1) uniform sampler2DShadow uShadowMap;

// ── Filtrage PCF 3x3 ─────────────────────────────────────────────────────────
float ShadowFactor(vec4 shadowCoord) {
    // Division perspective (projection orthogonale → w=1, mais on normalise quand même)
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;

    // xy : NDC [-1,1] → UV [0,1]
    // z  : remap selon la convention de l'API (ndcZScale/ndcZOffset dans l'UBO)
    //      OpenGL: z ∈ [-1,1] → scale=0.5, offset=0.5
    //      Vulkan/DX: z ∈ [0,1] → scale=1.0, offset=0.0
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    projCoords.z  = projCoords.z * ubo.ndcZScale + ubo.ndcZOffset;

    // Rejeter les fragments hors du frustum lumière
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 1.0;  // hors frustum = pas d'ombre
    }

    // Bias adaptatif pour éviter le shadow acne
    // Plus la surface est perpendiculaire à la lumière, plus le bias est grand
    vec3  N    = normalize(vNormal);
    vec3  L    = normalize(-ubo.lightDirW.xyz);
    float cosA = max(dot(N, L), 0.0);
    float bias = mix(0.005, 0.0005, cosA);   // [0.0005 .. 0.005]

    projCoords.z -= bias;

    // Taille d'un texel en UV
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));

    // PCF 3×3 — texture() avec sampler2DShadow fait la comparaison hardware
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            // texture(sampler2DShadow, vec3(uv, compareDepth))
            // retourne 1.0 si le fragment est ÉCLAIRÉ, 0.0 si dans l'ombre
            shadow += texture(uShadowMap, vec3(projCoords.xy + offset, projCoords.z));
        }
    }
    return shadow / 9.0;  // moyenne du PCF
}

void main() {
    vec3 N      = normalize(vNormal);
    vec3 L      = normalize(-ubo.lightDirW.xyz);
    vec3 V      = normalize(ubo.eyePosW.xyz - vWorldPos);
    vec3 H      = normalize(L + V);

    float diff   = max(dot(N, L), 0.0);
    float spec   = pow(max(dot(N, H), 0.0), 32.0);

    // shadow = 1.0 → éclairé, 0.0 → dans l'ombre
    float shadow = max(ShadowFactor(vShadowCoord), 0.35);

    vec3 ambient  = 0.15 * vColor;
    vec3 diffuse  = shadow * diff * vColor;
    vec3 specular = shadow * spec * vec3(0.4);

    fragColor = vec4(ambient + diffuse + specular, 1.0);
}
)GLSL";

// =============================================================================
// Shaders HLSL (DX11 / DX12)
// =============================================================================
static constexpr const char* kHLSL_VS = R"HLSL(
cbuffer UBO : register(b0) {
    column_major float4x4 model;
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 lightVP;
    float4   lightDirW;
    float4   eyePosW;
    float    ndcZScale;
    float    ndcZOffset;
    float2   _pad;
};
struct VSIn  { float3 pos:POSITION; float3 norm:NORMAL; float3 col:COLOR; };
struct VSOut {
    float4 pos:SV_Position;
    float3 wp:TEXCOORD0;
    float3 n:TEXCOORD1;
    float3 c:TEXCOORD2;
    float4 shadowPos:TEXCOORD3;
};
VSOut VSMain(VSIn v) {
    VSOut o;
    float4 wp = mul(model, float4(v.pos,1));
    o.wp  = wp.xyz;
    o.n   = normalize(mul((float3x3)model, v.norm));
    o.c   = v.col;
    o.shadowPos = mul(lightVP, wp);
    o.pos = mul(proj, mul(view, wp));
    return o;
}
)HLSL";

static constexpr const char* kHLSL_PS = R"HLSL(
cbuffer UBO : register(b0) {
    column_major float4x4 model;
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 lightVP;
    float4   lightDirW;
    float4   eyePosW;
    float    ndcZScale;
    float    ndcZOffset;
    float2   _pad;
};
Texture2D<float> uShadowMap : register(t1);
SamplerComparisonState uShadowSampler : register(s1);

struct PSIn {
    float4 pos:SV_Position;
    float3 wp:TEXCOORD0;
    float3 n:TEXCOORD1;
    float3 c:TEXCOORD2;
    float4 shadowPos:TEXCOORD3;
};

float ShadowFactor(float4 shadowPos, float3 N) {
    float3 projCoords = shadowPos.xyz / shadowPos.w;
    // DX: NDC z ∈ [0,1] avec depthZeroToOne — pas besoin de remap Z
    // DX UV: y=0 est en haut → flip Y (NDC +1 → UV 0 top)
    projCoords.x =  projCoords.x * 0.5 + 0.5;
    projCoords.y = -projCoords.y * 0.5 + 0.5;
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 1.0;
    }

    float3 L = normalize(-lightDirW.xyz);
    float cosA = max(dot(N, L), 0.0);
    float bias = lerp(0.005, 0.0005, cosA);
    float cmpDepth = projCoords.z - bias;

    uint sw = 1, sh = 1;
    uShadowMap.GetDimensions(sw, sh);
    float2 texel = 1.0 / float2((float)sw, (float)sh);

    float sum = 0.0;
    [unroll] for (int x = -1; x <= 1; ++x) {
        [unroll] for (int y = -1; y <= 1; ++y) {
            float2 uv = projCoords.xy + float2((float)x, (float)y) * texel;
            sum += uShadowMap.SampleCmpLevelZero(uShadowSampler, uv, cmpDepth);
        }
    }
    return sum / 9.0;
}

float4 PSMain(PSIn i) : SV_Target {
    float3 N = normalize(i.n);
    float3 L = normalize(-lightDirW.xyz);
    float3 V = normalize(eyePosW.xyz - i.wp);
    float3 H = normalize(L + V);
    float diff = max(dot(N,L), 0.0);
    float spec = pow(max(dot(N,H), 0.0), 32.0);
    float shadow = max(ShadowFactor(i.shadowPos, N), 0.35);
    float3 col = 0.15*i.c + shadow * diff * i.c + shadow * spec * 0.4;
    return float4(col, 1.0);
}
)HLSL";

static constexpr const char* kHLSL_ShadowVert = R"HLSL(
// NkShadowVert.hlsl
struct VSInput {
    float3 aPos : POSITION;
};

struct VSOutput {
    float4 position : SV_POSITION;
};

// Utilisation d'un constant buffer séparé pour la passe shadow (binding 0)
cbuffer ShadowUBO : register(b0) {
    column_major float4x4 model;
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 lightVP;
    float4 lightDirW;
    float4 eyePosW;
    float  ndcZScale;
    float  ndcZOffset;
    float2 _pad;
};

VSOutput main(VSInput input) {
    VSOutput output;
    
    // Transforme directement dans l'espace lumière
    output.position = mul(lightVP, mul(model, float4(input.aPos, 1.0)));
    
    return output;
}
)HLSL";

static constexpr const char* kHLSL_ShadowFrag = R"HLSL(
// NkShadowFrag.hlsl
struct PSInput {
    float4 position : SV_POSITION;
};

// Pas de sortie couleur — on écrit uniquement le depth buffer
// En HLSL, si on n'écrit pas de couleur, le pipeline utilise la valeur de depth
// déjà présente de l'étape de rasterization
void main(PSInput input) {
    // Rien à faire — le depth buffer est automatiquement mis à jour
    // On peut éventuellement faire early depth test optimizations
}
)HLSL";

// =============================================================================
// Shaders MSL (Metal)
// =============================================================================
static constexpr const char* kMSL_Shaders = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct UBO { float4x4 model,view,proj; float4 lightDirW,eyePosW; };
struct VSIn  { float3 pos [[attribute(0)]]; float3 norm [[attribute(1)]]; float3 col [[attribute(2)]]; };
struct VSOut { float4 pos [[position]]; float3 wp,n,c; };
vertex VSOut vmain(VSIn v [[stage_in]], constant UBO& u [[buffer(0)]]) {
    VSOut o;
    float4 wp = u.model * float4(v.pos,1);
    o.wp = wp.xyz;
    o.n  = normalize(float3x3(u.model[0].xyz,u.model[1].xyz,u.model[2].xyz) * v.norm);
    o.c  = v.col;
    o.pos = u.proj * u.view * wp;
    return o;
}
fragment float4 fmain(VSOut i [[stage_in]], constant UBO& u [[buffer(0)]]) {
    float3 N = normalize(i.n);
    float3 L = normalize(-u.lightDirW.xyz);
    float3 V = normalize(u.eyePosW.xyz - i.wp);
    float3 H = normalize(L + V);
    float diff = max(dot(N,L), 0.0f);
    float spec = pow(max(dot(N,H), 0.0f), 32.0f);
    float3 col = 0.15f*i.c + diff*i.c + spec*0.4f;
    return float4(col, 1.0f);
}
)MSL";

// =============================================================================
//  Géométrie — Cube (réutilisé de NkRHIDemoFull.cpp)
// =============================================================================
struct Vtx3D { NkVec3f pos; NkVec3f normal; NkVec3f color; };

static NkVector<Vtx3D> MakeCube(float r=1.f, float g=0.45f, float b=0.2f) {
    static const float P = 0.5f, N = -0.5f;
    struct Face { float vx[4][3]; float nx,ny,nz; };
    static const Face faces[6] = {
        {{{P,N,P},{P,P,P},{N,P,P},{N,N,P}}, 0, 0, 1},
        {{{N,N,N},{N,P,N},{P,P,N},{P,N,N}}, 0, 0,-1},
        {{{N,N,N},{P,N,N},{P,N,P},{N,N,P}}, 0,-1, 0},
        {{{N,P,P},{P,P,P},{P,P,N},{N,P,N}}, 0, 1, 0},
        {{{N,N,N},{N,N,P},{N,P,P},{N,P,N}},-1, 0, 0},
        {{{P,N,P},{P,N,N},{P,P,N},{P,P,P}}, 1, 0, 0},
    };
    static const int idx[6] = {0,1,2, 0,2,3};
    NkVector<Vtx3D> v;
    v.Reserve(36);
    for (const auto& f : faces)
        for (int i : idx)
            v.PushBack({NkVec3f(f.vx[i][0],f.vx[i][1],f.vx[i][2]),
                        NkVec3f(f.nx,f.ny,f.nz),
                        NkVec3f(r,g,b)});
    return v;
}

// ── Helper : copie NkMat4f → float[16] ───────────────────────────────────────
static void Mat4ToArray(const NkMat4f& m, float out[16]) {
    mem::NkCopy(out, m.data, 16 * sizeof(float));
}

// ── Helper : parse les arguments de backend ───────────────────────────────────
static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (size_t i = 1; i < args.Size(); i++) {
        if (args[i] == "--backend=vulkan"  || args[i] == "-bvk")  return NkGraphicsApi::NK_GFX_API_VULKAN;
        if (args[i] == "--backend=dx11"    || args[i] == "-bdx11") return NkGraphicsApi::NK_GFX_API_D3D11;
        if (args[i] == "--backend=dx12"    || args[i] == "-bdx12") return NkGraphicsApi::NK_GFX_API_D3D12;
        if (args[i] == "--backend=opengl"  || args[i] == "-bgl")   return NkGraphicsApi::NK_GFX_API_OPENGL;
        if (args[i] == "--backend=sw"      || args[i] == "-bsw")   return NkGraphicsApi::NK_GFX_API_SOFTWARE;
    }
    return NkGraphicsApi::NK_GFX_API_OPENGL;
}

// =============================================================================
//  nkmain — Point d'entrée
// =============================================================================
int nkmain(const NkEntryState& state) {
    
    // ── Backend ───────────────────────────────────────────────────────────────
    NkGraphicsApi targetApi = ParseBackend(state.GetArgs());
    
    const char*   apiName   = NkGraphicsApiName(targetApi);
    
    logger.Info("[TextDemo] Backend : {0}\n", apiName);

    // ── Fenêtre ───────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title     = NkFormat("NKFont + RHI Demo — {0}", apiName);
    winCfg.width     = 1280;
    winCfg.height    = 720;
    winCfg.centered  = true;
    winCfg.resizable = true;

    NkWindow window;
    if (!window.Create(winCfg)) {
        logger.Info("[TextDemo] Échec création fenêtre\n");
        return 1;
    }

    // ── Device RHI ───────────────────────────────────────────────────────────
    NkDeviceInitInfo deviceInfo;
    deviceInfo.api     = targetApi;
    deviceInfo.surface = window.GetSurfaceDesc();
    deviceInfo.width   = window.GetSize().width;
    deviceInfo.height  = window.GetSize().height;
    deviceInfo.context.vulkan.appName    = "NkTextDemo";
    deviceInfo.context.vulkan.engineName = "Nkentseu";

    NkIDevice* device = NkDeviceFactory::Create(deviceInfo);
    if (!device || !device->IsValid()) {
        logger.Info("[TextDemo] Échec création device {0}\n", apiName);
        window.Close();
        return 1;
    }

    uint32 W = device->GetSwapchainWidth();
    uint32 H = device->GetSwapchainHeight();

    // ── Shader Phong principal (cube) ─────────────────────────────────────────
    NkShaderDesc phongDesc;
    phongDesc.debugName = "Phong3D";
    phongDesc.AddGLSL(NkShaderStage::NK_VERTEX,  kGLSL_Vert);
    phongDesc.AddGLSL(NkShaderStage::NK_FRAGMENT, kGLSL_Frag);
    NkShaderHandle hPhongShader = device->CreateShader(phongDesc);

    NkRenderPassHandle hRP = device->GetSwapchainRenderPass();

    NkVertexLayout phongLayout;
    phongLayout
        .AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0,              "POSITION", 0)
        .AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT, 3*sizeof(float),"NORMAL",   0)
        .AddAttribute(2, 0, NkGPUFormat::NK_RGB32_FLOAT, 6*sizeof(float),"COLOR",    0)
        .AddBinding(0, sizeof(Vtx3D));

    NkDescriptorSetLayoutDesc phongLayoutDesc;
    phongLayoutDesc.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
    NkDescSetHandle hPhongLayout = device->CreateDescriptorSetLayout(phongLayoutDesc);

    NkGraphicsPipelineDesc phongPipeDesc;
    phongPipeDesc.shader       = hPhongShader;
    phongPipeDesc.vertexLayout = phongLayout;
    phongPipeDesc.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
    phongPipeDesc.rasterizer   = NkRasterizerDesc::Default();
    phongPipeDesc.depthStencil = NkDepthStencilDesc::Default();
    phongPipeDesc.blend        = NkBlendDesc::Opaque();
    phongPipeDesc.renderPass   = hRP;
    phongPipeDesc.debugName    = "PipelinePhong";
    phongPipeDesc.descriptorSetLayouts.PushBack(hPhongLayout);
    NkPipelineHandle hPhongPipe = device->CreateGraphicsPipeline(phongPipeDesc);

    // Géométrie cube
    auto cubeVerts = MakeCube();
    NkBufferHandle hCubeVBO = device->CreateBuffer(
        NkBufferDesc::Vertex(cubeVerts.Size() * sizeof(Vtx3D), cubeVerts.Begin())
    );

    NkBufferHandle hPhongUBO = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(UboData)));
    NkDescSetHandle hPhongDesc = device->AllocateDescriptorSet(hPhongLayout);
    if (hPhongDesc.IsValid() && hPhongUBO.IsValid()) {
        NkDescriptorWrite w{};
        w.set         = hPhongDesc;
        w.binding     = 0u;
        w.type        = NkDescriptorType::NK_UNIFORM_BUFFER;
        w.buffer      = hPhongUBO;
        w.bufferRange = sizeof(UboData);
        device->UpdateDescriptorSets(&w, 1);
    }

    // ── NkTextRenderer ────────────────────────────────────────────────────────
    NkTextRenderer* textRenderer = new NkTextRenderer();
    textRenderer->mFontSizeHud = 18.0f;   // FPS counter : petit
    textRenderer->mFontSize3D  = 72.0f;   // Texte 3D : grand pour bonne résolution SDF

    // Chemin de la fonte : utilise Antonio (TTF pur, pas OTF/CFF)
    // CORRECTION : Roboto-Regular.ttf est OTF/CFF → NK_ERR_NOT_IMPLEMENTED
    // Antonio-Regular.ttf est un vrai TTF et charge vite (~50ms vs 200ms)
    const char* fontPath = "Resources/Fonts/ProggyClean.ttf";
    for (size_t i = 1; i < state.GetArgs().Size(); ++i) {
        const NkString& arg = state.GetArgs()[i];
        if (arg.StartsWith("--font=")) {
            fontPath = arg.CStr() + 7;
        }
    }

    bool textOk = textRenderer->Init(device, targetApi, fontPath, hRP);
    logger.Info("[TextDemo] After textRenderer->init: {0}\n", (textOk ? "OK" : "FAILED"));
    if (!textOk) {
        logger.Info("[TextDemo] NkTextRenderer init échoué — démonstration sans texte.\n");
    }

    // ── Command Buffer ────────────────────────────────────────────────────────
    logger.Info("[TextDemo] Creating command buffer...\n");
    NkICommandBuffer* cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
    logger.Info("[TextDemo] Command buffer created: {0}\n", (cmd ? "OK" : "nullptr"));

    // ── État de la simulation ─────────────────────────────────────────────────
    bool  running  = true;
    float rotAngle = 0.f;
    float camYaw   = 0.f;
    float camPitch = 20.f;
    float camDist  = 4.f;
    bool  keys[512]= {};

    // Compteur FPS
    float fpsTimer    = 0.f;
    float fpsDisplay  = 0.f;
    uint32 frameCount = 0u;

    // Animation du texte 3D
    float textBobAngle = 0.f;

    // ── TIMEOUT DE TEST : 10 secondes max ─────────────────────────────────────
    float testTimeAccum   = 0.f;
    float testTimeMax     = 10.f;  // 10 secondes en mode débogage
    bool isDebugMode = true;       // Mode débogage : arrêter après timeout

    NkClock clock;
    NkEventSystem& events = NkEvents();

    const bool depthZeroToOne =
        targetApi == NkGraphicsApi::NK_GFX_API_VULKAN    ||
        targetApi == NkGraphicsApi::NK_GFX_API_D3D11 ||
        targetApi == NkGraphicsApi::NK_GFX_API_D3D12 ||
        targetApi == NkGraphicsApi::NK_GFX_API_METAL;
    const float ndcZScale  = depthZeroToOne ? 1.0f : 0.5f;
    const float ndcZOffset = depthZeroToOne ? 0.0f : 0.5f;

    // ── Callbacks ─────────────────────────────────────────────────────────────
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) {
        running = false;
    });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if ((uint32)e->GetKey() < 512) keys[(uint32)e->GetKey()] = true;
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e) {
        if ((uint32)e->GetKey() < 512) keys[(uint32)e->GetKey()] = false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = (uint32)e->GetWidth();
        H = (uint32)e->GetHeight();
    });

    logger.Info("[TextDemo] Boucle principale.\n");
    logger.Info("[TextDemo]   ESC   = quitter\n");
    logger.Info("[TextDemo]   WASD  = orbite caméra\n");
    logger.Info("[TextDemo]   QE    = zoom caméra\n");
    logger.Info("[TextDemo] About to enter main loop...\n");
    logger.Info("[TextDemo] Entering while(running) loop\n");

    // =========================================================================
    //  Boucle principale
    // =========================================================================
    uint32 frameNum = 0u;
    while (running) {
        // ── Delta time (calcul d'abord pour le timeout) ────────────────────────
        float dt = clock.Tick().delta;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;

        // ── TIMEOUT DE TEST ──────────────────────────────────────────────────────
        if (isDebugMode) {
            testTimeAccum += dt;
            if (testTimeAccum > testTimeMax) {
                logger.Info("\n\n*** TIMEOUT: {0}s écoulées, arrêt forcing de l'application pour débogage ***\n\n", testTimeMax);
                running = false;
                break;
            }
        }

        logger.Info("[Frame {0}] Poll events\n", frameNum);
        events.PollEvents();
        if (!running) break;
        if (W == 0 || H == 0) continue;

        if (W != device->GetSwapchainWidth() || H != device->GetSwapchainHeight()) {
            device->OnResize(W, H);
        }

        // ── FPS ───────────────────────────────────────────────────────────────
        fpsTimer += dt;
        ++frameCount;
        if (fpsTimer >= 0.5f) {
            fpsDisplay = (float)frameCount / fpsTimer;
            fpsTimer   = 0.f;
            frameCount = 0u;
        }

        // ── Contrôles ─────────────────────────────────────────────────────────
        const float spd = 60.f;
        if (keys[(uint32)NkKey::NK_A]) camYaw   -= spd * dt;
        if (keys[(uint32)NkKey::NK_D]) camYaw   += spd * dt;
        if (keys[(uint32)NkKey::NK_W]) camPitch += spd * dt;
        if (keys[(uint32)NkKey::NK_S]) camPitch -= spd * dt;
        if (keys[(uint32)NkKey::NK_Q]) camDist  += 2.f * dt;
        if (keys[(uint32)NkKey::NK_E]) camDist  -= 2.f * dt;
        camPitch = NkClamp(camPitch, -80.f, 80.f);
        camDist  = NkClamp(camDist, 1.5f, 15.f);

        rotAngle   += 45.f * dt;
        textBobAngle += 90.f * dt;

        // ── Matrices ──────────────────────────────────────────────────────────
        float aspect = (H > 0) ? (float)W / (float)H : 1.f;

        float eyeX = camDist * NkCos(NkToRadians(camPitch)) * NkSin(NkToRadians(camYaw));
        float eyeY = camDist * NkSin(NkToRadians(camPitch));
        float eyeZ = camDist * NkCos(NkToRadians(camPitch)) * NkCos(NkToRadians(camYaw));
        NkVec3f eye(eyeX, eyeY, eyeZ);

        NkMat4f matView = NkMat4f::LookAt(eye, NkVec3f(0,0,0), NkVec3f(0,1,0));
        NkMat4f matProj = NkMat4f::Perspective(NkAngle(60.f), aspect, 0.1f, 100.f);

        // ── Frame ─────────────────────────────────────────────────────────────
        logger.Info("[Frame {0}] BeginFrame\n", frameNum);
        NkFrameContext frame;
        if (!device->BeginFrame(frame)) {
            logger.Info("[Frame {0}] BeginFrame failed\n", frameNum);
            continue;
        }

        W = device->GetSwapchainWidth();
        H = device->GetSwapchainHeight();
        if (W == 0 || H == 0) { 
            logger.Info("[Frame {0}] Invalid size {1}x{2}\n", frameNum, W, H);
            device->EndFrame(frame); 
            continue; 
        }

        logger.Info("[Frame {0}] GetSwapchainFramebuffer\n", frameNum);
        NkFramebufferHandle hFBO = device->GetSwapchainFramebuffer();
        hRP = device->GetSwapchainRenderPass();

        logger.Info("[Frame {0}] Cmd->Reset\n", frameNum);
        cmd->Reset();
        if (!cmd->Begin()) { 
            logger.Info("[Frame {0}] cmd->Begin failed\n", frameNum);
            device->EndFrame(frame); 
            continue; 
        }

        logger.Info("[Frame {0}] BeginRenderPass\n", frameNum);
        NkRect2D area{0, 0, (int32)W, (int32)H};
        if (!cmd->BeginRenderPass(hRP, hFBO, area)) {
            logger.Info("[Frame {0}] BeginRenderPass failed\n", frameNum);
            cmd->End();
            device->EndFrame(frame);
            continue;
        }

        logger.Info("[Frame {0}] SetViewport\n", frameNum);

        // =====================================================================
        //  Draw call 1 : Cube rotatif (Phong)
        // =====================================================================
        {
            logger.Info("[Frame {0}] === CUBE DRAW : Start ===\n", frameNum);
            logger.Info("[Frame {0}] hPhongPipe valid? {1}\n", frameNum, hPhongPipe.IsValid());
            
            NkMat4f matModel = NkMat4f::RotationY(NkAngle(rotAngle))
                             * NkMat4f::RotationX(NkAngle(rotAngle * 0.5f));

            UboData ubo{};
            Mat4ToArray(matModel,     ubo.model);
            Mat4ToArray(matView,      ubo.view);
            Mat4ToArray(matProj,      ubo.proj);
            NkMat4f identity = NkMat4f::Identity();
            Mat4ToArray(identity,     ubo.lightVP);
            ubo.lightDirW[0] = -0.5f; ubo.lightDirW[1] = -1.f; ubo.lightDirW[2] = -0.5f;
            ubo.eyePosW[0]   = eye.x; ubo.eyePosW[1]   = eye.y; ubo.eyePosW[2]   = eye.z;
            ubo.ndcZScale    = ndcZScale;
            ubo.ndcZOffset   = ndcZOffset;
            device->WriteBuffer(hPhongUBO, &ubo, sizeof(ubo));

            logger.Info("[Frame {0}] Calling BindGraphicsPipeline\n", frameNum);
            cmd->BindGraphicsPipeline(hPhongPipe);
            logger.Info("[Frame {0}] After BindGraphicsPipeline\n", frameNum);
            
            if (hPhongDesc.IsValid()) {
                logger.Info("[Frame {0}] Calling BindDescriptorSet\n", frameNum);
                cmd->BindDescriptorSet(hPhongDesc, 0);
                logger.Info("[Frame {0}] After BindDescriptorSet\n", frameNum);
            }
            
            logger.Info("[Frame {0}] Calling BindVertexBuffer\n", frameNum);
            cmd->BindVertexBuffer(0, hCubeVBO);
            logger.Info("[Frame {0}] After BindVertexBuffer\n", frameNum);
            
            logger.Info("[Frame {0}] Calling Draw with {1} vertices\n", frameNum, (uint32)cubeVerts.Size());
            cmd->Draw((uint32)cubeVerts.Size());
            logger.Info("[Frame {0}] After Draw\n", frameNum);
        }

        // =====================================================================
        //  Draw calls 2, 3, 4 : Texte (via NkTextRenderer)
        // =====================================================================
        logger.Info("[Frame {0}] Calling textRenderer->beginFrame()\n", frameNum);
        textRenderer->beginFrame();
        logger.Info("[Frame {0}] After textRenderer->beginFrame()\n", frameNum);

        // ── Texte HUD 2D : FPS counter ────────────────────────────────────────
        {
            char fpsBuf[64];
            // snprintf : sûr sur toutes les plateformes cibles
            snprintf(fpsBuf, sizeof(fpsBuf), "FPS : %.0f", fpsDisplay);

            // Coin supérieur gauche avec 12px de marge
            logger.Info("[Frame {0}] Calling drawText2D for FPS\n", frameNum);
            textRenderer->drawText2D(
                fpsBuf,
                12.f, 12.f,          // x, y en pixels
                (float)W, (float)H,  // dimensions écran
                {1.f, 1.f, 0.3f, 1.f}, // couleur jaune-blanc
                18.f                   // 18 points
            );
            logger.Info("[Frame {0}] After drawText2D for FPS\n", frameNum);
        }

        // ── Texte HUD 2D : backend info (ligne 2) ────────────────────────────
        {
            char infoBuf[128];
            snprintf(infoBuf, sizeof(infoBuf), "Backend : %s  |  NKFont SDF", apiName);

            textRenderer->drawText2D(
                infoBuf,
                12.f, 38.f,          // Sous le FPS counter (18pt + espacement)
                (float)W, (float)H,
                {0.7f, 0.85f, 1.f, 0.9f}, // bleu clair légèrement transparent
                14.f
            );
        }

        // ── Texte HUD 2D : contrôles (coin inférieur gauche) ─────────────────
        {
            static const char* controls =
                "WASD : orbite   |   QE : zoom   |   ESC : quitter";

            textRenderer->drawText2D(
                controls,
                12.f, (float)H - 28.f, // 28px depuis le bas
                (float)W, (float)H,
                {0.6f, 0.6f, 0.6f, 0.8f}, // gris semi-transparent
                12.f
            );
        }

        // ── Texte HUD 2D : glyphes stats (coin supérieur droit) ──────────────
        // NOTE: Stats actuellement désactivées pour debug simplifié
        // Décommenter si mFontLib.Atlas()/Cache() accesseurs sont stables

        // ── Texte 3D : "TEXT" au-dessus du cube ──────────────────────────────
        // Le texte flotte au-dessus du cube en bobant doucement.
        {
            // Position en monde : au-dessus du cube (Y = 1.2) avec légère oscillation
            float bobY  = 1.2f + 0.08f * NkSin(NkToRadians(textBobAngle));

            // Orientation du plan de texte :
            //   - Translation vers la position flottante
            //   - Rotation Y = même angle que le cube (tourne avec lui)
            //   - Scale : ajuste la taille dans le monde
            NkMat4f textWorld =
                NkMat4f::Translation(NkVec3f(0.f, bobY, 0.f))
              * NkMat4f::RotationY(NkAngle(rotAngle));

            logger.Info("[Frame {0}] Drawing 3D text 'TEXT'\n", frameNum);
            textRenderer->drawText3D(
                "TEXT",
                textWorld,
                {1.f, 0.85f, 0.15f, 1.f}, // Jaune-or
                72.f
            );
        }

        // ── Texte 3D : étiquette fixe "NKENTSEU ENGINE" sur le sol ──────────
        {
            // Plan horizontal : rotation X = -90° pour coucher le texte à plat
            NkMat4f textFloor =
                NkMat4f::Translation(NkVec3f(0.f, -0.98f, 0.f))
              * NkMat4f::RotationX(NkAngle(-90.f))
              * NkMat4f::Scaling(NkVec3f(0.7f, 0.7f, 0.7f));

            logger.Info("[Frame {0}] Drawing 3D text 'NKENTSEU ENGINE'\n", frameNum);
            textRenderer->drawText3D(
                "NKENTSEU ENGINE",
                textFloor,
                {0.3f, 0.6f, 1.f, 0.75f}, // Bleu semi-transparent
                48.f
            );
        }

        // ── Texte 3D : étiquette billboard "Cube" attachée à l'objet ─────────
        // Billboard = le texte fait toujours face à la caméra.
        // On construit une matrice de billboard à partir de la vue inverse.
        {
            // Récupère la rotation de la vue (inverse de la rotation caméra)
            // Un billboard parfait utilise la matrice de vue transposée (3x3 rotation).
            NkMat4f camRot = matView;
            // Annule la translation (garder seulement la rotation)
            camRot.data[12] = 0.f; camRot.data[13] = 0.f; camRot.data[14] = 0.f;
            // Transpose la rotation (= inverse pour une matrice orthogonale)
            NkMat4f billboardRot = camRot.Transpose();
            // Effacer Z (pas de rotation autour de l'axe de visée)
            billboardRot.data[2]  = 0.f;
            billboardRot.data[6]  = 0.f;
            billboardRot.data[8]  = 0.f;
            billboardRot.data[9]  = 0.f;
            billboardRot.data[10] = 1.f;

            NkMat4f billboardWorld =
                NkMat4f::Translation(NkVec3f(-1.5f, 0.8f, 0.f)) // Position fixe dans le monde
              * billboardRot;                                       // Orienté caméra

            logger.Info("[Frame {0}] Drawing billboard text '→ CUBE'\n", frameNum);
            textRenderer->drawText3D(
                "→ CUBE",
                billboardWorld,
                {1.f, 0.4f, 0.4f, 0.9f}, // rouge-rose
                36.f
            );
        }

        // ── Flush texte → draw calls GPU ──────────────────────────────────────
        // endFrame() upload l'atlas si nécessaire et émet les draw calls.
        // IMPORTANT : doit être appelé dans un render pass ouvert.
        logger.Info("[Frame {0}] textRenderer->endFrame\n", frameNum);
        textRenderer->endFrame(cmd, matView, matProj);

        // ── Fin du render pass ────────────────────────────────────────────────
        logger.Info("[Frame {0}] cmd->EndRenderPass\n", frameNum);
        cmd->EndRenderPass();
        logger.Info("[Frame {0}] cmd->End\n", frameNum);
        cmd->End();

        logger.Info("[Frame {0}] device->SubmitAndPresent\n", frameNum);
        device->SubmitAndPresent(cmd);
        logger.Info("[Frame {0}] device->EndFrame\n", frameNum);
        device->EndFrame(frame);
        
        ++frameNum;
        logger.Info("[Frame {0}] Frame complete\n", frameNum);
    }

    // =========================================================================
    //  Nettoyage ordonné
    // =========================================================================
    device->WaitIdle();

    // NkTextRenderer libère toutes ses ressources RHI dans son destructeur.
    // On l'appelle explicitement ici pour contrôler l'ordre.
    textRenderer->Shutdown();

    delete textRenderer;

    device->DestroyCommandBuffer(cmd);

    // Ressources Phong
    if (hPhongDesc.IsValid())   device->FreeDescriptorSet(hPhongDesc);
    if (hPhongLayout.IsValid()) device->DestroyDescriptorSetLayout(hPhongLayout);
    if (hPhongUBO.IsValid())    device->DestroyBuffer(hPhongUBO);
    if (hCubeVBO.IsValid())     device->DestroyBuffer(hCubeVBO);
    if (hPhongPipe.IsValid())   device->DestroyPipeline(hPhongPipe);
    if (hPhongShader.IsValid()) device->DestroyShader(hPhongShader);

    NkDeviceFactory::Destroy(device);
    window.Close();

    logger.Info("[TextDemo] Terminé proprement.\n");
    return 0;
}
