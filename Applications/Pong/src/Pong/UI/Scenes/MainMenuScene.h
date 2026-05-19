#pragma once
// =============================================================================
// MainMenuScene.h
// -----------------------------------------------------------------------------
// Menu principal — version condensee a 5 items.
//
// Decision de design :
//   - JOUER est UN bouton qui ouvre le flow "choix du mode + lancement". Le
//     choix Local/Reseau/vsIA/IAvsIA, la difficulte IA et la selection des
//     obstacles se font DANS ce flow (per-partie), pas au menu principal.
//   - OPTIONS regroupe uniquement les options GLOBALES (audio, graphiques,
//     controles, reseau). Pas de "Difficulte IA" ni "Obstacles" ici.
//   - MODE COMPETITION : tournois 1v1, paris, spectateurs.
//   - CLASSEMENT : meilleurs joueurs (online + local).
//   - QUITTER : ferme l'app.
//
// Layout : responsive avec 2 colonnes en paysage et single-column compact en
// portrait/petit ecran (mobile). Dimensions/police proportionnelles a la
// taille du viewport (zone safe).
//
// Icones : dessinees proceduralement (zero asset) — la police embarquee
// (Karla) ne contient pas de glyphes emoji.
//
// Navigation : fleches haut/bas pour se deplacer, ENTER active, ESC quitte.
// =============================================================================

#include "Pong/UI/Scene.h"

namespace nkentseu
{
    namespace pong
    {

        class MainMenuScene : public Scene
        {
            public:
                // ── Identifiants d'item ──────────────────────────────────────────
                // Ordre vertical dans le panneau droit. Les "section titles" sont
                // dessines mais non selectionnables — la nav clavier saute par
                // dessus.
                // PUBLIC car les tables de description (MenuItemDesc) au scope
                // namespace dans le .cpp y referent.
                enum ItemId
                {
                    Item_Play        = 0,  // porte d'entree vers le flow de partie
                    Item_Competition = 1,  // tournois, paris, spectateurs
                    Item_Leaderboard = 2,  // classements online + local
                    Item_Options     = 3,  // audio/graphiques/controles globaux
                    Item_Supporter   = 4,  // partage + dons (Nkentseu+Jenga+jeux)
                    Item_Quit        = 5,  // ferme l'app
                    kItemCount       = 6
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
                float mCardItemYs[6]= { 0, 0, 0, 0, 0, 0 };  // kItemCount=6

                // ── Actions ──────────────────────────────────────────────────────
                /// Action declenchee par ENTER ou clic sur @p item.
                void ActivateItem(AppContext& ctx, ItemId item);
                /// Retourne l'index de l'item sous (px, py) ou -1.
                int  HitTestItem(float px, float py) const;
        };

    } // namespace pong
} // namespace nkentseu
