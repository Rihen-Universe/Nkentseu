#pragma once
// =============================================================================
// SettingsScene.h
// =============================================================================
// Écran des paramètres Songoo — vitesse, son, difficulté IA.
// Layout responsive avec sliders/toggles et bouton retour.
// =============================================================================

#include "Songoo/UI/Scene.h"

namespace nkentseu
{
    namespace songoo
    {

        class SettingsScene : public Scene
        {
        public:
            static constexpr int kItemCount = 3;  // Vitesse, Son, Difficulté IA

            enum SettingId
            {
                Setting_GameSpeed   = 0,  // Multiplicateur de vitesse (1.0x à 2.0x)
                Setting_Sound       = 1,  // Toggle son (on/off)
                Setting_AIDifficulty= 2   // Difficulté IA (Easy, Medium, Hard)
            };

            SettingsScene()  = default;
            ~SettingsScene() override = default;

            const char* Name() const noexcept override { return "Settings"; }

            void OnEnter (AppContext& ctx) override;
            void OnUpdate(AppContext& ctx, float dt) override;
            void OnRender(AppContext& ctx) override;
            void OnEvent (AppContext& ctx, NkEvent& ev) override;

        private:
            float mTime        = 0.0f;
            float mEnterAnim   = 0.0f;  // Fade-in: 0 -> 1
            int   mFocusIndex  = 0;

            // État des paramètres (persiste dans GameSettings global)
            float mGameSpeed       = 1.0f;    // 1.0x à 2.0x
            bool  mSoundEnabled    = true;
            int   mAIDifficulty    = 1;       // 0=Easy, 1=Medium, 2=Hard

            // Géométrie (sync chaque frame pour hit-test)
            float mItemYs[3]       = { 0, 0, 0 };
            float mItemX           = 0.0f;
            float mItemW           = 0.0f;
            float mItemH           = 0.0f;
            float mItemGap         = 0.0f;
            float mBackX           = 0.0f;
            float mBackY           = 0.0f;
            float mBackW           = 0.0f;
            float mBackH           = 0.0f;
        };

    } // namespace songoo
} // namespace nkentseu
