// =============================================================================
// NkEditeurAppareils.h — simuler la ZONE SURE sans avoir l'appareil
//
// A QUOI SERT CE FICHIER
//   Montrer, sur un ecran de bureau, ce que la mise en page donnera sur un
//   telephone a encoche, sur une tablette, dans un navigateur.
//
// ⚠️ POURQUOI C'EST LA PREMIERE FONCTION DE L'EDITEUR, ET PAS LA DERNIERE
//   Regle du depot (Rodolf, 2026-08-18) : « L'editeur doit pouvoir SIMULER des
//   zones sures pour qu'on voie le debordement AVANT de deployer. Sans ca, le
//   defaut se decouvre sur le telephone, quand il coute le plus cher. »
//   Un bouton sous l'indicateur de geste n'est pas mal place : il est
//   INATTEIGNABLE. Et on ne le voit jamais depuis sa machine de developpement —
//   c'est le defaut structurellement invisible par excellence.
//
// ⚠️ CES CHIFFRES SONT DES ORDRES DE GRANDEUR, PAS DES MESURES
//   Ils viennent des documentations publiques d'Apple et de Google, pas d'un
//   appareil pose sur la table. Ils servent a VOIR un debordement, pas a
//   certifier une mise en page. Un profil qui pretendrait a l'exactitude sans
//   l'avoir mesuree serait pire qu'utile : on lui ferait confiance.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un appareil de plus -> la table ci-dessous, avec sa PROVENANCE
//   - une orientation     -> deja gere : Tourner() echange et permute les zones
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKEvent/NkSafeArea.h"

namespace nkentseu {
	namespace editeur {

		struct NkProfilAppareil {
				const char *nom = "Bureau";
				uint32 largeur = 1280;
				uint32 hauteur = 720;
				float32 densite = 1.f;
				NkSafeAreaInsets zoneSure; ///< en pixels, en PORTRAIT
		};

		/// Les profils. Ordre volontaire : bureau d'abord (le cas ou l'on
		/// travaille), puis du plus contraint au moins contraint — c'est sur le
		/// plus contraint qu'on veut tomber en cherchant.
		inline int32 NkNbProfils() noexcept {
			return 6;
		}

		inline NkProfilAppareil NkProfil(int32 i) noexcept {
			NkProfilAppareil p;
			switch (i) {
				case 1: // encoche haute + indicateur de geste : le cas dur
					p.nom = "Telephone a encoche";
					p.largeur = 393;
					p.hauteur = 852;
					p.densite = 3.f;
					p.zoneSure = NkSafeAreaInsets(59.f, 34.f, 0.f, 0.f);
					break;
				case 2:
					p.nom = "Telephone Android";
					p.largeur = 412;
					p.hauteur = 915;
					p.densite = 2.625f;
					p.zoneSure = NkSafeAreaInsets(24.f, 24.f, 0.f, 0.f);
					break;
				case 3:
					p.nom = "Telephone compact";
					p.largeur = 360;
					p.hauteur = 640;
					p.densite = 2.f;
					p.zoneSure = NkSafeAreaInsets(24.f, 0.f, 0.f, 0.f);
					break;
				case 4:
					p.nom = "Tablette";
					p.largeur = 820;
					p.hauteur = 1180;
					p.densite = 2.f;
					p.zoneSure = NkSafeAreaInsets(24.f, 20.f, 0.f, 0.f);
					break;
				case 5:
					p.nom = "Navigateur";
					p.largeur = 960;
					p.hauteur = 600;
					p.densite = 1.f;
					break;
				default:
					p.nom = "Bureau";
					p.largeur = 1280;
					p.hauteur = 720;
					p.densite = 1.f;
					break;
			}
			return p;
		}

		/// Passe un profil en paysage.
		///
		/// ⚠️ LES ZONES SURE NE SE CONTENTENT PAS D'ECHANGER LEURS AXES : en
		/// paysage, l'encoche passe sur un COTE (gauche ou droite selon le sens
		/// de rotation) et l'indicateur de geste reste EN BAS. Traiter la
		/// rotation comme un simple echange largeur/hauteur donne une simulation
		/// fausse — et fausse dans le sens rassurant, ce qui est le pire.
		inline NkProfilAppareil NkTourner(const NkProfilAppareil &p) noexcept {
			NkProfilAppareil r = p;
			r.largeur = p.hauteur;
			r.hauteur = p.largeur;
			r.zoneSure.top = 0.f;
			r.zoneSure.bottom = p.zoneSure.bottom * 0.6f; // l'indicateur reste, plus fin
			r.zoneSure.left = p.zoneSure.top;			  // l'encoche passe a gauche
			r.zoneSure.right = 0.f;
			return r;
		}

	} // namespace editeur
} // namespace nkentseu
