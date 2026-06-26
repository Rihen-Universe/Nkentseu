#pragma once
// =============================================================================
// NkRenderWindow.h — NkRenderTarget concret : rendu dans une NkWindow
//
// Equivalent de sf::RenderWindow. Wrappe :
//   - une NkWindow& (la fenetre cible)
//   - un NkIGraphicsContext* cree via NkContextFactory (le contexte GPU lie a
//     la fenetre, choisit l'API : OpenGL / Vulkan / DX11 / DX12 / Software)
//   - un NkIRenderer2D* cree via NkRenderer2DFactory (le moteur de rendu 2D
//     du backend correspondant)
//
// LIFECYCLE
//   Constructor : cree contexte + renderer. Detruits en sequence inverse au
//   destructor (renderer d'abord, puis contexte). Auto-resize : le swapchain
//   doit etre recree quand la fenetre change de taille (callee externe via
//   NkWindowResizeEvent / NkWindowDpiEvent — voir aussi project_session_20260529_d_harmony).
//
// CHOIX D'API
//   Par defaut, on prend le NkGraphicsApi de NkContextDesc fourni. Si tu veux
//   un fallback automatique (essaie Vulkan puis OpenGL puis Software), utilise
//   NkContextFactory::CreateWithFallback (ctor surcharge a venir).
//
// USAGE
//   NkContextDesc desc;
//   desc.api = NkGraphicsApi::NK_OPENGL;
//   NkRenderWindow target(window, desc);
//   while (open) {
//       target.Clear({30, 30, 30, 255});
//       target.Draw(sprite);
//       target.Display();
//   }
// =============================================================================

#include "NKCanvas/Renderer/Targets/NkRenderTarget.h"
#include "NKCanvas/Renderer/Core/NkRenderer2D.h"   // facade en valeur
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKMemory/NkUniquePtr.h"

namespace nkentseu {

    class NkWindow;
    class NkIGraphicsContext;

    namespace renderer {

        class NkIRenderer2D;

        class NkRenderWindow : public NkRenderTarget {
            public:
                /// Cree contexte + renderer pour la NkWindow donnee, avec la
                /// configuration NkContextDesc (API graphique, swap interval, …).
                /// CHEMIN NORMAL : NKCanvas possede et detruit le contexte.
                NkRenderWindow(NkWindow& window, const NkContextDesc& desc) noexcept;

                /// Variante AVANCEE : l'utilisateur fournit lui-meme le
                /// NkIGraphicsContext (deja Initialize()). NKCanvas cree juste le
                /// Renderer2D par-dessus et NE POSSEDE PAS le contexte (l'appelant
                /// reste responsable de son Shutdown()/destruction, APRES celle du
                /// NkRenderWindow). Utile quand l'app veut piloter elle-meme la
                /// creation du contexte (partage multi-target, backend custom, etc.).
                NkRenderWindow(NkWindow& window, NkIGraphicsContext* externalContext) noexcept;

                /// Destructor : detruit renderer puis contexte dans l'ordre (le
                /// contexte uniquement s'il est possede — cf. ctor a contexte fourni).
                ~NkRenderWindow() noexcept override;

                NkRenderWindow(const NkRenderWindow&)            = delete;
                NkRenderWindow& operator=(const NkRenderWindow&) = delete;

                // ── Validite ───────────────────────────────────────────────────
                /// Retourne true si le contexte + renderer ont ete cree avec succes.
                bool IsValid() const noexcept;

                // ── NkRenderTarget : implementations virtuelles ────────────────

                void Clear(const NkColor2D& color = NkColor2D::Black) override;
                void Display() override;

                void     SetView(const NkView2D& view) override;
                NkView2D GetView()        const override;
                NkView2D GetDefaultView() const override;

                void     SetViewport(NkRect2i viewport) override;
                NkRect2i GetViewport() const override;

                math::NkVec2u GetSize() const override;

                // ── Re-expose les surcharges Draw du parent NkRenderTarget ─────
                // (name-hiding C++ : declarer un override de Draw(raw vertices)
                // cache les autres signatures heritees ; `using` les ramene).
                using NkRenderTarget::Draw;

                void Draw(const NkVertex* vertices, uint32 count,
                          NkPrimitiveType primitive,
                          const NkRenderStates& states = NkRenderStates::Default()) override;

                NkVec2f MapPixelToCoords(NkVec2i pixel) const override;
                NkVec2i MapCoordsToPixel(NkVec2f point) const override;

                NkIRenderer2D*       GetRenderer()       noexcept override { return mRenderer.Get(); }
                const NkIRenderer2D* GetRenderer() const noexcept override { return mRenderer.Get(); }

                NkRenderer2D&        GetRenderer2D()       noexcept override { return mFacade; }
                const NkRenderer2D&  GetRenderer2D() const noexcept override { return mFacade; }

                /// Capture le backbuffer presente vers `path` (format selon extension
                /// via NkImage). Readback specifique au backend (DX11 ; autres a venir).
                /// A appeler apres Display(). Cf. NkRenderWindowCapture.cpp.
                bool Capture(const char* path) const override;

                // ── Acces avance ───────────────────────────────────────────────
                NkIGraphicsContext*       GetContext()       noexcept { return mContext; }
                const NkIGraphicsContext* GetContext() const noexcept { return mContext; }
                NkWindow&                 GetWindow()        noexcept { return *mWindow; }
                const NkWindow&           GetWindow()  const noexcept { return *mWindow; }

                // ── Swapchain recreation (a appeler par l'app sur events) ──────
                //
                // L'app doit ecouter NkWindowResizeEvent ET NkWindowDpiEvent et
                // appeler OnResize() avec les nouvelles dimensions physiques.
                // Le NkIGraphicsContext recree alors la swapchain en interne
                // (chaque backend : Vulkan vkDeviceWaitIdle + destroy/create
                // swapchain ; OpenGL viewport ; DX11/12 ResizeBuffers ; Metal
                // CAMetalLayer.drawableSize). Voir doctrine DPI dans la memoire
                // session 20260529 : NkWindowDpiEvent et NkWindowResizeEvent
                // doivent appeler la meme routine.

                /// A appeler depuis le handler NkWindowResizeEvent.
                /// width/height en pixels PHYSIQUES (post-DPI).
                bool OnResize(uint32 width, uint32 height) noexcept;

                /// A appeler depuis le handler NkWindowDpiEvent. Recalcule la
                /// taille physique courante via NkWindow::GetSize() et delegue
                /// a OnResize. (Le NkWindow a deja absorbe le DPI change a ce
                /// stade : sa GetSize() retourne la nouvelle taille pixel.)
                bool OnDpiChange() noexcept;

                /// À appeler au retour d'arrière-plan (Android : NkWindowShownEvent).
                /// Recrée la surface de présentation depuis le native window courant
                /// (sinon écran noir après un passage en arrière-plan).
                bool RecreateSurface() noexcept;

            private:
                NkWindow*                          mWindow{nullptr};
                NkIGraphicsContext*                mContext{nullptr};
                memory::NkUniquePtr<NkIRenderer2D> mRenderer;
                NkRenderer2D                       mFacade;          ///< facade user-facing (rebind apres init)
                bool                               mFrameOpen{false}; ///< true entre Begin() et End() interne
                bool                               mOwnsContext{true}; ///< false si contexte fourni par l'utilisateur
        };

    } // namespace renderer
} // namespace nkentseu
