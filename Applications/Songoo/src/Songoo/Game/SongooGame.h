#pragma once
// =============================================================================
// SongooGame.h — Application Songo'o
// Architecture identique à celle du Songoo existant dans le repo Nkentseu :
//   - GLContext  : contexte OpenGL/ES via NkIGraphicsContext
//   - GLRenderer2D : renderer 2D batché (quads, textures, texte)
//   - FontAtlas  : atlas de glyphes 5 tailles
//   - SceneManager : pile de scènes (intro, menu, gameplay, gameover…)
//   - AudioManager : sons courts NKAudio + streams musicaux
// =============================================================================

#include "Songoo/Game/GameTypes.h"
#include "Songoo/Render/GLContext.h"
#include "Songoo/Render/GLRenderer2D.h"
#include "Songoo/Render/FontAtlas.h"
#include "Songoo/Audio/AudioManager.h"
#include "Songoo/UI/AppContext.h"
#include "Songoo/UI/SceneManager.h"
#include "NKCore/NkTypes.h"

namespace nkentseu { class NkWindow; class NkEvent; }

namespace nkentseu { namespace songoo {

    class SongooGame {
    public:
        explicit SongooGame(NkWindow& window) noexcept : mWindow(window) {}
        ~SongooGame() = default;

        // ── Lifecycle ─────────────────────────────────────────────────────────
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
        NkWindow&      mWindow;
        GLContext      mGL;
        GLRenderer2D   mRenderer;
        FontAtlas      mFont;
        GameSettings   mSettings;
        SceneManager   mScenes;
        AudioManager   mAudio;

        bool           mQuit      = false;
        float          mTime      = 0.0f;
        uint32         mViewportW = 0;
        uint32         mViewportH = 0;

        AppContext BuildContext();
    };

}} // namespace nkentseu::songoo
