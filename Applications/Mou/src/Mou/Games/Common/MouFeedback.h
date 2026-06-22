// =============================================================================
// Games/Common/MouFeedback.h
// Système de feedback partagé : étoiles HUD (visibles, diminuent à l'échec),
// effets (bonne/mauvaise action, mission réussie/échouée) + signaux audio (Cue).
// Les jeux appellent Good/Bad/Win aux bons moments ; tout le reste est géré ici.
// =============================================================================
#pragma once

#ifndef MOU_FEEDBACK_H
#define MOU_FEEDBACK_H

#include "Games/Common/MouFx.h"

namespace mou {

    struct MouFrame;

    class MouFeedback {
    public:
        enum class State { Playing, Won, Failed };
        // Signal sonore à jouer (consommé par le jeu -> NKAudio, plus tard).
        enum class Cue { None, Good, Bad, Win, Fail, Star };

        void Init(nkentseu::uint32 starTex) noexcept { mStarTex = starTex; }

        /// Début de niveau : remet les étoiles au max et efface les effets.
        void ResetLevel(nkentseu::int32 maxStars = 3) noexcept;

        /// Petite action satisfaisante (ex. compter un objet) : étincelle légère.
        void Tap(nkentseu::float32 x, nkentseu::float32 y) noexcept;
        /// Bonne action (sous-mission réussie) : confettis + étincelles à (x,y).
        void Good(nkentseu::float32 x, nkentseu::float32 y) noexcept;
        /// Mauvaise action : puff + perte d'une étoile. @return false si plus d'étoiles (=> Failed).
        bool Bad(nkentseu::float32 x, nkentseu::float32 y) noexcept;
        /// Mission réussie : pluie de confettis + écran Bravo (garde les étoiles restantes).
        /// @param viewW largeur de l'écran (pour étaler la pluie de confettis).
        void Win(nkentseu::float32 viewW) noexcept;

        void Update(nkentseu::float32 dt) noexcept;

        // Rendu en 3 temps : effets monde -> HUD étoiles -> overlay plein écran.
        void RenderFx(const MouFrame& frame) const noexcept;
        void RenderHud(const MouFrame& frame) const noexcept;
        void RenderOverlay(const MouFrame& frame) const noexcept;

        /// Facteur d'échelle de la mascotte : rebondit brièvement après Good/Win.
        nkentseu::float32 MascotScale() const noexcept;

        State GetState() const noexcept { return mState; }
        bool  IsPlaying() const noexcept { return mState == State::Playing; }
        nkentseu::int32 StarsLeft() const noexcept { return mLeft; }
        bool  ReadyToAdvance() const noexcept { return mState == State::Won && mTimer > 2.3f; }
        bool  ReadyToRetry()   const noexcept { return mState == State::Failed && mTimer > 2.0f; }

        /// Récupère + efface le dernier signal audio (à router vers NKAudio).
        Cue   ConsumeCue() noexcept { Cue c = mCue; mCue = Cue::None; return c; }

    private:
        MouFx            mFx;
        nkentseu::uint32 mStarTex = 0;
        nkentseu::int32  mMax = 3, mLeft = 3;
        nkentseu::int32  mLostIdx = -1;          // étoile en cours d'animation de perte
        nkentseu::float32 mLostT = 0.f;
        State            mState = State::Playing;
        nkentseu::float32 mTimer = 0.f;          // depuis Won/Failed
        nkentseu::float32 mPulse = 0.f;          // animation HUD/overlay
        nkentseu::float32 mMascotT = 999.f;      // temps depuis le dernier rebond mascotte
        Cue              mCue = Cue::None;
    };

}  // namespace mou

#endif // MOU_FEEDBACK_H
