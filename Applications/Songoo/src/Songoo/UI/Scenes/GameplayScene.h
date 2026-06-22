#pragma once
// =============================================================================
// GameplayScene.h — CORRIGÉE
//
// CORRECTIONS appliquées :
//   BUG 1  : Background.png rechargée chaque frame → pré-chargée dans OnEnter
//   BUG 2  : Police TTF rechargée chaque frame dans RenderHand → pré-chargée
//   BUG 3  : Coordonnées de clic non transformées pour le bas du plateau →
//            utilise le ScaleX/Y issu de ComputeLayout (espace logique)
//   BUG 4  : glowTime incrémenté deux fois → une seule incrémentation dans OnUpdate
//   BUG 9  : Police timesbd.ttf (Windows uniquement) → FontAtlas natif
//   BUG 10 : ExecuteMove() appelé dans FinishAnimation() UNIQUEMENT,
//            l'animation est purement visuelle (ne touche pas mBoard)
// =============================================================================

#include "Songoo/UI/Scene.h"
#include "Songoo/Game/SongooBoard.h"
#include "Songoo/Render/Texture2D.h"

namespace nkentseu { namespace songoo {

    class GameplayScene : public Scene {
    public:
        GameplayScene()  = default;
        ~GameplayScene() override = default;

        const char* Name() const noexcept override { return "Gameplay"; }

        void OnEnter (AppContext& ctx) override;
        void OnUpdate(AppContext& ctx, float dt) override;
        void OnRender(AppContext& ctx) override;
        void OnEvent (AppContext& ctx, NkEvent& ev) override;
        void OnExit  (AppContext& ctx) override;
        void OnPause (AppContext& ctx) override;
        void OnResume(AppContext& ctx) override;

    private:
        // ── Logique ───────────────────────────────────────────────────────────
        SongooBoard mBoard;
        int         mCurrentPlayer = 0;
        bool        mGameOver      = false;

        // ── Layout responsive ─────────────────────────────────────────────────
        float mScale  = 1.f;
        struct PitGeo { float cx, cy, radius; };
        PitGeo mPitGeo[14];
        float  mRetourBtnX=0, mRetourBtnY=0, mRetourBtnW=0, mRetourBtnH=0;

        // ── CORRECTION BUG 1 : textures pré-chargées une seule fois ───────────
        Texture2D mTrouTex[16];   // trou0..trou15.png
        Texture2D mBgTex;         // Background.png — chargée dans OnEnter
        Texture2D mHandTex;       // hand.png

        // ── Animation main (logique identique à l'original) ───────────────────
        // CORRECTION BUG 10 : l'animation ne modifie PAS mBoard.
        // Elle lit GetPitGrains() pour l'affichage, mais ne modifie pas les trous.
        struct AnimState {
            bool  active      = false;
            int   srcIdx      = 0;   // trou source (index 0..13)
            int   curIdx      = 0;   // trou courant de la main
            int   grainesLeft = 0;   // graines encore à déposer
            float handX       = 0.f;
            float handY       = 0.f;
            float targetX     = 0.f;
            float targetY     = 0.f;
            float speed       = 700.f;
            float pauseTimer  = 0.f;
            bool  dropping    = false;

            // Table horaire locale (copie de kCW pour usage dans l'anim)
            static constexpr int kCW[14] = {0,1,2,3,4,5,6,13,12,11,10,9,8,7};
            int cwPos[14];
            void InitCW() { for(int i=0;i<14;i++) cwPos[kCW[i]]=i; }
            int  NextPit(int pit) const { return kCW[(cwPos[pit]+1)%14]; }
        } mAnim;

        // Snapshot visuel des graines (sépare logique / affichage pendant anim)
        // Avant le début d'un coup, on capture l'état du board.
        // L'anim décrémente ce snapshot visuellement. Le board n'est touché
        // qu'à FinishAnimation().
        int mVisualPits[14]; // état visuel pendant l'animation

        // ── Timing & feedback ─────────────────────────────────────────────────
        float mTime          = 0.f;
        // CORRECTION BUG 4 : glowTime incrémenté UNE SEULE FOIS dans OnUpdate
        float mGlowTime      = 0.f;
        float mEnterAnim     = 0.f;
        int   mHoveredPit    = -1;
        bool  mShowInvalidFb = false;
        float mInvalidFbT    = 0.f;
        char  mStatusMsg[96] = "Au tour du Joueur 1";

        // ── Helpers ───────────────────────────────────────────────────────────
        void ComputeLayout(AppContext& ctx);
        int  HitTestPit(float px, float py) const;
        // CORRECTION BUG 3 : les coordonnées reçues sont déjà en espace logique
        // (Nkentseu gère la transformation fenêtre→logique via SafeArea).
        // On n'a plus besoin de SDL_RenderCoordinatesFromWindow.
        void TryMove(AppContext& ctx, int pitIdx);
        void UpdateAnimation(float dt, AppContext& ctx);
        void FinishAnimation(AppContext& ctx);  // BUG 10 : appelle ExecuteMove ici
        void SwitchPlayer(AppContext& ctx);
        void TriggerIAMove(AppContext& ctx);

        // ── Rendu ─────────────────────────────────────────────────────────────
        void DrawBackground(AppContext& ctx);
        void DrawPlayerGlow(AppContext& ctx);
        void DrawBoard(AppContext& ctx);
        void DrawPit(AppContext& ctx, int i);
        void DrawHand(AppContext& ctx);
        void DrawUI(AppContext& ctx);

        int  VisualGrains(int pit) const;
    };

}} // namespace nkentseu::songoo
