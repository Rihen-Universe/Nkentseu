#pragma once
// =============================================================================
// SupporterScene.h
// -----------------------------------------------------------------------------
// Ecran SUPPORTER : permet au joueur de soutenir le developpeur (a.k.a. nous
// — Rihen) via 2 voies complementaires :
//   1) Partager le jeu (intent natif Android, navigator.share() Web, copie
//      clipboard sur desktop).
//   2) Don financier preset : 0$ / 1$ / 2$ / 5$ / 10$ + montant libre.
//      Chaque montant ouvre une URL hostee (rihen.example/donate?amount=X)
//      qui gere le multi-mode paiement (Mobile Money / Visa / PayPal / etc.)
//      cote serveur — voir [[pong-main-menu-supporter-todo]].
//
// L'URL d'agregateur est un placeholder configurable. L'integration paiement
// sera plug-and-play une fois que l'user a cree son compte marchand.
// =============================================================================

#include "Pong/UI/Scene.h"

namespace nkentseu
{
    namespace pong
    {

        class SupporterScene : public Scene
        {
        public:
            // Montants preset (USD). 0 = "merci gratuit", autres = vrais dons.
            static constexpr int kAmountCount = 5;
            // -1 = bouton LIBRE (saisie manuelle, ouvre l'URL sans montant).
            // 0..kAmountCount-1 = preset.

            SupporterScene()  = default;
            ~SupporterScene() override = default;

            const char* Name() const noexcept override { return "Supporter"; }

            void OnEnter (AppContext& ctx) override;
            void OnUpdate(AppContext& ctx, float dt) override;
            void OnRender(AppContext& ctx) override;
            void OnEvent (AppContext& ctx, NkEvent& ev) override;

        private:
            float mTime      = 0.0f;
            float mEnterAnim = 0.0f;

            // Bouton RETOUR sticky haut-gauche
            float mBackX = 0.0f, mBackY = 0.0f;
            float mBackW = 0.0f, mBackH = 0.0f;

            // Bouton PARTAGER (banderole pleine largeur)
            float mShareX = 0.0f, mShareY = 0.0f;
            float mShareW = 0.0f, mShareH = 0.0f;

            // Cards de don (5 presets + 1 libre = 6 boutons)
            float mAmountX[kAmountCount + 1] = {0};
            float mAmountY[kAmountCount + 1] = {0};
            float mAmountW = 0.0f, mAmountH = 0.0f;

        public:
            // ── Reseaux sociaux ──────────────────────────────────────────────
            // 5 cards : LinkedIn, GitHub, Facebook, Instagram, X (Twitter).
            // Click ouvre l'URL externe (via le futur ShellOpen / Intent).
            static constexpr int kSocialCount = 5;
        private:
            float mSocialX[kSocialCount] = {0};
            float mSocialY[kSocialCount] = {0};
            float mSocialW = 0.0f, mSocialH = 0.0f;

            // Touch id du tap en cours
            long long mActiveTouchId = -1;

            // ── Scroll vertical ─────────────────────────────────────────────
            // Le contenu (story + share + amounts + identifiants) deborde sur
            // mobile portrait. Scroll par drag/molette/touch.
            float mScrollY    = 0.0f;
            float mMaxScroll  = 0.0f;
            float mDragStartY = 0.0f;
            float mDragLastY  = 0.0f;
            bool  mDragActive = false;
            bool  mDragWasScroll = false;
            bool  mMouseDown = false;
            long long mDragTouchId = -1;

            // Zone top reservee (hors scroll, header sticky)
            float mTopReserve = 0.0f;

            // Helpers
            bool HitTestBack  (float sx, float sy) const;
            bool HitTestShare (float sx, float sy) const;
            int  HitTestAmount(float sx, float sy) const;
            int  HitTestSocial(float sx, float sy) const;
            float ClampScroll (float v) const;
            /// Lance le partage natif (intent / clipboard / log).
            void DoShare      (AppContext& ctx);
            /// Ouvre la page de don avec @p amountIndex (-1 = libre).
            void DoDonate     (AppContext& ctx, int amountIndex);
            /// Ouvre l'URL du reseau social @p idx.
            void DoOpenSocial (AppContext& ctx, int idx);
        };

    } // namespace pong
} // namespace nkentseu
