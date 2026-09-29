//
// NkUnkenyLiaisons.h
// =============================================================================
// Description :
//   La TABLE DE LIAISONS d'un jeu : « l'action Sauter <- la touche Espace, le
//   bouton Sud de la manette, la zone en bas a droite de l'ecran ». Le jeu ne
//   parle que d'actions (NkUnkenyActions.h) ; cette table dit d'ou elles
//   viennent, et elle se reconfigure et s'ecrit dans un fichier.
//
// Caracteristiques :
//   - L'entree reutilise NkInputCode de NKEvent, et son TEXTE : « Key:SPACE »,
//     « Gamepad:SOUTH », « GamepadAxis:LEFT_X », « Mouse:LEFT ». Unkeny n'a pas
//     de second vocabulaire des touches : un fichier ecrit ici se lit avec les
//     noms que NkLoadBindings connait deja.
//   - Les zones d'ecran (bouton et joystick virtuel) sont en coordonnees
//     NORMALISEES [0, 1] de la surface de jeu : la meme table sert au
//     telephone et a la tablette.
//   - Chaque liaison appartient a un joueur (0..3), ou a TOUS (-1) : une seule
//     ligne « lier Sauter Gamepad:SOUTH j=* » donne a chaque joueur le bouton
//     Sud de SA manette.
//   - Aucune allocation par trame : un tableau fixe de 128 liaisons.
//
// Le format texte, une liaison par ligne (# = commentaire) :
//     lier  <action> <entree> [echelle] [zm=<zone morte>] [j=<joueur>|j=*]
//     zone  <action> <x> <y> <l> <h> [echelle] [j=...]
//     stickx / sticky <action> <x> <y> <l> <h> [echelle] [r=<rayon>] [zm=...] [j=...]
//   Les actions sont NOMMEES par le jeu AVANT la lecture (NommerAction) : le
//   fichier ne decide que des entrees, jamais de ce qui existe. Meme regle que
//   NkLoadBindings.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYLIAISONS_H__
#define __NKENTSEU_UNKENY_NKUNKENYLIAISONS_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKEvent/NkEventDispatcher.h"
#include "Unkeny/Entree/NkUnkenyActions.h"

namespace nkentseu {
	namespace unkeny {

		/// Joueurs d'une meme partie (ecran partage, plusieurs manettes).
		static const int32 NK_UNKENY_JOUEURS_MAX = 4;
		/// Une liaison pour TOUS les joueurs : chacun la lit sur sa manette.
		static const int32 NK_UNKENY_TOUS_LES_JOUEURS = -1;
		static const int32 NK_UNKENY_LIAISONS_MAX = 128;
		static const int32 NK_UNKENY_NOM_ACTION_MAX = 32;

		enum class NkNatureLiaison : uint8 {
			NK_ENTREE = 0,	 ///< un NkInputCode : touche, souris, molette, bouton ou axe de manette
			NK_ZONE_BOUTON,	 ///< un doigt dans la zone vaut `echelle`
			NK_ZONE_STICK_X, ///< joystick virtuel : son axe horizontal (droite = +)
			NK_ZONE_STICK_Y	 ///< joystick virtuel : son axe vertical (HAUT = +, comme une manette XInput)
		};

		/// Un rectangle de la surface de jeu, en fractions de sa largeur et de
		/// sa hauteur (0,0 = en haut a gauche).
		struct NkZoneEcran {
				float32 x = 0.f;
				float32 y = 0.f;
				float32 l = 0.f;
				float32 h = 0.f;

				bool Contient(float32 u, float32 v) const noexcept {
					return u >= x && u <= x + l && v >= y && v <= y + h;
				}
				bool operator==(const NkZoneEcran &o) const noexcept {
					return x == o.x && y == o.y && l == o.l && h == o.h;
				}
		};

		struct NkLiaison {
				int32 action = -1;
				int32 joueur = 0; ///< 0..3, ou NK_UNKENY_TOUS_LES_JOUEURS
				NkNatureLiaison nature = NkNatureLiaison::NK_ENTREE;
				NkInputCode code;
				/// Ce que la liaison APPORTE a l'action : une touche enfoncee vaut
				/// `echelle` (-1 pour « Gauche » sur un axe Avancer), un axe est
				/// multiplie par elle (-1 l'inverse).
				float32 echelle = 1.f;
				/// Axes et sticks : en deca, zero ; au-dela, la course restante est
				/// ramenee a 0..1 (sinon l'action sauterait de 0 a `zoneMorte`).
				float32 zoneMorte = 0.f;
				NkZoneEcran zone;
				/// Stick virtuel : la course, en fraction de la HAUTEUR de la surface.
				float32 rayon = 0.08f;
		};

		/// Ce qu'une lecture de texte a fait, et ce qu'elle a refuse.
		struct NkRapportLiaisons {
				uint32 appliquees = 0;
				NkVector<NkString> erreurs; ///< une par ligne refusee, avec son numero

				bool Ok() const noexcept {
					return erreurs.Empty();
				}
		};

		class NkLiaisons {
			public:
				// --- Les noms des actions (le jeu les donne) ---------------------
				bool NommerAction(int32 action, const char *nom) noexcept;
				const char *NomAction(int32 action) const noexcept;
				/// -1 si le nom est inconnu.
				int32 ActionParNom(const char *nom) const noexcept;

				// --- Lier --------------------------------------------------------
				/// Rend l'indice de la liaison, ou -1 (table pleine, action hors
				/// plage, joueur hors plage).
				int32 Lier(const NkLiaison &liaison) noexcept;
				int32 LierTouche(int32 action, NkKey touche, float32 echelle = 1.f, int32 joueur = 0) noexcept;
				int32 LierSouris(int32 action, NkMouseButton bouton, float32 echelle = 1.f, int32 joueur = 0) noexcept;
				int32 LierBouton(int32 action, NkGamepadButton bouton, float32 echelle = 1.f,
								 int32 joueur = NK_UNKENY_TOUS_LES_JOUEURS) noexcept;
				int32 LierAxe(int32 action, NkGamepadAxis axe, float32 echelle = 1.f, float32 zoneMorte = 0.15f,
							  int32 joueur = NK_UNKENY_TOUS_LES_JOUEURS) noexcept;
				int32 LierZone(int32 action, const NkZoneEcran &zone, float32 echelle = 1.f, int32 joueur = 0) noexcept;
				/// Un joystick virtuel : DEUX liaisons (X puis Y) sur la meme zone.
				/// Rend l'indice de la premiere, ou -1.
				int32 LierStick(int32 actionX, int32 actionY, const NkZoneEcran &zone, float32 rayon = 0.08f,
								float32 zoneMorte = 0.1f, int32 joueur = 0) noexcept;

				/// Reconfigure : la liaison `indice` prend une AUTRE entree (le menu
				/// « appuyez sur une touche »). Ni son action ni son echelle ne
				/// changent. false si l'indice n'est pas une liaison d'entree.
				bool Relier(int32 indice, const NkInputCode &entree) noexcept;
				bool Retirer(int32 indice) noexcept;
				/// Retire toutes les liaisons de l'action (d'un joueur, ou de tous : -1).
				void RetirerAction(int32 action, int32 joueur = NK_UNKENY_TOUS_LES_JOUEURS) noexcept;
				/// Oublie les liaisons. Les NOMS d'actions restent : c'est le jeu
				/// qui les a donnes, pas le fichier.
				void Vider() noexcept;

				int32 Nombre() const noexcept {
					return mNombre;
				}
				/// ⚠️ Aucun controle d'indice : 0 <= i < Nombre().
				const NkLiaison &operator[](int32 i) const noexcept {
					return mLiaisons[i];
				}
				/// La premiere liaison d'entree de l'action portant ce code, ou -1.
				int32 Trouver(int32 action, const NkInputCode &entree) const noexcept;
				/// Une liaison vise-t-elle cette entree ? (l'appelant peut alors
				/// considerer l'evenement comme CONSOMME par le jeu)
				bool Concerne(const NkInputCode &entree) const noexcept;

				// --- Le texte ----------------------------------------------------
				NkString Ecrire() const;
				/// @param viderAvant false : ajoute aux liaisons existantes.
				NkRapportLiaisons Lire(const NkString &texte, bool viderAvant = true);

			private:
				NkLiaison mLiaisons[NK_UNKENY_LIAISONS_MAX];
				int32 mNombre = 0;
				char mNoms[NK_UNKENY_ACTIONS_MAX][NK_UNKENY_NOM_ACTION_MAX] = {};
		};

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYLIAISONS_H__
