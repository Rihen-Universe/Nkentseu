// =============================================================================
// Games/Common/MouGame.h
// Classe abstraite de base pour tous les jeux Mú.
// =============================================================================
#pragma once

#ifndef MOU_GAME_H
#define MOU_GAME_H

#include "NKCore/NkTypes.h"
#include "NKEvent/NkEvent.h"
#include "Games/Common/MouFrame.h"
#include "Games/Common/MouFeedback.h"

namespace mou {

    class MouAssets;  // chargeur d'assets (SVG -> texture)

    // =========================================================================
    // MouGame — contrat commun (calqué sur NkoungGame)
    // =========================================================================
    class MouGame {
    public:
        virtual ~MouGame() = default;

        /// Injecté par la plateforme AVANT Init() : accès au chargeur d'assets.
        void SetAssets(MouAssets* a) noexcept { mAssets = a; }

        /// Création du jeu. @return false si non jouable.
        virtual bool Init() noexcept = 0;

        /// Logique par frame. @param dt secondes.
        virtual void Update(nkentseu::float32 dt) noexcept = 0;

        /// Rendu par frame (via le contexte responsive).
        virtual void Render(const MouFrame& frame) noexcept = 0;

        /// Événement utilisateur (souris / tactile / clavier).
        virtual void OnEvent(nkentseu::NkEvent* event) noexcept = 0;

        /// Libère les ressources avant destruction.
        virtual void Unload() noexcept = 0;

        /// Titre affichable (ex. "Les Couleurs").
        virtual const char* GetTitle() const noexcept = 0;

        /// true si le jeu veut revenir au menu.
        virtual bool WantExit() const noexcept { return false; }

        // ── Système de niveaux (voir docs/00-plateforme-gdd.md §4) ───────────
        /// Charge un niveau par index (0-based). Défaut : no-op (sera surchargé).
        virtual void LoadLevel(nkentseu::int32 levelIndex) noexcept { (void)levelIndex; }

        /// Nombre de niveaux disponibles pour ce jeu.
        virtual nkentseu::int32 LevelCount() const noexcept { return 0; }

        /// true quand le niveau courant est terminé.
        virtual bool IsLevelComplete() const noexcept { return false; }

        /// Étoiles obtenues sur le niveau courant (0..3).
        virtual nkentseu::int32 GetStars() const noexcept { return 0; }

        /// Récupère + efface le dernier signal audio (à router vers MouAudio).
        /// Défaut : aucun ; les jeux à feedback le surchargent (return mFb.ConsumeCue()).
        virtual MouFeedback::Cue ConsumeAudioCue() noexcept { return MouFeedback::Cue::None; }

        /// Récupère + efface une demande de voix (clé de fichier, ex. "animaux_lion").
        /// Les jeux n'ont qu'à écrire dans mPendingVoice ; la plateforme la consomme.
        virtual const char* ConsumeVoiceCue() noexcept { const char* v = mPendingVoice; mPendingVoice = nullptr; return v; }

    protected:
        MouAssets*  mAssets = nullptr;       // injecté via SetAssets()
        const char* mPendingVoice = nullptr; // voix demandée par le jeu (clé), consommée par la plateforme
    };

}  // namespace mou

#endif // MOU_GAME_H
