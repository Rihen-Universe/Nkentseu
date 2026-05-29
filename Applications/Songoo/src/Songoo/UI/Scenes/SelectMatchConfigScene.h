#pragma once
// =============================================================================
// SelectMatchConfigScene.h
// -----------------------------------------------------------------------------
// Ecran fusionne (difficulte IA + obstacles + futur power-ups) intercale dans
// le flow JOUER apres SelectMode. Affichage conditionnel :
//   - section "Difficulte IA"  : uniquement si mode = VsAI ou IAvsAI
//   - section "Obstacles"      : toujours (8 toggles)
//
// Layout : zone scrollable verticale (drag tactile / molette / fleches) avec
// bouton "LANCER LA PARTIE" sticky en bas. Le bouton reste accessible meme si
// le contenu deborde verticalement.
//
// Apres validation :
//   - settings->difficulty = niveau choisi (si section visible)
//   - settings->obsActive[8] = etat des toggles
//   - Replace par GameplayScene.
// =============================================================================

#include "Songoo/UI/Scene.h"

namespace nkentseu
{
    namespace songoo
    {

        class SelectMatchConfigScene : public Scene
        {
        public:
            static constexpr int kDiffCount = 6;
            static constexpr int kObsCount  = 8;

            SelectMatchConfigScene()  = default;
            ~SelectMatchConfigScene() override = default;

            const char* Name() const noexcept override { return "SelectMatchConfig"; }

            void OnEnter (AppContext& ctx) override;
            void OnUpdate(AppContext& ctx, float dt) override;
            void OnRender(AppContext& ctx) override;
            void OnEvent (AppContext& ctx, NkEvent& ev) override;

        private:
            float mTime       = 0.0f;
            float mEnterAnim  = 0.0f;

            // Etat selectionnable
            int   mDiffIndex  = 2;     ///< IA principale (Competiteur par defaut).
                                       ///< Raquette droite en VsAI, gauche en AIvsAI.
            int   mDiffIndexR = 2;     ///< IA secondaire (raquette droite en AIvsAI seulement).
            bool  mObsActive[kObsCount] = { true, true, true, true,
                                            true, true, true, true };
            bool  mShowDifficulty = false;  ///< vrai si mode IA (VsAI ou AIvsAI)
            bool  mShowDifficultyR = false; ///< vrai SI mode AIvsAI (2e grille)

            // ── Parametres de match configurables ──────────────────────────
            int   mScoreIndex = 3;     ///< index dans kScoreOptions (0..N-1)
            int   mTimeIndex  = 0;     ///< index dans kTimeOptions (0 = AUCUN)
            bool  mWinByTwo   = false;
            int   mSpeedIndex = 0;     ///< index dans kSpeedOptions (0 = x1.0)

            // Geometrie des steppers (sync chaque frame, en coords MONDE)
            float mScoreMinusX = 0.0f, mScoreMinusY = 0.0f;
            float mScorePlusX  = 0.0f, mScorePlusY  = 0.0f;
            float mTimeMinusX  = 0.0f, mTimeMinusY  = 0.0f;
            float mTimePlusX   = 0.0f, mTimePlusY   = 0.0f;
            float mSpeedMinusX = 0.0f, mSpeedMinusY = 0.0f;
            float mSpeedPlusX  = 0.0f, mSpeedPlusY  = 0.0f;
            float mTogglePtsX  = 0.0f, mTogglePtsY  = 0.0f;
            float mStepperW    = 0.0f, mStepperH    = 0.0f;
            float mToggleW     = 0.0f, mToggleH     = 0.0f;

            // ── Stepper hold-to-repeat ─────────────────────────────────────
            // Quand l'utilisateur maintient un bouton -/+ enfonce sur un
            // stepper (score / time / vitesse), on auto-repete l'action
            // pour eviter 91 clics manuels jusqu'a x10. La toggle (winByTwo,
            // id=5) n'auto-repete pas.
            //   mArmedStepper : id du bouton (1..7) en cours de hold (0 = none)
            //   mHoldTime     : duree depuis le press (s)
            //   mRepeatAccum  : accumulateur pour le tick repeat
            int   mArmedStepper = 0;
            float mHoldTime     = 0.0f;
            float mRepeatAccum  = 0.0f;
            /// true quand le press a deja consume l'evenement (stepper ou
            /// toggle). Empeche le release de re-firer la meme action.
            bool  mPressConsumed = false;
            static constexpr float kHoldDelay      = 0.40f;  ///< delai avant repeat
            static constexpr float kRepeatInterval = 0.05f;  ///< 20 ticks/sec

            // Scroll vertical
            float mScrollY    = 0.0f;
            float mMaxScroll  = 0.0f;
            // Pour le drag tactile / souris hold : on ne scrolle que si drag,
            // pas si simple tap (sinon impossible de toggler un obstacle).
            // Threshold : si on a bouge de plus de kDragThreshold, on considere
            // que c'est un scroll, pas un tap.
            float mDragStartY = 0.0f;
            float mDragLastY  = 0.0f;
            bool  mDragActive = false;
            bool  mDragWasScroll = false;
            long long mDragTouchId = -1;
            bool  mMouseDown = false;

            // Geometrie cards (recalcule chaque frame, dans coords MONDE
            // c-a-d sans appliquer mScrollY). Pour hit-test on convertit
            // ecran -> monde en ajoutant mScrollY.
            float mDiffX[kDiffCount] = {0};
            float mDiffY[kDiffCount] = {0};
            float mDiffXR[kDiffCount] = {0};  ///< Geometrie cards 2e grille (AIvsAI)
            float mDiffYR[kDiffCount] = {0};
            float mDiffW = 0.0f, mDiffH = 0.0f;

            float mObsX [kObsCount]  = {0};
            float mObsY [kObsCount]  = {0};
            float mObsW = 0.0f, mObsH = 0.0f;
            // Position de la case a cocher (rect) sur chaque card. La case
            // est tap-able independamment du corps : tap sur la case = toggle
            // active, tap sur le corps = ouvrir le panneau d'edition.
            float mObsCheckX[kObsCount] = {0};
            float mObsCheckY[kObsCount] = {0};
            float mObsCheckW = 0.0f, mObsCheckH = 0.0f;

            // ── Panneau modal d'edition d'un obstacle ───────────────────────
            // Quand mEditingObsIndex >= 0, on dessine un panneau modal qui
            // permet de modifier count, powerLevel, chaotic du type donne.
            int   mEditingObsIndex = -1;     ///< -1 = pas de panneau ouvert
            // Geometrie du panneau (coords ECRAN)
            float mEditPanelX = 0.0f, mEditPanelY = 0.0f;
            float mEditPanelW = 0.0f, mEditPanelH = 0.0f;
            // Boutons du panneau
            float mEditCountMinusX = 0.0f, mEditCountMinusY = 0.0f;
            float mEditCountPlusX  = 0.0f, mEditCountPlusY  = 0.0f;
            float mEditPowerMinusX = 0.0f, mEditPowerMinusY = 0.0f;
            float mEditPowerPlusX  = 0.0f, mEditPowerPlusY  = 0.0f;
            float mEditChaoticX    = 0.0f, mEditChaoticY    = 0.0f;
            float mEditChaoticW    = 0.0f, mEditChaoticH    = 0.0f;
            float mEditCloseX      = 0.0f, mEditCloseY      = 0.0f;
            float mEditApplyX      = 0.0f, mEditApplyY      = 0.0f;
            float mEditApplyW      = 0.0f, mEditApplyH      = 0.0f;
            float mEditBtnW        = 0.0f, mEditBtnH        = 0.0f;
            // Valeurs temporaires en cours d'edition
            int   mEditCount       = 0;
            int   mEditPowerLevel  = 2;
            bool  mEditChaotic     = true;

            // Bouton LANCER (sticky bas, en coords ECRAN — pas scrollable)
            float mLaunchX = 0.0f, mLaunchY = 0.0f;
            float mLaunchW = 0.0f, mLaunchH = 0.0f;
            // Bouton RETOUR (sticky haut-gauche, coords ECRAN)
            float mBackX = 0.0f, mBackY = 0.0f;
            float mBackW = 0.0f, mBackH = 0.0f;

            // Zones top/bottom reservees (hors scroll)
            float mTopReserve    = 0.0f;
            float mBottomReserve = 0.0f;

            // Actions
            void   Launch(AppContext& ctx);
            int    HitTestDiff(float worldX, float worldY) const;
            /// Hit-test sur la 2e grille de difficulte (IA droite, AIvsAI seulement).
            int    HitTestDiffR(float worldX, float worldY) const;
            int    HitTestObs (float worldX, float worldY) const;
            bool   HitTestLaunch(float screenX, float screenY) const;
            bool   HitTestBack  (float screenX, float screenY) const;
            float  ClampScroll(float v) const;
            /// Retourne 1=score-, 2=score+, 3=time-, 4=time+, 5=winByTwo toggle.
            int    HitTestParam(float worldX, float worldY) const;
            /// Test sur la case a cocher de la card obstacle index i (coords MONDE).
            bool   HitTestObsCheck(float worldX, float worldY, int& outIndex) const;
            /// Test sur les boutons du panneau d'edition (coords ECRAN).
            /// Retour : 1=count-, 2=count+, 3=power-, 4=power+, 5=chaotic, 6=close, 7=apply.
            int    HitTestEdit (float screenX, float screenY) const;
            void   OpenEditPanel(int obsIndex, const GameSettings& s);
            void   ApplyEditPanel(AppContext& ctx);
            void   CloseEditPanel();
        };

    } // namespace songoo
} // namespace nkentseu
