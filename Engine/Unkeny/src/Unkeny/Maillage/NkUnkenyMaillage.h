//
// NkUnkenyMaillage.h
// =============================================================================
// Description :
//   LE MAILLAGE 2D (2026-10-02, R31 de la feuille de route d'UnkenyEditor).
//   Demande de Rihen du 01/10 : « un composant de maillage 2D (mesh renderer /
//   mesh editing) avec sa fenetre d'edition : editer les sommets, decouper en
//   parties, et donner une physique independante a chaque partie si on le
//   souhaite ».
//
//   Un NkMaillage2D est un ensemble de SOMMETS (position, UV, couleur) relies
//   en TRIANGLES ; une texture se deforme avec eux. Les triangles sont ranges
//   en PARTIES (le corps, un bras, un eclat de vitre) : chaque partie a son
//   ordre de dessin et, si on le veut, SA physique (un corps rigide, ou un
//   corps mou XPBD), ses collisionneurs generes depuis son contour, et des
//   LIENS vers les autres parties (soudure, pivot, ressort). En jeu, le
//   maillage suit ses parties : une gelee dont les morceaux bougent, un decor
//   qui se brise (Maillage/NkUnkenyMaillagePhysique.h).
//
// Caracteristiques :
//   - DONNEES SEULEMENT, copiables bit a bit, a CAPACITE FIXE (la regle de
//     NkUnkenyComposants.h) : la photo (Jouer / Arreter, Ctrl+Z), le prefab et
//     le fichier .nkscene le portent sans code nouveau, CHAMP PAR CHAMP
//     (NkChampsMaillage2D, declare par NkScene::Init).
//   - CHAQUE SOMMET APPARTIENT A UNE SEULE PARTIE. Une couture entre deux
//     parties est faite de sommets DOUBLES (un par partie, a la meme place) :
//     c'est ce qui permet a deux parties de se separer. NkMaillageRefaireCoutures
//     tient cette regle apres chaque geste ; l'editeur deplace les doubles
//     ensemble (la couture reste fermee tant qu'on ne joue pas).
//   - LES OS (R30, a venir) : chaque sommet porte DEJA jusqu'a 4 os et leurs
//     poids (`os`, `poids`). Le squelette vivra dans NKAnima (contraint au
//     plan) et deformera CE maillage par ces poids ; rien ici ne les lit
//     encore, et un maillage sans os (NK_MAILLAGE2D_SANS_OS partout) se dessine
//     comme aujourd'hui. Voir Applications/UnkenyEditor/design/06-maillage-2d.md.
//
// ⚠️ LES CAPACITES, ET POURQUOI ELLES SONT CELLES-LA
//   128 sommets, 240 triangles, 8 parties, 16 liens. Un sprite de jeu se
//   decoupe en 30 a 100 sommets (l'editeur de sprite d'Unity en produit autant) ;
//   au-dela, la densite se regle (NkOptionsMaillage2D). La photo de CHAQUE
//   entite porte la taille de chaque composant decrit (NkScene::NkPhotoEntite::
//   extra) : environ 6,5 Ko de plus par entite, a peser avant d'augmenter.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYMAILLAGE_H__
#define __NKENTSEU_UNKENY_NKUNKENYMAILLAGE_H__

#include "Unkeny/Scene/NkUnkenyChamps.h"
#include "Unkeny/Scene/NkUnkenyComposants.h"

namespace nkentseu {
	namespace unkeny {

		struct NkRenduForme2D; // Scene/NkUnkenyFormes.h

		static constexpr uint32 NK_MAILLAGE2D_SOMMETS_MAX = 128u;
		static constexpr uint32 NK_MAILLAGE2D_TRIANGLES_MAX = 240u;
		static constexpr uint32 NK_MAILLAGE2D_PARTIES_MAX = 8u;
		static constexpr uint32 NK_MAILLAGE2D_LIENS_MAX = 16u;
		/// Os par sommet (R30) : 4, comme les poids de peau de NKRenderer et d'UE.
		static constexpr uint32 NK_MAILLAGE2D_OS = 4u;
		/// « Pas d'os » dans un emplacement de `os`.
		static constexpr uint8 NK_MAILLAGE2D_SANS_OS = 0xFFu;
		/// La somme des poids d'un sommet pondere (16 bits par poids, comme UE5).
		static constexpr uint16 NK_MAILLAGE2D_POIDS_PLEIN = 0xFFFFu;

		/// La physique d'une partie.
		enum class NkPhysiquePartie2D : uint8 {
			NK_AUCUNE = 0, ///< elle suit l'entite (son transform, ou son corps rigide s'il en a un)
			NK_RIGIDE,	   ///< un corps rigide, collisionneur genere depuis son contour
			NK_MOLLE	   ///< un corps mou XPBD : une particule par sommet, ses aretes en liens
		};

		/// Ce qui tient deux parties ensemble.
		enum class NkGenreLienParties2D : uint8 {
			NK_RIGIDE = 0, ///< soudure : position ET angle relatifs gardes
			NK_ELASTIQUE,  ///< deux ressorts aux bouts de la couture : ca plie et ca revient
			NK_PIVOT	   ///< un point commun, la rotation libre (une charniere)
		};

		/// Une PARTIE : un ensemble de triangles, son ordre de dessin, sa physique.
		struct NkPartieMaillage2D {
				char nom[16] = {};
				/// Sa couleur dans la fenetre d'edition (0 = celle de la palette).
				uint32 couleur = 0u;
				/// L'ordre de dessin DANS le maillage : la plus grande passe devant
				/// (un bras derriere le corps). A egalite, l'ordre des triangles.
				int32 ordre = 0;
				uint8 physique = 0;	 ///< NkPhysiquePartie2D
				uint8 typeCorps = 2; ///< NkTypeCorps (rigide) : dynamique par defaut
				/// Le collisionneur genere depuis le contour : faux, il ne touche rien.
				bool collision = true;
				float32 masse = 1.f; ///< kg (molle : repartie sur ses particules)
				float32 friction = 0.5f;
				float32 rebond = 0.1f;
				float32 raideur = 0.6f; ///< molle : raideur des aretes [0,1]
				float32 forme = 0.05f;	///< molle : appariement de forme [0,1] (la gelee revient)
				// --- TRANSITOIRES : l'etat EN JEU, jamais ecrit ni decrit ---------
				/// Rigide : le NkBodyId ; molle : l'id STABLE du corps de particules.
				uint32 corps = 0u;
				/// Molle : le rang de la premiere particule de la partie dans son corps.
				uint32 premier = 0u;
				/// Le centre de REPOS de la partie (repere du maillage), a la construction.
				NkVec2f centre{0.f, 0.f};
		};

		/// Un LIEN entre deux parties.
		struct NkLienParties2D {
				uint8 a = 0;
				uint8 b = 0;
				uint8 genre = 0;			 ///< NkGenreLienParties2D
				float32 raideur = 80.f;		 ///< elastique : N/m par ressort
				float32 amortissement = 3.f; ///< elastique : N.s/m
				/// 0 = incassable. Rigide et pivot : la FORCE (N) au-dela de laquelle le
				/// lien casse ; elastique : l'ALLONGEMENT (m).
				float32 rupture = 0.f;
				// --- TRANSITOIRES -------------------------------------------------
				/// Les deux corps rigides tenus (une partie AUCUNE : le corps de l'entite,
				/// ou l'ancre du maillage).
				uint32 corpsA = 0u;
				uint32 corpsB = 0u;
				bool casse = false;
				bool construit = false;
				/// Elastique : les ancres des deux ressorts, dans le repere de chaque corps.
				NkVec2f ancreA[2] = {};
				NkVec2f ancreB[2] = {};
		};

		/// LE COMPOSANT.
		struct NkMaillage2D {
				/// La texture deformee (NkTextures2D) ; 0 = les couleurs de sommet seules.
				uint32 texId = 0u;
				/// Multiplie la couleur de chaque sommet (et donc la texture).
				uint32 teinte = 0xFFFFFFFFu;
				/// L'ordre de dessin DANS LA SCENE, partage avec les sprites et les formes.
				int32 couche = 0;
				bool visible = true;
				uint8 nbSommets = 0;
				uint8 nbTriangles = 0;
				/// Au moins 1 des qu'il y a des triangles (la partie 0, « Corps »).
				uint8 nbParties = 0;
				uint8 nbLiens = 0;
				/// Faux (defaut) : les parties PHYSIQUES d'un meme maillage ne se
				/// touchent pas entre elles (un eclat ne se coince pas dans son voisin).
				bool partiesSeTouchent = false;
				/// L'asset d'origine (« Contenu/Maillages/Gelee.nkmesh2d »), vide = propre a l'entite.
				char source[64] = {};

				// --- Les sommets ---------------------------------------------------
				NkVec2f positions[NK_MAILLAGE2D_SOMMETS_MAX] = {}; ///< m, repere de l'entite
				NkVec2f uvs[NK_MAILLAGE2D_SOMMETS_MAX] = {};		 ///< dans la texture (0..1, v vers le bas)
				uint32 couleurs[NK_MAILLAGE2D_SOMMETS_MAX] = {};	 ///< RGBA ; 0 dans une case libre
				uint8 partieSommet[NK_MAILLAGE2D_SOMMETS_MAX] = {};
				/// R30 : jusqu'a 4 os par sommet (NK_MAILLAGE2D_SANS_OS = aucun) et leurs
				/// poids (somme NK_MAILLAGE2D_POIDS_PLEIN). Lus par personne aujourd'hui.
				uint8 os[NK_MAILLAGE2D_SOMMETS_MAX * NK_MAILLAGE2D_OS] = {};
				uint16 poids[NK_MAILLAGE2D_SOMMETS_MAX * NK_MAILLAGE2D_OS] = {};

				// --- Les triangles (sens TRIGONOMETRIQUE, y vers le haut) -----------
				uint8 triangles[NK_MAILLAGE2D_TRIANGLES_MAX * 3u] = {};
				uint8 partieTriangle[NK_MAILLAGE2D_TRIANGLES_MAX] = {};

				// --- Les parties et leurs liens ------------------------------------
				NkPartieMaillage2D parties[NK_MAILLAGE2D_PARTIES_MAX];
				NkLienParties2D liens[NK_MAILLAGE2D_LIENS_MAX];

				// --- TRANSITOIRES : l'etat EN JEU ----------------------------------
				bool construit = false; ///< la physique des parties est en place
				uint32 groupe = 0u;		///< le bit de groupe de collision de ses parties
				uint32 ancre = 0u;		///< le corps cinematique qui tient les parties a l'entite
		};

		// =====================================================================
		// LECTURE
		// =====================================================================
		/// La couleur d'edition de la partie `k` (la sienne, ou celle de la palette).
		uint32 NkCouleurPartie2D(const NkMaillage2D &m, uint32 k) noexcept;
		/// L'aire SIGNEE du triangle `t` (positive = sens trigonometrique).
		float32 NkAireTriangle2D(const NkMaillage2D &m, uint32 t) noexcept;
		/// Les triangles RETOURNES (aire <= 0, ou degeneres) : 0 pour un maillage sain.
		uint32 NkTrianglesRetournes2D(const NkMaillage2D &m) noexcept;
		/// Les indices, les parties et les comptes tiennent-ils ? (`pourquoi` : la premiere faute)
		bool NkMaillageCoherent2D(const NkMaillage2D &m, const char **pourquoi = nullptr) noexcept;
		/// La demi-boite des sommets (repere de l'entite, centree sur l'origine).
		NkVec2f NkDemiBoiteMaillage2D(const NkMaillage2D &m) noexcept;
		/// Le triangle qui contient `p` (repere du maillage), ou -1.
		int32 NkTriangleSous2D(const NkMaillage2D &m, const NkVec2f &p) noexcept;
		/// Distance signee d'un point (repere du maillage) : negative dans un
		/// triangle, sinon la distance au bord le plus proche.
		float32 NkDistanceMaillageLocal2D(const NkMaillage2D &m, const NkVec2f &p) noexcept;
		/// Le centre de la partie `k` (moyenne des triangles ponderee par l'aire).
		NkVec2f NkCentrePartie2D(const NkMaillage2D &m, uint32 k) noexcept;
		/// Le CONTOUR de la partie `k` (sa plus grande boucle de bord), sens
		/// trigonometrique, en positions ; `indices` (facultatif) recoit les sommets.
		uint32 NkContourPartie2D(const NkMaillage2D &m, uint32 k, NkVec2f *sortie, uint32 capacite,
								 uint8 *indices = nullptr) noexcept;
		/// Le contour de TOUT le maillage (les parties soudees), comme ci-dessus.
		uint32 NkContourMaillage2D(const NkMaillage2D &m, NkVec2f *sortie, uint32 capacite) noexcept;
		/// Les sommets qui sont a la meme place que `s` (les doubles d'une couture),
		/// `s` compris. Rend leur nombre.
		uint32 NkDoublesSommet2D(const NkMaillage2D &m, uint32 s, uint8 *sortie, uint32 capacite) noexcept;

		// =====================================================================
		// FABRICATION
		// =====================================================================
		/// Les reglages de la fabrication automatique.
		struct NkOptionsMaillage2D {
				/// La DENSITE : combien de mailles sur le plus grand cote (2..16). Plus
				/// elle est haute, plus il y a de sommets (et plus la deformation est souple).
				int32 densite = 6;
				/// Des sommets A L'INTERIEUR (sinon : le contour seul, triangule en
				/// oreilles -- une vitre rigide n'en a pas besoin, une gelee si).
				bool interieur = true;
				/// Sprite : un pixel compte s'il est au moins aussi opaque (0..255).
				uint8 seuilAlpha = 128u;
				/// Sprite : la simplification du contour, en pixels de l'image.
				float32 tolerancePx = 1.2f;
		};

		/// Vide (aucun sommet) ; texture, teinte, couche et visibilite gardees.
		void NkMaillageVider2D(NkMaillage2D &m) noexcept;
		/// Un RECTANGLE de `taille` (pivot 0..1 comme un sprite), en grille nx x ny,
		/// UV de toute l'image.
		bool NkMaillageRectangle2D(NkMaillage2D &m, const NkVec2f &taille, const NkVec2f &pivot, int32 nx, int32 ny) noexcept;
		/// Depuis un CONTOUR ferme (repere de l'entite, simple, l'un ou l'autre sens) :
		/// le contour reechantillonne, des sommets interieurs, triangulation de
		/// Delaunay restreinte a l'interieur. UV : la boite du contour -> 0..1.
		bool NkMaillageDepuisContour2D(NkMaillage2D &m, const NkVec2f *contour, uint32 n, const NkOptionsMaillage2D &o) noexcept;
		/// Depuis un SPRITE : le contour de l'ALPHA de son image (region UV comprise),
		/// sa plus grande ile ; la texture et la couleur du sprite sont reprises, les
		/// UV suivent la region. Une image sans pixel opaque : faux. Sans image
		/// (`rgba` nul) : le rectangle du sprite.
		bool NkMaillageDepuisSprite2D(NkMaillage2D &m, const NkSprite2D &s, const uint8 *rgba, int32 w, int32 h,
									  const NkOptionsMaillage2D &o) noexcept;
		/// Depuis une FORME 2D (son contour dessine, arrondis compris) ; les sommets
		/// prennent la couleur de remplissage. Une ligne ouverte : faux.
		bool NkMaillageDepuisForme2D(NkMaillage2D &m, const NkRenduForme2D &f, const NkOptionsMaillage2D &o) noexcept;
		/// Les UV recalcules depuis les positions, a la maniere d'un sprite (son
		/// rectangle et sa region d'atlas).
		void NkMaillageUVDepuisSprite2D(NkMaillage2D &m, const NkSprite2D &s) noexcept;

		// =====================================================================
		// EDITION (la fenetre d'edition, les bancs, les scripts)
		// =====================================================================
		/// Refait les triangles (Delaunay) sur les sommets actuels. `garderForme` :
		/// seuls restent les triangles dont le centre est DANS l'ancien maillage
		/// (une forme concave reste concave) ; sinon l'enveloppe convexe. Les
		/// triangles gardent la partie de l'ancien triangle qui les contient.
		bool NkMaillageTrianguler2D(NkMaillage2D &m, bool garderForme = true) noexcept;
		/// Refait les sommets depuis le CONTOUR actuel avec la densite `o` (les
		/// parties sont perdues : tout revient a la partie 0, liens compris).
		bool NkMaillageRefaire2D(NkMaillage2D &m, const NkOptionsMaillage2D &o) noexcept;
		/// Ajoute un sommet en `p` (repere du maillage). Dans un triangle : il le
		/// coupe en trois ; sur une arete : il coupe ses deux triangles ; dehors :
		/// il se relie a l'arete de bord la plus proche. L'UV et la couleur suivent
		/// le triangle (prolonges dehors). Rend son indice, ou -1 (plein).
		int32 NkMaillageAjouterSommet2D(NkMaillage2D &m, const NkVec2f &p) noexcept;
		/// Coupe l'arete (a, b) en son milieu. Rend le nouveau sommet, ou -1.
		int32 NkMaillageCouperArete2D(NkMaillage2D &m, uint32 a, uint32 b) noexcept;
		/// Retire les sommets coches dans `choisis` (un octet par sommet) et leurs
		/// triangles ; un trou interieur est rebouche. Rend le nombre retire.
		uint32 NkMaillageRetirerSommets2D(NkMaillage2D &m, const uint8 *choisis) noexcept;
		/// FAIRE UNE PARTIE : les triangles dont les trois sommets sont choisis
		/// deviennent une partie neuve (les coutures se dedoublent). Rend son indice,
		/// ou -1 (aucun triangle entier, ou 8 parties deja).
		int32 NkMaillageFairePartie2D(NkMaillage2D &m, const uint8 *choisis, const char *nom = nullptr) noexcept;
		/// Rend les triangles de la partie `k` a la partie 0 et la retire (k > 0) ;
		/// ses liens partent avec elle.
		bool NkMaillageRetirerPartie2D(NkMaillage2D &m, uint32 k) noexcept;
		/// Les sommets doubles d'une meme partie sont fusionnes, chaque sommet
		/// utilise par deux parties est dedouble, les sommets orphelins partent.
		/// Appelee par tous les gestes ci-dessus.
		void NkMaillageRefaireCoutures2D(NkMaillage2D &m) noexcept;
		/// Ajoute un lien entre les parties `a` et `b`. Rend son indice, ou -1.
		int32 NkMaillageAjouterLien2D(NkMaillage2D &m, uint32 a, uint32 b, NkGenreLienParties2D genre) noexcept;
		bool NkMaillageRetirerLien2D(NkMaillage2D &m, uint32 k) noexcept;
		/// Les parties `a` et `b` se touchent-elles (une couture) ?
		bool NkPartiesVoisines2D(const NkMaillage2D &m, uint32 a, uint32 b) noexcept;
		/// R30 : remet chaque sommet SANS os (poids nuls). Les fabriques l'appellent.
		void NkMaillageSansOs2D(NkMaillage2D &m) noexcept;
		/// Les positions multipliees par `facteur` (l'echelle cuite de l'editeur).
		void NkMaillageMettreAEchelle2D(NkMaillage2D &m, const NkVec2f &facteur) noexcept;

		/// Les champs du composant (sauvegarde, photo, prefab), declares par NkScene::Init.
		const NkChampSauve *NkChampsMaillage2D(uint32 &nombre) noexcept;

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYMAILLAGE_H__
