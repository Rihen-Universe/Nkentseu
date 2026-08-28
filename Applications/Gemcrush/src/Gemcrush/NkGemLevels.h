// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/NkGemLevels.h
// DESCRIPTION: Modes de jeu, définition des niveaux et progression sauvegardée.
//
//              TOUTE PARTIE A UN OBJECTIF, et c'est LUI qui décide de la
//              victoire — jamais le score. Le mode ne change pas le but, il
//              change la RESSOURCE qu'on dépense pour l'atteindre :
//                COUPS      : un nombre d'échanges
//                CHRONO     : un temps
//                OBJECTIFS  : des coups, mais des buts plus lourds
//              Le mode est porté par le NIVEAU, pas par un réglage global :
//              c'est ce qui permet d'alterner sur la carte d'aventure.
//
//              LES ÉTOILES MESURENT L'EFFICACITÉ, PAS LE SCORE. On démarre à
//              trois et elles fondent proportionnellement à la ressource
//              consommée. Finir vite ou en peu de coups vaut trois étoiles ;
//              finir de justesse en vaut une. Un score seul ne dirait pas
//              cela — on peut marquer beaucoup en jouant mal longtemps.
//
//              LES NIVEAUX SONT GÉNÉRÉS, PAS ÉCRITS — mais DÉTERMINISTES :
//              `NkMakeLevel(7)` rend toujours exactement le même niveau 7.
//              Un niveau qui changerait d'une partie à l'autre rendrait les
//              étoiles gagnées incomparables, donc la progression vide de sens.
//              Le jour où des niveaux dessinés à la main arrivent, ils
//              remplacent NkMakeLevel() sans toucher au reste.
//
// AUTEUR: Rihen
// DATE: 2026-08-27
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_GAME_NKGEMLEVELS_H
#define NKENTSEU_GAME_NKGEMLEVELS_H

#include "NKCore/NkTypes.h"
#include "Gemcrush/NkGem.h"

namespace nkentseu {
	namespace game {

		enum class NkGemMode : uint8 {
			NK_MODE_MOVES,	   ///< nombre d'échanges limité
			NK_MODE_TIMED,	   ///< chronomètre (mode défi)
			NK_MODE_OBJECTIVES ///< collecter des gemmes par couleur
		};

		const char *NkGemModeName(NkGemMode mode) noexcept;

		/// @brief Un objectif de collecte.
		struct NkGemObjectiveDef {
				NkGemColor color = NkGemColor::NK_GEM_COLOR_NONE;
				int32 goal = 0;
		};

		/// @brief Tout ce qui définit une partie. Aucune référence au rendu.
		struct NkGemLevelDef {
				int32 index = 1;
				NkGemMode mode = NkGemMode::NK_MODE_MOVES;

				int32 moves = 25;		 ///< mode COUPS et OBJECTIFS
				float32 seconds = 60.f;	 ///< mode CHRONO
				int32 targetScore = 2000; ///< repère de score affiché (n'ouvre PLUS les étoiles)

				NkGemObjectiveDef objectives[3];
				int32 objectiveCount = 0;

				int32 rowCount = 8;
				int32 columnCount = 8;
				int32 colorCount = 6; ///< nombre de couleurs en jeu (difficulté)
		};

		/// @brief Le niveau `index` (1-based). DÉTERMINISTE : même index, même
		///        niveau, à chaque appel et à chaque lancement.
		NkGemLevelDef NkMakeLevel(int32 index);

		/// @brief Nombre de niveaux de la carte d'aventure.
		constexpr int32 NK_GEM_LEVEL_COUNT = 30;

		/// @brief Étoiles restantes pour une part de ressource consommée.
		///
		/// Trois bandes égales : la 3e étoile tombe au tiers, la 2e aux deux
		/// tiers. `spent01` vaut 0 au départ et 1 quand la ressource est épuisée.
		/// @param won false -> zéro étoile : l'objectif n'a pas été atteint.
		/// @note Une victoire rend TOUJOURS au moins une étoile. Réussir de
		///       justesse doit rester une réussite.
		int32 NkStarsFromSpend(bool won, float32 spent01) noexcept;

		/// @brief Remplissage 0..1 de l'étoile numéro `starIndex` (0..2) pour une
		///        part consommée donnée. Sert au HUD : les étoiles fondent
		///        CONTINUMENT, elles ne sautent pas d'un cran.
		float32 NkStarFill(int32 starIndex, float32 spent01) noexcept;

		/// @brief Cette etoile est-elle ENCORE ALLUMEE ?
		///
		/// ⚠️ Le seuil n est pas une tolerance esthetique. En flottant,
		/// (1 - 2/3) x 3 ne vaut pas exactement 1 : une etoile exactement vide
		/// ressort a ~6e-8, donc "> 0" la comptait encore allumee. Mesure du
		/// banc (cas 7) : a ressource epuisee, l ecran montrait une etoile
		/// residuelle. UN SEUL predicat, partage par le dessin et le verdict.
		bool NkStarIsLit(int32 starIndex, float32 spent01) noexcept;

		// =================================================================
		// Progression — étoiles par niveau + déblocage, sauvegardées.
		// =================================================================
		class NkGemProgress {
			public:
				/// @brief Étoiles obtenues sur ce niveau (0..3). 0 = jamais réussi.
				int32 GetStars(int32 levelIndex) const noexcept;

				/// @brief Enregistre un résultat. Ne DIMINUE jamais un score déjà
				///        acquis : rejouer un niveau pour s'entraîner ne doit pas
				///        punir le joueur.
				/// @return true si c'est un nouveau record pour ce niveau.
				bool RecordResult(int32 levelIndex, int32 stars);

				/// @brief Plus haut niveau jouable (1-based).
				int32 GetHighestUnlocked() const noexcept {
					return mHighestUnlocked;
				}

				bool IsUnlocked(int32 levelIndex) const noexcept {
					return levelIndex >= 1 && levelIndex <= mHighestUnlocked;
				}

				int32 GetTotalStars() const noexcept;

				/// @brief Meilleur score toutes parties confondues — la seule
				///        mesure que l'accueil peut montrer avant d'entrer dans
				///        un niveau.
				int32 GetBestScore() const noexcept {
					return mBestScore;
				}

				/// @return true si c'est un nouveau record.
				bool RecordScore(int32 score) noexcept;

				/// @brief Charge depuis le disque. Une sauvegarde absente n'est PAS
				///        une erreur : c'est une première partie.
				bool Load();

				/// @brief Écrit sur le disque (répertoire de données applicatives —
				///        jamais à côté de l'exécutable, interdit sur mobile).
				bool Save() const;

				/// @brief Efface la progression (bouton du menu).
				void Reset();

			private:
				int32 mStars[NK_GEM_LEVEL_COUNT] = {0};
				int32 mHighestUnlocked = 1;
				int32 mBestScore = 0;
		};

	} // namespace game
} // namespace nkentseu

#endif // NKENTSEU_GAME_NKGEMLEVELS_H
