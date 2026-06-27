#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorShell.h
// @Brief   Coquille d'application d'editeur : fenetre + docking + panneaux.
// @Author  Rihen
// @License Proprietary - Free to use and modify
//
// NkEditorShell est la base reutilisable des editeurs Nkentseu (NKCode = IDE,
// et plus tard Nogee = editeur de moteur). Elle POSSEDE :
//   - la fenetre (NKWindow) + la cible de rendu (NKCanvas/NkRenderWindow),
//   - le contexte NKGui (qui contient fenetres/dock/layout/police/draw lists),
//   - la barre de menus (Fichier/Affichage/Fenetre + menu applicatif optionnel),
//   - la palette de commandes (Ctrl+P),
//   - la boucle principale (events -> frame -> docking -> panneaux -> rendu).
//
// L'application N'A QU'A : creer le shell, enregistrer ses panneaux/commandes,
// appeler Run(). Pipeline NKGui 2D pur (NKCanvas), independant du moteur 3D.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorExport.h"
#include "NKEditorKit/NkEditorContext.h"
#include "NKEditorKit/NkEditorPanel.h"
#include "NKEditorKit/NkEditorCommand.h"

#include "NKWindow/NKWindow.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkClock.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/UI/NkGuiCanvasBackend.h"
#include "NKGui/NKGui.h"
#include "NKMemory/NkUniquePtr.h"

namespace nkentseu {
    namespace editorkit {

        // Parametres de creation du shell.
        struct NKEDITORKIT_API NkEditorShellConfig {
            const char*    title       = "Nkentseu Editor";
            uint32         width       = 1280;
            uint32         height      = 720;
            bool           resizable   = true;
            NkGraphicsApi  graphicsApi = NkGraphicsApi::NK_GFX_API_AUTO;
        };

        // Menu applicatif optionnel : appele a l'interieur de la barre de menus,
        // apres « Fenetre », pour que l'app ajoute ses propres menus.
        using NkEditorAppMenuFn = void(*)(NkEditorFrameContext& ec, void* user);

        class NKEDITORKIT_API NkEditorShell {
        public:
            static constexpr int32 MAX_PANELS   = 64;
            static constexpr int32 MAX_COMMANDS = 128;

            NkEditorShell() = default;
            ~NkEditorShell();

            NkEditorShell(const NkEditorShell&)            = delete;
            NkEditorShell& operator=(const NkEditorShell&) = delete;

            // ── Cycle de vie ────────────────────────────────────────────────────
            bool Init(const NkEditorShellConfig& config) noexcept;
            int  Run() noexcept;        ///< boucle bloquante ; retourne le code de sortie
            void RequestClose() noexcept { mRunning = false; }

            // ── Enregistrement (le shell NE POSSEDE PAS les panneaux) ───────────
            bool AddPanel(NkEditorPanel* panel) noexcept;
            bool RegisterCommand(const char* name, NkEditorCommandFn fn,
                                 void* user = nullptr, const char* shortcut = "") noexcept;
            void SetAppMenu(NkEditorAppMenuFn fn, void* user = nullptr) noexcept {
                mAppMenuFn = fn; mAppMenuUser = user;
            }
            // Barre d'outils horizontale (sous la barre de titre, facon Visual Studio) :
            // l'app y dessine config/plateforme cible + bouton Build/Run + emulateur.
            void SetToolbar(NkEditorAppMenuFn fn, void* user = nullptr) noexcept {
                mToolbarFn = fn; mToolbarUser = user;
            }

            // ── Layout ──────────────────────────────────────────────────────────
            void ResetLayout() noexcept { mDockBootstrap = true; }
            // Persistance de disposition : à recâbler sur la sérialisation d'arbre NKGui
            // (TODO) ; stubs no-op en attendant pour conserver l'API.
            void SaveLayout(const char* path) noexcept { (void)path; }
            bool LoadLayout(const char* path) noexcept { (void)path; return false; }

            // ── Barre d'etat (footer VSCode) : texte gauche/droite mis par l'app ─
            void SetFooter(const char* left, const char* right = "") noexcept;
            // ── Infos centrees dans la barre de titre (ex. fichier actif) ────────
            void SetTitleInfo(const char* center) noexcept;

            // ── Acces (pour besoins avances) ────────────────────────────────────
            nkgui::NkGuiContext& Ui() noexcept { return mUI; }

        private:
            void BuildMenuBar(NkEditorFrameContext& ec, const nkgui::NkRect& rect) noexcept;
            void DrawTitleBar(NkEditorFrameContext& ec, const nkgui::NkRect& bar) noexcept;
            void DrawToolbar(NkEditorFrameContext& ec, const nkgui::NkRect& rect) noexcept;
            void HandleEdgeResize(float32 W, float32 H) noexcept;
            void DrawPanels(NkEditorFrameContext& ec) noexcept;
            void BootstrapDocking() noexcept;
            void DrawCommandPalette(NkEditorFrameContext& ec) noexcept;
            void ExecuteCommand(int32 index) noexcept;
            void HookEvents() noexcept;
            void MapEditKey(NkKey k, bool down) noexcept;
            void TryRunShortcut(NkKey k, bool shift) noexcept;
            void DrawActivityBar(const nkgui::NkRect& bar) noexcept;
            void DrawStatusBar(float32 footerH) noexcept;

            // === Fenetre / rendu ===
            NkWindow                                       mWindow;
            memory::NkUniquePtr<renderer::NkRenderWindow>  mRenderTarget;
            renderer::NkGuiCanvasBackend                   mBackend;

            // === NKGui (contexte + police possedee) ===
            nkgui::NkGuiContext mUI;
            nkgui::NkGuiFont    mFont;
            bool                mFontOk = false;

            // === Panneaux / commandes ===
            NkEditorPanel*    mPanels[MAX_PANELS]     = {};
            int32             mNumPanels              = 0;
            NkEditorCommand   mCommands[MAX_COMMANDS] = {};
            int32             mNumCommands            = 0;
            NkEditorAppMenuFn mAppMenuFn              = nullptr;
            void*             mAppMenuUser            = nullptr;
            NkEditorAppMenuFn mToolbarFn              = nullptr;
            void*             mToolbarUser            = nullptr;

            // === Palette de commandes ===
            bool  mPaletteOpen = false;
            int32 mPaletteSel  = 0;

            // === Barre de titre custom + footer + activity bar ===
            char  mTitle[160]       = {};
            char  mTitleCenter[200] = {};
            char  mFooterLeft[256]  = {};
            char  mFooterRight[128] = {};
            int32 mActivityIndex    = 0;     // icone selectionnee dans l'activity bar

            // === Etat boucle ===
            NkClock       mClock;
            bool          mRunning       = true;
            bool          mDockBootstrap = true;
            NkGraphicsApi mGraphicsApi   = NkGraphicsApi::NK_GFX_API_AUTO;
            uint32        mLastWidth     = 0;
            uint32        mLastHeight    = 0;
        };

    } // namespace editorkit
} // namespace nkentseu
