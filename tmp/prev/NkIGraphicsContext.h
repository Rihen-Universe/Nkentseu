#pragma once
// =============================================================================
// NkIGraphicsContext.h
// Interface abstraite commune à tous les contextes graphiques.
//
// Chaque backend (OpenGL, Vulkan, DX11, DX12, Metal, Software) implémente
// cette interface. L'application ne manipule que ce pointeur.
//
// Cycle de vie imposé :
//   Initialize()  → présente, appel unique
//   BeginFrame()  → avant tout rendu de la frame
//   EndFrame()    → après tout rendu, avant Present
//   Present()     → swap / présentation
//   Shutdown()    → libération des ressources, avant destruction
//
// Les méthodes de rendu métier (draw calls, pipelines, shaders…) sont dans
// des interfaces filles spécialisées (NkIRenderer) — hors scope de ce fichier.
// Ici on gère UNIQUEMENT le contexte et la surface.
// =============================================================================

#include "NkGraphicsApi.h"
#include "NkContextDesc.h"
#include "NKWindow/Core/NkSurface.h" // NkSurfaceDesc
#include "NKCore/NkTypes.h"

namespace nkentseu {

    class NkWindow;

    // -------------------------------------------------------------------------
    // NkSwapchainConfig — paramètres de redimensionnement
    // -------------------------------------------------------------------------
    struct NkSwapchainConfig {
        uint32 width    = 0;
        uint32 height   = 0;
        bool   vsync    = true;
    };

    // -------------------------------------------------------------------------
    // NkContextInfo — informations retournées après Init (debug / log)
    // -------------------------------------------------------------------------
    struct NkContextInfo {
        NkGraphicsApi api        = NkGraphicsApi::NK_API_NONE;
        const char*   renderer   = "";   // ex: "NVIDIA GeForce RTX 4090"
        const char*   vendor     = "";   // ex: "NVIDIA Corporation"
        const char*   version    = "";   // ex: "4.6.0 NVIDIA 546.01"
        uint32        vramMB     = 0;    // VRAM estimée en Mo (0 si inconnu)
        bool          debugMode  = false;
        bool          computeSupported = false;
    };

    // -------------------------------------------------------------------------
    // NkIGraphicsContext — interface pure
    // -------------------------------------------------------------------------
    class NkIGraphicsContext {
    public:
        virtual ~NkIGraphicsContext() = default;

        // --- Cycle de vie ---

        /// Initialise le contexte graphique à partir de la surface de la fenêtre.
        /// Doit être appelé UNE SEULE FOIS après NkWindow::Create().
        /// @return false si l'initialisation échoue (API non supportée, driver absent…)
        virtual bool Initialize(const NkWindow& window, const NkContextDesc& desc) = 0;

        /// Libère toutes les ressources GPU. Appelé avant la destruction.
        virtual void Shutdown() = 0;

        /// Indique si le contexte est prêt à rendre.
        virtual bool IsValid() const = 0;

        // --- Boucle de rendu ---

        /// Début de frame — acquire swapchain image (Vulkan/DX12), bind framebuffer (GL).
        /// @return false si la surface est perdue (redimensionnement, minimisation)
        virtual bool BeginFrame() = 0;

        /// Fin de frame — soumet les command buffers (Vulkan/DX12), flush (GL).
        virtual void EndFrame() = 0;

        /// Présentation — SwapBuffers (GL/WGL/GLX), Present (DXGI), presentDrawable (Metal).
        virtual void Present() = 0;

        // --- Gestion de la surface ---

        /// Appelé quand la fenêtre est redimensionnée.
        /// Recrée le swapchain / framebuffer avec les nouvelles dimensions.
        virtual bool OnResize(uint32 width, uint32 height) = 0;

        /// Active ou désactive la synchronisation verticale.
        virtual void SetVSync(bool enabled) = 0;
        virtual bool GetVSync() const = 0;

        // --- Informations ---

        virtual NkGraphicsApi  GetApi()  const = 0;
        virtual NkContextInfo  GetInfo() const = 0;
        virtual NkContextDesc  GetDesc() const = 0;
        virtual bool           SupportsCompute() const = 0;

        // --- Accès au contexte natif (pour les extensions / interop) ---
        // Retourne un pointeur opaque vers les données internes du backend.
        // Cast selon l'API :
        //   OpenGL  → NkOpenGLContextData*
        //   Vulkan  → NkVulkanContextData*
        //   DX11    → NkDX11ContextData*
        //   DX12    → NkDX12ContextData*
        //   Metal   → NkMetalContextData*
        //   Software→ NkSoftwareContextData*
        virtual void* GetNativeContextData() = 0;
    };

} // namespace nkentseu
