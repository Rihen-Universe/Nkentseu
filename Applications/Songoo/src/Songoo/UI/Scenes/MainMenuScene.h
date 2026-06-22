#pragma once
// =============================================================================
// MainMenuScene.h — Menu principal Songo'o
// Items : Nouvelle Partie / Histoire / Options / Crédits / Quitter
// Layout responsive avec cards centrées + navigation clavier/tactile
// Couleurs palette africaine camerounaise (terre cuite, or, vert forêt)
// =============================================================================

#include "Songoo/UI/Scene.h"

namespace nkentseu { namespace songoo {

    class MainMenuScene : public Scene {
    public:
        static constexpr int kItemCount = 5;

        enum ItemId {
            Item_Play    = 0,
            Item_Story   = 1,
            Item_Options = 2,
            Item_Credits = 3,
            Item_Quit    = 4,
        };

        const char* Name() const noexcept override { return "MainMenu"; }

        void OnEnter (AppContext& ctx) override;
        void OnUpdate(AppContext& ctx, float dt) override;
        void OnRender(AppContext& ctx) override;
        void OnEvent (AppContext& ctx, NkEvent& ev) override;
        void OnResumedFromChild(AppContext& ctx) override;

    private:
        float mTime       = 0.f;
        int   mFocusIndex = 0;
        float mEnterAnim  = 0.f;
        float mGracePeriod = 0.f;  // anti double-trigger après Pop enfant

        // Géométrie des cards (pour hit-test)
        float mCardListX  = 0.f, mCardListW = 0.f;
        float mCardItemH  = 0.f, mCardItemGap = 0.f;
        float mCardItemYs[kItemCount] = {};

        void  ActivateItem(AppContext& ctx, ItemId item);
        int   HitTestItem(float px, float py) const;
        void  ComputeLayout(AppContext& ctx, float scale);
    };

}} // namespace nkentseu::songoo
