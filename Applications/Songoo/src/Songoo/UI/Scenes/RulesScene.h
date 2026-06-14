#pragma once
// =============================================================================
// RulesScene.h
// -----------------------------------------------------------------------------
// Ecran "Regles du jeu" (onboarding) — accessible depuis le bouton OPTIONS
// du menu principal. Long ecran scrollable verticalement, organise en
// sections : objectif, modes, parametres, controles, obstacles, bonus,
// malus, drops.
//
// Le contenu reflete l'etat ACTUEL du jeu (pas le GDD ideal). A maintenir
// au gre des evolutions gameplay.
// =============================================================================

#include "Songoo/UI/Scene.h"

namespace nkentseu
{
    namespace songoo
    {

        class RulesScene : public Scene
        {
        public:
            RulesScene()  = default;
            ~RulesScene() override = default;

            const char* Name() const noexcept override { return "Rules"; }

            void OnEnter (AppContext& ctx) override;
            void OnUpdate(AppContext& ctx, float dt) override;
            void OnRender(AppContext& ctx) override;
            void OnEvent (AppContext& ctx, NkEvent& ev) override;

        private:
            float mTime       = 0.0f;
            float mEnterAnim  = 0.0f;

            // ── Scroll vertical ─────────────────────────────────────────────
            float mScrollY    = 0.0f;
            float mMaxScroll  = 0.0f;
            // Drag scroll (mouse + touch)
            float mDragStartY = 0.0f;
            float mDragLastY  = 0.0f;
            bool  mDragActive = false;
            bool  mDragWasScroll = false;
            long long mDragTouchId = -1;
            bool  mMouseDown = false;

            // ── Bouton RETOUR (sticky haut-gauche) ──────────────────────────
            float mBackX = 0.0f, mBackY = 0.0f;
            float mBackW = 0.0f, mBackH = 0.0f;

            // Zones top/bottom reservees (hors scroll)
            float mTopReserve    = 0.0f;
            float mBottomReserve = 0.0f;

            // ── Accordion ───────────────────────────────────────────────────
            // mExpandedSection = index de la section ouverte (-1 = aucune).
            // Click sur un titre :
            //   - si c'est la section deja ouverte -> on la ferme
            //   - sinon -> on ferme l'ancienne et on ouvre la nouvelle
            // Une seule section ouverte a la fois.
            static constexpr int kMaxSections = 16;
            int   mExpandedSection = -1;
            // Geometrie des barres de titre (coords MONDE, sync chaque frame
            // dans OnRender pour hit-test depuis OnEvent).
            float mTitleY[kMaxSections] = {0};
            float mTitleH = 0.0f;
            float mTitleX = 0.0f;
            float mTitleW = 0.0f;
            int   mNumSections = 0;

            // Helpers
            float ClampScroll  (float v) const;
            bool  HitTestBack  (float screenX, float screenY) const;
            /// Retourne l'index de la section dont le titre contient (wx, wy)
            /// en coords MONDE (apres translation du scroll), ou -1.
            int   HitTestTitle (float worldX, float worldY) const;
        };

    } // namespace songoo
} // namespace nkentseu
