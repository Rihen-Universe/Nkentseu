#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    NkModelerFold.h
// @Brief   L'ETAT DE PLIAGE DES GROUPES DE PROPRIETES, indexe par CLE.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI CE FICHIER EXISTE — un defaut MESURE, pas une preference
// -----------------------------------------------------------------------------
// L'etat de pliage vivait dans un `uint32 grpFold`, un BIT par groupe, le bit
// etant choisi a la main a chaque appel de `PaintPropGroup`. Mesure du 14/09, en
// extrayant les 27 appels du fichier et en les groupant par bit :
//
//     27 groupes,  16 bits distincts,  6 bits PARTAGES par 17 groupes
//       bit 1     : gi, postfx, shadow, ssao, xform      (cinq groupes)
//       bit 2     : amb, dim
//       bit 4     : floor, fog, rel                      (trois groupes)
//       bit 4096  : edsel, edt
//       bit 8192  : edtools, matp, out                   (trois groupes)
//       bit 16384 : edgeo, outdst
//     et `prop.g.cam` porte le bit 3, qui n'est PAS une puissance de deux : son
//     XOR bascule les bits 1 ET 2 d'un coup, donc SEPT groupes a la fois.
//
// Consequence visible aujourd'hui, sans rien changer : plier « SSAO » plie aussi
// GI, PostFX, Ombres et Transformation. C'est un defaut que Rodolf VOIT.
//
// POURQUOI PAR CLE, ET NON « PLUS DE BITS »
// -----------------------------------------------------------------------------
// Elargir a `uint64` reglerait 27 groupes et REPLANTERAIT le mur a 64 — juste
// apres l'ajout des blocs par operation (41 groupes), et avant que sculpture et
// texturing n'aient les leurs. Or **le bit est redondant** : chaque appel passe
// deja une CLE unique (`"prop.g.xxx"`), dont l'unicite est de toute facon exigee
// par le registre de survol et par le menu de groupe. Indexer par elle supprime
// le plafond au lieu de le repousser, et supprime la classe de bugs « deux
// groupes, un bit » — qui ne se voit qu'a l'usage, et sur un seul des deux.
//
// ELARGIR ETAIT-IL SEULEMENT SUR ? La question se pose AVANT de toucher au type :
// un champ qui se SERIALISE ne s'elargit pas sans casser la relecture. Verifie :
// `grpFold` n'apparait que dans sa declaration et trois usages, et **zero fois**
// dans `Project/`, ou vit la persistance. L'etat de pliage n'est jamais ecrit sur
// disque. Rien ne pouvait casser — mais il fallait le mesurer, pas le supposer.
//
// TROIS ETATS ET NON DEUX
// -----------------------------------------------------------------------------
// « jamais touche » n'est pas « deplie ». C'est ce qui permet a un groupe de
// naitre PLIE (la demande de Rodolf : « tous les blocs sont fermes par defaut »)
// sans que ce defaut ecrase le choix de l'utilisateur des qu'il l'a exprime.
// -----------------------------------------------------------------------------

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace nk3d {

		// Plafond du nombre de groupes CONNUS. 27 aujourd'hui, 41 avec les blocs
		// par operation ; 128 laisse la place a sculpture et texturing.
		// ⚠ LE DEPASSEMENT EST BORNE ET SE DIT, il n'est pas ignore : un tableau
		// dimensionne sur un commentaire a deja ecrase la pile dans ce depot.
		enum { kFoldMaxGroups = 128, kFoldKeyCap = 40 };

		// L'etat d'un groupe. `Inconnu` n'est pas `Deplie` : voir l'en-tete.
		enum class NkFoldState : uint8 { Inconnu = 0, Deplie = 1, Plie = 2 };

		struct NkFoldTable {
				char keys[kFoldMaxGroups][kFoldKeyCap] = {};
				uint8 states[kFoldMaxGroups] = {};
				int32 count = 0;
				// Nombre de groupes qui n'ont PAS pu entrer, faute de place. Il se
				// LIT (le banc le verifie a zero) : un depassement silencieux se
				// serait manifeste par des groupes qui « oublient » leur etat.
				int32 refuses = 0;

				static bool Eq(const char *a, const char *b) {
					if (!a || !b)
						return false;
					uint32 i = 0;
					for (; i < kFoldKeyCap && a[i] && b[i]; ++i)
						if (a[i] != b[i])
							return false;
					return i >= kFoldKeyCap || a[i] == b[i];
				}

				int32 IndexOf(const char *key) const {
					for (int32 i = 0; i < count; ++i)
						if (Eq(keys[i], key))
							return i;
					return -1;
				}

				// Ajoute la cle si elle manque. Rend son indice, ou -1 si la table
				// est pleine (et compte le refus).
				int32 Intern(const char *key) {
					if (!key || !*key)
						return -1;
					const int32 i = IndexOf(key);
					if (i >= 0)
						return i;
					if (count >= kFoldMaxGroups) {
						++refuses;
						return -1;
					}
					const int32 n = count++;
					uint32 j = 0;
					for (; j + 1 < kFoldKeyCap && key[j]; ++j)
						keys[n][j] = key[j];
					keys[n][j] = 0;
					states[n] = (uint8)NkFoldState::Inconnu;
					return n;
				}

				// L'etat EFFECTIF : le choix de l'utilisateur s'il en a fait un,
				// sinon le defaut du groupe. C'est la seule fonction que la vue
				// appelle pour decider du chevron.
				bool EstPlie(const char *key, bool plieParDefaut) const {
					const int32 i = IndexOf(key);
					if (i < 0 || states[i] == (uint8)NkFoldState::Inconnu)
						return plieParDefaut;
					return states[i] == (uint8)NkFoldState::Plie;
				}

				// Bascule. Elle PART de l'etat effectif : un groupe ne PLIE par
				// defaut et jamais touche doit se DEPLIER au premier clic, et non
				// se plier une seconde fois.
				void Basculer(const char *key, bool plieParDefaut) {
					const bool avant = EstPlie(key, plieParDefaut);
					const int32 i = Intern(key);
					if (i < 0)
						return; // table pleine : compte dans `refuses`, rien d'ecrit
					states[i] = (uint8)(avant ? NkFoldState::Deplie : NkFoldState::Plie);
				}

				// Pose un etat explicite (sert au « tout replier » d'un panneau).
				void Poser(const char *key, bool plie) {
					const int32 i = Intern(key);
					if (i < 0)
						return;
					states[i] = (uint8)(plie ? NkFoldState::Plie : NkFoldState::Deplie);
				}

				// Combien de groupes CONNUS sont deplies. C'est le compteur dont le
				// zero doit etre prouve avant de dire « tous les blocs sont plies ».
				int32 NbDeplies(bool plieParDefaut) const {
					int32 n = 0;
					for (int32 i = 0; i < count; ++i) {
						const bool plie = (states[i] == (uint8)NkFoldState::Inconnu)
											  ? plieParDefaut
											  : (states[i] == (uint8)NkFoldState::Plie);
						if (!plie)
							++n;
					}
					return n;
				}
		};

	} // namespace nk3d
} // namespace nkentseu
