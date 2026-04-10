// -----------------------------------------------------------------------------
// @File    NkUIRenderer.h
// @Brief   Interface abstraite NkUIRenderer + implémentation CPU (offline).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis
// @License Apache-2.0
//
// @Design
//  NkUIRenderer est l'interface que chaque backend implémente.
//  L'utilisateur instancie le backend adapté et appelle Submit(ctx).
//
//  Backends fournis :
//    NkUICPURenderer  — rendu offline via NKImage (pas de GPU)
//    NkUIOpenGLRenderer — GPU OpenGL (à implémenter côté utilisateur)
//
//  Le CPU renderer est self-contained : il produit une NkImage RGBA32
//  qu'on peut sauvegarder, afficher ou transmettre à n'importe quelle API.
//
//  Pour un rendu GPU custom, l'utilisateur hérite de NkUIRenderer et
//  implémente les 3 méthodes virtuelles. La DrawList contient des
//  vertex/index buffers standard et des draw calls avec textures.
// -----------------------------------------------------------------------------

#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: CPU/reference renderer interface and structures.
 * Main data: Renderer abstraction consuming NkUIDrawList commands.
 * Change this file when: New render backends need contract updates.
 */

#include "NKUI/NkUIContext.h"

namespace nkentseu
{
    namespace nkui
    {
        // ============================================================
        // Interface abstraite NkUIRenderer
        // ============================================================

        class NKUI_API NkUIRenderer
        {
            public:
                virtual ~NkUIRenderer() noexcept = default;

                /// Prépare le frame (redimensionnement si nécessaire, effacement)
                virtual void BeginFrame(int32 w, int32 h) noexcept = 0;

                /// Soumet toutes les DrawLists du contexte
                virtual void Submit(const NkUIContext& ctx) noexcept = 0;

                /// Finalise le frame (présentation, vidage des tampons)
                virtual void EndFrame() noexcept = 0;

                /// Optionnel : télécharge une texture, retourne un ID opaque
                virtual uint32 UploadTexture(const uint8* pixels,
                                             int32 w,
                                             int32 h,
                                             int32 channels) noexcept
                {
                    return 0;
                }

                /// Libère une texture
                virtual void FreeTexture(uint32 id) noexcept
                {
                    (void)id;
                }

                /// Retourne l'ID de la texture de l'atlas de polices
                virtual uint32 GetFontAtlas() const noexcept
                {
                    return 0;
                }
        };

        // ============================================================
        // NkUICPURenderer — rendu logiciel (offline) via pixel buffer
        // Pas de dépendance GPU. Produit une image RGBA32.
        // ============================================================

        class NKUI_API NkUICPURenderer final : public NkUIRenderer
        {
            public:
                NkUICPURenderer() noexcept = default;
                ~NkUICPURenderer() noexcept override;

                /// Initialise le renderer avec la taille du viewport
                bool Init(int32 w, int32 h) noexcept;

                /// Libère les ressources
                void Destroy() noexcept;

                void BeginFrame(int32 w, int32 h) noexcept override;
                void Submit(const NkUIContext& ctx) noexcept override;
                void EndFrame() noexcept override;

                /// Accède au tampon de pixels après EndFrame()
                const uint8* GetPixels() const noexcept
                {
                    return mPixels;
                }

                int32 GetWidth() const noexcept
                {
                    return mW;
                }

                int32 GetHeight() const noexcept
                {
                    return mH;
                }

                usize GetStride() const noexcept
                {
                    return static_cast<usize>(mW) * 4;
                }

                /// Sauvegarde le rendu au format PNG (nécessite NKImage)
                bool SavePNG(const char* path) const noexcept;

            private:
                uint8* mPixels = nullptr;   // Tampon RGBA
                int32  mW = 0;              // Largeur en pixels
                int32  mH = 0;              // Hauteur en pixels

                /// Soumet une DrawList vers le tampon de pixels
                void SubmitDrawList(const NkUIDrawList& dl) noexcept;

                /// Rastérise un triangle dans le tampon de pixels
                void DrawTriangle(const NkUIVertex& v0,
                                  const NkUIVertex& v1,
                                  const NkUIVertex& v2,
                                  NkRect clipRect) noexcept;

                /// Mélange source-over dans le tampon
                void BlendPixel(int32 x, int32 y,
                                uint8 r, uint8 g, uint8 b, uint8 a) noexcept;

                /// Échantillonnage bilinéaire de texture (simplifié)
                NkColor SampleTexture(uint32 texId,
                                      float32 u,
                                      float32 v) const noexcept;
        };

    } // namespace nkui
} // namespace nkentseu