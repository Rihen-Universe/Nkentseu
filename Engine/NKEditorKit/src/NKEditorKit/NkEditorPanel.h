#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorPanel.h
// @Brief   Classe de base d'un panneau d'editeur dockable.
// @Author  Rihen
// @License Proprietary - Free to use and modify
//
// Un NkEditorPanel = une fenetre logique de l'editeur (Explorateur, Inspecteur,
// Console, Viewport...). Le shell la dessine chaque frame quand elle est ouverte,
// flottante ou ancree (le shell gere la bascule). L'application derive cette
// classe et implemente OnUI(). Aucune dependance a NKReflection : le contenu est
// dessine imperativement (les panneaux pilotes par reflexion viendront plus tard,
// quand NKReflection<->NKSerialization sera murie — cf. ECOSYSTEM.md).
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorExport.h"
#include "NKEditorKit/NkEditorContext.h"

namespace nkentseu {
    namespace editorkit {

        class NKEDITORKIT_API NkEditorPanel {
        public:
            explicit NkEditorPanel(const char* title,
                                   NkEditorDockSide defaultSide = NkEditorDockSide::NK_CENTER) noexcept;
            virtual ~NkEditorPanel() = default;

            NkEditorPanel(const NkEditorPanel&)            = delete;
            NkEditorPanel& operator=(const NkEditorPanel&) = delete;

            // ── Identite ────────────────────────────────────────────────────────
            const char* Title() const noexcept { return mTitle; }

            // ── Etat d'ouverture (pilote le menu « Affichage ») ─────────────────
            bool* OpenPtr()       noexcept { return &mOpen; }
            bool  IsOpen()  const noexcept { return mOpen; }
            void  SetOpen(bool o) noexcept { mOpen = o; }

            // ── Docking ─────────────────────────────────────────────────────────
            bool             Dockable()    const noexcept { return mDockable; }
            void             SetDockable(bool d) noexcept { mDockable = d; }
            NkEditorDockSide DefaultSide() const noexcept { return mDefaultSide; }
            void             SetDefaultSide(NkEditorDockSide s) noexcept { mDefaultSide = s; }

            // ── Contenu (implemente par l'application) ──────────────────────────
            // Appele par le shell entre Begin/End (flottant) ou BeginDocked/EndDocked
            // (ancre). Dessiner via les helpers de `ec` (ec.Text, ec.Button, ...).
            virtual void OnUI(NkEditorFrameContext& ec) = 0;

        protected:
            char             mTitle[64]   = {};
            bool             mOpen        = true;
            bool             mDockable    = true;
            NkEditorDockSide mDefaultSide = NkEditorDockSide::NK_CENTER;
        };

    } // namespace editorkit
} // namespace nkentseu
