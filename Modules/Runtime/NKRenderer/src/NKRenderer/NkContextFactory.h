#pragma once
// =============================================================================
// NkContextFactory.h
// Factory centrale pour la création de contextes graphiques.
//
// Usage complet :
//
//   // 1. Préparer le descripteur
//   NkContextDesc desc = NkContextDesc::MakeVulkan(true);  // validation layers
//
//   // Pour OpenGL/GLX uniquement (Linux XLib/XCB) :
//   // NkContextFactory::PrepareWindowConfig(desc, windowConfig);
//   // → Injecte les hints GLX dans windowConfig avant NkWindow::Create()
//
//   // 2. Créer la fenêtre normalement
//   NkWindow window(windowConfig);
//
//   // 3. Créer le contexte
//   auto ctx = NkContextFactory::Create(desc, window);
//   if (!ctx) { /* erreur */ }
//
//   // 4. Boucle de rendu
//   while (window.IsOpen()) {
//       NkEvents::PollEvents();
//       if (ctx->BeginFrame()) {
//           /* draw calls */
//           ctx->EndFrame();
//           ctx->Present();
//       }
//   }
//
//   // 5. Nettoyage
//   ctx->Shutdown();
// =============================================================================

#include "NkIGraphicsContext.h"
#include "NkContextDesc.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include <memory>

namespace nkentseu {

    class NkWindow;

    class NkContextFactory {
    public:
        // -----------------------------------------------------------------------
        // Create — instancie et initialise le contexte correspondant à desc.api.
        //
        // Retourne nullptr si :
        //   - L'API n'est pas disponible sur cette plateforme
        //   - L'initialisation du contexte échoue
        //
        // Le contexte retourné est déjà initialisé (Initialize() a été appelé).
        // -----------------------------------------------------------------------
        static std::unique_ptr<NkIGraphicsContext>
            Create(const NkContextDesc& desc, const NkWindow& window);

        // -----------------------------------------------------------------------
        // PrepareWindowConfig — À appeler AVANT NkWindow::Create()
        //                       pour OpenGL/GLX (Linux XLib / XCB) uniquement.
        //
        // Effectue glXChooseFBConfig et injecte le résultat dans
        // windowConfig.surfaceHints (NK_GLX_VISUAL_ID + NK_GLX_FB_CONFIG_PTR).
        // NkWindow utilisera ces hints pour appeler XCreateWindow avec le bon Visual.
        //
        // Pour toutes les autres APIs (Vulkan, DX, Metal, EGL, Software) :
        // cette méthode est un no-op, il est safe de l'appeler quand même.
        //
        // @return true si les hints ont été injectés avec succès, ou si
        //         l'injection n'était pas nécessaire (autres APIs).
        // -----------------------------------------------------------------------
        static bool PrepareWindowConfig(const NkContextDesc& desc,
                                        NkWindowConfig& outConfig);

        // -----------------------------------------------------------------------
        // GetDefaultApi — retourne l'API graphique la plus performante
        //                 disponible sur la plateforme courante.
        // -----------------------------------------------------------------------
        static NkGraphicsApi GetDefaultApi();

        // -----------------------------------------------------------------------
        // IsApiSupported — vérifie à l'exécution si une API est disponible.
        //                  Plus précis que NkGraphicsApiIsAvailable() qui est
        //                  statique à la compilation.
        //                  (ex: Vulkan compilé mais driver absent → false)
        // -----------------------------------------------------------------------
        static bool IsApiSupported(NkGraphicsApi api);

    private:
        NkContextFactory() = delete;

        // Helpers d'injection des hints GLX
#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        static bool InjectGLXHints(const NkOpenGLDesc& gl, NkWindowConfig& config);
#endif
    };

} // namespace nkentseu
