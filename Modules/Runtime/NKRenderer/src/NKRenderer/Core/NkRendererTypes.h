#pragma once
// =============================================================================
// NKRenderer/Core/NkRendererTypes.h — v3 (unifié, STL-free, conflit résolu)
// =============================================================================
/**
 * @file NkRendererTypes.h
 * @brief Types fondamentaux du module NKRenderer — indépendants de NKRHI.
 * 
 * Ce fichier définit l'ensemble des types publics utilisés par le renderer :
 * - Handles opaques typés pour les ressources (textures, meshes, shaders...)
 * - Énumérations pour les formats, modes de rendu, états graphiques
 * - Structures géométriques (AABB, Rect, Transform) et couleurs
 * - Descripteurs de lumière et informations de scène
 * - Vertex standards et skinnés pour le pipeline graphique
 * 
 * 🔹 RÉSOLUTION DU CONFLIT DE HANDLES :
 *    • NKRHI définit des handles dans nkentseu:: (ex: NkTextureHandle)
 *    • NKRenderer définit ses handles dans nkentseu::renderer::
 *    • Pour éviter toute ambiguïté :
 *      - Les handles renderer sont préfixés NkRend* en interne
 *      - Des aliases courts (NkTextureHandle) sont fournis dans renderer::
 *      - L'utilisateur doit qualifier : renderer::NkTextureHandle
 *      - À ÉVITER : "using namespace nkentseu::renderer" dans un fichier
 *        qui inclut aussi des headers NKRHI
 * 
 * 🔹 Philosophie STL-free :
 *    • Aucun include de la STL standard (<vector>, <functional>, etc.)
 *    • Utilisation exclusive des conteneurs NK : NkVector, NkString
 *    • Types POD ou trivialement copiables pour compatibilité C/GPU
 * 
 * 🔹 Thread-safety :
 *    • Ce header ne contient que des définitions de types — thread-safe par nature
 *    • Les handles sont des valeurs (copyable) — pas de partage d'état implicite
 * 
 * @author nkentseu
 * @version 3.0
 * @date 2026
 */

#include "NKCore/NkTypes.h"
#include "NKMath/NkMath.h"
#include "NKMath/NkColor.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include <cstring>

// Constante mathématique utile pour les calculs graphiques
#ifndef NK_PI_F
    #define NK_PI_F 3.14159265358979323846f
#endif

namespace nkentseu {
    namespace renderer {

        // Import des types mathématiques pour usage direct (NkVec3f, NkMat4f, etc.)
        using namespace math;

        // =====================================================================
        // 🔗 Handles opaques typés — identification des ressources renderer
        // =====================================================================
        /**
         * @struct NkHandle
         * @brief Handle opaque typé de 64 bits pour identifier une ressource.
         * 
         * @tparam Tag Type tag vide pour discrimination compile-time.
         * 
         * 🔹 Caractéristiques :
         *    • ID = 0 signifie handle invalide (Null)
         *    • Comparaison et copie triviales (pas de gestion mémoire)
         *    • Taille fixe 8 bytes — passe par registre dans les appels API
         * 
         * 🔹 Pourquoi un template Tag ?
         *    • Évite les confusions entre types de ressources au compile-time
         *    • NkHandle<NkTagTexture> ≠ NkHandle<NkTagMesh> — erreur de type si mélangés
         *    • Zéro overhead runtime : les tags sont effacés après compilation
         * 
         * @warning Ne pas confondre avec les handles NKRHI (nkentseu::NkTextureHandle).
         *          Ceux-ci sont dans nkentseu::renderer:: et wrappent les handles
         *          NKRHI internes sans les exposer directement.
         * 
         * @example
         * @code
         * renderer::NkTextureHandle tex = renderer::NkTextureHandle::Null();
         * if (!tex.IsValid()) {
         *     tex = renderer.CreateTexture(...);  // API renderer
         * }
         * @endcode
         */
        template<typename Tag>
        struct NkHandle {
            /// ID numérique opaque — 0 = invalide
            nk_uint64 id = 0;

            /**
             * @brief Vérifie si le handle est valide (non-null).
             * @return true si id != 0.
             */
            [[nodiscard]] bool IsValid() const noexcept {
                return id != 0;
            }

            /**
             * @brief Comparaison d'égalité.
             * @param o Autre handle à comparer.
             * @return true si les deux handles ont le même ID.
             */
            [[nodiscard]] bool operator==(const NkHandle& o) const noexcept {
                return id == o.id;
            }

            /**
             * @brief Comparaison d'inégalité.
             */
            [[nodiscard]] bool operator!=(const NkHandle& o) const noexcept {
                return id != o.id;
            }

            /**
             * @brief Retourne un handle null (invalide).
             * @return NkHandle avec id = 0.
             */
            [[nodiscard]] static NkHandle Null() noexcept {
                return {0};
            }
        };

        // =====================================================================
        // 🏷️ Tags de type pour discrimination des handles
        // =====================================================================
        /**
         * @brief Tags vides utilisés pour spécialiser NkHandle<T> par type de ressource.
         * @note Ces structs sont vides — aucun membre, aucun overhead.
         *       Leur seul but est la discrimination de type au compile-time.
         */
        struct NkTagRendTexture      {};  ///< Tag pour les textures 2D/Cube/Array
        struct NkTagRendMesh         {};  ///< Tag pour les maillages (vertex/index buffers)
        struct NkTagRendMaterial     {};  ///< Tag pour les matériaux (shaders + paramètres)
        struct NkTagRendMatInst      {};  ///< Tag pour les instances de matériaux (overrides)
        struct NkTagRendShader       {};  ///< Tag pour les programmes de shaders
        struct NkTagRendCamera       {};  ///< Tag pour les caméras (view/proj matrices)
        struct NkTagRendRenderTarget {};  ///< Tag pour les render targets (FBO)
        struct NkTagRendFont         {};  ///< Tag pour les polices de caractères
        struct NkTagRendLight        {};  ///< Tag pour les sources lumineuses
        struct NkTagRendScene        {};  ///< Tag pour les scènes complètes
        struct NkTagRendEntity       {};  ///< Tag pour les entités de rendu (handles ECS)

        // =====================================================================
        // 🎮 Handles publics du renderer (préfixe NkRend* pour clarté)
        // =====================================================================
        /**
         * @brief Handle pour une texture renderer.
         * @note Alias de NkHandle<NkTagRendTexture>.
         */
        using NkRendTextureHandle      = NkHandle<NkTagRendTexture>;

        /**
         * @brief Handle pour un mesh renderer.
         */
        using NkRendMeshHandle         = NkHandle<NkTagRendMesh>;

        /**
         * @brief Handle pour un matériau renderer.
         */
        using NkRendMaterialHandle     = NkHandle<NkTagRendMaterial>;

        /**
         * @brief Handle pour une instance de matériau (overrides de paramètres).
         */
        using NkRendMaterialInstHandle = NkHandle<NkTagRendMatInst>;

        /**
         * @brief Handle pour un programme de shader.
         */
        using NkRendShaderHandle       = NkHandle<NkTagRendShader>;

        /**
         * @brief Handle pour une caméra.
         */
        using NkRendCameraHandle       = NkHandle<NkTagRendCamera>;

        /**
         * @brief Handle pour un render target (framebuffer).
         */
        using NkRendRenderTargetHandle = NkHandle<NkTagRendRenderTarget>;

        /**
         * @brief Handle pour une police de caractères.
         */
        using NkRendFontHandle         = NkHandle<NkTagRendFont>;

        /**
         * @brief Handle pour une source lumineuse.
         */
        using NkRendLightHandle        = NkHandle<NkTagRendLight>;

        /**
         * @brief Handle pour une entité de rendu (lien vers ECS).
         */
        using NkRendEntityHandle       = NkHandle<NkTagRendEntity>;

        // =====================================================================
        // 🔖 Aliases courts pour usage INTERNE au module NKRenderer
        // =====================================================================
        /**
         * @namespace aliases_internes
         * @brief Aliases sans préfixe pour usage UNIQUEMENT dans les .cpp du renderer.
         * 
         * @warning ⚠️ NE PAS UTILISER ces aliases dans des headers publics !
         *          Ils polluent le namespace de l'appelant et créent des ambiguïtés
         *          avec les handles NKRHI (nkentseu::NkTextureHandle, etc.).
         * 
         * @note Ces aliases sont fournis pour :
         *    • Réduire la verbosité dans l'implémentation interne du renderer
         *    • Faciliter la maintenance du code source du module
         * 
         * @example (dans NKRenderer/Core/NkTextureManager.cpp — OK) :
         * @code
         * using namespace nkentseu::renderer;  // OK dans un .cpp interne
         * NkTextureHandle tex = CreateTexture(...);  // Alias court
         * @endcode
         * 
         * @example (dans un header public ou code utilisateur — À ÉVITER) :
         * @code
         * // ❌ NE PAS FAIRE :
         * using namespace nkentseu::renderer;
         * NkTextureHandle h;  // Ambiguïté avec nkentseu::NkTextureHandle (NKRHI)
         * 
         * // ✅ FAIRE :
         * namespace rdr = nkentseu::renderer;
         * rdr::NkRendTextureHandle h;  // Clair et sans ambiguïté
         * @endcode
         */
        /// @{
        using NkTextureHandle      = NkRendTextureHandle;       ///< Alias court interne
        using NkMeshHandle         = NkRendMeshHandle;
        using NkMaterialHandle     = NkRendMaterialHandle;
        using NkMaterialInstHandle = NkRendMaterialInstHandle;
        using NkShaderHandle       = NkRendShaderHandle;
        using NkCameraHandle       = NkRendCameraHandle;
        using NkRenderTargetHandle = NkRendRenderTargetHandle;
        using NkFontHandle         = NkRendFontHandle;
        using NkLightHandle        = NkRendLightHandle;
        using NkEntityHandle       = NkRendEntityHandle;
        /// @}

        // =====================================================================
        // ✅ Résultat d'opération renderer
        // =====================================================================
        /**
         * @enum NkRenderResult
         * @brief Codes de retour pour les opérations du renderer.
         * 
         * 🔹 Usage :
         *    • Toutes les fonctions API du renderer retournent NkRenderResult
         *    • Utiliser NkOk(result) pour tester le succès de façon lisible
         * 
         * @note Les valeurs sont explicites pour faciliter le débogage et les logs.
         */
        enum class NkRenderResult : nk_uint32 {
            NK_OK = 0,                ///< Succès
            NK_ERROR_INVALID_DEVICE,  ///< Device GPU invalide ou perdu
            NK_ERROR_INVALID_HANDLE,  ///< Handle de ressource invalide
            NK_ERROR_COMPILE_FAILED,  ///< Échec de compilation de shader
            NK_ERROR_OUT_OF_MEMORY,   ///< Mémoire GPU/CPU insuffisante
            NK_ERROR_NOT_SUPPORTED,   ///< Fonctionnalité non supportée par le backend
            NK_ERROR_IO,              ///< Erreur de lecture/écriture fichier
            NK_ERROR_UNKNOWN,         ///< Erreur non catégorisée
        };

        /**
         * @brief Teste si un résultat indique un succès.
         * @param r Résultat à tester.
         * @return true si r == NK_OK.
         * 
         * @note Fonction inline pour performance — zéro overhead d'appel.
         */
        [[nodiscard]] inline bool NkOk(NkRenderResult r) noexcept {
            return r == NkRenderResult::NK_OK;
        }

        // =====================================================================
        // 🎨 Formats de pixel (abstraction au-dessus de NkGPUFormat)
        // =====================================================================
        /**
         * @enum NkPixelFormat
         * @brief Formats de texture et de render target supportés.
         * 
         * 🔹 Catégories :
         *    • UNORM 8/16 bits : NK_R8, NK_RGBA8, etc. (données normales)
         *    • FLOAT 16/32 bits : NK_RGBA16F, NK_RGBA32F (HDR, calcul)
         *    • SRGB : NK_SRGBA8, NK_BC1_SRGB (correction gamma automatique)
         *    • Depth/Stencil : NK_D24S8, NK_D32F (buffers de profondeur)
         *    • Compression : NK_BC1, NK_BC7, NK_ETC2 (réduction mémoire GPU)
         * 
         * @note La disponibilité exacte dépend du backend GPU (OpenGL, Vulkan, D3D).
         *       Utiliser NkRenderer::IsFormatSupported() pour vérifier au runtime.
         * 
         * @warning NK_UNDEFINED ne doit jamais être utilisé pour créer une ressource.
         *          C'est une valeur sentinel pour les champs optionnels.
         */
        enum class NkPixelFormat : nk_uint32 {
            NK_UNDEFINED = 0,           ///< Valeur invalide / non spécifiée

            // ── Formats UNORM 8 bits ─────────────────────────────────────
            NK_R8,                      ///< 1 canal, 8 bits non-signé
            NK_RG8,                     ///< 2 canaux, 8 bits chacun
            NK_RGB8,                    ///< 3 canaux, 8 bits chacun
            NK_RGBA8,                   ///< 4 canaux, 8 bits chacun (standard)

            // ── Formats SRGB (gamma-corrected) ───────────────────────────
            NK_SRGB8,                   ///< RGB8 avec espace couleur sRGB
            NK_SRGBA8,                  ///< RGBA8 avec espace couleur sRGB

            // ── Formats FLOAT 16/32 bits (HDR, calcul) ───────────────────
            NK_R16F, NK_RG16F,          ///< 1-2 canaux, float16
            NK_RGBA16F,                 ///< 4 canaux, float16 (HDR courant)
            NK_R32F, NK_RG32F,          ///< 1-2 canaux, float32
            NK_RGBA32F,                 ///< 4 canaux, float32 (précision max)
            NK_R11G11B10F,              ///< Format compact HDR (11-11-10 bits)

            // ── Formats Depth/Stencil ────────────────────────────────────
            NK_D24S8,                   ///< 24 bits depth + 8 bits stencil
            NK_D32F,                    ///< 32 bits float depth (précis)

            // ── Formats compressés (BC/ETC/ASTC) ─────────────────────────
            NK_BC1, NK_BC3, NK_BC5, NK_BC7,       ///< Compression BC (DX/OpenGL)
            NK_BC1_SRGB, NK_BC3_SRGB, NK_BC7_SRGB,///< Versions SRGB
            NK_ETC2_SRGB,                         ///< Compression ETC2 (mobile)
            NK_ASTC4x4_SRGB,                      ///< Compression ASTC (mobile haut de gamme)

            NK_COUNT,                   ///< Nombre de formats (pour itération)
        };

        // =====================================================================
        // 🔺 Topologie de primitives géométriques
        // =====================================================================
        /**
         * @enum NkPrimitive
         * @brief Mode d'assemblage des vertices en primitives pour le rendu.
         * 
         * @note La topologie affecte l'interprétation de l'index buffer :
         *       • NK_TRIANGLES : chaque triplet d'indices forme un triangle
         *       • NK_TRIANGLE_STRIP : triangles connectés partageant des arêtes
         *       • NK_LINES : chaque paire d'indices forme un segment
         * 
         * @warning Certaines topologies (ex: NK_TRIANGLE_STRIP) peuvent être
         *          moins performantes sur certains GPU — préférer NK_TRIANGLES
         *          sauf besoin spécifique (géométrie procédurale, tessellation).
         */
        enum class NkPrimitive : nk_uint32 {
            NK_TRIANGLES,       ///< Triangles indépendants (recommandé)
            NK_TRIANGLE_STRIP,  ///< Bande de triangles connectés
            NK_LINES,           ///< Segments de ligne indépendants
            NK_LINE_STRIP,      ///< Chaîne de segments connectés
            NK_POINTS,          ///< Points individuels (particules, debug)
        };

        // =====================================================================
        // 🎭 États de rasterization : remplissage, culling, orientation
        // =====================================================================
        /**
         * @enum NkFillMode
         * @brief Mode de remplissage des primitives pendant le rasterization.
         */
        enum class NkFillMode : nk_uint32 {
            NK_SOLID,     ///< Remplissage complet (standard)
            NK_WIREFRAME, ///< Affichage des arêtes uniquement (debug)
            NK_POINT,     ///< Affichage des sommets uniquement (debug)
        };

        /**
         * @enum NkCullMode
         * @brief Mode de culling des faces (optimisation de rendu).
         * 
         * @note Le culling élimine les faces non visibles avant le rasterization :
         *    • NK_BACK : cull les faces arrière (standard pour objets fermés)
         *    • NK_FRONT : cull les faces avant (rare, effets spéciaux)
         *    • NK_NONE : aucun culling (transparence, deux faces visibles)
         */
        enum class NkCullMode : nk_uint32 {
            NK_NONE,   ///< Aucun culling — toutes les faces rendues
            NK_BACK,   ///< Cull les faces arrière (winding CW vu de la caméra)
            NK_FRONT,  ///< Cull les faces avant (winding CCW vu de la caméra)
        };

        /**
         * @enum NkFrontFace
         * @brief Orientation considérée comme "face avant" pour le culling.
         * 
         * @note Détermine quel winding order (CCW/CW) définit une face visible :
         *    • NK_CCW (Counter-Clockwise) : standard OpenGL, faces CCW = avant
         *    • NK_CW (Clockwise) : standard DirectX, faces CW = avant
         * 
         * @warning Doit correspondre à l'ordre de winding des meshes importés.
         *          Un mismatch cause un culling inversé (faces invisibles).
         */
        enum class NkFrontFace : nk_uint32 {
            NK_CCW,  ///< Les vertices dans l'ordre anti-horaire définissent la face avant
            NK_CW,   ///< Les vertices dans l'ordre horaire définissent la face avant
        };

        // =====================================================================
        // 🌈 Modes de blending (transparence, effets)
        // =====================================================================
        /**
         * @enum NkBlendMode
         * @brief Mode de combinaison des couleurs source et destination.
         * 
         * 🔹 Formules (source = nouvelle couleur, dest = framebuffer) :
         *    • NK_OPAQUE : dest = source (remplacement, pas de transparence)
         *    • NK_ALPHA : dest = source*alpha + dest*(1-alpha) (transparence standard)
         *    • NK_ADDITIVE : dest = source + dest (effets de lumière, particules)
         *    • NK_MULTIPLY : dest = source * dest (ombres, modulations)
         *    • NK_PREMULT_ALPHA : dest = source + dest*(1-source.alpha)
         *      (pour textures pré-multipliées — évite les artefacts de bord)
         * 
         * @note Le blending est une opération coûteuse — éviter sur les objets opaques.
         *       Trier les objets transparents back-to-front pour un résultat correct.
         */
        enum class NkBlendMode : nk_uint32 {
            NK_OPAQUE,        ///< Remplacement pur (pas de blending)
            NK_ALPHA,         ///< Transparence alpha standard
            NK_ADDITIVE,      ///< Addition (effets lumineux)
            NK_MULTIPLY,      ///< Multiplication (ombres, modulateurs)
            NK_PREMULT_ALPHA, ///< Alpha pré-multiplié (textures PMA)
        };

        // =====================================================================
        // 🔍 Filtrage et wrapping des textures
        // =====================================================================
        /**
         * @enum NkFilterMode
         * @brief Mode de filtrage pour l'échantillonnage des textures.
         * 
         * @note Impact qualité/performance :
         *    • NK_NEAREST : plus rapide, pixelisé (pixel art, UI nette)
         *    • NK_LINEAR : interpolation bilinéaire (standard 3D)
         *    • NK_ANISOTROPIC : filtrage anisotrope (meilleure qualité en perspective, coûteux)
         */
        enum class NkFilterMode : nk_uint32 {
            NK_NEAREST,      ///< Échantillon le plus proche (pas d'interpolation)
            NK_LINEAR,       ///< Interpolation bilinéaire (lissage)
            NK_ANISOTROPIC,  ///< Filtrage anisotrope (qualité maximale en perspective)
        };

        /**
         * @enum NkMipFilter
         * @brief Mode de filtrage entre les niveaux de mipmaps.
         * 
         * @note Les mipmaps sont des versions réduites d'une texture pour le LOD :
         *    • NK_NONE : pas de mipmapping (flou à distance)
         *    • NK_NEAREST : choix du mipmap le plus proche (rapide)
         *    • NK_LINEAR : interpolation entre deux mipmaps (qualité)
         */
        enum class NkMipFilter : nk_uint32 {
            NK_NONE,     ///< Pas de filtrage de mipmaps
            NK_NEAREST,  ///< Choix du niveau de mipmap le plus proche
            NK_LINEAR,   ///< Interpolation entre deux niveaux de mipmaps
        };

        /**
         * @enum NkWrapMode
         * @brief Comportement lors du sampling en dehors des coordonnées [0,1].
         * 
         * @note Utile pour :
         *    • NK_REPEAT : textures de sol, murs, motifs répétitifs
         *    • NK_CLAMP : éviter les artefacts de bord sur les sprites/UI
         *    • NK_MIRROR : motifs symétriques, effets spéciaux
         */
        enum class NkWrapMode : nk_uint32 {
            NK_REPEAT,   ///< Répétition périodique (tileable)
            NK_MIRROR,   ///< Répétition avec inversion miroir
            NK_CLAMP,    ///< Étirement de la bordure (pas de répétition)
            NK_BORDER,   ///< Couleur de bordure fixe (souvent transparent)
        };

        // =====================================================================
        // 🖼️ Types de textures
        // =====================================================================
        /**
         * @enum NkTextureKind
         * @brief Dimensionnalité et usage d'une texture.
         * 
         * @note Influence l'API de création et les coordonnées de sampling :
         *    • NK_2D : texture classique (u,v)
         *    • NK_CUBE : cubemap pour environment mapping (direction 3D)
         *    • NK_2D_ARRAY : atlas de textures (u,v,layer)
         *    • NK_3D : texture volumétrique (u,v,w)
         *    • NK_RENDER_TARGET : texture attachée à un framebuffer
         *    • NK_DEPTH_STENCIL : buffer de profondeur/Stencil
         */
        enum class NkTextureKind : nk_uint32 {
            NK_2D,              ///< Texture 2D standard
            NK_CUBE,            ///< Cubemap (6 faces, environment mapping)
            NK_2D_ARRAY,        ///< Tableau de textures 2D (atlas)
            NK_3D,              ///< Texture volumétrique 3D
            NK_RENDER_TARGET,   ///< Texture cible de rendu (FBO attachment)
            NK_DEPTH_STENCIL,   ///< Buffer de profondeur + stencil
        };

        // =====================================================================
        // 🔲 MSAA (Multi-Sample Anti-Aliasing)
        // =====================================================================
        /**
         * @enum NkMSAA
         * @brief Niveau de suréchantillonnage pour l'anti-aliasing.
         * 
         * @note Impact performance/qualité :
         *    • NK_1X : pas d'AA (plus rapide, escaliers visibles)
         *    • NK_4X : bon compromis qualité/perf (standard)
         *    • NK_8X+ : haute qualité, coûteux en mémoire et fillrate
         * 
         * @warning Le support dépend du GPU et du backend. Vérifier avec
         *          NkRenderer::GetDeviceCaps() avant de demander un niveau élevé.
         */
        enum class NkMSAA : nk_uint32 {
            NK_1X = 1,   ///< Pas de suréchantillonnage
            NK_2X = 2,   ///< 2 échantillons par pixel
            NK_4X = 4,   ///< 4 échantillons (recommandé)
            NK_8X = 8,   ///< 8 échantillons (haute qualité)
            NK_16X = 16, ///< 16 échantillons (très haute qualité, coûteux)
        };

        // =====================================================================
        // ⚙️ Stages de shader (pipeline graphique)
        // =====================================================================
        /**
         * @enum NkShaderStageFlag
         * @brief Flags pour spécifier les stages d'un programme de shader.
         * 
         * @note Utilisation typique :
         *    • Graphics pipeline : NK_VERTEX | NK_FRAGMENT (optionnel : GEOMETRY, TESS_*)
         *    • Compute pipeline : NK_COMPUTE seul
         * 
         * @example
         * @code
         * auto flags = NkShaderStageFlag::NK_VERTEX | NkShaderStageFlag::NK_FRAGMENT;
         * if (NkHasStage(flags, NkShaderStageFlag::NK_VERTEX)) { ... }
         * @endcode
         */
        enum class NkShaderStageFlag : nk_uint32 {
            NK_VERTEX       = 1 << 0,  ///< Vertex shader (transformation des sommets)
            NK_FRAGMENT     = 1 << 1,  ///< Fragment/pixel shader (calcul de couleur)
            NK_COMPUTE      = 1 << 2,  ///< Compute shader (calcul général GPU)
            NK_GEOMETRY     = 1 << 3,  ///< Geometry shader (génération de primitives)
            NK_TESS_CTRL    = 1 << 4,  ///< Tessellation control shader
            NK_TESS_EVAL    = 1 << 5,  ///< Tessellation evaluation shader
            NK_ALL_GRAPHICS = (1<<0)|(1<<1)|(1<<3)|(1<<4)|(1<<5), ///< Tous les stages graphiques
            NK_ALL          = 0xFF,    ///< Tous les stages (mask complet)
        };

        /**
         * @brief Opérateur OR pour combiner des flags de shader stage.
         */
        [[nodiscard]] inline NkShaderStageFlag operator|(
            NkShaderStageFlag a, NkShaderStageFlag b) noexcept {
            return static_cast<NkShaderStageFlag>(
                static_cast<nk_uint32>(a) | static_cast<nk_uint32>(b)
            );
        }

        /**
         * @brief Teste si un flag de stage est présent dans un ensemble.
         * @param flags Ensemble de flags.
         * @param bit Flag à tester.
         * @return true si bit est inclus dans flags.
         */
        [[nodiscard]] inline bool NkHasStage(
            NkShaderStageFlag flags, NkShaderStageFlag bit) noexcept {
            return (static_cast<nk_uint32>(flags) & static_cast<nk_uint32>(bit)) != 0;
        }

        // =====================================================================
        // 🔢 Format des indices de mesh
        // =====================================================================
        /**
         * @enum NkIndexFormat
         * @brief Taille des indices dans un index buffer.
         * 
         * @note Choix selon la taille du mesh :
         *    • NK_UINT16 : meshes ≤ 65535 vertices (économie mémoire)
         *    • NK_UINT32 : meshes > 65535 vertices (universalité)
         * 
         * @warning Un mismatch entre le format déclaré et les données réelles
         *          cause un rendu corrompu ou un crash GPU.
         */
        enum class NkIndexFormat : nk_uint32 {
            NK_UINT16,  ///< Indices 16 bits (0..65535)
            NK_UINT32,  ///< Indices 32 bits (0..4 milliards)
        };

        // =====================================================================
        // 📏 Opérations de comparaison pour le test de profondeur
        // =====================================================================
        /**
         * @enum NkDepthOp
         * @brief Fonction de comparaison pour le depth test.
         * 
         * @note Impact sur l'ordre de rendu et la visibilité :
         *    • NK_LESS : standard (le fragment le plus proche gagne)
         *    • NK_ALWAYS : désactive le depth test (UI, overlays)
         *    • NK_EQUAL : rendu de contours, effets de silhouette
         * 
         * @warning Le depth buffer doit être correctement clearé et écrit
         *          pour que le test fonctionne comme attendu.
         */
        enum class NkDepthOp : nk_uint32 {
            NK_NEVER,         ///< Toujours échouer (rien ne passe)
            NK_LESS,          ///< Passer si depth < buffer (standard)
            NK_EQUAL,         ///< Passer si depth == buffer
            NK_LESS_EQUAL,    ///< Passer si depth <= buffer
            NK_GREATER,       ///< Passer si depth > buffer (inversé)
            NK_NOT_EQUAL,     ///< Passer si depth != buffer
            NK_GREATER_EQUAL, ///< Passer si depth >= buffer
            NK_ALWAYS,        ///< Toujours passer (désactive le test)
        };

        // =====================================================================
        // 🎨 Couleur RGBA en flottants [0,1]
        // =====================================================================
        /**
         * @struct NkColorF
         * @brief Représentation d'une couleur en espace linéaire flottant.
         * 
         * 🔹 Caractéristiques :
         *    • Composantes r,g,b,a dans [0.0, 1.0]
         *    • Espace linéaire par défaut (pas de gamma) — convertir pour l'affichage
         *    • Opérateurs utiles : WithAlpha, Lerp, comparaison
         * 
         * 🔹 Conversion depuis NkColor (8 bits) :
         *    • NkColorF(NkColor{255,128,64,255}) → {1.0, 0.5, 0.25, 1.0}
         * 
         * @note Pour le rendu HDR, les valeurs peuvent dépasser 1.0 (clamping au tonemapping).
         * 
         * @example
         * @code
         * NkColorF base = NkColorF::Red();
         * NkColorF faded = base.WithAlpha(0.5f);  // Rouge semi-transparent
         * NkColorF mixed = base.Lerp(NkColorF::Blue(), 0.3f);  // 70% rouge, 30% bleu
         * @endcode
         */
        struct NkColorF {
            /// Composante rouge [0,1]
            nk_float32 r = 0.f;
            /// Composante verte [0,1]
            nk_float32 g = 0.f;
            /// Composante bleue [0,1]
            nk_float32 b = 0.f;
            /// Composante alpha [0,1] (1 = opaque, 0 = transparent)
            nk_float32 a = 1.f;

            /// Constructeur par défaut (noir opaque)
            NkColorF() noexcept = default;

            /**
             * @brief Constructeur avec valeurs explicites.
             * @param r Rouge [0,1].
             * @param g Vert [0,1].
             * @param b Bleu [0,1].
             * @param a Alpha [0,1] (par défaut 1.0 = opaque).
             */
            NkColorF(nk_float32 r, nk_float32 g, nk_float32 b, nk_float32 a = 1.f) noexcept
                : r(r), g(g), b(b), a(a) {}

            /**
             * @brief Constructeur depuis NkColor (8 bits par canal).
             * @param c Couleur 8 bits à convertir.
             * @note Division par 255.0 pour normaliser dans [0,1].
             */
            explicit NkColorF(const math::NkColor& c) noexcept
                : r(c.r / 255.f)
                , g(c.g / 255.f)
                , b(c.b / 255.f)
                , a(c.a / 255.f) {}

            // ── Couleurs prédéfinies ─────────────────────────────────────
            [[nodiscard]] static NkColorF White() noexcept { return {1, 1, 1, 1}; }
            [[nodiscard]] static NkColorF Black() noexcept { return {0, 0, 0, 1}; }
            [[nodiscard]] static NkColorF Transparent() noexcept { return {0, 0, 0, 0}; }
            [[nodiscard]] static NkColorF Red() noexcept { return {1, 0, 0, 1}; }
            [[nodiscard]] static NkColorF Green() noexcept { return {0, 1, 0, 1}; }
            [[nodiscard]] static NkColorF Blue() noexcept { return {0, 0, 1, 1}; }
            [[nodiscard]] static NkColorF Yellow() noexcept { return {1, 1, 0, 1}; }
            [[nodiscard]] static NkColorF Cyan() noexcept { return {0, 1, 1, 1}; }
            [[nodiscard]] static NkColorF Magenta() noexcept { return {1, 0, 1, 1}; }

            /**
             * @brief Retourne un gris avec la luminosité spécifiée.
             * @param v Valeur de gris [0,1] (0=noir, 1=blanc).
             */
            [[nodiscard]] static NkColorF Gray(nk_float32 v = 0.5f) noexcept {
                return {v, v, v, 1.f};
            }

            // ── Opérations utiles ────────────────────────────────────────
            /**
             * @brief Retourne une copie avec un nouvel alpha.
             * @param a_ Nouvelle valeur alpha [0,1].
             * @note La couleur d'origine n'est pas modifiée.
             */
            [[nodiscard]] NkColorF WithAlpha(nk_float32 a_) const noexcept {
                return {r, g, b, a_};
            }

            /**
             * @brief Interpolation linéaire vers une autre couleur.
             * @param to Couleur cible.
             * @param t Facteur d'interpolation [0,1] (0=cette couleur, 1=to).
             * @return Couleur interpolée.
             */
            [[nodiscard]] NkColorF Lerp(const NkColorF& to, nk_float32 t) const noexcept {
                return {
                    r + (to.r - r) * t,
                    g + (to.g - g) * t,
                    b + (to.b - b) * t,
                    a + (to.a - a) * t
                };
            }

            /**
             * @brief Comparaison d'égalité exacte (composante par composante).
             * @warning Attention aux comparaisons de flottants — préférer un epsilon
             *          pour les valeurs calculées.
             */
            [[nodiscard]] bool operator==(const NkColorF& o) const noexcept {
                return r == o.r && g == o.g && b == o.b && a == o.a;
            }
        };

        // =====================================================================
        // 📐 Rectangle flottant 2D
        // =====================================================================
        /**
         * @struct NkRectF
         * @brief Rectangle défini par sa position (x,y) et ses dimensions (w,h).
         * 
         * 🔹 Convention :
         *    • Origine en haut-gauche (comme la plupart des APIs 2D/UI)
         *    • Coordonnées en espace écran ou texture [0,width] × [0,height]
         *    • Right() et Bottom() sont exclusifs : [x, x+w) × [y, y+h)
         * 
         * @note Utile pour :
         *    • Viewports, scissor rects, UV coordinates, UI layout
         * 
         * @example
         * @code
         * NkRectF viewport(0, 0, 1920, 1080);
         * if (viewport.Contains(mouseX, mouseY)) { ... }
         * @endcode
         */
        struct NkRectF {
            /// Coordonnée X du coin haut-gauche
            nk_float32 x = 0;
            /// Coordonnée Y du coin haut-gauche
            nk_float32 y = 0;
            /// Largeur du rectangle
            nk_float32 w = 0;
            /// Hauteur du rectangle
            nk_float32 h = 0;

            /// Constructeur par défaut (rectangle nul)
            NkRectF() noexcept = default;

            /**
             * @brief Constructeur avec valeurs explicites.
             * @param x Position X.
             * @param y Position Y.
             * @param w Largeur.
             * @param h Hauteur.
             */
            NkRectF(nk_float32 x, nk_float32 y, nk_float32 w, nk_float32 h) noexcept
                : x(x), y(y), w(w), h(h) {}

            /// @brief Coordonnée X du bord droit (exclusif).
            [[nodiscard]] nk_float32 Right() const noexcept { return x + w; }
            /// @brief Coordonnée Y du bord bas (exclusif).
            [[nodiscard]] nk_float32 Bottom() const noexcept { return y + h; }
            /// @brief Centre du rectangle.
            [[nodiscard]] NkVec2f Center() const noexcept {
                return {x + w * 0.5f, y + h * 0.5f};
            }
            /// @brief Coin haut-gauche.
            [[nodiscard]] NkVec2f TopLeft() const noexcept { return {x, y}; }
            /// @brief Vérifie que le rectangle a une taille positive.
            [[nodiscard]] bool IsValid() const noexcept { return w > 0 && h > 0; }

            /**
             * @brief Teste si un point est à l'intérieur du rectangle.
             * @param px Coordonnée X du point.
             * @param py Coordonnée Y du point.
             * @return true si x <= px < x+w et y <= py < y+h.
             */
            [[nodiscard]] bool Contains(nk_float32 px, nk_float32 py) const noexcept {
                return px >= x && px < x + w && py >= y && py < y + h;
            }

            /**
             * @brief Version vectorielle de Contains().
             */
            [[nodiscard]] bool Contains(NkVec2f p) const noexcept {
                return Contains(p.x, p.y);
            }
        };

        // =====================================================================
        // 👁️ Viewport et zone de découpe (scissor)
        // =====================================================================
        /**
         * @struct NkViewportF
         * @brief Définit la zone de rendu et la plage de profondeur.
         * 
         * 🔹 Champs :
         *    • x,y : position du coin haut-gauche en pixels
         *    • width,height : dimensions de la zone de rendu
         *    • minDepth,maxDepth : plage de profondeur après projection [0,1] par défaut
         * 
         * @note Utile pour :
         *    • Rendu multi-viewport (split-screen, mini-maps)
         *    • Post-processing sur sous-régions
         *    • Rendu stéréo (VR) avec deux viewports côte à côte
         */
        struct NkViewportF {
            nk_float32 x = 0, y = 0;           ///< Position du coin haut-gauche
            nk_float32 width = 0, height = 0;  ///< Dimensions en pixels
            nk_float32 minDepth = 0.f;         ///< Valeur de profondeur minimale [0,1]
            nk_float32 maxDepth = 1.f;         ///< Valeur de profondeur maximale [0,1]

            /**
             * @brief Convertit en NkRectF (ignore minDepth/maxDepth).
             * @return Rectangle couvrant la zone du viewport.
             */
            [[nodiscard]] NkRectF ToRect() const noexcept {
                return {x, y, width, height};
            }
        };

        /**
         * @struct NkScissorI
         * @brief Rectangle de découpe en coordonnées entières (pixels).
         * 
         * @note Le scissor test discard les fragments en dehors de ce rectangle :
         *    • Utile pour l'UI (ne pas dessiner hors d'une fenêtre)
         *    • Optimisation : éviter le shading de pixels masqués
         * 
         * @warning Doit être mis à jour si le viewport change — les deux sont indépendants.
         */
        struct NkScissorI {
            nk_int32 x = 0, y = 0;           ///< Position du coin haut-gauche (pixels)
            nk_int32 width = 0, height = 0;  ///< Dimensions en pixels
        };

        // =====================================================================
        // 📦 AABB (Axis-Aligned Bounding Box) 3D
        // =====================================================================
        /**
         * @struct NkAABB
         * @brief Boîte englobante alignée aux axes pour tests de collision/frustum.
         * 
         * 🔹 Représentation :
         *    • min : coin inférieur-gauche-arrière (valeurs minimales par axe)
         *    • max : coin supérieur-droit-avant (valeurs maximales par axe)
         *    • Tous les points de l'objet satisfont : min ≤ point ≤ max (composante par composante)
         * 
         * 🔹 Méthodes utiles :
         *    • Center() : centre géométrique de la boîte
         *    • Extents() : demi-dimensions (de center à une face)
         *    • Size() : dimensions complètes (max - min)
         *    • BoundingRadius() : rayon de la sphère englobante (pour tests rapides)
         *    • Expand() : agrandit la boîte pour inclure un nouveau point
         * 
         * @note Les AABB sont rapides à tester mais conservateurs :
         *       une sphère dans une boîte cubique a beaucoup de "vide".
         *       Pour plus de précision, utiliser OBB (Oriented Bounding Box) ou mesh collision.
         * 
         * @example
         * @code
         * NkAABB box;
         * box.Expand({-1, -1, -1});  // Ajoute un coin
         * box.Expand({ 1,  1,  1});  // Ajoute l'autre coin → boîte de -1 à +1 sur chaque axe
         * if (FrustumContainsAABB(camera, box)) {
         *     RenderObject();  // L'objet est potentiellement visible
         * }
         * @endcode
         */
        struct NkAABB {
            /// Coin minimal (x,y,z les plus petits)
            NkVec3f min = {-0.5f, -0.5f, -0.5f};
            /// Coin maximal (x,y,z les plus grands)
            NkVec3f max = { 0.5f,  0.5f,  0.5f};

            /// @brief Centre géométrique de la boîte.
            [[nodiscard]] NkVec3f Center() const noexcept {
                return (min + max) * 0.5f;
            }

            /// @brief Demi-dimensions (distance du centre à une face).
            [[nodiscard]] NkVec3f Extents() const noexcept {
                return (max - min) * 0.5f;
            }

            /// @brief Dimensions complètes (largeur, hauteur, profondeur).
            [[nodiscard]] NkVec3f Size() const noexcept {
                return max - min;
            }

            /**
             * @brief Rayon de la sphère englobante la boîte.
             * @return Distance du centre au coin le plus éloigné.
             * @note Utile pour un test de visibilité rapide (sphere vs frustum).
             */
            [[nodiscard]] nk_float32 BoundingRadius() const noexcept {
                const NkVec3f e = Extents();
                return NkSqrt(e.x * e.x + e.y * e.y + e.z * e.z);
            }

            /// @brief Vérifie que min ≤ max sur tous les axes (boîte valide).
            [[nodiscard]] bool IsValid() const noexcept {
                return min.x <= max.x && min.y <= max.y && min.z <= max.z;
            }

            /**
             * @brief Crée une AABB à partir d'un centre et de demi-dimensions.
             * @param c Centre de la boîte.
             * @param e Demi-dimensions (extents).
             * @return AABB avec min = c-e, max = c+e.
             */
            [[nodiscard]] static NkAABB FromCenterExtents(
                NkVec3f c, NkVec3f e) noexcept {
                return {c - e, c + e};
            }

            /**
             * @brief Agrandit la boîte pour inclure un point.
             * @param p Point à inclure.
             * @note Modifie min/max in-place — la boîte ne peut que grandir.
             */
            void Expand(const NkVec3f& p) noexcept {
                min = NkVec3f(
                    NkMin(min.x, p.x),
                    NkMin(min.y, p.y),
                    NkMin(min.z, p.z)
                );
                max = NkVec3f(
                    NkMax(max.x, p.x),
                    NkMax(max.y, p.y),
                    NkMax(max.z, p.z)
                );
            }
        };

        // =====================================================================
        // 💾 Limite pour les push constants (données uniformes rapides)
        // =====================================================================
        /**
         * @brief Taille maximale des push constants en bytes.
         * @note Les push constants sont des uniformes envoyés directement
         *       dans la commande de draw — très rapides mais limités en taille.
         *       128 bytes est une limite commune à Vulkan/D3D12/OpenGL moderne.
         * 
         * @warning Dépasser cette limite cause une erreur de validation.
         *          Pour plus de données, utiliser un uniform buffer.
         */
        static constexpr nk_uint32 kNkMaxPushConstantBytes = 128u;

        // =====================================================================
        // 🎮 Mode de rendu global (debug/visualisation)
        // =====================================================================
        /**
         * @enum NkRenderMode
         * @brief Mode de visualisation pour le débogage et l'analyse.
         * 
         * @note Ces modes affectent le pipeline de rendu entier :
         *    • NK_SOLID : rendu normal (production)
         *    • NK_WIREFRAME : affichage des arêtes (vérification de géométrie)
         *    • NK_NORMALS : visualisation des normales (debug d'éclairage)
         *    • NK_OVERDRAW : heatmap des pixels redessinés (optimisation)
         *    • NK_DEPTH : visualisation du z-buffer (debug de profondeur)
         *    • NK_UV : affichage des coordonnées de texture (debug de mapping)
         *    • NK_ALBEDO_ONLY : sans éclairage (vérification des textures)
         *    • NK_LIGHTING_ONLY : éclairage seul (debug de lighting)
         * 
         * @warning Certains modes nécessitent des shaders spécifiques.
         *          Le renderer peut fallback vers NK_SOLID si non supporté.
         */
        enum class NkRenderMode : nk_uint32 {
            NK_SOLID,        ///< Rendu normal opaque + alpha (production)
            NK_WIREFRAME,    ///< Affichage des arêtes uniquement (debug géométrie)
            NK_NORMALS,      ///< Visualisation des normales (debug éclairage)
            NK_OVERDRAW,     ///< Heatmap des pixels redessinés (optimisation perf)
            NK_DEPTH,        ///< Visualisation du buffer de profondeur (debug z-fighting)
            NK_UV,           ///< Affichage des coordonnées UV (debug de mapping)
            NK_ALBEDO_ONLY,  ///< Rendu sans éclairage (vérification des textures)
            NK_LIGHTING_ONLY,///< Rendu de l'éclairage seul (debug de lighting)
        };

        // =====================================================================
        // 💡 Types de sources lumineuses
        // =====================================================================
        /**
         * @enum NkLightType
         * @brief Classification des différents types de lumières supportés.
         * 
         * @note Impact sur le shading et les performances :
         *    • NK_DIRECTIONAL : lumière infinie (soleil), calcul simple, shadow map globale
         *    • NK_POINT : lumière omnidirectionnelle, atténuation avec la distance
         *    • NK_SPOT : lumière directionnelle avec cône d'éclairage (projecteur)
         *    • NK_AREA : lumière étendue (soft shadows, coûteux — souvent approximée)
         *    • NK_AMBIENT : lumière uniforme globale (pas de direction, pas d'ombres)
         * 
         * @warning Le nombre de lumières actives impacte fortement les performances.
         *          Utiliser le culling de lumière et les light probes pour optimiser.
         */
        enum class NkLightType : nk_uint32 {
            NK_DIRECTIONAL,  ///< Lumière directionnelle infinie (soleil, lune)
            NK_POINT,        ///< Lumière ponctuelle omnidirectionnelle (ampoule)
            NK_SPOT,         ///< Lumière directionnelle conique (projecteur, phare)
            NK_AREA,         ///< Lumière étendue (soft shadows, néon — coûteux)
            NK_AMBIENT,      ///< Lumière ambiante globale (pas de direction, pas d'ombres)
        };

        // =====================================================================
        // 💡 Descripteur complet d'une source lumineuse
        // =====================================================================
        /**
         * @struct NkLightDesc
         * @brief Description d'une lumière pour le pipeline d'éclairage.
         * 
         * 🔹 Champs communs à tous les types :
         *    • type : classification de la lumière (Directional, Point, etc.)
         *    • color, intensity : couleur et puissance de la lumière
         *    • castShadow, enabled : flags de rendu
         * 
         * 🔹 Champs spécifiques :
         *    • position : utilisé par Point/Spot/Area (ignoré pour Directional)
         *    • direction : utilisé par Directional/Spot (normalisé, ignoré pour Point)
         *    • range : distance maximale d'impact (Point/Spot)
         *    • innerAngle/outerAngle : cône d'éclairage pour Spot (en degrés)
         * 
         * @note Les lumières sont typiquement stockées dans un NkVector<NkLightDesc>
         *       et uploadées au GPU via un uniform buffer ou push constants.
         * 
         * @example
         * @code
         * NkLightDesc sun;
         * sun.type = NkLightType::NK_DIRECTIONAL;
         * sun.direction = NkVec3f(0, -1, 0);  // Midi, lumière venant du haut
         * sun.color = NkColorF(1.f, 0.95f, 0.9f);  // Blanc légèrement chaud
         * sun.intensity = 2.f;  // Plus fort que la lumière ambiante
         * sun.castShadow = true;  // Active les ombres portées
         * @endcode
         */
        struct NkLightDesc {
            /// Type de la lumière (détermine quels champs sont utilisés)
            NkLightType type = NkLightType::NK_DIRECTIONAL;

            /// Position de la lumière dans l'espace monde (Point/Spot/Area)
            NkVec3f position = {0, 10, 0};

            /// Direction de la lumière (normalisée) pour Directional/Spot
            NkVec3f direction = {0, -1, 0};

            /// Couleur de la lumière en espace linéaire [0,1]
            NkColorF color = NkColorF::White();

            /// Intensité multiplicative (1 = normale, >1 = plus brillante)
            nk_float32 intensity = 1.f;

            /// Portée maximale d'atténuation (Point/Spot uniquement)
            nk_float32 range = 10.f;

            /// Angle intérieur du cône de lumière (Spot, en degrés)
            nk_float32 innerAngle = 15.f;

            /// Angle extérieur du cône (pénombre) pour Spot (en degrés)
            nk_float32 outerAngle = 30.f;

            /// Active le calcul des ombres portées pour cette lumière
            bool castShadow = false;

            /// Active/désactive la lumière sans la supprimer de la liste
            bool enabled = true;
        };

        // =====================================================================
        // 🌍 NkRenderScene — Données de scène pour un frame de rendu
        // =====================================================================
        /**
         * @struct NkRenderScene
         * @brief Regroupe toutes les données nécessaires pour rendre une frame.
         * 
         * 🔹 Rôle :
         *    • Interface CPU→GPU : ce struct est sérialisé/uploadé au GPU chaque frame
         *    • Contient les matrices de vue/projection, les lumières, les paramètres globaux
         * 
         * 🔹 Organisation :
         *    • Caméra : viewMatrix, projMatrix, cameraPos pour le calcul d'éclairage
         *    • Lumières : tableau dynamique de NkLightDesc (max 256 typiquement)
         *    • SunLight : lumière directionnelle principale pour les shadow maps
         *    • Environnement : envMap pour l'IBL, ambientIntensity, time pour les shaders
         * 
         * @note Ce struct est conçu pour être POD ou trivialement copiable
         *       afin de permettre un upload efficace vers le GPU (memcpy, uniform buffer).
         * 
         * @warning La taille totale doit rester raisonnable (< 64KB typiquement)
         *          pour tenir dans un uniform buffer standard. Pour plus de données,
         *          utiliser des buffers de stockage (SSBO) ou des textures.
         * 
         * @example
         * @code
         * NkRenderScene scene;
         * scene.viewMatrix = camera.GetViewMatrix();
         * scene.projMatrix = camera.GetProjMatrix();
         * scene.cameraPos = camera.GetPosition();
         * 
         * scene.lights.PushBack(sunLight);
         * scene.lights.PushBack(pointLight);
         * 
         * scene.envMap = skyboxTextureHandle;
         * scene.time = gameTime;
         * scene.deltaTime = frameTime;
         * 
         * renderer.UpdateSceneData(scene);  // Upload au GPU
         * @endcode
         */
        struct NkRenderScene {
            // ── Caméra principale ─────────────────────────────────────────
            /// Matrice de vue (monde → espace caméra)
            NkMat4f viewMatrix;
            /// Matrice de projection (espace caméra → clip space)
            NkMat4f projMatrix;
            /// Position de la caméra en espace monde (pour l'éclairage view-dependent)
            NkVec3f cameraPos;

            // ── Lumières dynamiques ──────────────────────────────────────
            /// Liste des lumières actives dans la scène (max typique : 256)
            NkVector<NkLightDesc> lights;

            // ── Lumière directionnelle principale (soleil) ───────────────
            /// Description de la lumière solaire pour les shadow maps globales
            NkLightDesc sunLight;
            /// Flag indiquant si sunLight est actif et doit être utilisé
            bool hasSunLight = false;

            // ── Environnement et paramètres globaux ──────────────────────
            /// Handle de la cubemap d'environnement pour l'IBL (Image-Based Lighting)
            NkTextureHandle envMap;
            /// Intensité de la lumière ambiante globale [0,1]
            nk_float32 ambientIntensity = 0.2f;
            /// Temps de jeu en secondes (pour les animations, effets temporels)
            nk_float32 time = 0.f;
            /// Durée de la frame précédente en secondes (pour le frame-independent)
            nk_float32 deltaTime = 0.f;
        };

        // =====================================================================
        // 👤 Vertex standards pour le pipeline graphique
        // =====================================================================
        /**
         * @struct NkVertex
         * @brief Layout de vertex standard pour le rendu 3D.
         * 
         * 🔹 Layout mémoire (ordre important pour l'alignement GPU) :
         *    • position : NkVec3f (12 bytes) — position en espace objet
         *    • normal : NkVec3f (12 bytes) — normale pour l'éclairage
         *    • uv : NkVec2f (8 bytes) — coordonnées de texture
         *    • tangent : NkVec4f (16 bytes) — espace tangent pour normal mapping
         *                xyz = vecteur tangent, w = signe du bitangent
         * 
         * 🔹 Taille totale : 48 bytes — aligné sur 16 bytes pour performances GPU.
         * 
         * @note Ce layout est compatible avec la majorité des moteurs et formats
         *       d'import (FBX, glTF, OBJ). Pour des besoins spécifiques,
         *       définir un layout custom et l'uploader via NkVertexBufferDesc.
         * 
         * @warning L'ordre des champs doit correspondre exactement au layout
         *          déclaré dans le vertex shader (location/bindings).
         *          Un mismatch cause un rendu corrompu ou un crash.
         */
        struct NkVertex {
            /// Position du vertex en espace objet/local
            NkVec3f position;
            /// Normale du vertex (pour l'éclairage de Phong/Blinn)
            NkVec3f normal;
            /// Coordonnées de texture (u,v) pour le mapping
            NkVec2f uv;
            /// Espace tangent pour normal mapping : xyz=tangent, w=signe du bitangent
            NkVec4f tangent;
        };

        /**
         * @struct NkSkinnedVertex
         * @brief Extension de NkVertex pour l'animation squelettique (skinning).
         * 
         * 🔹 Champs additionnels :
         *    • boneIds[4] : indices des 4 os influençant ce vertex (uint32)
         *    • boneWeights[4] : poids d'influence de chaque os (float [0,1], somme = 1)
         * 
         * 🔹 Taille totale : 48 (base) + 16 (ids) + 16 (weights) = 80 bytes.
         * 
         * @note Le skinning est typiquement effectué dans le vertex shader :
         *       position_world = Σ weight[i] * (boneMatrix[i] * position_local)
         * 
         * @warning Les boneIds doivent être valides (index dans le tableau des matrices d'os).
         *          Les weights doivent être normalisés (somme = 1) pour éviter les artefacts.
         */
        struct NkSkinnedVertex : public NkVertex {
            /// Indices des 4 os influençant ce vertex (0 = aucun, ou index dans bonePalette)
            nk_uint32 boneIds[4] = {};
            /// Poids d'influence de chaque os (doivent sommer à 1.0)
            nk_float32 boneWeights[4] = {};
        };

        // =====================================================================
        // 📊 Statistiques de rendu pour le profiling
        // =====================================================================
        /**
         * @struct NkRendererStats
         * @brief Métriques de performance collectées pendant le rendu d'une frame.
         * 
         * 🔹 Catégories :
         *    • drawCalls* : nombre d'appels de dessin (coût CPU de submission)
         *    • triangles/vertices : charge géométrique (coût GPU de rasterization)
         *    • batchCount : nombre de batches (optimisation : moins = mieux)
         *    • textureBinds/pipelineChanges : changements d'état (coût GPU)
         *    • gpuTimeMs/cpuTimeMs : temps passé sur chaque processeur
         *    • gpuMemoryBytes : mémoire GPU utilisée (textures, buffers, etc.)
         * 
         * @note Ces stats sont typiquement collectées via des queries GPU (timestamp)
         *       et des compteurs CPU. Leur précision dépend du backend et du hardware.
         * 
         * @warning La collecte de stats a un coût — désactiver en production
         *          ou échantillonner périodiquement pour minimiser l'overhead.
         * 
         * @example
         * @code
         * NkRendererStats stats = renderer.GetFrameStats();
         * if (stats.gpuTimeMs > 16.6f) {  // > 60 FPS target
         *     printf("Frame trop lente : %.2fms GPU\n", stats.gpuTimeMs);
         *     // Activer un mode LOD plus agressif, réduire la résolution, etc.
         * }
         * @endcode
         */
        struct NkRendererStats {
            /// Nombre total d'appels de dessin (draw calls)
            nk_uint32 drawCalls = 0;
            /// Dont draw calls pour le rendu 2D/UI
            nk_uint32 drawCalls2D = 0;
            /// Dont draw calls pour le rendu 3D
            nk_uint32 drawCalls3D = 0;
            /// Nombre total de triangles rasterisés
            nk_uint32 triangles = 0;
            /// Nombre total de vertices traités
            nk_uint32 vertices = 0;
            /// Nombre de batches (groupes d'objets rendus en un draw call)
            nk_uint32 batchCount = 0;
            /// Nombre de changements de texture (coût de bind)
            nk_uint32 textureBinds = 0;
            /// Nombre de changements de pipeline/shader (coût élevé)
            nk_uint32 pipelineChanges = 0;
            /// Temps GPU de la frame en millisecondes (query timestamp)
            nk_float32 gpuTimeMs = 0.f;
            /// Temps CPU de préparation de la frame en millisecondes
            nk_float32 cpuTimeMs = 0.f;
            /// Mémoire GPU totale utilisée en bytes (textures, buffers, RT)
            nk_uint64 gpuMemoryBytes = 0;
        };

    } // namespace renderer
} // namespace nkentseu