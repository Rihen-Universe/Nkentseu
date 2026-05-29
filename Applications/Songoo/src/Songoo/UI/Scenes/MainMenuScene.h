#pragma once
// =============================================================================
// MainMenuScene.h
// =============================================================================
// Menu principal Songoo - 4 items: Jouer / Comment Jouer / Options / Quitter.
// Layout responsive avec cards centrees + navigation clavier/tactile.
// =============================================================================

#include "Songoo/UI/Scene.h"

namespace nkentseu
{
    namespace songoo
    {

        class MainMenuScene : public Scene
        {
            public:
                static constexpr int kItemCount = 4;

                enum ItemId
                {
                    Item_Play   = 0,  // Lancer une partie
                    Item_Rules  = 1,  // Comment jouer (règles Mancala)
                    Item_Options= 2,  // Paramètres (audio, graphiques)
                    Item_Quit   = 3   // Quitter l'app
                };

                MainMenuScene()  = default;
                ~MainMenuScene() override = default;

                const char* Name() const noexcept override { return "MainMenu"; }

                void OnEnter (AppContext& ctx) override;
                void OnUpdate(AppContext& ctx, float dt) override;
                void OnRender(AppContext& ctx) override;
                void OnEvent (AppContext& ctx, NkEvent& ev) override;

            private:
                // Temps ecoule (anims pulse / blink).
                float mTime          = 0.0f;
                // Index de l'item actuellement focalise (clavier ou hover).
                int   mFocusIndex    = 0;
                // Anim entree (slide-in des items, 0 -> 1 sur ~0.5s).
                float mEnterAnim     = 0.0f;

                // ── Geometrie cards (sync via ComputeLayout chaque frame) ────────
                // Permet a OnEvent (tap / clic) de retrouver l'item touche
                // sans recalculer le layout.
                float mCardListX    = 0.0f;
                float mCardListW    = 0.0f;
                float mCardItemH    = 0.0f;
                float mCardItemGap  = 0.0f;
                float mCardItemYs[4]= { 0, 0, 0, 0 };  // kItemCount=4

                // ── Actions ──────────────────────────────────────────────────────
                /// Action declenchee par ENTER ou clic sur @p item.
                void ActivateItem(AppContext& ctx, ItemId item);
                /// Retourne l'index de l'item sous (px, py) ou -1.
                int  HitTestItem(float px, float py) const;
        };

    } // namespace songoo
} // namespace nkentseu
