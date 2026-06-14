// =============================================================================
// Games/Common/NkoungGame.h
// Classe abstraite pour tous les jeux Nkoung.
// =============================================================================
#pragma once

#ifndef NKOUNG_GAME_H
#define NKOUNG_GAME_H

#include "NKCore/NkTypes.h"
#include "NKEvent/NkEvent.h"
#include "Games/Common/NkoungFrame.h"

namespace nkoung {

    // =========================================================================
    // NkoungGame — classe de base virtuelle pour tous les jeux
    // =========================================================================
    class NkoungGame {
    public:
        virtual ~NkoungGame() = default;

        /// Appelé une fois à la création du jeu.
        /// @return true si succès, false si échec (jeu non jouable).
        virtual bool Init() noexcept = 0;

        /// Appelé chaque frame. Logique du jeu : mise à jour d'état, entrées, etc.
        /// @param dt Delta time en secondes.
        virtual void Update(nkentseu::float32 dt) noexcept = 0;

        /// Appelé chaque frame pour le rendu. Dessine via le contexte responsive.
        /// @param frame Contexte : draw list, polices, taille, zone sûre, pointeur unifié.
        virtual void Render(const NkoungFrame& frame) noexcept = 0;

        /// Gère un événement utilisateur (clavier, souris, tactile, gamepad).
        /// @param event L'événement polymorphe.
        virtual void OnEvent(nkentseu::NkEvent* event) noexcept = 0;

        /// Appelé juste avant la destruction du jeu.
        /// Libère les ressources (niveaux, textures, sons, etc.).
        virtual void Unload() noexcept = 0;

        /// Retourne le titre affichable du jeu (ex. "Laser Puzzle").
        virtual const char* GetTitle() const noexcept = 0;

        /// Retourne true si le jeu souhaite quitter la scène jeu et retourner au menu.
        virtual bool WantExit() const noexcept { return false; }

        /// Getter optionnel : titre du niveau actuel (pour afficher en UI).
        virtual const char* GetCurrentLevelTitle() const noexcept { return "Unknown"; }

        /// Getter optionnel : progression (0..1) pour les jeux à niveaux linéaires.
        virtual nkentseu::float32 GetProgress() const noexcept { return 0.f; }
    };

}  // namespace nkoung

#endif // NKOUNG_GAME_H
