#pragma once
// -----------------------------------------------------------------------------
// @File    NkIEditorRenderer.h
// @Brief   Interface du backend de RENDU de la coquille d'editeur.
// @License Proprietary - Free to use and modify
//
// NkEditorShell delegue TOUT le rendu (creation du contexte GPU, frame, soumission
// des draw-lists NKGui, upload des atlas de police/images) a un NkIEditorRenderer.
// Ainsi la coquille (menus / docking / palette / panneaux) est INDEPENDANTE du
// systeme de rendu :
//   - IDE (NKCode)            -> impl NKCanvas (NkEditorCanvasRenderer, defaut),
//   - app anim / moteur de jeu-> impl NKRHI/NKRenderer (fournie par l'app).
//
// L'interface n'expose QUE des types NKGui / NKWindow purs : NKEditorKit reste
// « 2D pur » (aucune dependance NKRHI ni NKRenderer ; l'impl NKRHI vit AILLEURS).
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorExport.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKMath/NKMath.h"                  // math::NkVec2u

namespace nkentseu {

    class NkWindow;   // forward (NKWindow)

    namespace editorkit {

        // Choix d'API graphique NEUTRE (decouple de NKCanvas ET NKRHI : leurs enums
        // NkGraphicsApi se dupliquent dans le namespace nkentseu et ne peuvent
        // cohabiter dans un meme TU). Chaque impl mappe vers son propre enum.
        enum class NkEditorGfxApi : uint8 {
            Auto = 0, OpenGL, Vulkan, DX11, DX12, Software
        };

        // Backend de rendu de la coquille. Possede le contexte GPU + le backend de
        // draw-lists NKGui. La FENETRE reste possedee par le shell (passee a Init).
        class NKEDITORKIT_API NkIEditorRenderer {
            public:
                virtual ~NkIEditorRenderer() = default;

                // Cree le contexte GPU lie a `window` + le backend de draw-lists.
                // `api` = API demandee (Auto -> choix par defaut de l'impl).
                virtual bool Init(NkWindow& window, NkEditorGfxApi api) = 0;
                virtual void Shutdown() = 0;
                virtual bool IsValid() const = 0;

                // Taille du framebuffer courant (px).
                virtual math::NkVec2u Size() const = 0;
                virtual void OnResize(uint32 width, uint32 height) = 0;

                // Cycle de frame : BeginFrame (begin + clear) -> SubmitDrawList(s) -> EndFrame (present).
                virtual void BeginFrame() = 0;
                virtual void SubmitDrawList(const nkgui::NkGuiDrawList& dl, uint32 fbW, uint32 fbH) = 0;
                virtual void EndFrame() = 0;

                // Upload des textures referencees par texId dans les draw-lists.
                // Gray8 = atlas de police (etendu en RGBA blanc + alpha) ; RGBA8 = image.
                virtual bool UploadFontGray8(uint32 texId, const uint8* pixels, int32 w, int32 h) = 0;
                virtual bool UploadImageRGBA(uint32 texId, const uint8* pixels, int32 w, int32 h) = 0;
        };

    } // namespace editorkit
} // namespace nkentseu
