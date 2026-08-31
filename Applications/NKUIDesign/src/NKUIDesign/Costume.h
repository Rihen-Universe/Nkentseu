#pragma once
// Costume.h — le COSTUME EXACT de la maquette Banani (remandat du 2026-08-31).
//
//  « Va planche après planche et code-les À L'IDENTIQUE — THÈME, DESIGN,
//  POLICE, TOUT. » Ce fichier porte ce que l'exactitude exige et que le reste
//  du programme n'a pas à connaître :
//
//   1. LES POLICES DE LA MAQUETTE. L'export déclare `--font-body: Inter` et
//      des corps de 9 à 16 px. Inter est EMBARQUÉE (NkEmbeddedFontId::Inter,
//      licence OFL) — chaque corps est un atlas propre, téléversé par le
//      crochet `NkEditorShell::UploadAppFont` (texIds +16..+23, réservés).
//      ⚠️ SEULE LA GRAISSE 400 EST EMBARQUÉE. Les 500/600/700 de la maquette
//      sont APPROXIMÉES par un double trait décalé (TexteGras) — c'est dit
//      ici et dans le rapport, pas caché : embarquer les vraies graisses
//      (OFL, même famille) est la suite propre.
//
//   2. LES ICÔNES DE LA MAQUETTE, primitive par primitive. L'export ne
//      contient AUCUN import Lucide (mesuré, doc 11) : chaque icône est un
//      SVG inline dont les tracés sont recopiés ICI dans le vocabulaire de la
//      liste de dessin (AddLine/AddRect/AddPolyline), aux tailles du JSX.
//      Les couleurs restent des PARAMÈTRES : l'appelant les résout par les
//      rôles du thème, jamais en dur (règle du dépôt).
//
//  Ce fichier ne dessine RIEN de lui-même : ce sont des fonctions que les
//  panneaux appellent. Aucun état, aucune dépendance au document.

#include <NKGui/NKGui.h>

namespace nkuidesign {
	namespace costume {

		using nkentseu::float32;
		using nkentseu::int32;
		using nkentseu::uint32;
		using nkgui::NkColor;
		using nkgui::NkGuiDrawList;
		using nkgui::NkGuiFont;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		// ═════════════════════════════════════════════════════════════════════
		//  1. LES POLICES — un atlas par corps de la maquette
		// ═════════════════════════════════════════════════════════════════════

		/// LE RÉGLAGE UNIQUE DE TAILLE DU TEXTE D'INTERFACE (test de Rodolf,
		/// 31/08 : « le header donc les menus, le titre, les boutons, le texte
		/// des onglets… le texte dans toute l'interface est trop petit,
		/// agrandis encore un peu »). UN seul bouton, pas des retouches par
		/// widget : chaque corps de la maquette (9..16) est rendu à +2 px, et
		/// la police d'interface de la coquille (menus, palette) passe par la
		/// même fonction (main.cpp). Les largeurs se MESURENT sur les vraies
		/// polices au moment du dessin, donc tout suit ; les hauteurs de
		/// rangées et de bandes sont validées sur capture. Les noms px9..px16
		/// continuent de nommer le corps DE LA MAQUETTE, pas le corps rendu.
		/// 2e passe de Rodolf (31/08) : « le texte est ENCORE trop petit,
		/// partout » — +2 est devenu +4. Un seul chiffre à changer ici.
		inline float32 CorpsMaquette(float32 px) {
			return px + 4.f;
		}

		struct Polices {
				NkGuiFont px9;	///< badges de rôle, min/max, sections 9-10
				NkGuiFont px10; ///< libellés de champs, sections MAJUSCULES
				NkGuiFont px11; ///< menus, onglets, corps des champs
				NkGuiFont px12; ///< rangées d'arbre, titres de panneaux
				NkGuiFont px13; ///< valeurs fortes (cartes du Dashboard)
				NkGuiFont px15; ///< le « + » des onglets
				NkGuiFont px16; ///< titre du document (« Connexion »)
				bool ok = false;

				/// Charge les sept corps (Inter embarquée) et les téléverse par le
				/// crochet du kit. `dpi` : même règle que la coquille (les corps de
				/// la maquette sont des px logiques).
				template <typename Shell>
				void Charger(Shell &sh, float32 dpi) {
					struct Ligne {
							NkGuiFont *f;
							float32 px;
					} lignes[] = {{&px9, 9.f},	 {&px10, 10.f}, {&px11, 11.f}, {&px12, 12.f},
								  {&px13, 13.f}, {&px15, 15.f}, {&px16, 16.f}};
					ok = true;
					for (uint32 i = 0; i < 7; ++i) {
						// `CorpsMaquette` : le SEUL endroit qui agrandit — voir
						// le bloc au-dessus de la structure.
						if (!lignes[i].f->LoadEmbedded(nkentseu::NkEmbeddedFontId::Inter,
													   CorpsMaquette(lignes[i].px) * dpi)
							|| !sh.UploadAppFont(*lignes[i].f, i))
							ok = false;
					}
				}
		};

		/// L'instance unique (posée par main.cpp au démarrage, lue partout).
		inline Polices &Fontes() {
			static Polices p;
			return p;
		}

		/// L'ATLAS DU COSTUME LE PLUS PROCHE d'une taille PHYSIQUE visée, et
		/// l'échelle résiduelle pour l'atteindre exactement (le palier de la
		/// correction « le texte suit le zoom »). UN seul endroit : le peintre
		/// (NkDesignPaint::TextHex) et le champ d'édition en place partagent ce
		/// choix — deux copies auraient divergé au premier corps ajouté.
		inline const NkGuiFont *AtlasProche(float32 visePhys, float32 &echelle) {
			auto &F = Fontes();
			const NkGuiFont *cand[7] = {&F.px9,	 &F.px10, &F.px11, &F.px12,
										&F.px13, &F.px15, &F.px16};
			const NkGuiFont *best = nullptr;
			float32 bd = 1.0e9f, rendu = 0.f;
			for (int32 i = 0; i < 7; ++i) {
				if (!cand[i]->Valid() || !cand[i]->Face())
					continue;
				const float32 t = cand[i]->Face()->fontSize;
				const float32 d = t > visePhys ? t - visePhys : visePhys - t;
				if (d < bd) {
					bd = d;
					best = cand[i];
					rendu = t;
				}
			}
			echelle = (best && rendu > 0.f) ? visePhys / rendu : 1.f;
			return best;
		}

		// ── Texte : normal, et « gras » approximé (graisses non embarquées) ──
		inline void Texte(NkGuiDrawList &dl, const NkGuiFont &f, float32 x, float32 yHaut,
						  const char *t, const NkColor &c) {
			if (!f.Valid() || !t)
				return;
			dl.AddText(f.Face(), f.TexId(), {x, yHaut + f.Ascent()}, t, c);
		}
		/// fw500 ≈ 0.3 px, fw600 ≈ 0.5 px, fw700 ≈ 0.8 px de double trait.
		inline void TexteGras(NkGuiDrawList &dl, const NkGuiFont &f, float32 x, float32 yHaut,
							  const char *t, const NkColor &c, float32 e = 0.5f) {
			Texte(dl, f, x, yHaut, t, c);
			Texte(dl, f, x + e, yHaut, t, c);
		}
		inline float32 Largeur(const NkGuiFont &f, const char *t) {
			return f.Valid() ? f.MeasureWidth(t) : 0.f;
		}
		/// y du HAUT d'une ligne pour centrer verticalement dans [y, y+h].
		inline float32 CentrerY(const NkGuiFont &f, float32 y, float32 h) {
			return y + (h - (f.Valid() ? f.LineHeight() : 12.f)) * 0.5f;
		}

		// ═════════════════════════════════════════════════════════════════════
		//  2. LES ICÔNES — chaque tracé recopié du JSX (viewBox = px écran)
		// ═════════════════════════════════════════════════════════════════════
		//  Convention : (x, y) = coin HAUT-GAUCHE de la boîte de l'icône ; les
		//  offsets internes sont ceux du SVG d'origine, à l'unité près.

		// chevron 9×9 : « M3 2 L6 4.5 L3 7 » (droite) / « M2 3 L4.5 6 L7 3 » (bas)
		inline void ChevronDroit9(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			const NkVec2 p[3] = {{x + 3.f, y + 2.f}, {x + 6.f, y + 4.5f}, {x + 3.f, y + 7.f}};
			dl.AddPolyline(p, 3, c, 1.2f);
		}
		inline void ChevronBas9(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			const NkVec2 p[3] = {{x + 2.f, y + 3.f}, {x + 4.5f, y + 6.f}, {x + 7.f, y + 3.f}};
			dl.AddPolyline(p, 3, c, 1.2f);
		}

		// page 11×11 : rect 9×9 rx1 + 2 lignes (3,4→8,4) (3,6→7,6)
		inline void IcPage(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddRect({x + 1.f, y + 1.f, 9.f, 9.f}, c, 1.2f, 1.f);
			dl.AddLine({x + 3.f, y + 4.f}, {x + 8.f, y + 4.f}, c, 1.f);
			dl.AddLine({x + 3.f, y + 6.f}, {x + 7.f, y + 6.f}, c, 1.f);
		}
		// panneau 11×11 : rect (1,2 9×7 rx1) + ligne médiane y=5
		inline void IcPanneau(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddRect({x + 1.f, y + 2.f, 9.f, 7.f}, c, 1.2f, 1.f);
			dl.AddLine({x + 1.f, y + 5.f}, {x + 10.f, y + 5.f}, c, 1.f);
		}
		// bouton 11×11 : rect (1,3 9×5 rx1.5) + ligne centrée (3.5,5.5→7.5,5.5)
		inline void IcBouton(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddRect({x + 1.f, y + 3.f, 9.f, 5.f}, c, 1.2f, 1.5f);
			dl.AddLine({x + 3.5f, y + 5.5f}, {x + 7.5f, y + 5.5f}, c, 1.f);
		}
		// texte 11×11 : « M2 3 H9 M5.5 3 V9 »
		inline void IcTexte(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddLine({x + 2.f, y + 3.f}, {x + 9.f, y + 3.f}, c, 1.2f);
			dl.AddLine({x + 5.5f, y + 3.f}, {x + 5.5f, y + 9.f}, c, 1.2f);
		}
		// loupe 13×13 : cercle (5.5,5.5 r4) + trait (8.5,8.5→12,12)
		inline void IcLoupe(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddCircle({x + 5.5f, y + 5.5f}, 4.f, c, 1.2f);
			dl.AddLine({x + 8.5f, y + 8.5f}, {x + 12.f, y + 12.f}, c, 1.2f);
		}
		// document d'onglet 10×10 : rect 8×8 rx1 + 2 barres pleines
		inline void IcDocOnglet(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddRect({x + 1.f, y + 1.f, 8.f, 8.f}, c, 1.2f, 1.f);
			NkColor b = c;
			b.a = (nkentseu::uint8)(c.a * 0.7f);
			dl.AddRectFilled({x + 3.f, y + 3.f, 4.f, 1.5f}, b, 0.3f);
			b.a = (nkentseu::uint8)(c.a * 0.4f);
			dl.AddRectFilled({x + 3.f, y + 5.5f, 3.f, 1.f}, b, 0.3f);
		}

		// ── Les 7 outils du rail flottant (13×13, JSX FloatingToolRail) ──
		// flèche de sélection PLEINE : « M2.5 2 L2.5 11 L5.5 8 L7.5 13 L9 12.2
		// L7 7.5 L11 7.5 Z »
		inline void OutilFleche(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			const NkVec2 p[7] = {{x + 2.5f, y + 2.f}, {x + 2.5f, y + 11.f}, {x + 5.5f, y + 8.f},
								 {x + 7.5f, y + 13.f}, {x + 9.f, y + 12.2f}, {x + 7.f, y + 7.5f},
								 {x + 11.f, y + 7.5f}};
			// Le polygone n'est pas convexe : deux triangles le remplissent
			// exactement (corps de flèche + queue).
			dl.AddTriangleFilled(p[0], p[1], p[6], c);
			dl.AddTriangleFilled(p[1], p[2], p[6], c);
			dl.AddTriangleFilled(p[2], p[5], p[6], c);
			dl.AddTriangleFilled(p[2], p[3], p[4], c);
			dl.AddTriangleFilled(p[2], p[4], p[5], c);
		}
		// cadre (artboard) : 4 barres pleines en croisillon
		inline void OutilCadre(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddRectFilled({x + 1.5f, y + 2.5f, 1.8f, 8.f}, c, 0.4f);
			dl.AddRectFilled({x + 9.7f, y + 2.5f, 1.8f, 8.f}, c, 0.4f);
			dl.AddRectFilled({x + 3.f, y + 1.f, 7.f, 1.8f}, c, 0.4f);
			dl.AddRectFilled({x + 3.f, y + 10.2f, 7.f, 1.8f}, c, 0.4f);
		}
		// rectangle : rect (1.5,3 10×7.5 rx1)
		inline void OutilRect(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddRect({x + 1.5f, y + 3.f, 10.f, 7.5f}, c, 1.2f, 1.f);
		}
		// ellipse (variante) : cercle contour centré
		inline void OutilEllipse(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddCircle({x + 6.5f, y + 6.5f}, 4.5f, c, 1.2f);
		}
		// ligne (variante) : diagonale
		inline void OutilLigne(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddLine({x + 2.f, y + 10.5f}, {x + 11.f, y + 2.5f}, c, 1.4f);
		}
		// plume : pentagone pointe + trait « M9.5 2 L12 4.5 L5 12 L2 12 L2 9 Z »
		inline void OutilPlume(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			const NkVec2 p[5] = {{x + 9.5f, y + 2.f}, {x + 12.f, y + 4.5f}, {x + 5.f, y + 12.f},
								 {x + 2.f, y + 12.f}, {x + 2.f, y + 9.f}};
			dl.AddPolyline(p, 5, c, 1.2f, true);
			dl.AddLine({x + 7.8f, y + 3.8f}, {x + 10.3f, y + 6.3f}, c, 1.f);
		}
		// texte (outil) : « M2.5 2.5 H10.5 M6.5 2.5 V10.5 M4.5 10.5 H8.5 »
		inline void OutilTexte(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddLine({x + 2.5f, y + 2.5f}, {x + 10.5f, y + 2.5f}, c, 1.2f);
			dl.AddLine({x + 6.5f, y + 2.5f}, {x + 6.5f, y + 10.5f}, c, 1.2f);
			dl.AddLine({x + 4.5f, y + 10.5f}, {x + 8.5f, y + 10.5f}, c, 1.2f);
		}
		// image : rect + soleil + montagne
		inline void OutilImage(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddRect({x + 1.5f, y + 2.5f, 10.f, 8.f}, c, 1.2f, 1.f);
			dl.AddCircleFilled({x + 4.5f, y + 5.5f}, 0.9f, c);
			const NkVec2 p[5] = {{x + 1.5f, y + 10.f}, {x + 4.5f, y + 7.f}, {x + 7.f, y + 9.f},
								 {x + 8.5f, y + 7.5f}, {x + 11.5f, y + 10.f}};
			dl.AddPolyline(p, 5, c, 1.f);
		}
		// règle : rect (1.5,4.5 10×4 rx0.4) + 3 graduations
		inline void OutilRegle(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddRect({x + 1.5f, y + 4.5f, 10.f, 4.f}, c, 1.1f, 0.4f);
			dl.AddLine({x + 3.5f, y + 4.5f}, {x + 3.5f, y + 5.8f}, c, 0.9f);
			dl.AddLine({x + 6.5f, y + 4.5f}, {x + 6.5f, y + 7.f}, c, 0.9f);
			dl.AddLine({x + 9.5f, y + 4.5f}, {x + 9.5f, y + 5.8f}, c, 0.9f);
		}
		// chevron de variante 4×4 (coin bas-droit d'un bouton de famille)
		inline void ChevronVariante4(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			const NkVec2 p[3] = {{x + 0.5f, y + 1.f}, {x + 2.f, y + 2.5f}, {x + 3.5f, y + 1.f}};
			dl.AddPolyline(p, 3, c, 0.8f);
		}

		// ── La bascule de mode (icônes 11×11, JSX DesignCanvasV2) ──
		// Design : un « A » — « M2 8.5 L5.5 2 L9 8.5 » + barre (3.2,6.5→7.8,6.5)
		inline void ModeDesign(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddLine({x + 2.f, y + 8.5f}, {x + 5.5f, y + 2.f}, c, 1.3f);
			dl.AddLine({x + 5.5f, y + 2.f}, {x + 9.f, y + 8.5f}, c, 1.3f);
			dl.AddLine({x + 3.2f, y + 6.5f}, {x + 7.8f, y + 6.5f}, c, 1.3f);
		}
		// Behavior : 3 cercles r1.5 + 2 courbes (approchées par 2 segments)
		inline void ModeBehavior(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddCircle({x + 2.5f, y + 5.5f}, 1.5f, c, 1.2f);
			dl.AddCircle({x + 8.5f, y + 2.5f}, 1.5f, c, 1.2f);
			dl.AddCircle({x + 8.5f, y + 8.5f}, 1.5f, c, 1.2f);
			const NkVec2 h1[3] = {{x + 4.f, y + 5.5f}, {x + 5.8f, y + 3.f}, {x + 7.f, y + 2.5f}};
			dl.AddPolyline(h1, 3, c, 1.f);
			const NkVec2 h2[3] = {{x + 4.f, y + 5.5f}, {x + 5.8f, y + 8.f}, {x + 7.f, y + 8.5f}};
			dl.AddPolyline(h2, 3, c, 1.f);
		}
		// Animation : horloge — cercle r4 + aiguilles (5.5,2.5→5.5,5.5→7.5,7)
		inline void ModeAnimation(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddCircle({x + 5.5f, y + 5.5f}, 4.f, c, 1.2f);
			dl.AddLine({x + 5.5f, y + 2.5f}, {x + 5.5f, y + 5.5f}, c, 1.2f);
			dl.AddLine({x + 5.5f, y + 5.5f}, {x + 7.5f, y + 7.f}, c, 1.2f);
		}
		// Split : 2 rects 4×9 rx0.5
		inline void ModeSplit(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddRect({x + 1.f, y + 1.f, 4.f, 9.f}, c, 1.2f, 0.5f);
			dl.AddRect({x + 6.f, y + 1.f, 4.f, 9.f}, c, 1.2f, 0.5f);
		}

		// ── Le cluster de zoom (JSX DesignCanvasV2) ──
		// chevron déroulant 7×5 : « M1 1 L3.5 3.5 L6 1 »
		inline void ChevronCombo7(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			const NkVec2 p[3] = {{x + 1.f, y + 1.f}, {x + 3.5f, y + 3.5f}, {x + 6.f, y + 1.f}};
			dl.AddPolyline(p, 3, c, 1.2f);
		}
		// grille 12×12 : « M1 4 H11 M1 8 H11 M4 1 V11 M8 1 V11 »
		inline void IcGrille(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddLine({x + 1.f, y + 4.f}, {x + 11.f, y + 4.f}, c, 1.1f);
			dl.AddLine({x + 1.f, y + 8.f}, {x + 11.f, y + 8.f}, c, 1.1f);
			dl.AddLine({x + 4.f, y + 1.f}, {x + 4.f, y + 11.f}, c, 1.1f);
			dl.AddLine({x + 8.f, y + 1.f}, {x + 8.f, y + 11.f}, c, 1.1f);
		}
		// aimant 12×12 : U (approché par polyline sur la courbe du JSX) + 3 pôles
		inline void IcAimant(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			const NkVec2 u[7] = {{x + 2.f, y + 2.5f},  {x + 2.2f, y + 5.6f}, {x + 3.4f, y + 7.4f},
								 {x + 6.f, y + 8.f},   {x + 8.6f, y + 7.4f}, {x + 9.8f, y + 5.6f},
								 {x + 10.f, y + 2.5f}};
			dl.AddPolyline(u, 7, c, 1.2f);
			dl.AddLine({x + 2.f, y + 1.5f}, {x + 2.f, y + 4.f}, c, 1.4f);
			dl.AddLine({x + 10.f, y + 1.5f}, {x + 10.f, y + 4.f}, c, 1.4f);
			dl.AddLine({x + 6.f, y + 8.f}, {x + 6.f, y + 11.f}, c, 1.4f);
		}

		// ── Le rail latéral droit (icônes 14×14, JSX SideRail) ──
		// bibliothèque : 4 carreaux 5×5 rx1
		inline void IcCarreaux(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddRect({x + 1.f, y + 1.f, 5.f, 5.f}, c, 1.2f, 1.f);
			dl.AddRect({x + 8.f, y + 1.f, 5.f, 5.f}, c, 1.2f, 1.f);
			dl.AddRect({x + 1.f, y + 8.f, 5.f, 5.f}, c, 1.2f, 1.f);
			dl.AddRect({x + 8.f, y + 8.f, 5.f, 5.f}, c, 1.2f, 1.f);
		}
		// étoile IA 5 branches (contour) : les 10 sommets du JSX
		inline void IcEtoile(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			const NkVec2 p[10] = {{x + 7.f, y + 2.f},	{x + 8.5f, y + 5.5f}, {x + 12.5f, y + 6.f},
								  {x + 9.5f, y + 9.f},	{x + 10.5f, y + 13.f}, {x + 7.f, y + 11.f},
								  {x + 3.5f, y + 13.f}, {x + 4.5f, y + 9.f},	{x + 1.5f, y + 6.f},
								  {x + 5.5f, y + 5.5f}};
			dl.AddPolyline(p, 10, c, 1.2f, true);
		}
		// œil-vague : cercle r4 + vague interne
		inline void IcOeilVague(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddCircle({x + 7.f, y + 7.f}, 4.f, c, 1.2f);
			const NkVec2 v[5] = {{x + 5.f, y + 7.f}, {x + 6.f, y + 6.f}, {x + 7.f, y + 7.f},
								 {x + 8.f, y + 8.f}, {x + 9.f, y + 7.f}};
			dl.AddPolyline(v, 5, c, 1.1f);
		}

		// ── Le rail bas (icônes 12×12, JSX BottomRail) ──
		// console : chevron « 2,3 5,6 2,9 » + trait (6,9→10,9)
		inline void IcConsole(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			const NkVec2 p[3] = {{x + 2.f, y + 3.f}, {x + 5.f, y + 6.f}, {x + 2.f, y + 9.f}};
			dl.AddPolyline(p, 3, c, 1.2f);
			dl.AddLine({x + 6.f, y + 9.f}, {x + 10.f, y + 9.f}, c, 1.2f);
		}
		// œil : amande (2 arcs approchés) + pupille r2
		inline void IcOeil(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			const NkVec2 h[5] = {{x + 1.f, y + 6.f}, {x + 3.5f, y + 3.4f}, {x + 6.f, y + 2.7f},
								 {x + 8.5f, y + 3.4f}, {x + 11.f, y + 6.f}};
			dl.AddPolyline(h, 5, c, 1.2f);
			const NkVec2 b[5] = {{x + 1.f, y + 6.f}, {x + 3.5f, y + 8.6f}, {x + 6.f, y + 9.3f},
								 {x + 8.5f, y + 8.6f}, {x + 11.f, y + 6.f}};
			dl.AddPolyline(b, 5, c, 1.2f);
			dl.AddCircle({x + 6.f, y + 6.f}, 2.f, c, 1.2f);
		}

		// ── L'inspecteur V2 (petites icônes de champ) ──
		// flèche « expand » 8×8 : « M1 4 H7 M5 2 L7 4 L5 6 » (accent)
		inline void IcExpand(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddLine({x + 1.f, y + 4.f}, {x + 7.f, y + 4.f}, c, 1.1f);
			dl.AddLine({x + 5.f, y + 2.f}, {x + 7.f, y + 4.f}, c, 1.1f);
			dl.AddLine({x + 7.f, y + 4.f}, {x + 5.f, y + 6.f}, c, 1.1f);
		}
		// « fixed » 8×8 : ligne H + 2 taquets verticaux
		inline void IcFixed(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			dl.AddLine({x + 1.f, y + 4.f}, {x + 7.f, y + 4.f}, c, 1.1f);
			dl.AddLine({x + 4.f, y + 1.f}, {x + 4.f, y + 3.f}, c, 1.1f);
			dl.AddLine({x + 4.f, y + 5.f}, {x + 4.f, y + 7.f}, c, 1.1f);
		}
		// chevron replié 8×8 : « M2 2 L6 4 L2 6 »
		inline void ChevronReplie8(NkGuiDrawList &dl, float32 x, float32 y, const NkColor &c) {
			const NkVec2 p[3] = {{x + 2.f, y + 2.f}, {x + 6.f, y + 4.f}, {x + 2.f, y + 6.f}};
			dl.AddPolyline(p, 3, c, 1.2f);
		}
		// alignements 12×12 — 0..2 : gauche/centreH/droite, 3..5 : haut/milieu/bas
		inline void IcAlign(NkGuiDrawList &dl, float32 x, float32 y, uint32 quel, const NkColor &c) {
			switch (quel) {
				case 0: // « M2 2 L2 10 M4 6 L2 6 M8 4 L8 8 »
					dl.AddLine({x + 2.f, y + 2.f}, {x + 2.f, y + 10.f}, c, 1.2f);
					dl.AddLine({x + 2.f, y + 6.f}, {x + 4.f, y + 6.f}, c, 1.2f);
					dl.AddLine({x + 8.f, y + 4.f}, {x + 8.f, y + 8.f}, c, 1.2f);
					break;
				case 1: // « M6 2 L6 10 M3 6 L9 6 »
					dl.AddLine({x + 6.f, y + 2.f}, {x + 6.f, y + 10.f}, c, 1.2f);
					dl.AddLine({x + 3.f, y + 6.f}, {x + 9.f, y + 6.f}, c, 1.2f);
					break;
				case 2: // « M10 2 L10 10 M8 6 L10 6 M4 4 L4 8 »
					dl.AddLine({x + 10.f, y + 2.f}, {x + 10.f, y + 10.f}, c, 1.2f);
					dl.AddLine({x + 8.f, y + 6.f}, {x + 10.f, y + 6.f}, c, 1.2f);
					dl.AddLine({x + 4.f, y + 4.f}, {x + 4.f, y + 8.f}, c, 1.2f);
					break;
				case 3: // « M2 2 L10 2 M6 4 L6 2 M4 8 L8 8 »
					dl.AddLine({x + 2.f, y + 2.f}, {x + 10.f, y + 2.f}, c, 1.2f);
					dl.AddLine({x + 6.f, y + 2.f}, {x + 6.f, y + 4.f}, c, 1.2f);
					dl.AddLine({x + 4.f, y + 8.f}, {x + 8.f, y + 8.f}, c, 1.2f);
					break;
				case 4: // « M2 6 L10 6 M6 3 L6 9 »
					dl.AddLine({x + 2.f, y + 6.f}, {x + 10.f, y + 6.f}, c, 1.2f);
					dl.AddLine({x + 6.f, y + 3.f}, {x + 6.f, y + 9.f}, c, 1.2f);
					break;
				default: // « M2 10 L10 10 M6 8 L6 10 M4 4 L8 4 »
					dl.AddLine({x + 2.f, y + 10.f}, {x + 10.f, y + 10.f}, c, 1.2f);
					dl.AddLine({x + 6.f, y + 8.f}, {x + 6.f, y + 10.f}, c, 1.2f);
					dl.AddLine({x + 4.f, y + 4.f}, {x + 8.f, y + 4.f}, c, 1.2f);
					break;
			}
		}

		// ── Le logo TopHeader (56×56) : dégradé 135° + 4 carreaux + diagonale ──
		// Le dégradé 3 arrêts (#1a5fb4 0 % → #2f81f7 60 % → #a371f7 100 %) est
		// approché par un rect bilinéaire (coins exacts, milieu interpolé) —
		// écart nommé dans le rapport.
		inline void LogoBanani(NkGuiDrawList &dl, const NkRect &r) {
			const NkColor tl = {26, 95, 180, 255};	// #1a5fb4
			const NkColor br = {163, 113, 247, 255}; // #a371f7
			const NkColor mi = {47, 129, 247, 255};	// #2f81f7 (l'arrêt à 60 %)
			dl.AddRectFilledMultiColor(r, tl, mi, br, mi);
			// carreaux 11×11 rx1.5 sur grille 30×30 centrée
			const float32 ox = r.x + (r.w - 30.f) * 0.5f, oy = r.y + (r.h - 30.f) * 0.5f;
			struct Q {
					float32 x, y, a;
			} q[4] = {{2.f, 2.f, 0.95f}, {17.f, 2.f, 0.5f}, {2.f, 17.f, 0.5f}, {17.f, 17.f, 0.2f}};
			for (int32 i = 0; i < 4; ++i) {
				NkColor b = {255, 255, 255, (nkentseu::uint8)(255.f * q[i].a)};
				dl.AddRectFilled({ox + q[i].x, oy + q[i].y, 11.f, 11.f}, b, 1.5f);
			}
			dl.AddLine({ox + 10.f, oy + 10.f}, {ox + 20.f, oy + 20.f},
					   {255, 255, 255, 77}, 1.2f);
		}

		// ── Pilule de badge (rôle « Button », compteurs) ──
		/// Rend la largeur occupée. `fond` est déjà à 13 % si c'est le badge de
		/// rôle (la maquette écrit #2f81f722 / bord #2f81f740).
		inline float32 BadgePilule(NkGuiDrawList &dl, const NkGuiFont &f9, float32 x, float32 y,
								   float32 h, const char *txt, const NkColor &teinte) {
			const float32 tw = Largeur(f9, txt);
			const float32 w = tw + 8.f;
			NkColor fond = teinte;
			fond.a = 34; // ≈ 22 hex
			NkColor bord = teinte;
			bord.a = 64; // ≈ 40 hex
			dl.AddRectFilled({x, y, w, h}, fond, h * 0.5f);
			dl.AddRect({x, y, w, h}, bord, 1.f, h * 0.5f);
			TexteGras(dl, f9, x + 4.f, CentrerY(f9, y, h), txt, teinte, 0.4f);
			return w;
		}

	} // namespace costume
} // namespace nkuidesign
