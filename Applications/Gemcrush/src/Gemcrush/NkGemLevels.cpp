// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/NkGemLevels.cpp
// DESCRIPTION: Génération déterministe des niveaux et sauvegarde de la
//              progression.
// AUTEUR: Rihen
// DATE: 2026-08-27
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "Gemcrush/NkGemLevels.h"
#include "NKMath/NKMath.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace game {

		namespace {

			/// Générateur congruentiel local, semé par l'index du niveau.
			///
			/// ⚠️ PAS math::NkRandom : celui-là est un état GLOBAL partagé avec le
			/// remplissage du plateau. S'en servir ici rendrait la définition d'un
			/// niveau dépendante du nombre de gemmes tirées avant — donc variable
			/// d'une partie à l'autre, ce que cette fonction promet d'éviter.
			struct LevelRandom {
					uint32 state;

					explicit LevelRandom(int32 seed) noexcept
						: state(static_cast<uint32>(seed) * 2654435761u + 0x9E3779B9u) {
					}

					uint32 Next() noexcept {
						state = state * 1664525u + 1013904223u;
						return (state >> 8) & 0x00FFFFFFu;
					}

					int32 Range(int32 minimum, int32 maximum) noexcept {
						if (maximum <= minimum) {
							return minimum;
						}
						return minimum + static_cast<int32>(Next() % static_cast<uint32>(maximum - minimum));
					}
			};

			NkPath SaveFilePath() {
				return NkDirectory::GetAppDataDirectory() / "Rihen" / "GemCrush" / "progression.txt";
			}

		} // namespace

		const char *NkGemModeName(NkGemMode mode) noexcept {
			switch (mode) {
				case NkGemMode::NK_MODE_TIMED:
					return "CHRONO";
				case NkGemMode::NK_MODE_OBJECTIVES:
					return "OBJECTIFS";
				case NkGemMode::NK_MODE_MOVES:
				default:
					return "COUPS";
			}
		}

		// =====================================================================
		// Étoiles = efficacité
		// =====================================================================
		int32 NkStarsFromSpend(bool won, float32 spent01) noexcept {
			if (!won) {
				return 0;
			}
			// ⚠️ LE VERDICT DERIVE DE L AFFICHAGE, il ne le recopie pas.
			//
			// Ces deux fonctions portaient chacune leurs seuils. Mesure du banc
			// (cas 7) : a un tiers EXACT de ressource depensee, l ecran montrait
			// deux etoiles et le verdict en accordait trois. Un joueur voit cet
			// ecart ; rien dans le code ne le signalait. On compte donc les
			// etoiles ENCORE ENTAMEES, avec la meme fonction que le dessin.
			int32 remaining = 0;
			for (int32 i = 0; i < 3; ++i) {
				if (NkStarIsLit(i, spent01)) {
					++remaining;
				}
			}
			return math::NkMax(1, remaining); // gagner de justesse reste gagner
		}

		bool NkStarIsLit(int32 starIndex, float32 spent01) noexcept {
			return NkStarFill(starIndex, spent01) > 0.005f;
		}

		float32 NkStarFill(int32 starIndex, float32 spent01) noexcept {
			// L'étoile 0 est la première perdue : elle se vide sur la 1re bande.
			const float32 spent = math::NkClamp(spent01, 0.f, 1.f);
			const float32 bandStart = static_cast<float32>(math::NkClamp(starIndex, 0, 2)) / 3.f;
			const float32 local = (spent - bandStart) * 3.f;
			return math::NkClamp(1.f - local, 0.f, 1.f);
		}

		// =====================================================================
		// NkMakeLevel — la courbe de difficulté, en un seul endroit.
		// =====================================================================
		NkGemLevelDef NkMakeLevel(int32 index) {
			NkGemLevelDef level;
			level.index = math::NkClamp(index, 1, NK_GEM_LEVEL_COUNT);
			LevelRandom random(level.index);

			// -- Mode : un cycle lisible, pas un tirage --------------------
			// Le joueur doit pouvoir ANTICIPER. Un mode tiré au hasard donne
			// l'impression que le jeu décide contre lui.
			//   1..2   : COUPS (on apprend)
			//   puis   : 1 CHRONO tous les 4, 1 OBJECTIFS tous les 3
			if (level.index <= 2) {
				level.mode = NkGemMode::NK_MODE_MOVES;
			} else if (level.index % 4 == 0) {
				level.mode = NkGemMode::NK_MODE_TIMED;
			} else if (level.index % 3 == 0) {
				level.mode = NkGemMode::NK_MODE_OBJECTIVES;
			} else {
				level.mode = NkGemMode::NK_MODE_MOVES;
			}

			// -- Taille du plateau : grandit lentement ---------------------
			// Au-delà de 9, une case devient trop petite pour un doigt sur un
			// téléphone étroit (cf. NkGemLayout : la case suit la largeur).
			level.rowCount = 8;
			level.columnCount = (level.index >= 12) ? 9 : 8;
			if (level.index >= 24) {
				level.rowCount = 9;
			}

			// -- Couleurs : 5 au début (plus facile), 6 ensuite ------------
			level.colorCount = (level.index <= 4) ? 5 : 6;

			// -- Ressource et cible ---------------------------------------
			const float32 growth = static_cast<float32>(level.index - 1);
			switch (level.mode) {
				case NkGemMode::NK_MODE_TIMED: {
					// Le chrono se RACCOURCIT, la cible monte : la pression vient
					// des deux côtés, sinon le mode devient trivial au niveau 20.
					level.seconds = math::NkMax(45.f, 90.f - growth * 1.4f);
					level.moves = 0;
					level.targetScore = 1600 + static_cast<int32>(growth * 190.f);
					break;
				}
				case NkGemMode::NK_MODE_OBJECTIVES: {
					level.moves = math::NkMax(18, 30 - static_cast<int32>(growth * 0.35f));
					level.targetScore = 1400 + static_cast<int32>(growth * 130.f);
					break;
				}
				case NkGemMode::NK_MODE_MOVES:
				default: {
					level.moves = math::NkMax(16, 28 - static_cast<int32>(growth * 0.30f));
					level.targetScore = 1800 + static_cast<int32>(growth * 210.f);
					break;
				}
			}

			// -- Objectifs : TOUJOURS présents, et ils DÉCIDENT de la victoire.
			// Un niveau sans but explicite ne se gagne pas, il s'arrête — et le
			// joueur ne sait pas ce qu'on attendait de lui.
			const int32 wanted = (level.mode == NkGemMode::NK_MODE_OBJECTIVES) ? 3 : ((level.index % 2 == 0) ? 2 : 1);
			level.objectiveCount = 0;
			for (int32 i = 0; i < wanted; ++i) {
				NkGemColor color = NkGemColor::NK_GEM_COLOR_RED;
				for (int32 attempt = 0; attempt < 24; ++attempt) {
					color = static_cast<NkGemColor>(random.Range(1, level.colorCount + 1));
					bool duplicate = false;
					for (int32 k = 0; k < level.objectiveCount; ++k) {
						duplicate = duplicate || (level.objectives[k].color == color);
					}
					if (!duplicate) {
						break;
					}
				}
				level.objectives[level.objectiveCount].color = color;
				// Les buts suivent la RESSOURCE, pas seulement le numéro de
				// niveau : un but calibré sur 28 coups devient infaisable quand
				// la courbe descend à 16. Repère mesuré : un échange rapporte
				// ~3 à 5 gemmes de la couleur visée en comptant les cascades.
				const float32 budget = (level.mode == NkGemMode::NK_MODE_TIMED)
										   ? (level.seconds / 3.2f) // ~1 échange toutes les 3,2 s
										   : static_cast<float32>(level.moves);
				const float32 share = (level.mode == NkGemMode::NK_MODE_OBJECTIVES) ? 0.62f : 0.45f;
				const int32 base = static_cast<int32>(budget * share);
				level.objectives[level.objectiveCount].goal = math::NkMax(6, base - i * 3);
				++level.objectiveCount;
			}
			return level;
		}

		// =====================================================================
		// Progression
		// =====================================================================
		int32 NkGemProgress::GetStars(int32 levelIndex) const noexcept {
			if (levelIndex < 1 || levelIndex > NK_GEM_LEVEL_COUNT) {
				return 0;
			}
			return mStars[levelIndex - 1];
		}

		bool NkGemProgress::RecordResult(int32 levelIndex, int32 stars) {
			if (levelIndex < 1 || levelIndex > NK_GEM_LEVEL_COUNT) {
				return false;
			}
			const int32 clamped = math::NkClamp(stars, 0, 3);
			const bool record = clamped > mStars[levelIndex - 1];
			if (record) {
				mStars[levelIndex - 1] = clamped;
			}
			// Réussir (au moins 1 étoile) débloque le suivant. Échouer ne
			// verrouille rien : on ne retire jamais un accès déjà donné.
			if (clamped >= 1 && levelIndex >= mHighestUnlocked && levelIndex < NK_GEM_LEVEL_COUNT) {
				mHighestUnlocked = levelIndex + 1;
			}
			return record;
		}

		bool NkGemProgress::RecordScore(int32 score) noexcept {
			if (score <= mBestScore) {
				return false;
			}
			mBestScore = score;
			return true;
		}

		int32 NkGemProgress::GetTotalStars() const noexcept {
			int32 total = 0;
			for (int32 i = 0; i < NK_GEM_LEVEL_COUNT; ++i) {
				total += mStars[i];
			}
			return total;
		}

		void NkGemProgress::Reset() {
			for (int32 i = 0; i < NK_GEM_LEVEL_COUNT; ++i) {
				mStars[i] = 0;
			}
			mHighestUnlocked = 1;
			mBestScore = 0;
		}

		// ---------------------------------------------------------------------
		// Format de sauvegarde : une ligne d'entiers séparés par des espaces.
		//   <version> <plus haut debloque> <etoiles du niveau 1> ... <niveau N>
		//
		// Volontairement trivial : ce fichier doit se lire à l'œil quand une
		// progression semble fausse, et un format qu'on ne peut pas lire à
		// l'œil demande un outil pour être diagnostiqué.
        // ---------------------------------------------------------------------
		bool NkGemProgress::Load() {
			const NkPath path = SaveFilePath();
			if (!NkFile::Exists(path)) {
				return false; // première partie : ce n'est PAS une erreur
			}
			const NkString content = NkFile::ReadAllText(path);
			if (content.Size() == 0) {
				return false;
			}

			// +3 : version, plus haut debloque, N etoiles, meilleur score.
			int32 values[NK_GEM_LEVEL_COUNT + 3] = {0};
			int32 count = 0;
			int32 current = 0;
			bool inNumber = false;
			for (usize i = 0; i <= content.Size() && count < NK_GEM_LEVEL_COUNT + 3; ++i) {
				const char c = (i < content.Size()) ? content[i] : ' ';
				if (c >= '0' && c <= '9') {
					current = current * 10 + static_cast<int32>(c - '0');
					inNumber = true;
				} else if (inNumber) {
					values[count++] = current;
					current = 0;
					inNumber = false;
				}
			}
			if (count < 2 || values[0] != 1) {
				logger.Warn("[gemcrush] sauvegarde illisible ou d'une autre version — progression repartie a zero");
				return false;
			}
			mHighestUnlocked = math::NkClamp(values[1], 1, NK_GEM_LEVEL_COUNT);
			for (int32 i = 0; i < NK_GEM_LEVEL_COUNT; ++i) {
				mStars[i] = (i + 2 < count) ? math::NkClamp(values[i + 2], 0, 3) : 0;
			}
			// Champ AJOUTE apres coup : une sauvegarde ecrite avant son
			// existence n'a pas ce nombre, et c'est un cas NORMAL — pas une
			// erreur de version. On lit 0 et on continue.
			mBestScore = (NK_GEM_LEVEL_COUNT + 2 < count) ? math::NkMax(0, values[NK_GEM_LEVEL_COUNT + 2]) : 0;
			return true;
		}

		bool NkGemProgress::Save() const {
			const NkPath path = SaveFilePath();
			NkDirectory::CreateRecursive(path.GetParent());

			NkString content = NkString::Fmt("1 {0}", mHighestUnlocked);
			for (int32 i = 0; i < NK_GEM_LEVEL_COUNT; ++i) {
				content += NkString::Fmt(" {0}", mStars[i]);
			}
			content += NkString::Fmt(" {0}", mBestScore);
			content += "\n";
			if (!NkFile::WriteAllText(path, content)) {
				// Le verdict CHANGE : on ne fait pas semblant d'avoir sauvegardé.
				logger.Warn("[gemcrush] sauvegarde de la progression IMPOSSIBLE : {0}", path.ToString().CStr());
				return false;
			}
			return true;
		}

	} // namespace game
} // namespace nkentseu
