// =============================================================================
// NkUnkenySieges.h — qui tient chaque place : un humain, ou l'ordinateur
//
// ⚠️ CE FICHIER EST DE GENRE, PAS DU NOYAU
//   Il appartient au vocabulaire des jeux A TOURS. Un RPG, un jeu de
//   plateforme ou un shoot ne l'incluent jamais — et Unkeny n'est pas un moteur
//   de jeux a tours : les trois jeux de plateau ecrits avant lui ont servi a
//   MESURER ce qui se repete, ils n'ont pas defini sa portee.
//
// A QUOI SERT CE FICHIER
//   Il porte la SEULE notion de mode dont un jeu a tours ait besoin. « contre
//   l'ordinateur », « a deux sur le meme ecran », « simulation » et « trois
//   humains contre une IA » ne sont pas quatre modes : ce sont quatre
//   configurations du MEME tableau de sieges.
//
// ⚠️ POURQUOI LE MODE N'EST PAS STOCKE
//   Mesure du 2026-09-01 sur les trois jeux : un menu pose un mode, et des
//   bascules en pied de page changent un siege en cours de partie. Si le mode
//   etait garde a cote des sieges, les deux se contrediraient des la premiere
//   bascule — et l'ecran afficherait « contre l'ordinateur » pendant que deux
//   humains jouent.
//   `NkModeSieges` n'est donc qu'un RACCOURCI vers une configuration. On le
//   passe a `NkAppliquerMode`, on ne le retient jamais.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un controleur de plus (reseau, rejeu) -> NkControleur, et le menu suit
//   - un mode de plus                       -> NkModeSieges + NkAppliquerMode
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace unkeny {

		/// Nombre maximal de sieges. Quatre couvre les dames, les echecs, le
		/// ludo et la plupart des jeux de plateau.
		static const int32 NK_UNKENY_SIEGES_MAX = 4;

		enum class NkControleur : uint8 {
			NK_HUMAIN = 0,
			NK_IA
			// ⚠️ Ajouter NK_RESEAU ici plutot qu'un booleen ailleurs : un
			// controleur est une NATURE, et deux booleens paralleles ("estIA",
			// "estDistant") finissent par decrire un etat impossible.
		};

		/// L'ecran affiche. Deux seulement : on choisit, puis on joue.
		enum class NkEcranJeu : uint8 { NK_MENU = 0, NK_PARTIE };

		/// Un raccourci vers une configuration de sieges. Jamais stocke.
		enum class NkModeSieges : uint8 {
			NK_UN_HUMAIN = 0,	 ///< 1 humain, le reste en IA
			NK_DEUX_HUMAINS,
			NK_TROIS_HUMAINS,
			NK_TOUS_HUMAINS,
			NK_SIMULATION		 ///< aucun humain : les IA jouent entre elles
		};

		/// La table des sieges. `nb` dit combien sont en jeu (2 aux dames et aux
		/// echecs, 4 au ludo).
		struct NkSieges {
				int32 nb = 2;
				NkControleur qui[NK_UNKENY_SIEGES_MAX] = {NkControleur::NK_HUMAIN, NkControleur::NK_IA,
														  NkControleur::NK_IA, NkControleur::NK_IA};

				bool EstHumain(int32 siege) const noexcept {
					return siege >= 0 && siege < nb && qui[siege] == NkControleur::NK_HUMAIN;
				}

				void Basculer(int32 siege) noexcept {
					if (siege < 0 || siege >= nb) {
						return;
					}
					qui[siege] = (qui[siege] == NkControleur::NK_HUMAIN) ? NkControleur::NK_IA : NkControleur::NK_HUMAIN;
				}

				int32 NbHumains() const noexcept {
					int32 n = 0;
					for (int32 i = 0; i < nb; ++i) {
						if (qui[i] == NkControleur::NK_HUMAIN) {
							++n;
						}
					}
					return n;
				}

				/// Vrai quand PERSONNE n'est humain. Un jeu s'en sert pour
				/// accelerer le rythme : une simulation qu'on regarde n'a pas
				/// besoin des pauses qu'on laisse a un joueur.
				bool EstSimulation() const noexcept {
					return NbHumains() == 0;
				}
		};

		/// Applique un mode a une table de sieges. `nb` est conserve.
		inline void NkAppliquerMode(NkSieges &sieges, NkModeSieges mode) noexcept {
			int32 humains = 1;
			switch (mode) {
				case NkModeSieges::NK_DEUX_HUMAINS: humains = 2; break;
				case NkModeSieges::NK_TROIS_HUMAINS: humains = 3; break;
				case NkModeSieges::NK_TOUS_HUMAINS: humains = sieges.nb; break;
				case NkModeSieges::NK_SIMULATION: humains = 0; break;
				default: humains = 1; break;
			}
			if (humains > sieges.nb) {
				humains = sieges.nb;
			}
			for (int32 i = 0; i < NK_UNKENY_SIEGES_MAX; ++i) {
				sieges.qui[i] = (i < humains) ? NkControleur::NK_HUMAIN : NkControleur::NK_IA;
			}
		}

		/// Traduit `--mode=<texte>` en mode. Rend le mode par defaut sur une
		/// valeur inconnue, et l'appelant peut le DIRE — un mode inconnu
		/// silencieusement remplace fait mesurer autre chose que ce qu'on demande.
		inline NkModeSieges NkModeDepuisTexte(const char *texte, bool &reconnu) noexcept {
			reconnu = true;
			if (texte == nullptr) {
				reconnu = false;
				return NkModeSieges::NK_UN_HUMAIN;
			}
			auto egal = [](const char *a, const char *b) {
				int32 i = 0;
				for (; a[i] != '\0' && b[i] != '\0'; ++i) {
					if (a[i] != b[i]) {
						return false;
					}
				}
				return a[i] == b[i];
			};
			if (egal(texte, "solo")) return NkModeSieges::NK_UN_HUMAIN;
			if (egal(texte, "duo")) return NkModeSieges::NK_DEUX_HUMAINS;
			if (egal(texte, "trois")) return NkModeSieges::NK_TROIS_HUMAINS;
			if (egal(texte, "tous")) return NkModeSieges::NK_TOUS_HUMAINS;
			if (egal(texte, "ia")) return NkModeSieges::NK_SIMULATION;
			reconnu = false;
			return NkModeSieges::NK_UN_HUMAIN;
		}

	} // namespace unkeny
} // namespace nkentseu
