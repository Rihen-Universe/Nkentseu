#pragma once
// =============================================================================
// ViewerApp.h
// -----------------------------------------------------------------------------
// Application principale NkImageDemo : viewer minimaliste de fichiers image
// charges via NkImage::Load(). Affiche une image plein ecran (ratio preserve)
// avec un HUD bas affichant nom + format + dimensions + nb canaux.
// Navigation : fleches GAUCHE / DROITE entre fichiers, ECHAP pour quitter.
// =============================================================================

#include "Demo/Render/GLContext.h"
#include "Demo/Render/GLRenderer2D.h"
#include "Demo/Render/FontAtlas.h"
#include "Demo/Render/Texture2D.h"
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include <atomic>

namespace nkentseu
{
    class NkWindow;
    class NkEvent;
    class NkImage;
}

namespace nkentseu
{
    namespace demo
    {

        /// Metadonnees d'une image chargee (extraites a la volee).
        struct ImageMeta
        {
            NkString fileName;
            NkString fullPath;
            NkString format;       ///< "PNG", "JPEG", "SVG", ...
            int      width    = 0;
            int      height   = 0;
            int      channels = 0;
            bool     animated = false;
            int      frameCount = 1;
        };

        class ViewerApp
        {
        public:
            explicit ViewerApp(NkWindow& window) noexcept : mWindow(window) {}
            ~ViewerApp() = default;

            bool Init();
            void Shutdown();
            void OnResize(uint32 w, uint32 h);
            void Update(float dt);
            void Render();
            void OnEvent(NkEvent& ev);
            void OnPause();
            void OnResume();
            bool RecreateSurface();

            bool WantsQuit() const noexcept { return mQuit; }
            void RequestQuit() noexcept     { mQuit = true; }

        private:
            void ScanFolder(const NkString& folder);
            void LoadCurrent();
            void NavigateNext(int delta);

            NkWindow&             mWindow;
            GLContext             mGL;
            GLRenderer2D          mRenderer;
            FontAtlas             mFont;

            // Catalogue d'images du dossier scannee.
            NkVector<NkString>    mFiles;
            int                   mCurrentIdx = 0;

            // Image courante (texture GL + meta).
            Texture2D             mTexture;
            ImageMeta             mMeta;
            bool                  mLoaded   = false;

            // ── Animation GIF (multi-frame) ──────────────────────────────────
            // Quand on charge un .gif anime, on decode toutes les frames via
            // NkGIFCodec::DecodeAnimation et on les upload en N textures GL.
            // La frame affichee avance selon mAnimDelays[i] (milliseconds).
            NkVector<Texture2D*>  mAnimTextures;   ///< N textures, owned
            NkVector<uint32>      mAnimDelaysMs;   ///< delais par frame
            int                   mAnimFrame  = 0; ///< frame courante
            float                 mAnimAccum  = 0.0f; ///< temps cumule sur la frame courante (sec)
            uint16                mAnimLoopCount = 0; ///< 0 = infini

            void ClearAnimation();

            // ── HDR (Radiance .hdr) ──────────────────────────────────────────
            // Quand on charge un .hdr, on decode RGB96F via NkHDRCodec puis
            // tonemap Reinhard + correction gamma via ConvertToTexture, et
            // on upload le RGBA8 dans mTexture. L'user peut ajuster
            // l'exposure au clavier (E / Shift+E ou Up/Down avec mIsHdr).
            bool                  mIsHdr        = false;
            float                 mHdrExposure  = 1.0f;
            float                 mHdrGamma     = 2.2f;
            // Cache des pixels float decodes pour re-tonemap a la volee quand
            // l'exposure change, sans relire le fichier. Owned via NkImage.
            NkImage*              mHdrSource    = nullptr;

            void RebuildHdrTexture();

            // UI state
            float                 mTime     = 0.0f;
            uint32                mViewportW = 0;
            uint32                mViewportH = 0;
            bool                  mQuit     = false;
        };

    } // namespace demo
} // namespace nkentseu
