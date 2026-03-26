#pragma once
/**
 * @File    NkUIRenderer.h
 * @Brief   Interface abstraite NkUIRenderer + implémentation CPU (offline).
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkUIRenderer est l'interface que chaque backend implémente.
 *  L'utilisateur instancie le backend adapté et appelle Submit(ctx).
 *
 *  Backends fournis :
 *    NkUICPURenderer  — rendu offline via NKImage (pas de GPU)
 *    NkUIOpenGLRenderer — GPU OpenGL (à implémenter côté utilisateur)
 *
 *  Le CPU renderer est self-contained : il produit une NkImage RGBA32
 *  qu'on peut sauvegarder, afficher ou transmettre à n'importe quelle API.
 *
 *  Pour un rendu GPU custom, l'utilisateur hérite de NkUIRenderer et
 *  implémente les 3 méthodes virtuelles. La DrawList contient des
 *  vertex/index buffers standard et des draw calls avec textures.
 */
#include "NkUI/NkUIContext.h"

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────────────
        //  Interface abstraite
        // ─────────────────────────────────────────────────────────────────────────────

        class NKUI_API NkUIRenderer {
        public:
            virtual ~NkUIRenderer() noexcept = default;

            /// Prépare le frame (resize si nécessaire, clear)
            virtual void BeginFrame(int32 w, int32 h) noexcept = 0;

            /// Soumet toutes les DrawLists du contexte
            virtual void Submit(const NkUIContext& ctx)  noexcept = 0;

            /// Finalise le frame (présentation, flush)
            virtual void EndFrame() noexcept = 0;

            /// Optionnel : upload d'une texture, retourne un ID opaque
            virtual uint32 UploadTexture(const uint8* pixels,
                                        int32 w, int32 h, int32 channels) noexcept { return 0; }
            virtual void   FreeTexture(uint32 id) noexcept { (void)id; }
            virtual uint32 GetFontAtlas() const noexcept { return 0; }
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUICPURenderer — rendu offline via pixel buffer
        //  Pas de dépendance GPU. Produit une NkImage RGBA32.
        // ─────────────────────────────────────────────────────────────────────────────

        class NKUI_API NkUICPURenderer final : public NkUIRenderer {
        public:
            NkUICPURenderer() noexcept = default;
            ~NkUICPURenderer() noexcept override;

            /// Initialise avec la taille du viewport
            bool Init(int32 w, int32 h) noexcept;
            void Destroy() noexcept;

            void BeginFrame(int32 w, int32 h) noexcept override;
            void Submit(const NkUIContext& ctx)  noexcept override;
            void EndFrame() noexcept override;

            /// Accède au pixel buffer résultant (après EndFrame)
            const uint8* GetPixels() const noexcept { return mPixels; }
            int32  GetWidth()  const noexcept { return mW; }
            int32  GetHeight() const noexcept { return mH; }
            usize  GetStride() const noexcept { return static_cast<usize>(mW)*4; }

            /// Sauvegarde le rendu (nécessite NKImage)
            bool SavePNG(const char* path) const noexcept;

        private:
            uint8*  mPixels = nullptr;
            int32   mW = 0, mH = 0;

            /// Soumet une DrawList vers le pixel buffer
            void SubmitDrawList(const NkUIDrawList& dl) noexcept;

            /// Rastérise un triangle dans le pixel buffer
            void DrawTriangle(const NkUIVertex& v0,
                            const NkUIVertex& v1,
                            const NkUIVertex& v2,
                            NkRect clipRect) noexcept;

            /// Blending source-over
            void BlendPixel(int32 x, int32 y,
                            uint8 r, uint8 g, uint8 b, uint8 a) noexcept;

            /// Échantillonnage bilinéaire de texture
            NkColor SampleTexture(uint32 texId,
                                float32 u, float32 v) const noexcept;
        };
    }
} // namespace nkentseu
