//
// NkUnkenyMaillagePhysique.h
// =============================================================================
// Description :
//   LA PHYSIQUE PAR PARTIE d'un maillage 2D (2026-10-02, R31), et la place de
//   ses sommets EN JEU. C'est ce qui fait d'un maillage une gelee dont les
//   morceaux bougent, ou un decor qui se brise.
//
//   A la maniere de l'« asset physique » d'Unreal 5 : le composant DECRIT (pour
//   chaque partie : rien, un corps rigide, ou un corps mou ; et les liens), et
//   les corps n'existent que dans la scene QUI JOUE. Ils naissent au premier
//   Pas d'une scene physique (NkMaillagesConstruire, appelee par NkScene::Pas),
//   et meurent avec l'entite, a Restaurer (Arreter), ou quand elle s'eteint.
//
//     partie AUCUNE   ses sommets suivent l'entite (son transform -- donc son
//                     corps rigide, si l'entite en a un) ;
//     partie RIGIDE   un corps rigide « libre » de la scene (NkScene::
//                     CreerCorpsLibre), pose au centre de la partie ; son
//                     collisionneur sort du CONTOUR de la partie : enveloppe
//                     convexe a 8 sommets si dynamique (ce que NKCollision sait
//                     faire tomber), contour exact (chaine) sinon ;
//     partie MOLLE    un corps de particules (NKPhysics, XPBD) : une particule
//                     par sommet, les aretes de ses triangles en liens, les
//                     diagonales en liens de flexion, et l'appariement de forme
//                     (la gelee revient). Ses sommets SONT ses particules.
//
//   Les LIENS : rigide <-> rigide par les joints de NKPhysics (soudure, pivot) ou
//   par deux ressorts (elastique) ; mou <-> rigide par les attaches de
//   NkParticules2D ; mou <-> mou en UN seul corps de particules (sinon leurs
//   particules confondues a la couture se repousseraient) ; une partie AUCUNE
//   est tenue par le corps de l'entite, ou a defaut par une ANCRE cinematique
//   qui suit le transform de l'entite.
//
// ⚠️ CE QUI N'EST PAS PHOTOGRAPHIE
//   L'etat des corps des parties n'entre pas dans la photo de la scene : une
//   photo prise EN JEU rend le maillage au repos (ses parties repartent de leur
//   place). C'est le cas de Jouer / Arreter (la photo est prise avant) et de
//   Ctrl+Z (en edition seulement) : ils ne voient rien de cette limite.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYMAILLAGEPHYSIQUE_H__
#define __NKENTSEU_UNKENY_NKUNKENYMAILLAGEPHYSIQUE_H__

#include "Unkeny/Maillage/NkUnkenyMaillage.h"

namespace nkentseu {
	namespace unkeny {

		class NkScene;

		/// Construit la physique des parties de chaque maillage qui n'en a pas encore
		/// (entite active, scene physique), et detruit celle des entites eteintes.
		/// NkScene::Pas l'appelle avant ses pas fixes : rien a faire a la main.
		void NkMaillagesConstruire(NkScene &scene);
		/// A chaque pas fixe, AVANT la physique : les ressorts des liens elastiques,
		/// les ancres qui suivent leur entite.
		void NkMaillagesAvantPasFixe(NkScene &scene, float32 dt);
		/// Apres le pas : les liens qui CASSENT (force ou allongement au-dela de
		/// leur rupture).
		void NkMaillagesApresPasFixe(NkScene &scene, float32 dt);
		/// Detruit la physique d'UN maillage (corps, liens, ancre ; la matiere aussi
		/// si `particules`) et remet ses transitoires a zero.
		void NkMaillageDetruirePhysique(NkScene &scene, NkMaillage2D &m, bool particules = true);
		/// Remet les transitoires a zero SANS rien detruire (la photo les a recopies,
		/// les corps qu'ils designent n'existent pas dans cette scene).
		void NkMaillageOublierPhysique(NkMaillage2D &m) noexcept;

		/// Les positions MONDE des sommets, telles qu'on les dessine : en jeu, celles
		/// de la physique des parties ; sinon le transform. Rend leur nombre.
		uint32 NkMaillagePositionsMonde(const NkScene &scene, const NkTransform2D &t, const NkMaillage2D &m,
										NkVec2f *sortie) noexcept;
		/// Distance SIGNEE d'un point du monde au maillage tel qu'il est dessine
		/// (negative dedans) : la prise au clic de l'editeur.
		float32 NkDistanceMaillage2D(const NkScene &scene, const NkTransform2D &t, const NkMaillage2D &m,
									 const NkVec2f &p) noexcept;
		/// La partie `k` est-elle EN JEU (son corps existe) ?
		bool NkPartieEnJeu2D(const NkScene &scene, const NkMaillage2D &m, uint32 k) noexcept;

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYMAILLAGEPHYSIQUE_H__
