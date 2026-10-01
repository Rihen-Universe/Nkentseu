//
// NkStyleNodal.h
// =============================================================================
// Description :
//   LES JETONS DU STYLE NODAL -- un seul endroit pour TOUTES les couleurs,
//   rayons, epaisseurs et tailles de la programmation nodale du kit : la toile
//   (NkCanevasNoeuds), le menu contextuel des noeuds (NkMenuNoeuds) et le
//   panneau « Mon Blueprint » (NkMonBlueprint).
//
//   SOURCE : LA CHARTE DE RIHEN (editeur_nodal.free, decodee le 01/10 dans
//   References/Blueprint/CHARTE.md, planches 01 a 08) et sa reference
//   principale (planches/capture_reference.png). Changer CE fichier change
//   toute la programmation nodale ; la logique n'y touche pas.
//
// Ce que la charte fixe (et que ces jetons portent) :
//   - fond #121212 et GRILLE DE POINTS (pas 22, jamais des lignes) ; corps
//     #212121, rayon 0, contour 1 px #33333C ; champs #2B2B2B ;
//   - en-tete = CATEGORIE (8 teintes), rayon 5 en haut, 12 en haut a gauche
//     pour un evenement ; FILET sous l'en-tete = NATURE : orange #F79A28 (le
//     noeud execute), petrole #0A555F (il calcule) ;
//   - prise de DONNEE : languette de ratio 1:3,7, haute comme la moitie de
//     l'en-tete, collee au bord et entierement DEHORS, creuse = non branchee,
//     pleine = branchee ; prise d'EXECUTION : rectangle A POINTE, orange, la
//     seule a chevaucher le corps ; l'entree d'execution est sur l'EN-TETE ;
//   - 7 familles de types (la couleur dit la famille, le glyphe le type) ;
//     fil de valeur 2 px couleur du type, fil d'execution 3,5 px TOUJOURS
//     orange, fil tire en pointille gris ;
//   - etats : survol (filet #5A5A68), selection (bordure claire), erreur
//     (corps #2A1A1A, bordure #E4443C, la raison ECRITE dans le noeud),
//     lasso orange pointille, cadre #4E9A5A (double filet, bandeau 20 px) ;
//   - trois paliers de dezoom : 100 % tout, 55 % les valeurs partent, 25 % un
//     rectangle de la categorie et son filet.
//
//   Ce que la charte ne dit pas (§ 13) et qui est PROPOSE ici dans son esprit :
//   la trace d'execution (« Simuler » : fils servis, compteurs « x3 »), et les
//   couleurs de la coloration du code des noeuds Expression / Code (prises sur
//   la reference principale).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKEDITORKIT_NKSTYLENODAL_H__
#define __NKENTSEU_NKEDITORKIT_NKSTYLENODAL_H__

#include "NKCore/NkTypes.h"
#include "NKGui/Core/NkGuiTypes.h"

#include <cstring>

namespace nkentseu {
	namespace editorkit {

		/// La CATEGORIE d'un noeud : la couleur de son en-tete (charte, planche 01
		/// panneau 12). Les evenements et les actions sont « flot · instruction » ;
		/// un evenement se reconnait a sa FORME (coin 12 px, aucune entree
		/// d'execution), pas a sa couleur.
		enum class NkNatureNoeud : uint8 {
			NK_DEFAUT = 0, ///< inconnu
			NK_EVENEMENT,  ///< flot · instruction (+ coin 12, pas d'entree d'execution)
			NK_FLUX,	   ///< flot · instruction
			NK_ACTION,	   ///< flot · instruction
			NK_VALEUR,	   ///< outillage · maths
			NK_FONCTION,   ///< variable · objet (un graphe appele, planche 08)
			NK_VARIABLE,   ///< variable · objet
			NK_CONTEXTE,   ///< entree / contexte
			NK_SORTIE,	   ///< sortie du graphe (texte sombre)
			NK_SURFACE,	   ///< surface / BSDF
			NK_TEXTURE,	   ///< texture · couleur
			NK_ERREUR,	   ///< erreur
			NK_NOMBRE
		};

		/// La forme d'un noeud a l'ecran (charte, planche 03).
		enum class NkFormeNoeud : uint8 {
			NK_NORMAL = 0, ///< en-tete + filet + rangees
			NK_COMPACT,	   ///< le « relais nomme » (la puce) : bloc-icone, libelle, une sortie
			NK_RELAIS,	   ///< le relais NU : un petit carre de la couleur du type
			NK_COMMENTAIRE ///< le CADRE : double filet, bandeau titre, compteur
		};

		/// « #RRVVBB » (les codes de la charte) et une opacite -> NkColor, par le
		/// constructeur 0xRRVVBBAA de NKMath (pas de convertisseur maison).
		inline nkgui::NkColor NkHex(uint32 rvb, uint8 a = 255) noexcept {
			return nkgui::NkColor((rvb << 8) | static_cast<uint32>(a));
		}

		/// Tous les jetons. Les valeurs par defaut SONT le style.
		struct NkJetonsNodal {
				// ── La toile ────────────────────────────────────────────────────
				nkgui::NkColor fond = NkHex(0x121212);
				nkgui::NkColor point = NkHex(0x2B2B33); ///< la grille de POINTS
				float32 pasGrille = 22.f;
				float32 taillePoint = 1.6f;

				// ── Les noeuds ──────────────────────────────────────────────────
				nkgui::NkColor corps = NkHex(0x212121);
				nkgui::NkColor bord = NkHex(0x33333C);
				nkgui::NkColor survol = NkHex(0x5A5A68);
				nkgui::NkColor ombre = NkHex(0x000000, 70);
				float32 rayonEntete = 5.f;
				float32 rayonEvenement = 12.f;
				float32 largeurMin = 190.f;
				float32 hauteurEntete = 26.f;		///< titre seul
				float32 hauteurEnteteDouble = 36.f; ///< titre + sous-titre
				float32 filet = 3.f;				///< sous l'en-tete
				float32 hauteurRangee = 24.f;
				float32 ecartGroupes = 8.f; ///< entre les sorties d'execution et les donnees
				float32 margeNoeud = 10.f;
				/// En-tete par categorie (NkNatureNoeud), et son texte.
				nkgui::NkColor entete[static_cast<uint32>(NkNatureNoeud::NK_NOMBRE)] = {
					NkHex(0x2A2A30), // inconnu
					NkHex(0x8A5A2A), // evenement : flot · instruction
					NkHex(0x8A5A2A), // flux
					NkHex(0x8A5A2A), // action
					NkHex(0x4A6B8A), // valeur : outillage · maths
					NkHex(0x6B4A8A), // fonction (graphe appele)
					NkHex(0x6B4A8A), // variable · objet
					NkHex(0x0A555F), // entree / contexte
					NkHex(0xF79A28), // sortie du graphe
					NkHex(0x2A6B6B), // surface / BSDF
					NkHex(0x8A6B2A), // texture · couleur
					NkHex(0x8A3A30), // erreur
				};
				nkgui::NkColor texteEntete = NkHex(0xEEF2F6);
				nkgui::NkColor texteEnteteSombre = NkHex(0x2A1A08); ///< sur l'en-tete orange
				nkgui::NkColor triangleTitre = NkHex(0xF3E9DA);	///< le « ▼ » devant le titre
				nkgui::NkColor aideTitre = NkHex(0x9FB4C8);		///< le « ? » a droite
				/// Le FILET : le noeud execute / il calcule / on ne sait pas.
				nkgui::NkColor filetExecute = NkHex(0xF79A28);
				nkgui::NkColor filetCalcule = NkHex(0x0A555F);
				nkgui::NkColor filetInconnu = NkHex(0x5A5A64);
				nkgui::NkColor texte = NkHex(0xC8CCD4);		///< libelle de rangee, valeur
				nkgui::NkColor attenue = NkHex(0x7A7A85);
				nkgui::NkColor branchee = NkHex(0x6A6A6A); ///< « branchée » a la place du champ
				nkgui::NkColor libelleBranche = NkHex(0xF79A28);
				nkgui::NkColor champ = NkHex(0x2B2B2B);
				nkgui::NkColor champBord = NkHex(0x3A3A44);
				float32 largeurChamp = 92.f;
				float32 hauteurChamp = 17.f;

				// ── Les prises ──────────────────────────────────────────────────
				/// Donnee : languette dehors, ratio 1:3,7, moitie de l'en-tete.
				float32 priseLargeur = 4.5f;
				float32 priseHauteur = 13.f;
				float32 priseContour = 1.6f;
				/// Execution : rectangle a pointe (pointe a 58 %), seule a chevaucher le corps.
				float32 execLargeur = 11.f;
				float32 execHauteur = 13.f;
				float32 execDehors = 3.f;
				nkgui::NkColor exec = NkHex(0xF79A28);
				/// Pastille de type (glyphe) : fond = type a 22 %.
				float32 pastilleAlpha = 0.22f;

				// ── Les fils ────────────────────────────────────────────────────
				float32 epaisseurExec = 3.5f;
				float32 epaisseurDonnee = 2.f;
				nkgui::NkColor filTire = NkHex(0x7A7A85);
				/// (PROPOSE, hors charte § 13.3) la trace d'execution : un fil qui a
				/// servi garde son orange plein et gagne une lueur ; celui qui n'a
				/// pas servi passe a `alphaEstompe`.
				nkgui::NkColor lueur = NkHex(0xF79A28);
				float32 lueurLarge = 11.f;
				float32 alphaEstompe = 0.32f;

				// ── Les etats ───────────────────────────────────────────────────
				/// ⚠ CONTRADICTION de la charte (§ 12.3) : planche 04 = orange 1,8 px ;
				/// planche 01 (corrigee le 23/08) = clair 1,6 px, « l'orange est
				/// reserve au lasso ». Retenu : CLAIR (la plus recente des regles).
				nkgui::NkColor selection = NkHex(0xE8E8EE);
				float32 epaisseurSelection = 1.6f;
				nkgui::NkColor erreur = NkHex(0xE4443C);
				nkgui::NkColor corpsErreur = NkHex(0x2A1A1A);
				nkgui::NkColor lasso = NkHex(0xF79A28);
				/// (PROPOSE) les compteurs de la trace, en pastille d'en-tete.
				nkgui::NkColor compteur = NkHex(0xF79A28);
				nkgui::NkColor compteurZero = NkHex(0xE4443C);

				// ── Le cadre (« commentaire ») et le relais ────────────────────
				nkgui::NkColor cadre = NkHex(0x4E9A5A);
				nkgui::NkColor cadreTexte = NkHex(0x10240F);
				float32 cadreRemplissage = 0.08f;
				float32 hauteurEnteteCommentaire = 20.f; ///< ⚠ 20 px (texte valide), la planche 05 dessine 26
				float32 cadreRetrait = 8.f;
				float32 rayonRelais = 5.f;

				// ── La barre d'outils verticale et la « Portee » ───────────────
				nkgui::NkColor outilFond = NkHex(0x1E1E24, 240);
				nkgui::NkColor outilBord = NkHex(0x33333C);
				nkgui::NkColor outilActif = NkHex(0xF79A28);
				nkgui::NkColor outilIcone = NkHex(0xC8CCD4);
				nkgui::NkColor outilIconeActif = NkHex(0x2A1A08);
				float32 outilTaille = 32.f;

				// ── Les blocs de code (Expression, Code) -- PROPOSE (§ 13.1) ───
				/// Pris sur la reference principale (noeud « Evaluate »).
				nkgui::NkColor codeFond = NkHex(0x1A1A1A);
				nkgui::NkColor codeFonction = NkHex(0xE5604A); ///< $round
				nkgui::NkColor codeIdent = NkHex(0xE8CF4A);	   ///< testsTotal
				nkgui::NkColor codeNombre = NkHex(0xB57CFF);   ///< 100
				nkgui::NkColor codeOperateur = NkHex(0xE5604A);
				nkgui::NkColor codeTexte = NkHex(0x9FE8A8); ///< « ... »
				nkgui::NkColor codeAutre = NkHex(0xC8CCD4);
				nkgui::NkColor codeErreur = NkHex(0xE4443C);

				// ── Le menu contextuel (un seul panneau de recherche, planche 07) ──
				nkgui::NkColor menuFond = NkHex(0x1E1E24, 252);
				nkgui::NkColor menuBord = NkHex(0x33333C);
				nkgui::NkColor menuSurvol = NkHex(0x2E2E38);
				nkgui::NkColor menuCategorie = NkHex(0xF79A28);
				nkgui::NkColor menuOmbre = NkHex(0x000000, 140);
				float32 menuLargeur = 340.f;
				float32 menuHauteur = 430.f;
				float32 menuRangee = 22.f;

				// ── Le panneau « Mon Blueprint » ───────────────────────────────
				nkgui::NkColor panneauFond = NkHex(0x17171B);
				nkgui::NkColor sectionFond = NkHex(0x1E1E24);
				nkgui::NkColor sectionTexte = NkHex(0xEEF2F6);
				nkgui::NkColor elementSurvol = NkHex(0x26262C);
				nkgui::NkColor elementChoisi = NkHex(0x3A2A18);
				nkgui::NkColor elementChoisiBord = NkHex(0xF79A28);
				float32 hauteurSection = 26.f;
				float32 hauteurElement = 22.f;

				// ── Les paliers de dezoom ──────────────────────────────────────
				float32 palierValeurs = 0.55f;	///< en dessous : valeurs et champs partent
				float32 palierRectangle = 0.30f; ///< en dessous : le rectangle de la categorie
		};

		/// Le jeu de jetons courant (le seul a changer pour suivre la charte).
		inline const NkJetonsNodal &NkJetonsNodalParDefaut() noexcept {
			static const NkJetonsNodal j;
			return j;
		}

		/// LES 7 FAMILLES de la charte (planche 02) : la couleur dit la FAMILLE,
		/// le GLYPHE dit le type.
		inline nkgui::NkColor NkCouleurTypeNodal(const char *type) noexcept {
			struct T {
					const char *nom;
					uint32 c;
			};
			static const T k[] = {{"exec", 0xF79A28},	 {"booleen", 0x17B2EB}, {"entier", 0x17B2EB},  {"reel", 0x17B2EB},
								  {"vec2", 0xC0EB81},	 {"vec3", 0xC0EB81},	{"texte", 0xF2559B},   {"couleur", 0xD9B6A3},
								  {"shader", 0xD9B6A3},	 {"texture", 0xD9B6A3}, {"entite", 0x81EBEB},  {"objet", 0x81EBEB},
								  {"asset", 0x81EBEB}};
			for (const T &t : k) {
				if (type != nullptr && std::strcmp(type, t.nom) == 0) {
					return NkHex(t.c);
				}
			}
			return NkHex(0x9AA3AD); // quelconque : gris, absence de couleur
		}

		/// Le GLYPHE d'un type (planche 02) : « 1.0 », « 12 », « V/F », « abc »...
		inline const char *NkGlypheTypeNodal(const char *type) noexcept {
			struct T {
					const char *nom;
					const char *g;
			};
			static const T k[] = {{"reel", "1.0"},	 {"entier", "12"},	{"booleen", "V/F"}, {"texte", "abc"}, {"vec2", "XY"},
								  {"vec3", "XYZ"},	 {"couleur", "RVB"}, {"entite", "OBJ"},	{"objet", "OBJ"}, {"asset", "REF"},
								  {"shader", "SH"},	 {"texture", "TEX"}, {"exec", "->"}};
			for (const T &t : k) {
				if (type != nullptr && std::strcmp(type, t.nom) == 0) {
					return t.g;
				}
			}
			return "?";
		}

		/// `c` a l'opacite multipliee par `a` (0..1).
		inline nkgui::NkColor NkAlphaNodal(nkgui::NkColor c, float32 a) noexcept {
			const float32 v = static_cast<float32>(c.a) * (a < 0.f ? 0.f : (a > 1.f ? 1.f : a));
			c.a = static_cast<uint8>(v + 0.5f);
			return c;
		}

	} // namespace editorkit
} // namespace nkentseu

#endif // __NKENTSEU_NKEDITORKIT_NKSTYLENODAL_H__
