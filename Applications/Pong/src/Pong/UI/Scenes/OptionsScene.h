#pragma once
// =============================================================================
// OptionsScene.h
// -----------------------------------------------------------------------------
// Hub OPTIONS : liste de categories de reglages globaux que l'utilisateur peut
// ouvrir indeendamment. La premiere et seule active aujourd'hui est
// "AIDE & TUTORIEL" (-> RulesScene en accordion). Les autres categories
// (AUDIO / GRAPHIQUES / CONTROLES / RESEAU) sont stubs visuels en attendant
// leur implementation.
//
// Le placeholder permet de garder la stratification claire : Options =
// reglages GLOBAUX, pas per-match (les reglages per-match sont dans
// SelectMatchConfigScene).
// =============================================================================

#include "Pong/UI/Scene.h"

namespace nkentseu
{
    namespace pong
    {

        class OptionsScene : public Scene
        {
        public:
            // Categories (alignees sur le GDD §6 — audio, graphiques, gameplay,
            // controles, reseau). On y ajoute "Aide" comme entree-clef.
            enum Category
            {
                Cat_Help     = 0,
                Cat_Audio    = 1,
                Cat_Graphics = 2,
                Cat_Controls = 3,
                Cat_Network  = 4,
                kCatCount    = 5
            };

            OptionsScene()  = default;
            ~OptionsScene() override = default;

            const char* Name() const noexcept override { return "Options"; }

            void OnEnter            (AppContext& ctx) override;
            void OnUpdate           (AppContext& ctx, float dt) override;
            void OnRender           (AppContext& ctx) override;
            void OnEvent            (AppContext& ctx, NkEvent& ev) override;
            void OnResumedFromChild (AppContext& ctx) override;

        private:
            float mTime      = 0.0f;
            float mEnterAnim = 0.0f;
            int   mFocusIndex = 0;

            // Grace period anti auto-trigger (sec restantes). Arme a 0.20s
            // au OnEnter ET au retour d'une scene enfant (OnResumedFromChild).
            // Pendant ce delai, on ignore mouse releases et touch ends pour
            // eviter que le release du click "RETOUR" sur la scene enfant
            // ne triggere aussi le RETOUR ici (cf Options > Tutoriel bug).
            float mInputArmDelay = 0.0f;

            // Geometrie des cards (sync chaque frame en coords ECRAN).
            float mCardX[kCatCount] = {0};
            float mCardY[kCatCount] = {0};
            float mCardW = 0.0f, mCardH = 0.0f;

            // Bouton RETOUR sticky haut-gauche
            float mBackX = 0.0f, mBackY = 0.0f;
            float mBackW = 0.0f, mBackH = 0.0f;

            // Touch id du tap en cours (seul ce touch est traite sur EndEvent
            // — evite qu'un TouchEnd "fuite" de la scene precedente apres un
            // Pop synchrone declenche aussi un tap ici).
            long long mActiveTouchId = -1;

            // Helpers
            int  HitTestCard(float screenX, float screenY) const;
            bool HitTestBack(float screenX, float screenY) const;
            void Activate   (AppContext& ctx, Category cat);
        };

    } // namespace pong
} // namespace nkentseu
