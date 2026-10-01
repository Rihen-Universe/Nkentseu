//
// NkEditeurDetails.cpp
// =============================================================================
// Description :
//   La colonne de droite, en deux onglets :
//     Details  l'INSPECTEUR de l'entite selectionnee, a la maniere d'Unity : son
//              nom, puis une CARTE par composant, puis « Ajouter un composant ».
//     Monde    les proprietes de la SCENE (gravite, temperature, affichage
//              de la matiere)
//
// Caracteristiques :
//   - Une carte (2026-09-30) : un EN-TETE -- repli, icone, nom, case « actif »
//     quand le composant en a une (sprite, matiere, animation, animateur), menu
//     « ⋮ » (Reinitialiser, Retirer, Monter / Descendre, Copier / Coller les
//     valeurs) -- puis des rangees a DEUX COLONNES, libelle a gauche, valeur a
//     droite. Le libelle d'un nombre se TIRE a la souris pour le changer
//     (Maj x10, Ctrl /10), comme dans Unity.
//   - Chaque carte sait CE QUE le solveur doit recevoir quand on la modifie
//     (ActualiserCorps, Teleporter via Deplacer...) : c'est le contenu de
//     l'ancien inspecteur, repris rangee par rangee.
//   - « Ajouter un composant », en bas, ouvre le menu des composants AVEC SA
//     RECHERCHE (on tape, il filtre : NkEditeurDessinerMenuOuvert).
//   - Les cartes Lumiere 2D et Emetteur (2026-09-30) reprennent, rangee par
//     rangee, les sections de NkEditeurLumiere.cpp (la case de l'en-tete
//     allume / active ; les preregrages de l'emetteur sont des boutons).
//   - La carte Hierarchie : parent, enfants, prefab et surcharges, Detacher et
//     Creer un prefab (les actions de la branche hierarchie). L'Animateur
//     (NKAnima) est une carte comme les autres, sans Retirer ni Reinitialiser :
//     son modele est un fichier, pas des valeurs.
//
// ⚠️ ET CE N'EST PAS `NkEditorInspector`, DELIBEREMENT
//   Celui du kit est pilote par NKReflection et n'affiche que des classes
//   REFLECHIES ; les composants d'Unkeny sont des structures nues.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurInterface.h"
#include "Editeur/NkEditeurLumiere.h"
#include "Editeur/NkEditeurLumiere.h"
#include "Editeur/NkEditeurReferences.h"
#include "Editeur/NkEditeurPlacer.h"
#include "Editeur/NkEditeurActions.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/NkEditorTextField.h"
#include "NKEditorKit/NkThemeToGui.h"
#include "NKGui/Widgets/NkGuiWidgets.h"

#include <cmath>
#include <cstdio>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkGuiContext;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {

			// (2026-10-01) La colonne des LIBELLES est `ui.detailsColonne` (0,40 par
			// defaut) : sa cloison se TIRE, comme celle d'Unreal.
			constexpr float32 RANG_H = 24.f;   ///< une rangee : le RYTHME d'Unreal, constant
			constexpr float32 ENTETE_H = 26.f; ///< l'en-tete d'une carte

			// ── LA RECHERCHE ET LA CARTE EN COURS (etape 2, document 02 §5) ──
			// Rangee tait une rangee qui ne repond pas a la recherche, et note que la
			// carte en cours en a montre une (`detailsCartesTrouvees`).
			const char *gRecherche = nullptr;
			int32 gCarte = -1;
			bool gCarteRepond = false; ///< le NOM de la carte repond : toutes ses rangees restent
			/// La derniere rangee dessinee (la cloison des noms se tire sur elle).
			float32 gRangeeX = 0.f, gRangeeW = 0.f;

			/// Une rangee de `libelle` se montre-t-elle sous la recherche ?
			bool Repond(const char *libelle) {
				if (gRecherche == nullptr || gRecherche[0] == '\0' || gCarteRepond) {
					return true;
				}
				return libelle != nullptr && libelle[0] != '\0' && NkEditeurContientSansCasse(libelle, gRecherche);
			}

			/// Un titre en GRAS (la police n'a qu'une graisse : le meme texte, decale
			/// d'un demi-pixel, epaissit le trait sans le flouter).
			void TexteGras(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, float32 x, float32 y, const char *t, const NkColor &c) {
				renderer::NkTexte(dl, f, x, y, t, c);
				renderer::NkTexte(dl, f, x + 0.6f, y, t, c);
			}

			/// La CATEGORIE d'une carte (les pastilles d'Unreal) : 1 General,
			/// 2 Acteur, 3 Physique, 4 Rendu, 5 Animation, 6 Audio.
			int32 CategorieDe(NkCarteEditeur c) {
				switch (c) {
					case NkCarteEditeur::NK_TRANSFORM:
						return 1;
					case NkCarteEditeur::NK_HIERARCHIE:
					case NkCarteEditeur::NK_ANCRAGE:
						return 2;
					case NkCarteEditeur::NK_COLLISIONNEUR:
					case NkCarteEditeur::NK_CORPS:
					case NkCarteEditeur::NK_CORPS_MOU:
						return 3;
					case NkCarteEditeur::NK_SPRITE:
					case NkCarteEditeur::NK_LUMIERE:
					case NkCarteEditeur::NK_EMETTEUR:
					case NkCarteEditeur::NK_FORME:
						return 4;
					case NkCarteEditeur::NK_ANIMATION:
					case NkCarteEditeur::NK_ANIMATEUR:
						return 5;
					case NkCarteEditeur::NK_SOURCE:
						return 6;
					default:
						return 0;
				}
			}
			const char *const kCategories[7] = {"Tout", "Général", "Acteur", "Physique", "Rendu", "Animation", "Audio"};

			/// Le libelle d'une entite : son etiquette, ou son indice a defaut.
			NkString NomDe(NkScene &scene, ecs::NkEntityId id) {
				const NkEtiquette *e = scene.Monde().Get<NkEtiquette>(id);
				if (e != nullptr && e->nom[0] != '\0') {
					return NkString(e->nom);
				}
				return NkString::Format("Entite %u", static_cast<uint32>(id.index));
			}

			NkColor Melange(const NkColor &a, const NkColor &b, float32 t) {
				auto m = [t](uint8 x, uint8 y) {
					return static_cast<uint8>(static_cast<float32>(x) + (static_cast<float32>(y) - static_cast<float32>(x)) * t);
				};
				return NkColor{m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
			}

			/// Ce qu'une carte a besoin de savoir pour se peindre et ecouter.
			struct NkInspecteur {
					NkEditeurCadre &c;
					ecs::NkEntityId id;
					NkRect zone; ///< la zone DEFILABLE : hors d'elle, rien n'est survole
					NkColor fondEntete;
					NkColor fondEnteteSurvol;
					NkColor fondCorps;
					NkColor bord;
			};

			bool Survol(const NkInspecteur &I, const NkRect &r) {
				const nkgui::NkVec2 p = I.c.ctx.input.mousePos;
				return NkEditeurDans(r, p) && NkEditeurDans(I.zone, p);
			}

			bool Clic(const NkInspecteur &I, const NkRect &r) {
				return Survol(I, r) && I.c.ctx.input.mouseClicked[0];
			}

			void Espace(NkGuiContext &ctx, float32 h) {
				(void)ctx.NextItemRect(0.f, h);
			}

			// =================================================================
			// LES ICONES DES CARTES (tracees : la police n'a pas ces glyphes)
			// =================================================================
			void IconeCarte(nkgui::NkGuiDrawList &dl, NkCarteEditeur carte, const NkRect &r, const NkColor &col) {
				const float32 cx = r.x + r.w * 0.5f;
				const float32 cy = r.y + r.h * 0.5f;
				auto P = [](float32 x, float32 y) { return NkVec2{x, y}; };
				switch (carte) {
					case NkCarteEditeur::NK_TRANSFORM:
						dl.AddLine(P(cx - 5.f, cy + 5.f), P(cx + 6.f, cy + 5.f), NkColor{215, 85, 85, 255}, 1.6f);
						dl.AddLine(P(cx - 5.f, cy + 5.f), P(cx - 5.f, cy - 6.f), NkColor{95, 195, 105, 255}, 1.6f);
						dl.AddCircleFilled(P(cx - 5.f, cy + 5.f), 1.8f, col);
						break;
					case NkCarteEditeur::NK_SPRITE:
						dl.AddRect(NkRect{cx - 6.f, cy - 5.f, 12.f, 10.f}, col, 1.2f);
						dl.AddTriangleFilled(P(cx - 5.f, cy + 4.f), P(cx - 1.f, cy - 1.f), P(cx + 2.f, cy + 4.f), col);
						dl.AddCircleFilled(P(cx + 3.f, cy - 2.f), 1.5f, col);
						break;
					case NkCarteEditeur::NK_COLLISIONNEUR:
						for (int32 k = 0; k < 4; ++k) {
							const float32 t = -6.f + 4.f * static_cast<float32>(k);
							dl.AddLine(P(cx + t, cy - 6.f), P(cx + t + 2.f, cy - 6.f), col, 1.2f);
							dl.AddLine(P(cx + t, cy + 6.f), P(cx + t + 2.f, cy + 6.f), col, 1.2f);
							dl.AddLine(P(cx - 6.f, cy + t), P(cx - 6.f, cy + t + 2.f), col, 1.2f);
							dl.AddLine(P(cx + 6.f, cy + t), P(cx + 6.f, cy + t + 2.f), col, 1.2f);
						}
						break;
					case NkCarteEditeur::NK_CORPS:
						dl.AddCircleFilled(P(cx, cy - 1.5f), 4.2f, col);
						dl.AddLine(P(cx, cy + 3.f), P(cx, cy + 7.f), col, 1.2f);
						dl.AddTriangleFilled(P(cx - 2.2f, cy + 5.f), P(cx + 2.2f, cy + 5.f), P(cx, cy + 7.5f), col);
						break;
					case NkCarteEditeur::NK_CORPS_MOU:
						dl.AddCircleFilled(P(cx - 1.f, cy + 1.f), 4.2f, col);
						dl.AddCircleFilled(P(cx + 2.6f, cy - 2.f), 2.6f, col);
						break;
					case NkCarteEditeur::NK_SOURCE:
						dl.AddRectFilled(NkRect{cx - 6.f, cy - 2.f, 3.f, 4.f}, col);
						dl.AddTriangleFilled(P(cx - 3.f, cy - 2.f), P(cx + 1.f, cy - 6.f), P(cx + 1.f, cy + 6.f), col);
						dl.AddTriangleFilled(P(cx - 3.f, cy - 2.f), P(cx + 1.f, cy + 6.f), P(cx - 3.f, cy + 2.f), col);
						dl.AddLine(P(cx + 3.5f, cy - 3.f), P(cx + 3.5f, cy + 3.f), col, 1.2f);
						dl.AddLine(P(cx + 6.f, cy - 5.f), P(cx + 6.f, cy + 5.f), col, 1.2f);
						break;
					case NkCarteEditeur::NK_ANIMATION:
						dl.AddRect(NkRect{cx - 6.f, cy - 5.f, 12.f, 10.f}, col, 1.2f);
						for (int32 k = 0; k < 3; ++k) {
							const float32 x = cx - 4.5f + 3.6f * static_cast<float32>(k);
							dl.AddRectFilled(NkRect{x, cy - 3.8f, 1.6f, 1.6f}, col);
							dl.AddRectFilled(NkRect{x, cy + 2.2f, 1.6f, 1.6f}, col);
						}
						break;
					case NkCarteEditeur::NK_ANIMATEUR:
						dl.AddCircleFilled(P(cx - 4.f, cy - 3.f), 2.6f, col);
						dl.AddCircleFilled(P(cx + 4.f, cy + 3.f), 2.6f, col);
						dl.AddLine(P(cx - 4.f, cy - 3.f), P(cx + 4.f, cy + 3.f), col, 1.2f);
						dl.AddLine(P(cx - 4.f, cy - 3.f), P(cx + 4.f, cy - 3.f), col, 1.f);
						break;
					case NkCarteEditeur::NK_LUMIERE:
						// Un soleil, comme l'icone du viseur.
						for (int32 k = 0; k < 8; ++k) {
							const float32 a = 0.785398f * static_cast<float32>(k);
							dl.AddLine(P(cx + std::cos(a) * 4.f, cy + std::sin(a) * 4.f), P(cx + std::cos(a) * 7.f, cy + std::sin(a) * 7.f), col, 1.2f);
						}
						dl.AddCircleFilled(P(cx, cy), 3.f, col);
						break;
					case NkCarteEditeur::NK_EMETTEUR: {
						// Une flamme : un losange pointe en haut.
						const NkVec2 pts[4] = {P(cx, cy - 7.f), P(cx + 4.5f, cy + 1.f), P(cx, cy + 6.f), P(cx - 4.5f, cy + 1.f)};
						dl.AddTriangleFilled(pts[0], pts[1], pts[2], col);
						dl.AddTriangleFilled(pts[0], pts[2], pts[3], col);
						break;
					}
					case NkCarteEditeur::NK_FORME:
						// Un cercle et un triangle qui se chevauchent : « des formes ».
						dl.AddCircle(P(cx - 2.f, cy - 1.5f), 4.2f, col, 1.2f);
						dl.AddTriangleFilled(P(cx + 1.f, cy - 1.f), P(cx + 6.5f, cy + 6.f), P(cx - 4.5f, cy + 6.f), col);
						break;
					case NkCarteEditeur::NK_ANCRAGE:
						// Un coin d'ecran et le point qui s'y accroche.
						dl.AddRect(NkRect{cx - 6.f, cy - 5.f, 12.f, 10.f}, col, 1.2f);
						dl.AddLine(P(cx + 1.f, cy - 2.5f), P(cx + 3.5f, cy - 2.5f), col, 1.2f);
						dl.AddLine(P(cx + 3.5f, cy - 2.5f), P(cx + 3.5f, cy), col, 1.2f);
						dl.AddCircleFilled(P(cx + 0.5f, cy + 0.5f), 1.6f, col);
						break;
					case NkCarteEditeur::NK_HIERARCHIE:
						dl.AddRectFilled(NkRect{cx - 6.f, cy - 6.f, 5.f, 4.f}, col);
						dl.AddLine(P(cx - 3.5f, cy - 2.f), P(cx - 3.5f, cy + 4.f), col, 1.2f);
						dl.AddLine(P(cx - 3.5f, cy + 0.5f), P(cx + 1.f, cy + 0.5f), col, 1.2f);
						dl.AddLine(P(cx - 3.5f, cy + 4.f), P(cx + 1.f, cy + 4.f), col, 1.2f);
						dl.AddRectFilled(NkRect{cx + 1.f, cy - 1.f, 5.f, 3.f}, col);
						dl.AddRectFilled(NkRect{cx + 1.f, cy + 2.5f, 5.f, 3.f}, col);
						break;
					default:
						break;
				}
			}

			/// L'ACTEUR (l'en-tete et la racine de l'arbre) : le cube d'Unreal, en 2D
			/// un carre arrondi et son pivot.
			void IconeActeur(nkgui::NkGuiDrawList &dl, const NkRect &r, const NkColor &col, const NkColor &accent) {
				const NkRect b{r.x + 2.f, r.y + 2.f, r.w - 4.f, r.h - 4.f};
				dl.AddRect(b, col, 1.4f, 3.f);
				dl.AddRectFilled(NkRect{b.x + 1.f, b.y + 1.f, b.w - 2.f, (b.h - 2.f) * 0.45f}, Melange(col, accent, 0.6f), 2.f);
				dl.AddCircleFilled(NkVec2{b.x + b.w * 0.5f, b.y + b.h * 0.68f}, 1.8f, accent);
			}

			// =================================================================
			// L'EN-TETE ET LA FIN D'UNE CARTE
			// =================================================================
			/// Repli, icone, case « actif » (si `actif`), nom, menu « ⋮ ». Rend true
			/// si le CORPS est a peindre (la carte est depliee).
			bool Entete(NkInspecteur &I, NkCarteEditeur carte, bool *actif) {
				NkGuiContext &ctx = I.c.ctx;
				NkEditeurInterface &ui = I.c.ui;
				auto &dl = ctx.DL();
				Espace(ctx, 6.f);
				const NkRect r0 = ctx.NextItemRect(0.f, ENTETE_H);
				const NkRect r{r0.x + 4.f, r0.y, r0.w - 8.f, r0.h};
				const uint32 bit = 1u << static_cast<uint32>(carte);
				const bool ouverte = (ui.cartesRepliees & bit) == 0u;
				const float32 cy = r.y + r.h * 0.5f;
				const NkRect rMenu{r.x + r.w - 22.f, r.y + 3.f, 18.f, r.h - 6.f};
				const NkRect rCase{r.x + 44.f, cy - 7.f, 14.f, 14.f};
				const bool surMenu = Survol(I, rMenu);
				const bool surCase = actif != nullptr && Survol(I, rCase);
				const bool survol = Survol(I, r);

				dl.AddRectFilled(r, survol && !surMenu && !surCase ? I.fondEnteteSurvol : I.fondEntete, 4.f);
				if (ouverte) {
					// Deplie, le bas de l'en-tete se RACCORDE au corps : coins carres.
					dl.AddRectFilled(NkRect{r.x, r.y + r.h - 5.f, r.w, 5.f}, survol && !surMenu && !surCase ? I.fondEnteteSurvol : I.fondEntete);
				}
				dl.AddRect(r, I.bord, 1.f, 4.f);

				const NkColor texte = (actif != nullptr && !*actif) ? I.c.pal.attenue : I.c.pal.texte;
				// Unreal : la categorie sur une BANDE plus claire que les rangees.
				dl.AddRectFilled(NkRect{r.x + 1.f, r.y + 1.f, r.w - 2.f, 1.f}, Melange(I.fondEntete, I.c.pal.texte, 0.10f));
				// Le repli : un triangle, vers le bas deplie, vers la droite replie.
				const float32 tx = r.x + 12.f;
				if (ouverte) {
					dl.AddTriangleFilled(NkVec2{tx - 4.f, cy - 2.f}, NkVec2{tx + 4.f, cy - 2.f}, NkVec2{tx, cy + 3.f}, I.c.pal.attenue);
				} else {
					dl.AddTriangleFilled(NkVec2{tx - 2.f, cy - 4.f}, NkVec2{tx - 2.f, cy + 4.f}, NkVec2{tx + 3.f, cy}, I.c.pal.attenue);
				}
				IconeCarte(dl, carte, NkRect{r.x + 22.f, cy - 8.f, 16.f, 16.f}, texte);
				float32 x = r.x + 44.f;
				if (actif != nullptr) {
					dl.AddRectFilled(rCase, I.c.pal.champ, 2.f);
					dl.AddRect(rCase, surCase ? I.c.pal.accent : I.bord, 1.f, 2.f);
					if (*actif) {
						dl.AddLine(NkVec2{rCase.x + 3.f, cy}, NkVec2{rCase.x + 6.f, cy + 3.5f}, I.c.pal.accent, 2.f);
						dl.AddLine(NkVec2{rCase.x + 6.f, cy + 3.5f}, NkVec2{rCase.x + 11.f, cy - 3.5f}, I.c.pal.accent, 2.f);
					}
					x += 20.f;
				}
				const float32 ty = r.y + (r.h - renderer::NkTexteHauteurLigne(I.c.police, 16.f)) * 0.5f;
				TexteGras(dl, I.c.police, x, ty, NkCarteEditeurNom(carte), texte);
				// Le menu « ⋮ » : trois points.
				if (surMenu || (ui.menu == NkMenuEditeur::NK_CARTE && ui.carteMenu == static_cast<int32>(carte))) {
					dl.AddRectFilled(rMenu, I.c.pal.boutonSurvol, 2.f);
				}
				for (int32 k = -1; k <= 1; ++k) {
					dl.AddCircleFilled(NkVec2{rMenu.x + rMenu.w * 0.5f, rMenu.y + rMenu.h * 0.5f + static_cast<float32>(k) * 4.f}, 1.4f, texte);
				}

				if (Clic(I, rMenu)) {
					// Le menu d'une AUTRE carte est remplace, pas referme.
					if (ui.menu == NkMenuEditeur::NK_CARTE && ui.carteMenu != static_cast<int32>(carte)) {
						ui.menu = NkMenuEditeur::NK_AUCUN;
					}
					ui.carteMenu = static_cast<int32>(carte);
					NkEditeurOuvrirMenu(I.c, NkMenuEditeur::NK_CARTE, rMenu);
				} else if (actif != nullptr && Clic(I, rCase)) {
					*actif = !*actif;
				} else if (Clic(I, r)) {
					ui.cartesRepliees ^= bit;
				}
				return ouverte;
			}

			/// Le bas d'une carte depliee.
			void Fin(NkInspecteur &I) {
				NkGuiContext &ctx = I.c.ctx;
				auto &dl = ctx.DL();
				const NkRect r0 = ctx.NextItemRect(0.f, 6.f);
				const NkRect r{r0.x + 4.f, r0.y, r0.w - 8.f, r0.h};
				dl.AddRectFilled(r, I.fondCorps);
				dl.AddRectFilled(NkRect{r.x, r.y, 1.f, r.h}, I.bord);
				dl.AddRectFilled(NkRect{r.x + r.w - 1.f, r.y, 1.f, r.h}, I.bord);
				dl.AddRectFilled(NkRect{r.x, r.y + r.h - 1.f, r.w, 1.f}, I.bord);
			}

			// =================================================================
			// LES RANGEES « LIBELLE | VALEUR »
			// =================================================================
			/// Reserve une rangee, peint son fond et son libelle ; rend le
			/// rectangle du CHAMP (a droite) et celui du LIBELLE (a gauche).
			/// (2026-10-01) Rend FAUX, sans rien reserver, quand la recherche tait la
			/// rangee : l'appelant s'arrete la.
			bool Rangee(NkInspecteur &I, const char *libelle, float32 h, NkRect &champ, NkRect &lib) {
				if (!Repond(libelle)) {
					champ = lib = NkRect{0.f, 0.f, 0.f, 0.f};
					return false;
				}
				if (gCarte >= 0) {
					I.c.ui.detailsCartesTrouvees |= 1u << static_cast<uint32>(gCarte);
				}
				NkGuiContext &ctx = I.c.ctx;
				auto &dl = ctx.DL();
				const NkRect r0 = ctx.NextItemRect(0.f, h);
				const NkRect r{r0.x + 4.f, r0.y, r0.w - 8.f, r0.h};
				dl.AddRectFilled(r, I.fondCorps);
				dl.AddRectFilled(NkRect{r.x, r.y, 1.f, r.h}, I.bord);
				dl.AddRectFilled(NkRect{r.x + r.w - 1.f, r.y, 1.f, r.h}, I.bord);
				// La colonne des noms ALIGNEE sur toutes les cartes, et sa CLOISON
				// fine (Unreal) : elle se tire (OngletDetails).
				const float32 colonne = r.w * I.c.ui.detailsColonne;
				I.c.ui.detailsCloisonX = r.x + colonne - 4.f;
				gRangeeX = r.x;
				gRangeeW = r.w;
				dl.AddRectFilled(NkRect{r.x + colonne - 4.f, r.y, 1.f, r.h}, Melange(I.bord, I.fondCorps, 0.35f));
				lib = NkRect{r.x + 12.f, r.y, colonne - 20.f, r.h};
				// La colonne des FLECHES DE REMISE est reservee sur TOUTES les rangees
				// (Unreal) : les champs de toutes les cartes finissent au meme x.
				champ = NkRect{r.x + colonne, r.y + 2.f, r.w - colonne - 8.f - 24.f, r.h - 4.f};
				if (libelle != nullptr && libelle[0] != '\0') {
					NkEditeurInterface::NkRangeeDetails rd;
					rd.carte = gCarte;
					rd.libelle = NkString(libelle);
					rd.champ = champ;
					I.c.ui.detailsRangees.PushBack(rd);
				}
				if (libelle != nullptr && libelle[0] != '\0') {
					const float32 ty = r.y + (r.h - renderer::NkTexteHauteurLigne(I.c.police, 16.f)) * 0.5f;
					dl.PushClipRect(lib, true);
					renderer::NkTexte(dl, I.c.police, lib.x, ty, libelle, I.c.pal.texte);
					dl.PopClipRect();
				}
				return true;
			}

			/// Le LIBELLE d'un nombre se TIRE (Unity) : a droite il augmente, a
			/// gauche il diminue, `pas` par pixel ; Maj x10, Ctrl /10.
			bool Frotter(NkInspecteur &I, const NkRect &lib, float32 &v, float32 pas, float32 vmin, float32 vmax) {
				NkGuiContext &ctx = I.c.ctx;
				NkEditeurInterface &ui = I.c.ui;
				const nkgui::NkGuiInput &in = ctx.input;
				const uint32 id = static_cast<uint32>(ctx.GetId("frotte"));
				const bool survol = Survol(I, lib);
				if (survol || ui.frotteId == id) {
					ctx.wantCursor = nkgui::NkGuiCursor::ResizeEW;
				}
				if (ui.frotteId == 0u && survol && in.mouseClicked[0]) {
					ui.frotteId = id;
					ui.frotteX = in.mousePos.x;
					return false;
				}
				if (ui.frotteId != id) {
					return false;
				}
				if (!in.mouseDown[0]) {
					ui.frotteId = 0u;
					return false;
				}
				const float32 dx = in.mousePos.x - ui.frotteX;
				ui.frotteX = in.mousePos.x;
				if (dx == 0.f) {
					return false;
				}
				const float32 k = in.shiftDown ? 10.f : (in.ctrlDown ? 0.1f : 1.f);
				v += dx * pas * k;
				v = v < vmin ? vmin : (v > vmax ? vmax : v);
				return true;
			}

			bool Nombre(NkInspecteur &I, const char *libelle, float32 &v, float32 pas, float32 vmin = -1.0e9f, float32 vmax = 1.0e9f) {
				NkGuiContext &ctx = I.c.ctx;
				NkRect champ, lib;
				if (!Rangee(I, libelle, RANG_H, champ, lib)) {
					return false;
				}
				ctx.PushId(libelle);
				bool change = Frotter(I, lib, v, pas, vmin, vmax);
				ctx.SetNextItemRect(champ);
				change |= nkgui::DragFloat(ctx, "##v", v, pas, vmin, vmax);
				ctx.PopId();
				return change;
			}

			bool Glissiere(NkInspecteur &I, const char *libelle, float32 &v, float32 vmin, float32 vmax) {
				NkGuiContext &ctx = I.c.ctx;
				NkRect champ, lib;
				if (!Rangee(I, libelle, RANG_H, champ, lib)) {
					return false;
				}
				ctx.PushId(libelle);
				bool change = Frotter(I, lib, v, (vmax - vmin) / 300.f, vmin, vmax);
				ctx.SetNextItemRect(champ);
				change |= nkgui::SliderFloat(ctx, "##v", v, vmin, vmax);
				ctx.PopId();
				return change;
			}

			bool Entier(NkInspecteur &I, const char *libelle, int32 &v, int32 vmin = -1000000, int32 vmax = 1000000) {
				NkGuiContext &ctx = I.c.ctx;
				NkRect champ, lib;
				if (!Rangee(I, libelle, RANG_H, champ, lib)) {
					return false;
				}
				ctx.PushId(libelle);
				float32 f = static_cast<float32>(v);
				bool change = false;
				if (Frotter(I, lib, f, 0.1f, static_cast<float32>(vmin), static_cast<float32>(vmax))) {
					const int32 n = static_cast<int32>(std::floor(f + 0.5f));
					change = n != v;
					v = n;
				}
				ctx.SetNextItemRect(champ);
				change |= nkgui::DragInt(ctx, "##v", v, 0.25f, vmin, vmax);
				ctx.PopId();
				return change;
			}

			bool Case(NkInspecteur &I, const char *libelle, bool &v) {
				NkGuiContext &ctx = I.c.ctx;
				NkRect champ, lib;
				if (!Rangee(I, libelle, RANG_H, champ, lib)) {
					return false;
				}
				ctx.PushId(libelle);
				ctx.SetNextItemRect(NkRect{champ.x, champ.y, champ.h, champ.h});
				const bool change = nkgui::Checkbox(ctx, "##v", v);
				ctx.PopId();
				return change;
			}

			/// Une couleur RGBA : NKGui la veut en quatre canaux 0..1.
			bool Teinte(NkInspecteur &I, const char *libelle, uint32 &rgba) {
				NkGuiContext &ctx = I.c.ctx;
				NkRect champ, lib;
				if (!Rangee(I, libelle, RANG_H, champ, lib)) {
					return false;
				}
				// (2026-10-01) Les conversions sont celles de math::NkColor / NkColorF
				// (NKMath/NkColor.h), pas des decalages faits main.
				const math::NkColorF cf = math::NkColor(rgba).ToColorF();
				float32 col[4] = {cf.r, cf.g, cf.b, cf.a};
				ctx.PushId(libelle);
				ctx.SetNextItemRect(champ);
				const bool change = nkgui::ColorEdit4(ctx, "##v", col);
				ctx.PopId();
				if (!change) {
					return false;
				}
				rgba = math::NkColor(math::NkColorF(col[0], col[1], col[2], col[3])).ToUint32A();
				return true;
			}

			/// Un AXE (X, Y, Z ; L et H d'une taille) se marque d'un LISERE de couleur
			/// au bord gauche de son champ, toute sa hauteur (Unreal, `ue58_a.png`) ;
			/// une autre etiquette (min / max, deb / fin) garde ses lettres.
			bool EstAxe(const char *lettre) {
				return lettre != nullptr && lettre[0] != '\0' && lettre[1] == '\0' &&
					   (lettre[0] == 'X' || lettre[0] == 'Y' || lettre[0] == 'Z' || lettre[0] == 'L' || lettre[0] == 'H');
			}

			/// Le LISERE d'axe : un rectangle de la couleur de l'axe, colle au bord
			/// gauche du champ, de toute sa hauteur ; il se TIRE (comme la lettre
			/// d'avant). Releve pour le banc (detailsLiseres). Rend sa largeur.
			float32 Lisere(NkInspecteur &I, const NkRect &cellule, const NkColor &col) {
				constexpr float32 L = 4.f;
				const NkRect r{cellule.x, cellule.y, L, cellule.h};
				I.c.ctx.DL().AddRectFilled(r, col, 2.f);
				NkEditeurInterface::NkLisereAxe la;
				la.lisere = r;
				la.champ = cellule;
				la.couleur = col.ToUint32A();
				I.c.ui.detailsLiseres.PushBack(la);
				return L;
			}

			/// Le bouton « remettre » au bout d'une rangee (la fleche d'Unreal) ; il
			/// prend sa place a DROITE du champ, qui retrecit d'autant. (2026-10-01)
			/// Comme Unreal, il ne PARAIT que si la valeur s'ecarte de son defaut
			/// (`modifie`) ; sa place reste prise, les champs ne sautent pas.
			bool Remettre(NkInspecteur &I, NkRect &champ, bool modifie = true) {
				// Dans la colonne reservee par Rangee, a droite du champ.
				const NkRect r{champ.x + champ.w + 4.f, champ.y, 20.f, champ.h};
				if (!modifie) {
					return false;
				}
				++I.c.ui.detailsRemises;
				const bool clic = NkEditeurBouton(I.c, r, "", false, NkEditeurDans(I.zone, I.c.ctx.input.mousePos), &I.c.ctx.DL());
				// Une fleche qui revient : un arc et sa pointe.
				auto &dl = I.c.ctx.DL();
				const float32 cx = r.x + r.w * 0.5f;
				const float32 cy = r.y + r.h * 0.5f;
				NkVec2 arc[10];
				for (int32 k = 0; k < 10; ++k) {
					const float32 t = 0.6f + 4.6f * static_cast<float32>(k) / 9.f;
					arc[k] = NkVec2{cx + 5.f * std::cos(t), cy - 5.f * std::sin(t)};
				}
				dl.AddPolyline(arc, 10, I.c.pal.attenue, 1.3f, false);
				dl.AddTriangleFilled(NkVec2{arc[0].x - 3.5f, arc[0].y - 1.f}, NkVec2{arc[0].x + 1.5f, arc[0].y - 3.5f},
									 NkVec2{arc[0].x + 1.f, arc[0].y + 2.f}, I.c.pal.attenue);
				return clic;
			}

			/// Deux nombres sur une rangee (position, taille...). (2026-10-01, retour 5
			/// de Rihen) Un AXE n'ecrit plus sa lettre : le LISERE rouge X / vert Y
			/// d'Unreal, colle au bord gauche du champ, toute sa hauteur ; il se TIRE.
			/// `verrou` (facultatif) : le cadenas d'Unreal dans la colonne du nom.
			bool Paire(NkInspecteur &I, const char *libelle, float32 &a, float32 &b, float32 pas, float32 vmin = -1.0e9f,
					   float32 vmax = 1.0e9f, const char *la = "X", const char *lb = "Y", bool *remise = nullptr, bool modifie = true,
					   bool *verrou = nullptr) {
				NkGuiContext &ctx = I.c.ctx;
				auto &dl = ctx.DL();
				NkRect champ, lib;
				if (!Rangee(I, libelle, RANG_H, champ, lib)) {
					return false;
				}
				if (remise != nullptr) {
					*remise = Remettre(I, champ, modifie);
				}
				if (verrou != nullptr) {
					// Le CADENAS, au bout de la colonne du nom : ferme, les proportions
					// sont gardees (X entraine Y, et l'inverse).
					const NkRect rv{lib.x + lib.w - 14.f, lib.y + (lib.h - 14.f) * 0.5f, 14.f, 14.f};
					I.c.ui.detailsVerrou = rv;
					const bool sur = Survol(I, rv);
					const NkColor cv = *verrou ? I.c.pal.accent : (sur ? I.c.pal.texte : I.c.pal.attenue);
					dl.AddRectFilled(NkRect{rv.x + 2.f, rv.y + 6.f, 10.f, 7.f}, cv, 1.5f);
					NkVec2 anse[7];
					const float32 ox = *verrou ? 0.f : 3.f; // ouvert : l'anse s'ecarte
					for (int32 k = 0; k < 7; ++k) {
						const float32 t = 3.14159f * static_cast<float32>(k) / 6.f;
						anse[k] = NkVec2{rv.x + 7.f + ox - 3.2f * std::cos(t), rv.y + 6.f - 4.f * std::sin(t)};
					}
					dl.AddPolyline(anse, 7, cv, 1.4f, false);
					if (sur && ctx.input.mouseClicked[0]) {
						*verrou = !*verrou;
					}
				}
				ctx.PushId(libelle);
				bool change = false;
				const float32 moitie = (champ.w - 4.f) * 0.5f;
				for (int32 k = 0; k < 2; ++k) {
					float32 &v = k == 0 ? a : b;
					const NkRect cellule{champ.x + static_cast<float32>(k) * (moitie + 4.f), champ.y, moitie, champ.h};
					const NkColor col = k == 0 ? NkColor{220, 95, 95, 255} : NkColor{105, 200, 115, 255};
					const char *etiquette = k == 0 ? la : lb;
					ctx.PushId(k == 0 ? "a" : "b");
					if (EstAxe(etiquette)) {
						// Le champ PREND toute la cellule ; le lisere se pose PAR-DESSUS
						// son bord gauche (dessine apres lui).
						ctx.SetNextItemRect(cellule);
						change |= nkgui::DragFloat(ctx, "##v", v, pas, vmin, vmax);
						const float32 l = Lisere(I, cellule, col);
						change |= Frotter(I, NkRect{cellule.x, cellule.y, l + 2.f, cellule.h}, v, pas, vmin, vmax);
					} else {
						const NkRect lettre{cellule.x, cellule.y, 14.f, cellule.h};
						renderer::NkTexteDansBoite(dl, I.c.petite, lettre, etiquette, col);
						change |= Frotter(I, lettre, v, pas, vmin, vmax);
						ctx.SetNextItemRect(NkRect{cellule.x + 15.f, cellule.y, cellule.w - 15.f, cellule.h});
						change |= nkgui::DragFloat(ctx, "##v", v, pas, vmin, vmax);
					}
					ctx.PopId();
				}
				ctx.PopId();
				return change;
			}

			/// UNE composante avec sa lettre coloree -- la rotation d'un Transform :
			/// Z, bleue, comme l'axe qui sort de l'ecran.
			bool Unique(NkInspecteur &I, const char *libelle, float32 &v, float32 pas, float32 vmin, float32 vmax, const char *lettre,
						const NkColor &couleur, bool *remise, bool modifie = true) {
				NkGuiContext &ctx = I.c.ctx;
				NkRect champ, lib;
				if (!Rangee(I, libelle, RANG_H, champ, lib)) {
					return false;
				}
				if (remise != nullptr) {
					*remise = Remettre(I, champ, modifie);
				}
				ctx.PushId(libelle);
				bool change = false;
				if (EstAxe(lettre)) {
					// Le lisere bleu Z d'Unreal, comme ceux de Paire.
					ctx.SetNextItemRect(champ);
					change |= nkgui::DragFloat(ctx, "##v", v, pas, vmin, vmax);
					const float32 l = Lisere(I, champ, couleur);
					change |= Frotter(I, NkRect{champ.x, champ.y, l + 2.f, champ.h}, v, pas, vmin, vmax);
				} else {
					const NkRect rl{champ.x, champ.y, 14.f, champ.h};
					renderer::NkTexteDansBoite(ctx.DL(), I.c.petite, rl, lettre, couleur);
					change = Frotter(I, rl, v, pas, vmin, vmax);
					ctx.SetNextItemRect(NkRect{champ.x + 15.f, champ.y, champ.w - 15.f, champ.h});
					change |= nkgui::DragFloat(ctx, "##v", v, pas, vmin, vmax);
				}
				ctx.PopId();
				return change;
			}

			/// Un choix parmi quelques valeurs, SEGMENTE (UI_SPEC §3.1).
			bool Choix(NkInspecteur &I, const char *libelle, const char *const *noms, int32 n, int32 &valeur) {
				NkRect champ, lib;
				if (!Rangee(I, libelle, RANG_H, champ, lib)) {
					return false;
				}
				const float32 w = champ.w / static_cast<float32>(n);
				bool change = false;
				const bool actif = NkEditeurDans(I.zone, I.c.ctx.input.mousePos);
				for (int32 i = 0; i < n; ++i) {
					const NkRect r{champ.x + static_cast<float32>(i) * w, champ.y, w - 2.f, champ.h};
					if (NkEditeurBouton(I.c, r, "", i == valeur, actif, &I.c.ctx.DL()) && i != valeur) {
						valeur = i;
						change = true;
					}
					renderer::NkTexteDansBoite(I.c.ctx.DL(), I.c.petite, r, noms[i], i == valeur ? I.c.pal.surAccent : I.c.pal.texte);
				}
				return change;
			}

			/// Une valeur en LECTURE seule.
			void Info(NkInspecteur &I, const char *libelle, const char *valeur) {
				NkRect champ, lib;
				if (!Rangee(I, libelle, RANG_H, champ, lib)) {
					return;
				}
				auto &dl = I.c.ctx.DL();
				const float32 ty = champ.y + (champ.h - renderer::NkTexteHauteurLigne(I.c.police, 16.f)) * 0.5f;
				dl.PushClipRect(champ, true);
				renderer::NkTexte(dl, I.c.police, champ.x + 4.f, ty, valeur, I.c.pal.attenue);
				dl.PopClipRect();
			}

			/// Une ligne de texte sur toute la largeur de la carte (les etats d'un
			/// animateur, une note).
			void Ligne(NkInspecteur &I, const char *texte, bool vif = false) {
				NkRect champ, lib;
				if (!Rangee(I, "", RANG_H - 4.f, champ, lib)) {
					return;
				}
				auto &dl = I.c.ctx.DL();
				const float32 ty = lib.y + (lib.h - renderer::NkTexteHauteurLigne(I.c.petite, 12.f)) * 0.5f;
				renderer::NkTexte(dl, I.c.petite, lib.x, ty, texte, vif ? I.c.pal.texte : I.c.pal.attenue);
			}

			/// Un ou deux boutons sur la largeur de la carte ; rend celui clique
			/// (0, 1) ou -1.
			int32 Boutons(NkInspecteur &I, const char *a, const char *b = nullptr, bool actifA = true, bool actifB = true) {
				NkRect champ, lib;
				if (!Rangee(I, "", RANG_H + 4.f, champ, lib)) {
					return -1;
				}
				const NkRect ligne{lib.x - 4.f, champ.y, champ.x + champ.w - lib.x + 4.f, champ.h};
				const bool dedans = NkEditeurDans(I.zone, I.c.ctx.input.mousePos);
				const int32 n = b != nullptr ? 2 : 1;
				const float32 w = (ligne.w - static_cast<float32>(n - 1) * 6.f) / static_cast<float32>(n);
				int32 clique = -1;
				for (int32 k = 0; k < n; ++k) {
					const NkRect r{ligne.x + static_cast<float32>(k) * (w + 6.f), ligne.y, w, ligne.h};
					const bool actif = k == 0 ? actifA : actifB;
					if (NkEditeurBouton(I.c, r, "", false, dedans && actif, &I.c.ctx.DL())) {
						clique = k;
					}
					renderer::NkTexteDansBoite(I.c.ctx.DL(), I.c.petite, r, k == 0 ? a : b, actif ? I.c.pal.texte : I.c.pal.attenue);
				}
				return clique;
			}

			// =================================================================
			// LA REFERENCE D'ASSET (Unreal, document 02 §5)
			// =================================================================
			/// La TEXTURE d'un sprite comme le « Static Mesh » d'Unreal : la vignette,
			/// la liste deroulante des images du Contenu (filtree par type, avec sa
			/// recherche), « utiliser la selection du Content Browser » (fleche),
			/// « parcourir » (loupe : saute a l'asset), et la fleche qui remet
			/// « Aucune ». Une image GLISSEE du Content Browser s'y depose
			/// (NkEditeurTiroir.cpp lit `detailsTexture`). Tout est retenu (Ctrl+Z).
			/// (2026-10-01, retour 4 de Rihen : « comment mettre une texture sur un
			/// sprite ? aujourd'hui rien ne le permet ».)
			void ReferenceTexture(NkInspecteur &I) {
				NkEditeurCadre &c = I.c;
				NkEditeurInterface &ui = c.ui;
				const NkSprite2D *s = c.m.scene.Monde().Get<NkSprite2D>(I.id);
				constexpr float32 H = 58.f;
				NkRect champ, lib;
				if (!Rangee(I, "Texture", H, champ, lib)) {
					return;
				}
				const NkRect rangee{lib.x - 12.f, lib.y, champ.x + champ.w + 8.f - (lib.x - 12.f), lib.h};
				ui.detailsTexture = rangee;
				const uint32 tex = s->texId;
				// La fleche de remise, a la hauteur d'une rangee simple, centree.
				bool remise = false;
				if (tex != 0u) {
					NkRect court{champ.x, champ.y + (champ.h - (RANG_H - 4.f)) * 0.5f, champ.w, RANG_H - 4.f};
					remise = Remettre(I, court);
					champ.w = court.w;
				}
				auto &dl = c.ctx.DL();
				const bool dedans = NkEditeurDans(I.zone, c.ctx.input.mousePos);
				// ── La vignette : l'image dans son rapport, sur le fond sombre ──
				const float32 cote = H - 10.f;
				const NkRect vig{champ.x, champ.y + (champ.h - cote) * 0.5f, cote, cote};
				dl.AddRectFilled(vig, c.pal.fond, 2.f);
				int32 tw = 0, th = 0;
				if (tex != 0u && c.m.textures.Taille(tex, tw, th) && tw > 0 && th > 0) {
					float32 iw = cote - 4.f, ih = cote - 4.f;
					if (tw > th) {
						ih = iw * static_cast<float32>(th) / static_cast<float32>(tw);
					} else {
						iw = ih * static_cast<float32>(tw) / static_cast<float32>(th);
					}
					const NkRect img{vig.x + (vig.w - iw) * 0.5f, vig.y + (vig.h - ih) * 0.5f, iw, ih};
					dl.AddImage(tex, img, NkVec2{s->uv0.x, s->uv0.y}, NkVec2{s->uv1.x, s->uv1.y}, NkColor{255, 255, 255, 255});
				} else {
					renderer::NkTexteDansBoite(dl, c.petite, vig, "Aucune", c.pal.attenue);
				}
				dl.AddRect(vig, c.pal.bord, 1.f, 2.f);
				// ── La liste deroulante : le nom de l'asset, et ▾ ──
				const NkString nav = NkEditeurNavDeTexture(c.m, tex);
				NkString nom("Aucune");
				if (tex != 0u) {
					nom = editorkit::NkDisqueNom(nav.Empty() ? c.m.textures.Nom(tex) : nav.CStr());
				}
				const float32 x0 = vig.x + vig.w + 6.f;
				const NkRect liste{x0, champ.y + 4.f, champ.x + champ.w - x0, 20.f};
				const bool ouverte = ui.menu == NkMenuEditeur::NK_TEXTURE_SPRITE;
				if (NkEditeurBouton(c, liste, "", ouverte, dedans, &dl)) {
					NkEditeurImagesDuContenu(c.m, ui.texturesProposees);
					NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_TEXTURE_SPRITE, liste);
				}
				const NkColor surListe = ouverte ? c.pal.surAccent : c.pal.texte;
				dl.PushClipRect(NkRect{liste.x + 4.f, liste.y, liste.w - 18.f, liste.h}, true);
				renderer::NkTexte(dl, c.petite, liste.x + 6.f, liste.y + (liste.h - renderer::NkTexteHauteurLigne(c.petite, 12.f)) * 0.5f,
								  nom.CStr(), surListe);
				dl.PopClipRect();
				const float32 fx = liste.x + liste.w - 9.f, fy = liste.y + liste.h * 0.5f;
				dl.AddTriangleFilled(NkVec2{fx - 4.f, fy - 2.f}, NkVec2{fx + 4.f, fy - 2.f}, NkVec2{fx, fy + 3.f}, surListe);
				// ── « Utiliser la selection du Content Browser » : la fleche ←
				NkString candidat;
				if (NkEditeurEstImage(ui.contenuActif.CStr())) {
					candidat = ui.contenuActif;
				}
				for (uint32 i = 0; i < ui.contenuChoisis.Size() && candidat.Empty(); ++i) {
					if (NkEditeurEstImage(ui.contenuChoisis[i].CStr())) {
						candidat = ui.contenuChoisis[i];
					}
				}
				const NkRect bSel{x0, liste.y + liste.h + 4.f, 24.f, 20.f};
				const NkRect bPar{bSel.x + bSel.w + 4.f, bSel.y, 24.f, 20.f};
				const bool clicSel = NkEditeurBouton(c, bSel, "", false, dedans && !candidat.Empty(), &dl);
				{
					const NkColor col = candidat.Empty() ? c.pal.attenue : c.pal.texte;
					const float32 cx = bSel.x + bSel.w * 0.5f, cy = bSel.y + bSel.h * 0.5f;
					dl.AddCircle(NkVec2{cx, cy}, 6.5f, col, 1.2f);
					dl.AddLine(NkVec2{cx - 3.5f, cy}, NkVec2{cx + 3.5f, cy}, col, 1.4f);
					dl.AddTriangleFilled(NkVec2{cx - 4.5f, cy}, NkVec2{cx - 1.f, cy - 3.f}, NkVec2{cx - 1.f, cy + 3.f}, col);
				}
				// ── « Parcourir » : la loupe ; saute a l'asset dans le Content Browser
				const bool clicPar = NkEditeurBouton(c, bPar, "", false, dedans && !nav.Empty(), &dl);
				{
					const NkColor col = nav.Empty() ? c.pal.attenue : c.pal.texte;
					const float32 cx = bPar.x + bPar.w * 0.5f - 1.5f, cy = bPar.y + bPar.h * 0.5f - 1.5f;
					dl.AddCircle(NkVec2{cx, cy}, 4.5f, col, 1.4f);
					dl.AddLine(NkVec2{cx + 3.2f, cy + 3.2f}, NkVec2{cx + 7.f, cy + 7.f}, col, 1.8f);
				}
				ui.detailsTextureListe = liste;
				ui.detailsTextureSelection = bSel;
				ui.detailsTextureParcourir = bPar;
				// ── Une image TRAINEE depuis le Content Browser : la cible s'eclaire
				if (!ui.contenuGlisse.Empty() && NkEditeurEstImage(ui.contenuGlisse.CStr())) {
					const bool dessus = NkEditeurDans(rangee, c.ctx.input.mousePos);
					if (dessus) {
						dl.AddRectFilled(rangee, NkColor{c.pal.selection.r, c.pal.selection.g, c.pal.selection.b, 40}, 2.f);
					}
					dl.AddRect(rangee, c.pal.selection, dessus ? 2.f : 1.f, 2.f);
				}
				if (clicSel) {
					NkEditeurTextureSprite(c.m, I.id, candidat.CStr());
				} else if (clicPar) {
					NkEditeurContenuMontrer(ui, nav);
				} else if (remise) {
					NkEditeurSansTexture(c.m, I.id);
				}
			}

			// =================================================================
			// LES CARTES
			// =================================================================
			/// Le Transform a la maniere d'Unreal : Position, Rotation, Echelle, une
			/// rangee chacune, chaque composante dans son champ derriere sa lettre
			/// coloree (X rouge, Y vert, Z bleu) qui se TIRE, et la fleche qui remet
			/// la rangee a zero (a 1 pour l'echelle).
			void CarteTransform(NkInspecteur &I) {
				NkEditeurModele &m = I.c.m;
				if (!m.scene.Monde().Has<NkTransform2D>(I.id) || !Entete(I, NkCarteEditeur::NK_TRANSFORM, nullptr)) {
					return;
				}
				// ⚠️ La position passe par l'action Deplacer : un rigide est
				// TELEPORTE (sinon le solveur le ramene), un corps mou TRANSLATE.
				NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(I.id);
				NkVec2f centre = t->position;
				NkEditeurCentreSelection(m, centre);
				float32 x = centre.x;
				float32 y = centre.y;
				bool remise = false;
				if (Paire(I, "Position (m)", x, y, 0.02f, -1.0e9f, 1.0e9f, "X", "Y", &remise, x != 0.f || y != 0.f)) {
					NkEditeurDeplacer(m, NkVec2f(x, y));
				}
				if (remise) {
					NkEditeurDeplacer(m, NkVec2f(0.f, 0.f));
				}
				t = m.scene.Monde().Get<NkTransform2D>(I.id);
				if (!m.scene.Monde().Has<NkCorpsMou2D>(I.id)) {
					float32 rot = t->rotation * 57.2957795f;
					remise = false;
					const bool tourne = Unique(I, "Rotation (°)", rot, 0.5f, -360.f, 360.f, "Z", NkColor{95, 150, 235, 255}, &remise, rot != 0.f);
					if (tourne || remise) {
						t->rotation = remise ? 0.f : rot / 57.2957795f;
						if (m.scene.Monde().Has<NkCorps2D>(I.id)) {
							m.scene.ActualiserCorps(I.id); // le corps prend la nouvelle orientation
						}
					}
				} else {
					Info(I, "Rotation (°)", "(par particule)");
				}
				// L'ECHELLE est cuite dans les composants (NkEchelleEditeur) : la
				// taper, c'est ce que fait le gizmo R. En DERNIER : elle peut ajouter
				// un composant a l'entite, ce qui deplace ses donnees.
				NkVec2f e = NkEditeurEchelle(m, I.id);
				const NkVec2f avant = e;
				remise = false;
				if (Paire(I, "Échelle", e.x, e.y, 0.01f, 0.05f, 100.f, "X", "Y", &remise, e.x != 1.f || e.y != 1.f, &I.c.ui.echelleVerrou)) {
					// (2026-10-01) VERROUILLEE, l'echelle garde ses proportions : l'axe
					// change entraine l'autre (Unreal).
					if (I.c.ui.echelleVerrou) {
						if (e.x != avant.x && avant.x != 0.f) {
							e.y = avant.y * e.x / avant.x;
						} else if (e.y != avant.y && avant.y != 0.f) {
							e.x = avant.x * e.y / avant.y;
						}
					}
					NkEditeurPoserEchelle(m, I.id, e);
				}
				if (remise) {
					NkEditeurPoserEchelle(m, I.id, NkVec2f(1.f, 1.f));
				}
				Fin(I);
			}

			void CarteSprite(NkInspecteur &I) {
				NkSprite2D *s = I.c.m.scene.Monde().Get<NkSprite2D>(I.id);
				if (!Entete(I, NkCarteEditeur::NK_SPRITE, &s->visible)) {
					return;
				}
				Paire(I, "Taille (m)", s->taille.x, s->taille.y, 0.01f, 0.05f, 50.f, "L", "H");
				Paire(I, "Pivot", s->pivot.x, s->pivot.y, 0.01f, 0.f, 1.f);
				Teinte(I, "Teinte", s->couleur);
				Entier(I, "Couche", s->couche);
				// (2026-10-01) La reference d'asset d'Unreal, au lieu du nom en lecture seule.
				ReferenceTexture(I);
				Fin(I);
			}

			bool Angle(NkInspecteur &I, const char *libelle, float32 &radians, float32 mini, float32 maxi);

			/// Un choix parmi BEAUCOUP de valeurs (le genre d'une forme, l'ancre d'un
			/// HUD) : des boutons segmentes, `parLigne` par rangee ; le libelle sur la
			/// premiere seulement.
			bool ChoixGrille(NkInspecteur &I, const char *libelle, const char *const *noms, int32 n, int32 parLigne, int32 &valeur) {
				bool change = false;
				const bool actif = NkEditeurDans(I.zone, I.c.ctx.input.mousePos);
				I.c.ctx.PushId(libelle);
				const bool avant = gCarteRepond;
				for (int32 debut = 0; debut < n; debut += parLigne) {
					NkRect champ, lib;
					if (!Rangee(I, debut == 0 ? libelle : "", RANG_H, champ, lib)) {
						break;
					}
					// Les rangees suivantes se montrent avec la premiere (recherche).
					gCarteRepond = true;
					const float32 w = champ.w / static_cast<float32>(parLigne);
					for (int32 i = debut; i < debut + parLigne && i < n; ++i) {
						const NkRect r{champ.x + static_cast<float32>(i - debut) * w, champ.y, w - 2.f, champ.h};
						if (NkEditeurBouton(I.c, r, "", i == valeur, actif, &I.c.ctx.DL()) && i != valeur) {
							valeur = i;
							change = true;
						}
						renderer::NkTexteDansBoite(I.c.ctx.DL(), I.c.petite, r, noms[i], i == valeur ? I.c.pal.surAccent : I.c.pal.texte);
					}
				}
				gCarteRepond = avant;
				I.c.ctx.PopId();
				return change;
			}

			/// Une liste de POINTS dans une carte (sommets d'un collisionneur, points
			/// d'une ligne) : une rangee « titre (n) » avec « + » et « − », puis une
			/// rangee par point, son numero a gauche, X / Y a liseres a droite.
			bool Points(NkInspecteur &I, const char *titre, NkVec2f *pts, uint8 &nb, uint32 mini, uint32 maxi) {
				NkGuiContext &ctx = I.c.ctx;
				NkRect champ, lib;
				const NkString t = NkString::Format("%s (%u)", titre, static_cast<unsigned>(nb));
				if (!Repond(titre) || !Rangee(I, t.CStr(), RANG_H, champ, lib)) {
					return false;
				}
				bool change = false;
				const bool dedans = NkEditeurDans(I.zone, ctx.input.mousePos);
				const float32 w = (champ.w - 4.f) * 0.5f;
				const NkRect rp{champ.x, champ.y, w, champ.h};
				const NkRect rm{champ.x + w + 4.f, champ.y, w, champ.h};
				if (NkEditeurBouton(I.c, rp, "", false, dedans && nb < maxi, &ctx.DL()) && nb < maxi) {
					// Au milieu du dernier cote : la forme ne saute pas.
					const NkVec2f a = nb > 0u ? pts[nb - 1u] : NkVec2f(0.f, 0.f);
					const NkVec2f b = nb > 0u ? pts[0] : NkVec2f(0.5f, 0.f);
					pts[nb] = NkVec2f((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f + (nb > 1u ? 0.f : 0.5f));
					++nb;
					change = true;
				}
				renderer::NkTexteDansBoite(ctx.DL(), I.c.petite, rp, "+ point", nb < maxi ? I.c.pal.texte : I.c.pal.attenue);
				if (NkEditeurBouton(I.c, rm, "", false, dedans && nb > mini, &ctx.DL()) && nb > mini) {
					--nb;
					change = true;
				}
				renderer::NkTexteDansBoite(ctx.DL(), I.c.petite, rm, "− point", nb > mini ? I.c.pal.texte : I.c.pal.attenue);
				ctx.PushId(titre);
				for (uint32 i = 0; i < nb; ++i) {
					char num[16];
					std::snprintf(num, sizeof(num), "   %u", static_cast<unsigned>(i + 1u));
					// Le numero ne repond pas a la recherche : la liste suit son titre.
					const bool avant = gCarteRepond;
					gCarteRepond = true;
					change |= Paire(I, num, pts[i].x, pts[i].y, 0.01f);
					gCarteRepond = avant;
				}
				ctx.PopId();
				return change;
			}

			/// Le premier calque d'une couche (-1 : aucun, collisionneur eteint).
			int32 PremierCalque(uint32 couche) {
				for (uint32 i = 0; i < NK_CALQUES_COLLISION; ++i) {
					if ((couche & (1u << i)) != 0u) {
						return static_cast<int32>(i);
					}
				}
				return -1;
			}

			NkString NomCalque(const NkCalquesCollision2D &k, int32 i) {
				if (i < 0) {
					return NkString("(aucun)");
				}
				if (k.noms[i][0] != '\0') {
					return NkString(k.noms[i]);
				}
				return i == 0 ? NkString("Défaut") : NkString::Format("Calque %d", i);
			}

			/// Le CALQUE d'un collisionneur (‹ nom ›) et ce qu'il TOUCHE (les calques
			/// nommes que sa ligne de la matrice laisse passer). Rend vrai s'il change.
			bool Calque(NkInspecteur &I, NkCollisionneur2D &col) {
				NkCalquesCollision2D &kc = I.c.m.scene.Calques();
				bool change = false;
				int32 calque = PremierCalque(col.couche);
				NkRect champ, lib;
				if (Rangee(I, "Calque", RANG_H, champ, lib)) {
					auto &dl = I.c.ctx.DL();
					const bool dedans = NkEditeurDans(I.zone, I.c.ctx.input.mousePos);
					const NkRect rg{champ.x, champ.y, 22.f, champ.h};
					const NkRect rd{champ.x + champ.w - 22.f, champ.y, 22.f, champ.h};
					const NkRect rn{champ.x + 24.f, champ.y, champ.w - 48.f, champ.h};
					if (NkEditeurBouton(I.c, rg, "", false, dedans && calque > 0, &dl) && calque > 0) {
						col.couche = (col.couche & 0xFFFF0000u) | (1u << static_cast<uint32>(calque - 1));
						change = true;
					}
					renderer::NkTexteDansBoite(dl, I.c.petite, rg, "‹", I.c.pal.texte);
					if (NkEditeurBouton(I.c, rd, "", false, dedans && calque >= 0 && calque < static_cast<int32>(NK_CALQUES_COLLISION) - 1, &dl) &&
						calque >= 0 && calque < static_cast<int32>(NK_CALQUES_COLLISION) - 1) {
						col.couche = (col.couche & 0xFFFF0000u) | (1u << static_cast<uint32>(calque + 1));
						change = true;
					}
					renderer::NkTexteDansBoite(dl, I.c.petite, rd, "›", I.c.pal.texte);
					calque = PremierCalque(col.couche);
					dl.AddRectFilled(rn, I.c.pal.champ, 3.f);
					renderer::NkTexteDansBoite(dl, I.c.police, rn, NomCalque(kc, calque).CStr(), I.c.pal.texte);
				}
				NkString touche;
				int32 refuses = 0;
				for (uint32 j = 0; j < NK_CALQUES_COLLISION; ++j) {
					if (j != 0u && kc.noms[j][0] == '\0' && static_cast<int32>(j) != calque) {
						continue;
					}
					if (calque >= 0 && !kc.Touche(static_cast<uint32>(calque), j)) {
						++refuses;
						continue;
					}
					if (!touche.Empty()) {
						touche.Append(", ");
					}
					touche.Append(NomCalque(kc, static_cast<int32>(j)).CStr());
				}
				if (refuses > 0) {
					touche.Append(NkString::Format("  (%d refusé%s)", refuses, refuses > 1 ? "s" : "").CStr());
				}
				Info(I, "Touche", touche.Empty() ? "rien" : touche.CStr());
				return change;
			}

			void CarteCollisionneur(NkInspecteur &I) {
				NkEditeurModele &m = I.c.m;
				bool actif = NkEditeurCarteActive(m, I.id, NkCarteEditeur::NK_COLLISIONNEUR);
				const bool ouverte = Entete(I, NkCarteEditeur::NK_COLLISIONNEUR, &actif);
				NkEditeurActiverCarte(m, I.id, NkCarteEditeur::NK_COLLISIONNEUR, actif);
				// APRES la case : l'eteindre ajoute un composant a l'entite.
				NkCollisionneur2D *col = m.scene.Monde().Get<NkCollisionneur2D>(I.id);
				if (!ouverte) {
					return;
				}
				if (!actif) {
					Ligne(I, "Éteint : il ne touche plus rien (couche et masque à 0).");
				}
				NkScene &s = m.scene;
				bool change = false;
				// ── LE TYPE, AU CHOIX (2026-10-01, R33 : tout ce que montrait le bloc
				//    « Collision » sous les cartes vit ici). Boite, cercle, capsule et
				//    polygone se refont A LA TAILLE de ce qu'on voit
				//    (NkEditeurAjouterCollision) ; la chaine garde ses sommets ─────
				static const char *kTypes[5] = {"Boîte", "Cercle", "Capsule", "Polygone", "Chaîne"};
				static const NkForme2D kFormes[5] = {NkForme2D::NK_BOITE, NkForme2D::NK_CERCLE, NkForme2D::NK_CAPSULE, NkForme2D::NK_POLYGONE,
													 NkForme2D::NK_CHAINE};
				int32 t = 0;
				for (int32 k = 0; k < 5; ++k) {
					t = kFormes[k] == col->forme ? k : t;
				}
				if (ChoixGrille(I, "Type", kTypes, 5, 3, t)) {
					if (t < 4) {
						// NkCollisionEditeur : Boite, Cercle, Capsule, Polygone, dans cet ordre.
						NkEditeurAjouterCollision(m, I.id, static_cast<NkCollisionEditeur>(t));
					} else {
						NkEditeurRetenir(m);
						col = s.Monde().Get<NkCollisionneur2D>(I.id);
						if (col->nbSommets < 2u) {
							col->nbSommets = 2u;
							col->sommets[0] = NkVec2f(-col->demiTaille.x, 0.f);
							col->sommets[1] = NkVec2f(col->demiTaille.x, 0.f);
						}
						col->forme = NkForme2D::NK_CHAINE;
						change = true;
					}
					col = s.Monde().Get<NkCollisionneur2D>(I.id);
				}
				// Generer depuis ce que l'entite montre (forme 2D, pixels du sprite).
				const NkSprite2D *sp = s.Monde().Get<NkSprite2D>(I.id);
				const bool aForme = s.Monde().Has<NkRenduForme2D>(I.id);
				const bool aImage = sp != nullptr && sp->texId != 0u;
				if (aForme || aImage) {
					const int32 g = Boutons(I, "Depuis la forme", "Depuis le sprite", aForme, aImage);
					if (g == 0 && aForme) {
						NkEditeurAjouterCollision(m, I.id, NkCollisionEditeur::NK_DEPUIS_FORME);
					} else if (g == 1 && aImage) {
						NkEditeurAjouterCollision(m, I.id, NkCollisionEditeur::NK_DEPUIS_SPRITE);
					}
					col = s.Monde().Get<NkCollisionneur2D>(I.id);
				}
				// ── Les mesures du type ─────────────────────────────────────────
				if (col->forme == NkForme2D::NK_BOITE) {
					change |= Paire(I, "Demi-taille (m)", col->demiTaille.x, col->demiTaille.y, 0.01f, 0.02f, 50.f, "L", "H");
				} else if (col->forme == NkForme2D::NK_CERCLE || col->forme == NkForme2D::NK_CAPSULE) {
					change |= Nombre(I, "Rayon (m)", col->rayon, 0.01f, 0.02f, 20.f);
					if (col->forme == NkForme2D::NK_CAPSULE) {
						change |= Nombre(I, "Demi-longueur (m)", col->demiTaille.x, 0.01f, 0.f, 50.f);
					}
				} else {
					const bool polygone = col->forme == NkForme2D::NK_POLYGONE;
					if (!polygone) {
						change |= Case(I, "Chaîne fermée", col->boucle);
					}
					change |= Points(I, polygone ? "Sommets (8 max)" : "Sommets", col->sommets, col->nbSommets, polygone ? 3u : 2u,
									 polygone ? NK_POLYGONE_CONVEXE_MAX : NK_COLLISION_SOMMETS_MAX);
					if (polygone && col->nbSommets >= 3u && !NkEstConvexe2D(col->sommets, col->nbSommets)) {
						Ligne(I, "Polygone CONCAVE : prenez « Chaîne » (décor) ou « Depuis la forme ».");
					}
				}
				change |= Paire(I, "Décalage (m)", col->decalage.x, col->decalage.y, 0.01f);
				change |= Angle(I, "Rotation propre (°)", col->rotation, -360.f, 360.f);
				change |= Case(I, "Déclencheur (zone)", col->declencheur);
				// ── Le calque, et ce qu'il touche (la ligne de la matrice) ─────────
				change |= Calque(I, *col);
				if (Boutons(I, "Calques et matrice (réglages du projet)") == 0) {
					I.c.ui.reglagesCollision = true;
				}
				Case(I, "Éditer dans la vue (poignées)", I.c.ui.editionCollision);
				// ── Sans corps rigide, il ne touche rien en jeu ─────────────────────
				if (!s.Monde().Has<NkCorps2D>(I.id) && s.PhysiqueActive()) {
					Ligne(I, "Sans corps rigide : il ne touche rien en jeu.");
					const int32 k = Boutons(I, "Rendre solide (statique)", "Le faire tomber (dynamique)");
					if (k >= 0) {
						NkEditeurRetenir(m);
						NkCorps2D nb;
						nb.type = k == 0 ? NkTypeCorps::NK_STATIQUE : NkTypeCorps::NK_DYNAMIQUE;
						s.AjouterCorps(I.id, nb);
						// Dynamique : un contour concave devient son enveloppe convexe.
						NkEditeurFormeChangee(m, I.id);
					}
					col = s.Monde().Get<NkCollisionneur2D>(I.id);
				}
				if (change && col != nullptr) {
					// Regle a la main : la forme ne le refait plus.
					if (NkRenduForme2D *f = s.Monde().Get<NkRenduForme2D>(I.id)) {
						f->collisionSuit = false;
					}
					// ⚠️ La forme vit AUSSI dans le solveur : on refait le corps.
					if (s.Monde().Has<NkCorps2D>(I.id)) {
						s.ActualiserCorps(I.id);
					}
				}
				Fin(I);
			}

			void CarteCorps(NkInspecteur &I) {
				NkEditeurModele &m = I.c.m;
				bool actif = NkEditeurCarteActive(m, I.id, NkCarteEditeur::NK_CORPS);
				const bool ouverte = Entete(I, NkCarteEditeur::NK_CORPS, &actif);
				NkEditeurActiverCarte(m, I.id, NkCarteEditeur::NK_CORPS, actif);
				NkCorps2D *b = m.scene.Monde().Get<NkCorps2D>(I.id);
				if (!ouverte) {
					return;
				}
				if (!actif) {
					Ligne(I, "Éteint : cinématique, sans gravité (« Simulated » décoché).");
				}
				bool change = false;
				static const char *kTypes[3] = {"Statique", "Cinématique", "Dynamique"};
				int32 ty = static_cast<int32>(b->type);
				if (Choix(I, "Type", kTypes, 3, ty)) {
					b->type = static_cast<NkTypeCorps>(ty);
					// Un corps qui devient dynamique : le contour concave d'une forme
					// devient son enveloppe (le solveur ne fait pas tomber un concave).
					NkEditeurFormeChangee(m, I.id);
					b = m.scene.Monde().Get<NkCorps2D>(I.id);
					change = true;
				}
				change |= Nombre(I, "Masse (kg)", b->masse, 0.1f, 0.1f, 1000.f);
				change |= Glissiere(I, "Friction", b->friction, 0.f, 1.5f);
				change |= Glissiere(I, "Rebond", b->rebond, 0.f, 1.f);
				change |= Nombre(I, "Gravité (x)", b->echelleGravite, 0.01f, -4.f, 8.f);
				change |= Nombre(I, "Frein linéaire", b->amortissementLineaire, 0.01f, 0.f, 10.f);
				change |= Nombre(I, "Frein angulaire", b->amortissementAngulaire, 0.01f, 0.f, 10.f);
				change |= Case(I, "Rotation bloquée", b->rotationBloquee);
				// ⚠️ NkCorps2D est la DESCRIPTION ; le corps vit dans NKPhysics.
				// Sans cet appel, les Details montreraient une masse que le
				// solveur n'a pas.
				if (change) {
					m.scene.ActualiserCorps(I.id);
				}
				const NkVec2f v = m.scene.Vitesse(I.id);
				Info(I, "Vitesse", NkString::Format("%.2f m/s", static_cast<double>(math::NkSqrt(v.x * v.x + v.y * v.y))).CStr());
				Fin(I);
			}

			void CarteCorpsMou(NkInspecteur &I) {
				NkEditeurModele &m = I.c.m;
				NkCorpsMou2D *mou = m.scene.Monde().Get<NkCorpsMou2D>(I.id);
				if (!Entete(I, NkCarteEditeur::NK_CORPS_MOU, &mou->visible)) {
					return;
				}
				physics::NkParticules2D *p = m.scene.Particules();
				const int32 ci = p != nullptr ? p->IndexCorps(mou->corpsId) : -1;
				if (ci < 0) {
					Ligne(I, "(matière disparue)");
					Fin(I);
					return;
				}
				physics::NkCorpsP2D &corps = p->corps[static_cast<uint32>(ci)];
				Info(I, "Matériau", physics::NkMateriauP2DNom(corps.mat));
				Info(I, "Matière",
					 NkString::Format("%u particules, %u liens", static_cast<unsigned>(corps.nombre),
									  static_cast<unsigned>(p->LiensActifsDuCorps(static_cast<uint32>(ci))))
						 .CStr());
				Teinte(I, "Couleur", mou->couleur);
				// Les parametres du SOLVEUR : lus a chaque sous-pas, donc
				// modifiables a chaud, sans rien refaire.
				Glissiere(I, "Raideur", corps.raideur, 0.f, 1.f);
				Glissiere(I, "Friction", corps.friction, 0.f, 1.f);
				Glissiere(I, "Rebond", corps.rebond, 0.f, 1.f);
				if (corps.mat == physics::NkMateriauP2D::NK_BALLON) {
					Glissiere(I, "Pression", corps.pression, 0.f, 3.f);
				}
				if (physics::NkEstFluideP2D(corps.mat)) {
					Glissiere(I, "Viscosité", corps.viscosite, 0.f, 0.5f);
					Glissiere(I, "Cohésion", corps.cohesion, 0.f, 1.5f);
				}
				if (corps.mat == physics::NkMateriauP2D::NK_BLOB) {
					Nombre(I, "Plasticité (1/s)", corps.plasticite, 0.1f, 0.f, 30.f);
				}
				if (corps.mat == physics::NkMateriauP2D::NK_GELEE) {
					Glissiere(I, "Mémoire de forme", corps.formeRaideur, 0.f, 1.f);
				}
				Glissiere(I, "Résistance (rupture)", corps.resistance, 0.f, 3.f);
				Case(I, "Se touche elle-même", corps.autoCollision);
				Case(I, "Touche les rigides", corps.couplageRigide);
				const int32 b = Boutons(I, "Épingler tout", "Libérer");
				if (b == 0) {
					p->EpinglerCorps(static_cast<uint32>(ci), true);
				} else if (b == 1) {
					p->EpinglerCorps(static_cast<uint32>(ci), false);
				}
				Fin(I);
			}

			void CarteSource(NkInspecteur &I) {
				NkEditeurModele &m = I.c.m;
				bool actif = NkEditeurCarteActive(m, I.id, NkCarteEditeur::NK_SOURCE);
				const bool ouverte = Entete(I, NkCarteEditeur::NK_SOURCE, &actif);
				NkEditeurActiverCarte(m, I.id, NkCarteEditeur::NK_SOURCE, actif);
				NkSource2D *s = m.scene.Monde().Get<NkSource2D>(I.id);
				if (!ouverte) {
					return;
				}
				if (!actif) {
					Ligne(I, "Éteinte : muette.");
				}
				int32 son = static_cast<int32>(s->son);
				if (Entier(I, "Son (id)", son, 0, 100000)) {
					s->son = static_cast<uint32>(son);
				}
				Glissiere(I, "Volume", s->volume, 0.f, 2.f);
				Glissiere(I, "Hauteur", s->pitch, 0.25f, 4.f);
				Nombre(I, "Portée hors champ (m)", s->portee, 0.1f, 0.f, 100.f);
				Case(I, "Boucle", s->boucle);
				Case(I, "Au démarrage", s->auDemarrage);
				Case(I, "Spatial", s->spatial);
				Fin(I);
			}

			void CarteAnimation(NkInspecteur &I) {
				NkAnimSprite2D *a = I.c.m.scene.Monde().Get<NkAnimSprite2D>(I.id);
				// La case « actif » d'une animation, c'est qu'elle JOUE.
				bool joue = !a->enPause;
				const bool ouverte = Entete(I, NkCarteEditeur::NK_ANIMATION, &joue);
				a->enPause = !joue;
				if (!ouverte) {
					return;
				}
				int32 col = a->colonnes;
				int32 lig = a->lignes;
				if (Entier(I, "Colonnes de l'atlas", col, 1, 256)) {
					a->colonnes = static_cast<uint16>(col < 1 ? 1 : col);
				}
				if (Entier(I, "Lignes de l'atlas", lig, 1, 256)) {
					a->lignes = static_cast<uint16>(lig < 1 ? 1 : lig);
				}
				if (a->nbClips > 0) {
					NkClipSprite &clip = a->clips[0];
					int32 premiere = clip.premiere;
					int32 nombre = clip.nombre;
					if (Entier(I, "Clip 0 : 1re image", premiere, 0, 65535)) {
						clip.premiere = static_cast<uint16>(premiere < 0 ? 0 : premiere);
					}
					if (Entier(I, "Clip 0 : images", nombre, 1, 65535)) {
						clip.nombre = static_cast<uint16>(nombre < 1 ? 1 : nombre);
					}
					Glissiere(I, "Images / s", clip.imagesParSeconde, 0.f, 60.f);
				}
				Fin(I);
			}

			/// L'animateur : la machine a etats de NKAnima qui choisit le clip.
			/// Pas de Reinitialiser ni de Retirer : son modele est un fichier, et
			/// « + Ajouter » passe par NkComposantEditeur, qui ne le porte pas.
			void CarteAnimateur(NkInspecteur &I) {
				NkGuiContext &ctx = I.c.ctx;
				NkAnimateur2D *a = I.c.m.scene.Monde().Get<NkAnimateur2D>(I.id);
				bool actif = !a->enPause;
				const bool ouverte = Entete(I, NkCarteEditeur::NK_ANIMATEUR, &actif);
				a->enPause = !actif;
				if (!ouverte) {
					return;
				}
				Info(I, "Modèle", a->modele[0] != '\0' ? a->modele : "(aucun)");
				const anim::NkAnimStateMachine *mach = NkModeleAnimateur(a->modele);
				if (mach == nullptr) {
					// (2026-10-01, R33) Une scene rouverte : le controleur du Contenu
					// qui porte ce nom se relit d'un clic.
					Ligne(I, "Modèle non enregistré (scène rouverte ?)");
					if (Boutons(I, "Relire le contrôleur du Contenu") == 0 && !NkEditeurRetrouverControleur(I.c.m, a->modele)) {
						NkEditeurAnnoncer(I.c.m, NkString::Format("Aucun contrôleur « %s » dans le Contenu", a->modele).CStr());
					}
					Fin(I);
					return;
				}
				const NkString etat = NkEtatAnimateur2D(*a);
				Info(I, "État", etat.Empty() ? "(pas encore démarré)" : etat.CStr());
				// Les etats, un par ligne, indentes par niveau ; le chemin actif est
				// marque. C'est le graphe en texte : l'editeur de graphe (NKGraph)
				// viendra, pas un second modele d'etats ici.
				for (int32 s = 0; s < mach->GetStateCount(); ++s) {
					bool dedans = false;
					for (int32 k = a->execution.current; k >= 0; k = mach->GetStateParent(k)) {
						dedans = dedans || k == s;
					}
					NkString ligne(dedans ? "> " : "   ");
					for (int32 d = 0; d < mach->GetStateDepth(s); ++d) {
						ligne.Append("   ");
					}
					ligne.Append(mach->GetStateName(s).CStr());
					if (mach->IsSubMachine(s)) {
						ligne.Append("/");
					} else {
						ligne.Append(NkString::Format("  (clip %d)", mach->GetStateTag(s)).CStr());
					}
					Ligne(I, ligne.CStr(), dedans);
				}
				// Les parametres se REGLENT ici : en pause comme en jeu, c'est la
				// maniere la plus directe de voir la machine basculer.
				ctx.PushId("animateur");
				for (int32 i = 0; i < a->nbParams; ++i) {
					NkParamAnimateur2D &p = a->params[i];
					if (p.genre == NkGenreParamAnim::NK_BOOL) {
						bool v = p.valeur != 0.f;
						if (Case(I, p.nom, v)) {
							p.valeur = v ? 1.f : 0.f;
						}
					} else if (p.genre == NkGenreParamAnim::NK_DECLENCHEUR) {
						NkRect champ, lib;
						if (!Rangee(I, p.nom, RANG_H, champ, lib)) {
							continue;
						}
						const NkRect rb{champ.x, champ.y, champ.w * 0.45f, champ.h};
						if (NkEditeurBouton(I.c, rb, "", false, NkEditeurDans(I.zone, ctx.input.mousePos), &ctx.DL())) {
							p.valeur = 1.f;
						}
						renderer::NkTexteDansBoite(ctx.DL(), I.c.petite, rb, "Déclencher", I.c.pal.texte);
						renderer::NkTexte(ctx.DL(), I.c.petite, rb.x + rb.w + 8.f, champ.y + 3.f, p.valeur != 0.f ? "(posé)" : "(consommé)",
										  I.c.pal.attenue);
					} else {
						Nombre(I, p.nom, p.valeur, 0.05f);
					}
				}
				ctx.PopId();
				Fin(I);
			}

			/// Le nom de fichier d'un chemin (un prefab est nomme par son chemin).
			const char *Fichier(const char *chemin) {
				const char *f = chemin;
				for (const char *p = chemin; *p != '\0'; ++p) {
					if (*p == '/' || *p == '\\') {
						f = p + 1;
					}
				}
				return f;
			}

			/// La place de l'entite dans la scene : son parent, ses enfants, son
			/// prefab. C'est la hierarchie de l'Outliner, lue et modifiee ici.
			void CarteHierarchie(NkInspecteur &I) {
				NkEditeurModele &m = I.c.m;
				if (!Entete(I, NkCarteEditeur::NK_HIERARCHIE, nullptr)) {
					return;
				}
				const ecs::NkEntityId parent = m.scene.Parent(I.id);
				Info(I, "Parent", parent.IsValid() ? NomDe(m.scene, parent).CStr() : "(racine de la scène)");
				NkVector<ecs::NkEntityId> enfants;
				m.scene.Enfants(I.id, enfants);
				NkString liste = NkString::Format("%u", static_cast<unsigned>(enfants.Size()));
				for (uint32 k = 0; k < enfants.Size() && k < 3u; ++k) {
					liste.Append(k == 0 ? " : " : ", ");
					liste.Append(NomDe(m.scene, enfants[k]).CStr());
				}
				if (enfants.Size() > 3u) {
					liste.Append("...");
				}
				Info(I, "Enfants", liste.CStr());
				const NkInstancePrefab2D *inst = m.scene.Monde().Get<NkInstancePrefab2D>(I.id);
				if (inst != nullptr && inst->prefab != 0u) {
					Info(I, "Prefab", Fichier(m.prefabs.Nom(inst->prefab)));
					NkVector<NkString> surcharges;
					m.prefabs.Surcharges(m.scene, I.id, surcharges);
					Info(I, "Surcharges", NkString::Format("%u", static_cast<unsigned>(surcharges.Size())).CStr());
				} else {
					Info(I, "Prefab", "(aucun)");
				}
				const bool edition = m.etat == NkEtatJeu::NK_EDITION;
				const int32 b = Boutons(I, "Détacher du parent", "Créer un prefab", parent.IsValid(), edition);
				if (b == 0) {
					NkEditeurExecuter(I.c, NK_A_DETACHER);
				} else if (b == 1) {
					NkEditeurExecuter(I.c, NK_A_CREER_PREFAB);
				}
				Fin(I);
			}

			/// Une valeur en degres, stockee en radians.
			bool Angle(NkInspecteur &I, const char *libelle, float32 &radians, float32 mini, float32 maxi) {
				float32 d = radians * 57.2957795f;
				if (!Glissiere(I, libelle, d, mini, maxi)) {
					return false;
				}
				radians = d / 57.2957795f;
				return true;
			}

			/// La lumiere 2D (NkEditeurLumiere.h, 2026-09-30) : la case de l'en-tete
			/// l'ALLUME. Un rappel quand l'eclairage de la scene est eteint : une
			/// lumiere sans lui ne fait rien, et un reglage sans effet visible passe
			/// pour une panne.
			void CarteLumiere(NkInspecteur &I) {
				NkEditeurModele &m = I.c.m;
				NkLumiere2D *l = m.scene.Monde().Get<NkLumiere2D>(I.id);
				if (!Entete(I, NkCarteEditeur::NK_LUMIERE, &l->actif)) {
					return;
				}
				if (!m.scene.Eclairage().actif) {
					Ligne(I, "L'éclairage de la scène est éteint : cette lumière n'a pas d'effet.");
					if (Boutons(I, "Activer l'éclairage de la scène") == 0) {
						m.scene.Eclairage().actif = true;
						NkEditeurAnnoncer(m, "Eclairage 2D de la scene active");
					}
				}
				static const char *kTypes[3] = {"Ponctuelle", "Cône", "Direction."};
				int32 type = static_cast<int32>(l->type);
				if (Choix(I, "Type", kTypes, 3, type)) {
					l->type = static_cast<NkTypeLumiere2D>(type);
				}
				Teinte(I, "Couleur", l->couleur);
				Glissiere(I, "Intensité", l->intensite, 0.f, 4.f);
				if (l->type == NkTypeLumiere2D::NK_DIRECTIONNELLE) {
					Nombre(I, "Longueur des ombres (m)", l->portee, 0.05f, 0.5f, 50.f);
				} else {
					Nombre(I, "Portée (m)", l->portee, 0.05f, 0.1f, 30.f);
					Glissiere(I, "Atténuation", l->attenuation, 0.2f, 4.f);
				}
				if (l->type != NkTypeLumiere2D::NK_PONCTUELLE) {
					Angle(I, "Direction (°)", l->direction, -180.f, 180.f);
				}
				if (l->type == NkTypeLumiere2D::NK_SPOT) {
					Angle(I, "Ouverture (°)", l->ouverture, 1.f, 180.f);
					Glissiere(I, "Douceur du bord", l->douceur, 0.f, 1.f);
				}
				Glissiere(I, "Halo additif", l->halo, 0.f, 1.f);
				Case(I, "Porte des ombres", l->ombres);
				Fin(I);
			}

			/// L'emetteur de particules visuelles : la case de l'en-tete l'active ;
			/// ses preregrages en boutons, puis la recette, rangee par rangee.
			void CarteEmetteur(NkInspecteur &I) {
				NkEditeurModele &m = I.c.m;
				NkEmetteur2D *e = m.scene.Monde().Get<NkEmetteur2D>(I.id);
				if (!Entete(I, NkCarteEditeur::NK_EMETTEUR, &e->actif)) {
					return;
				}
				Info(I, "Préréglage", NkNomPresetEffet2D(e->preset));
				// Les preregrages, deux par rangee (0 est « aucun »).
				const int32 n = static_cast<int32>(NkPresetEffet2D::NK_COUNT);
				for (int32 p = 1; p < n; p += 2) {
					const char *a = NkNomPresetEffet2D(static_cast<NkPresetEffet2D>(p));
					const char *b = p + 1 < n ? NkNomPresetEffet2D(static_cast<NkPresetEffet2D>(p + 1)) : nullptr;
					const int32 k = Boutons(I, a, b);
					if (k >= 0) {
						NkEditeurAppliquerPreset(m, I.id, static_cast<NkPresetEffet2D>(p + k));
						e = m.scene.Monde().Get<NkEmetteur2D>(I.id);
					}
				}
				uint32 vivantes = 0u;
				const NkVector<NkParticuleEffet2D> &ps = m.scene.Effets().Particules();
				for (uint32 i = 0; i < ps.Size(); ++i) {
					vivantes += ps[i].emetteur == I.id.Pack() ? 1u : 0u;
				}
				Info(I, "Particules vivantes", NkString::Format("%u / %u", vivantes, e->maxParticules).CStr());
				Case(I, "En boucle", e->boucle);
				if (Boutons(I, "Rejouer") == 0) {
					m.scene.Effets().Rejouer(I.id.Pack());
				}
				if (!e->boucle) {
					Glissiere(I, "Durée d'émission (s)", e->duree, 0.f, 10.f);
				}
				int32 rafale = static_cast<int32>(e->rafale);
				if (Entier(I, "Rafale au départ", rafale, 0, 4096)) {
					e->rafale = static_cast<uint32>(rafale);
				}
				Glissiere(I, "Débit (par s)", e->debit, 0.f, 500.f);
				Paire(I, "Vie (s)", e->vieMin, e->vieMax, 0.01f, 0.02f, 60.f, "min", "max");
				e->vieMax = e->vieMax < e->vieMin ? e->vieMin : e->vieMax;
				Paire(I, "Vitesse (m/s)", e->vitesseMin, e->vitesseMax, 0.02f, 0.f, 100.f, "min", "max");
				Angle(I, "Direction (°)", e->direction, -180.f, 180.f);
				Angle(I, "Dispersion (°)", e->dispersion, 0.f, 180.f);
				Paire(I, "Gravité (m/s²)", e->gravite.x, e->gravite.y, 0.05f);
				Glissiere(I, "Frein de l'air (1/s)", e->frein, 0.f, 5.f);
				Paire(I, "Taille (m)", e->tailleDebut, e->tailleFin, 0.01f, 0.01f, 3.f, "déb", "fin");
				Teinte(I, "Couleur au début", e->couleurDebut);
				Teinte(I, "Couleur à mi-vie", e->couleurMilieu);
				Teinte(I, "Couleur à la fin", e->couleurFin);
				Case(I, "Additif (émet de la lumière)", e->additif);
				static const char *kFormes[3] = {"Douce", "Trait", "Pleine"};
				int32 forme = static_cast<int32>(e->forme);
				if (Choix(I, "Forme", kFormes, 3, forme)) {
					e->forme = static_cast<NkFormeParticule2D>(forme);
				}
				static const char *kZones[3] = {"Point", "Disque", "Ligne"};
				int32 zone = static_cast<int32>(e->zone);
				if (Choix(I, "Zone d'émission", kZones, 3, zone)) {
					e->zone = static_cast<NkZoneEmission2D>(zone);
				}
				if (e->zone == NkZoneEmission2D::NK_DISQUE) {
					Glissiere(I, "Rayon de la zone (m)", e->rayonZone, 0.f, 10.f);
				} else if (e->zone == NkZoneEmission2D::NK_LIGNE) {
					Glissiere(I, "Largeur de la zone (m)", e->largeurZone, 0.f, 60.f);
				}
				int32 graine = static_cast<int32>(e->graine);
				if (Entier(I, "Graine", graine, 0, 2147483000)) {
					e->graine = static_cast<uint32>(graine);
					m.scene.Effets().Rejouer(I.id.Pack()); // une autre graine : un autre effet, depuis le debut
				}
				int32 maxi = static_cast<int32>(e->maxParticules);
				if (Entier(I, "Plafond de particules", maxi, 1, 8192)) {
					e->maxParticules = static_cast<uint32>(maxi);
				}
				Case(I, "Éclaire (lumière liée)", e->eclaire);
				if (e->eclaire) {
					if (!m.scene.Eclairage().actif) {
						Ligne(I, "(sans effet tant que l'éclairage de la scène est éteint)");
					}
					Teinte(I, "Couleur de la lumière", e->couleurLumiere);
					Glissiere(I, "Intensité de la lumière", e->intensiteLumiere, 0.f, 4.f);
					Nombre(I, "Portée de la lumière (m)", e->porteeLumiere, 0.05f, 0.1f, 30.f);
					Glissiere(I, "Vacillement", e->scintillement, 0.f, 1.f);
					Case(I, "La lumière porte des ombres", e->ombresLumiere);
				}
				Fin(I);
			}

			/// La FORME 2D (2026-10-01, R33) : ce que montrait le bloc « Forme 2D » sous
			/// les cartes, rangee par rangee. La case de l'en-tete la rend visible ;
			/// un reglage qui change le contour refait le collisionneur s'il la suit
			/// (NkEditeurFormeChangee).
			void CarteForme(NkInspecteur &I) {
				NkEditeurModele &m = I.c.m;
				NkRenduForme2D *f = m.scene.Monde().Get<NkRenduForme2D>(I.id);
				if (!Entete(I, NkCarteEditeur::NK_FORME, &f->visible)) {
					return;
				}
				bool change = false;
				static const char *kGenres[9] = {"Rectangle", "Cercle", "Ellipse", "Triangle", "Étoile", "Polygone", "Capsule", "Ligne", "Libre"};
				int32 g = static_cast<int32>(f->genre);
				if (ChoixGrille(I, "Genre", kGenres, 9, 3, g)) {
					const NkGenreForme2D genre = static_cast<NkGenreForme2D>(g);
					// Les points d'exemple d'une ligne ou d'un polygone libre, s'il n'en
					// a pas : sans eux, la forme n'aurait que sa boite.
					if ((genre == NkGenreForme2D::NK_LIGNE && f->nbPoints < 2u) || (genre == NkGenreForme2D::NK_POLYGONE_LIBRE && f->nbPoints < 3u)) {
						const NkRenduForme2D d = NkFormeParDefaut(genre);
						f->nbPoints = d.nbPoints;
						for (uint32 i = 0; i < NK_FORME_POINTS_MAX; ++i) {
							f->points[i] = d.points[i];
						}
					}
					f->genre = genre;
					change = true;
				}
				const bool aPoints = f->genre == NkGenreForme2D::NK_LIGNE || f->genre == NkGenreForme2D::NK_POLYGONE_LIBRE;
				if (!aPoints) {
					change |= Paire(I, "Taille (m)", f->taille.x, f->taille.y, 0.01f, 0.02f, 200.f, "L", "H");
				}
				if (f->genre == NkGenreForme2D::NK_ETOILE) {
					int32 b = f->branches;
					if (Entier(I, "Branches", b, 3, 16)) {
						f->branches = static_cast<uint8>(b < 3 ? 3 : (b > 16 ? 16 : b));
						change = true;
					}
					change |= Glissiere(I, "Rayon intérieur", f->rayonInterieur, 0.1f, 0.95f);
				}
				if (f->genre == NkGenreForme2D::NK_POLYGONE_REGULIER) {
					int32 n = f->cotes;
					if (Entier(I, "Côtés", n, 3, 32)) {
						f->cotes = static_cast<uint8>(n < 3 ? 3 : (n > 32 ? 32 : n));
						change = true;
					}
				}
				if (f->genre == NkGenreForme2D::NK_LIGNE) {
					change |= Glissiere(I, "Épaisseur du trait (m)", f->epaisseur, 0.01f, 1.f);
				}
				if (aPoints) {
					change |= Points(I, f->genre == NkGenreForme2D::NK_LIGNE ? "Points (m)" : "Contour (m)", f->points, f->nbPoints,
									 f->genre == NkGenreForme2D::NK_LIGNE ? 2u : 3u, NK_FORME_POINTS_MAX);
				}
				Case(I, "Remplie", f->rempli);
				Teinte(I, "Remplissage", f->remplissage);
				Teinte(I, "Couleur du contour", f->couleurContour);
				Glissiere(I, "Contour (m)", f->epaisseurContour, 0.f, 0.5f);
				const bool aCoins = f->genre == NkGenreForme2D::NK_RECTANGLE || f->genre == NkGenreForme2D::NK_TRIANGLE ||
									f->genre == NkGenreForme2D::NK_ETOILE || f->genre == NkGenreForme2D::NK_POLYGONE_REGULIER ||
									f->genre == NkGenreForme2D::NK_POLYGONE_LIBRE;
				if (aCoins) {
					Glissiere(I, "Arrondi (m)", f->arrondi, 0.f, 1.f);
				}
				Glissiere(I, "Opacité", f->opacite, 0.f, 1.f);
				Entier(I, "Couche", f->couche);
				if (Case(I, "Le collisionneur suit", f->collisionSuit) && f->collisionSuit) {
					change = true;
				}
				if (change) {
					NkEditeurFormeChangee(m, I.id);
				}
				Fin(I);
			}

			/// L'ANCRAGE A L'ECRAN (HUD, document 03 §2.5) : l'ancre en grille, comme
			/// on voit le coin ; l'ecart en points ; la zone sure. Pas de case : il se
			/// retire par le menu « ⋮ ».
			void CarteAncrage(NkInspecteur &I) {
				NkAncrageEcran2D *a = I.c.m.scene.Monde().Get<NkAncrageEcran2D>(I.id);
				if (!Entete(I, NkCarteEditeur::NK_ANCRAGE, nullptr)) {
					return;
				}
				static const char *kAncres[9] = {"Haut G", "Haut", "Haut D", "Gauche", "Centre", "Droite", "Bas G", "Bas", "Bas D"};
				int32 k = a->ancre;
				if (ChoixGrille(I, "Ancre", kAncres, 9, 3, k)) {
					a->ancre = static_cast<uint8>(k);
				}
				Paire(I, "Écart (pt)", a->decalage.x, a->decalage.y, 0.5f, -100.f, 300.f);
				Case(I, "Dans la zone sûre", a->zoneSure);
				if (!a->zoneSure) {
					Ligne(I, "Au bord de l'écran : pour un fond, jamais pour un bouton.");
				}
				Fin(I);
			}

			void DessinerCarte(NkInspecteur &I, NkCarteEditeur carte) {
				switch (carte) {
					case NkCarteEditeur::NK_TRANSFORM:
						CarteTransform(I);
						break;
					case NkCarteEditeur::NK_SPRITE:
						CarteSprite(I);
						break;
					case NkCarteEditeur::NK_COLLISIONNEUR:
						CarteCollisionneur(I);
						break;
					case NkCarteEditeur::NK_CORPS:
						CarteCorps(I);
						break;
					case NkCarteEditeur::NK_CORPS_MOU:
						CarteCorpsMou(I);
						break;
					case NkCarteEditeur::NK_SOURCE:
						CarteSource(I);
						break;
					case NkCarteEditeur::NK_ANIMATION:
						CarteAnimation(I);
						break;
					case NkCarteEditeur::NK_ANIMATEUR:
						CarteAnimateur(I);
						break;
					case NkCarteEditeur::NK_HIERARCHIE:
						CarteHierarchie(I);
						break;
					case NkCarteEditeur::NK_LUMIERE:
						CarteLumiere(I);
						break;
					case NkCarteEditeur::NK_EMETTEUR:
						CarteEmetteur(I);
						break;
					case NkCarteEditeur::NK_FORME:
						CarteForme(I);
						break;
					case NkCarteEditeur::NK_ANCRAGE:
						CarteAncrage(I);
						break;
					default:
						break;
				}
			}

			/// Un choix parmi quelques valeurs, SEGMENTE (UI_SPEC §3.1) : des
			/// boutons sur une ligne, la valeur courante grisee.
			bool ChoixMonde(NkGuiContext &ctx, const char *libelle, const char *const *noms, int32 n, int32 &valeur) {
				nkgui::Text(ctx, NkString::Format("%s : %s", libelle, noms[valeur]).CStr());
				bool change = false;
				ctx.PushId(libelle);
				for (int32 i = 0; i < n; ++i) {
					if (i > 0) {
						ctx.SameLine();
					}
					ctx.BeginDisabled(i == valeur);
					if (nkgui::Button(ctx, noms[i])) {
						valeur = i;
						change = true;
					}
					ctx.EndDisabled();
				}
				ctx.PopId();
				return change;
			}

			/// Une ligne « cle : valeur » en lecture seule.
			void LigneMonde(NkGuiContext &ctx, const char *cle, const NkString &valeur) {
				nkgui::Text(ctx, NkString::Format("%s : %s", cle, valeur.CStr()).CStr());
			}

			/// Le champ Nom, en haut des Details. Le renommage est IMMEDIAT : il
			/// n'y a pas de « valider » a oublier, l'Outliner suit a la frappe.
			void ChampNom(NkEditeurCadre &c, ecs::NkEntityId id, const NkRect &r) {
				NkEditeurInterface &ui = c.ui;
				const nkgui::NkGuiInput &in = c.ctx.input;
				// Le tampon suit l'entite choisie.
				if (!(ui.nomDe == id)) {
					ui.nomDe = id;
					std::snprintf(ui.nom, sizeof(ui.nom), "%s", NomDe(c.m.scene, id).CStr());
					ui.nomFocus = false;
				}
				if (in.mouseClicked[0]) {
					ui.nomFocus = NkEditeurDans(r, in.mousePos);
				}
				// « Renommer » (menu contextuel, F2) : le focus, et tout le texte
				// choisi -- taper remplace le nom, comme partout.
				if (ui.renommerDemande) {
					ui.renommerDemande = false;
					ui.nomFocus = true;
					c.ctx.input.wantSelectAll = true;
				}
				if (ui.nomFocus && (in.KeyPressed(nkgui::NkGuiKey::Enter) || in.KeyPressed(nkgui::NkGuiKey::Escape))) {
					ui.nomFocus = false;
				}
				// Hors saisie, le tampon SUIT l'entite. ⚠️ Sans cela, un nom change
				// ailleurs (l'Outliner, en place) etait RECRIT a l'ancien des la trame
				// suivante par la ligne du bas, qui ecrit le tampon dans l'entite.
				if (!ui.nomFocus) {
					std::snprintf(ui.nom, sizeof(ui.nom), "%s", NomDe(c.m.scene, id).CStr());
				}
				c.ctx.dl.AddRectFilled(r, c.pal.champ, 2.f);
				c.ctx.dl.AddRect(r, ui.nomFocus ? c.pal.accent : c.pal.bord, 1.f, 2.f);
				editorkit::NkOverlayFieldStyle st;
				st.fond = false;
				st.bord = false;
				st.texte = c.pal.texte;
				const NkRect champ{r.x + 6.f, r.y, r.w - 8.f, r.h};
				editorkit::NkOverlayTextField(c.ctx, c.ctx.dl, c.police, champ, ui.nom, static_cast<int32>(sizeof(ui.nom)),
											  ui.nomFocus, &st);
				if (ui.nom[0] != '\0' && !(NomDe(c.m.scene, id) == NkString(ui.nom))) {
					NkEditeurRenommer(c.m, id, ui.nom);
				}
			}

			void OngletDetails(NkEditeurCadre &c, const NkRect &zone) {
				NkGuiContext &ctx = c.ctx;
				auto &dl = ctx.dl;
				c.ui.caseActif = NkRect{0.f, 0.f, 0.f, 0.f};
				c.ui.detailsTexture = NkRect{0.f, 0.f, 0.f, 0.f};
				c.ui.detailsLiseres.Clear();
				c.ui.detailsRangees.Clear();
				if (!c.m.aSelection || !c.m.scene.Monde().IsAlive(c.m.selection)) {
					// Un etat vide qui PARLE (UI_SPEC §0, regle 3) : il dit quoi faire.
					const float32 lh = renderer::NkTexteHauteurLigne(c.police, 16.f);
					const float32 cy = zone.y + zone.h * 0.35f;
					renderer::NkTexteCentre(dl, c.police, zone.x + zone.w * 0.5f, cy,
											"Sélectionnez une entité pour voir ses détails.", c.pal.attenue);
					renderer::NkTexteCentre(dl, c.petite, zone.x + zone.w * 0.5f, cy + lh + 6.f,
											"Cliquez-la dans la vue ou l'Outliner, ou créez-en une (Ctrl+E).",
											c.pal.attenue);
					return;
				}
				const ecs::NkEntityId id = c.m.selection;

				// ── L'EN-TETE D'UNREAL (2026-10-01, document 02 §5 ; Rihen : « tres
				//    beau ») : l'icone du type, la case « active », le NOM, et
				//    « + Ajouter » (le plus vert d'Unreal) a droite ─────────────────
				const float32 rangeeH = 26.f;
				const float32 lhP = renderer::NkTexteHauteurLigne(c.police, 16.f);
				const float32 lhS = renderer::NkTexteHauteurLigne(c.petite, 12.f);
				const float32 y0 = zone.y + 8.f;
				const float32 wA = renderer::NkTexteLargeur(c.police, "Ajouter") + 36.f;
				const NkRect ajouter{zone.x + zone.w - 10.f - wA, y0, wA, rangeeH};
				c.ui.detailsAjouter = ajouter;
				{
					const bool ouvertA = c.ui.menu == NkMenuEditeur::NK_COMPOSANT;
					if (NkEditeurBouton(c, ajouter, "", ouvertA, true)) {
						NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_COMPOSANT, ajouter);
					}
					const NkColor vert = editorkit::NkThemeUnpack(c.theme.GetOuRepli(editorkit::NkRole::StatusOk, editorkit::NkRole::AccentUi));
					const float32 px = ajouter.x + 15.f, py = ajouter.y + rangeeH * 0.5f;
					dl.AddRectFilled(NkRect{px - 5.f, py - 1.1f, 10.f, 2.2f}, vert, 1.f);
					dl.AddRectFilled(NkRect{px - 1.1f, py - 5.f, 2.2f, 10.f}, vert, 1.f);
					renderer::NkTexte(dl, c.police, ajouter.x + 26.f, ajouter.y + (rangeeH - lhP) * 0.5f, "Ajouter",
									  ouvertA ? c.pal.surAccent : c.pal.texte);
				}
				// L'icone du TYPE (l'acteur), comme le cube d'Unreal devant « Floor ».
				const NkRect icone{zone.x + 10.f, y0 + (rangeeH - 18.f) * 0.5f, 18.f, 18.f};
				IconeActeur(dl, icone, c.pal.texte, c.pal.accent);
				// (2026-10-01) LA CASE « ACTIVE », a gauche du nom (GameObject.active
				// de Unity) : decochee, l'entite et sa descendance ne sont ni rendues,
				// ni simulees, ni animees -- en edition comme en jeu, sauve dans la
				// scene et les prefabs (NkUnkenyActif.h). Ctrl+Z l'annule.
				// ⚠️ CE N'EST PAS L'OEIL DE L'OUTLINER : l'oeil ne cache qu'en EDITION et
				//    le jeu montre tout ; la case, elle, change LE JEU. Les deux restent.
				const NkRect caseR{icone.x + icone.w + 8.f, y0 + (rangeeH - 16.f) * 0.5f, 16.f, 16.f};
				c.ui.caseActif = caseR;
				{
					const bool soi = c.m.scene.EstActiveSoi(id);
					const bool enEffet = c.m.scene.EstActive(id);
					const bool survol = NkEditeurDans(caseR, ctx.input.mousePos);
					dl.AddRectFilled(caseR, c.pal.champ, 3.f);
					dl.AddRect(caseR, survol ? c.pal.accent : c.pal.bord, 1.f, 3.f);
					if (soi) {
						// Cochee mais eteinte PAR UN PARENT : la coche, attenuee -- on voit
						// qu'elle est a soi, et que ce n'est pas ici que ca se rallume.
						const NkColor coche = enEffet ? c.pal.accent : c.pal.attenue;
						const float32 cy = caseR.y + caseR.h * 0.5f;
						dl.AddLine(NkVec2{caseR.x + 3.f, cy}, NkVec2{caseR.x + 6.5f, cy + 3.5f}, coche, 2.f);
						dl.AddLine(NkVec2{caseR.x + 6.5f, cy + 3.5f}, NkVec2{caseR.x + 12.5f, cy - 3.5f}, coche, 2.f);
					}
					if (survol && ctx.input.mouseClicked[0]) {
						NkEditeurActiverEntite(c.m, id, !soi);
					}
				}
				const NkRect nomR{caseR.x + caseR.w + 8.f, y0, ajouter.x - 8.f - (caseR.x + caseR.w + 8.f), rangeeH};
				ChampNom(c, id, nomR);
				NkString type = NkString::Format("type : %s", NkEditeurTypeDe(c.m.scene, id));
				if (!c.m.scene.EstActiveSoi(id)) {
					type.Append("  —  désactivée (ni rendue, ni simulée)");
				} else if (!c.m.scene.EstActive(id)) {
					type.Append("  —  désactivée par un parent");
				}
				renderer::NkTexte(dl, c.petite, nomR.x, y0 + rangeeH + 3.f, type.CStr(), c.pal.attenue, zone.x + zone.w - 10.f - nomR.x);
				float32 y = y0 + rangeeH + 3.f + lhS + 8.f;

				// La RECHERCHE a change : toutes les cartes se redessinent une trame
				// (leurs rangees qui repondent les marquent), puis celles sans
				// reponse se taisent.
				const NkString recherche(c.ui.detailsRecherche);
				const bool cherche = !recherche.Empty();
				const bool nouvelle = !(recherche == c.ui.detailsRechercheVue);
				if (nouvelle) {
					c.ui.detailsCartesTrouvees = 0u;
					c.ui.detailsRechercheVue = recherche;
				}
				c.ui.detailsCartesDessinees = 0u;
				c.ui.detailsRemises = 0u;
				auto Montrer = [&](NkCarteEditeur carte) {
					const int32 k = static_cast<int32>(carte);
					if (c.ui.detailsComposant >= 0 && c.ui.detailsComposant != k) {
						return false;
					}
					if (c.ui.detailsCategorie != 0 && CategorieDe(carte) != c.ui.detailsCategorie) {
						return false;
					}
					const bool nomRepond = cherche && NkEditeurContientSansCasse(NkCarteEditeurNom(carte), recherche.CStr());
					if (cherche && !nouvelle && !nomRepond && (c.ui.detailsCartesTrouvees & (1u << static_cast<uint32>(k))) == 0u) {
						return false;
					}
					gCarte = k;
					gCarteRepond = nomRepond;
					c.ui.detailsCartesDessinees |= 1u << static_cast<uint32>(k);
					return true;
				};

				// ── L'arbre, la recherche, les pastilles et les cartes, dans une zone
				//    DEFILABLE : a petite hauteur, les proprietes gardent la place ──
				const float32 haut = y;
				const NkRect corps{zone.x, haut, zone.w, zone.y + zone.h - haut};
				if (!nkgui::BeginChild(ctx, "details.composants", corps, false)) {
					return;
				}
				gRecherche = c.ui.detailsRecherche;
				// Chaque entite garde ses propres identifiants de champs : sans
				// cette cle, un glisser commence sur une caisse finirait sur une autre.
				char cle[24];
				std::snprintf(cle, sizeof(cle), "e%llu", static_cast<unsigned long long>(id.Pack()));
				ctx.PushId(cle);
				// Les rangees d'une carte se TOUCHENT : l'espacement vertical de NKGui
				// y ouvrirait des fentes (et casserait le bord des cartes).
				const float32 espacement = ctx.layout.itemSpacingY;
				ctx.layout.itemSpacingY = 0.f;
				// ── L'ARBRE DES COMPOSANTS (Unreal : « Floor (Instance) » puis ses
				//    composants) : un clic n'affiche que ce composant, l'acteur les
				//    montre tous ─────────────────────────────────────────────────────
				auto &dc = ctx.DL();
				{
					NkVector<int32> cartes;
					cartes.PushBack(-1);
					if (c.m.scene.Monde().Has<NkTransform2D>(id)) {
						cartes.PushBack(static_cast<int32>(NkCarteEditeur::NK_TRANSFORM));
					}
					for (uint32 k = 0; k < static_cast<uint32>(NkCarteEditeur::NK_COUNT); ++k) {
						const NkCarteEditeur carte = static_cast<NkCarteEditeur>(c.ui.ordreCartes[k]);
						if (carte != NkCarteEditeur::NK_TRANSFORM && NkEditeurAUneCarte(c.m, id, carte)) {
							cartes.PushBack(static_cast<int32>(carte));
						}
					}
					// --details=NOM : le composant demande au demarrage.
					if (!c.ui.demDetails.Empty()) {
						for (uint32 k = 1; k < cartes.Size(); ++k) {
							if (c.ui.demDetails == NkString(NkCarteEditeurNom(static_cast<NkCarteEditeur>(cartes[k])))) {
								c.ui.detailsComposant = cartes[k];
							}
						}
						c.ui.demDetails = NkString();
					}
					bool present = c.ui.detailsComposant < 0;
					for (uint32 k = 1; k < cartes.Size(); ++k) {
						present = present || cartes[k] == c.ui.detailsComposant;
					}
					if (!present) {
						c.ui.detailsComposant = -1;
					}
					constexpr float32 LIGNE = 22.f;
					// QUATRE lignes visibles au plus (Unreal garde l'arbre court) ; la
					// molette fait defiler le reste.
					const uint32 total = static_cast<uint32>(cartes.Size());
					const uint32 n = total < 4u ? total : 4u;
					const int32 maxDefil = static_cast<int32>(total - n);
					c.ui.detailsArbreDefil = c.ui.detailsArbreDefil > maxDefil ? maxDefil : (c.ui.detailsArbreDefil < 0 ? 0 : c.ui.detailsArbreDefil);
					const NkRect r0 = ctx.NextItemRect(0.f, LIGNE * static_cast<float32>(n) + 6.f + 8.f);
					const NkRect boite{r0.x + 4.f, r0.y, r0.w - 8.f, LIGNE * static_cast<float32>(n) + 6.f};
					dc.AddRectFilled(boite, Melange(c.pal.panneau, c.pal.fond, 0.55f), 3.f);
					dc.AddRect(boite, c.pal.bord, 1.f, 3.f);
					c.ui.detailsArbre.Clear();
					c.ui.detailsArbreCartes.Clear();
					if (maxDefil > 0 && NkEditeurDans(boite, ctx.input.mousePos) && ctx.input.wheel != 0.f) {
						c.ui.detailsArbreDefil -= ctx.input.wheel > 0.f ? 1 : -1;
						c.ui.detailsArbreDefil = c.ui.detailsArbreDefil > maxDefil ? maxDefil : (c.ui.detailsArbreDefil < 0 ? 0 : c.ui.detailsArbreDefil);
					}
					for (uint32 v = 0; v < n; ++v) {
						const uint32 k = v + static_cast<uint32>(c.ui.detailsArbreDefil);
						const NkRect ligne{boite.x + 3.f, boite.y + 3.f + static_cast<float32>(v) * LIGNE, boite.w - 6.f, LIGNE};
						const bool choisie = cartes[k] == c.ui.detailsComposant;
						const bool survol = NkEditeurDans(ligne, ctx.input.mousePos) && NkEditeurDans(corps, ctx.input.mousePos);
						if (choisie) {
							dc.AddRectFilled(ligne, Melange(c.pal.accent, c.pal.panneau, 0.2f), 2.f);
						} else if (survol) {
							dc.AddRectFilled(ligne, c.pal.boutonSurvol, 2.f);
						}
						const float32 retrait = k == 0 ? 6.f : 24.f;
						const NkRect ic{ligne.x + retrait, ligne.y + (LIGNE - 16.f) * 0.5f, 16.f, 16.f};
						NkString libelle;
						if (k == 0) {
							IconeActeur(dc, ic, c.pal.texte, c.pal.accent);
							libelle = NomDe(c.m.scene, id) + " (Instance)";
						} else {
							IconeCarte(dc, static_cast<NkCarteEditeur>(cartes[k]), ic, c.pal.texte);
							libelle = NkString(NkCarteEditeurNom(static_cast<NkCarteEditeur>(cartes[k])));
						}
						const NkColor tl = choisie ? c.pal.surAccent : c.pal.texte;
						dc.PushClipRect(ligne, true);
						renderer::NkTexte(dc, c.police, ic.x + ic.w + 7.f, ligne.y + (LIGNE - lhP) * 0.5f, libelle.CStr(), tl);
						dc.PopClipRect();
						if (survol && ctx.input.mouseClicked[0]) {
							c.ui.detailsComposant = cartes[k];
						}
						c.ui.detailsArbre.PushBack(ligne);
						c.ui.detailsArbreCartes.PushBack(cartes[k]);
					}
					if (maxDefil > 0) {
						const float32 hb = boite.h - 6.f;
						const float32 hp = hb * static_cast<float32>(n) / static_cast<float32>(total);
						const float32 yp = boite.y + 3.f + (hb - hp) * static_cast<float32>(c.ui.detailsArbreDefil) / static_cast<float32>(maxDefil);
						dc.AddRectFilled(NkRect{boite.x + boite.w - 5.f, yp, 3.f, hp}, c.pal.attenue, 1.5f);
					}
				}

				// ── LA RECHERCHE dans les proprietes, puis les PASTILLES de
				//    categorie (Unreal : General, Acteur, ... et « Tout ») ────────────
				{
					static const int32 kOrdre[7] = {1, 2, 3, 4, 5, 6, 0};
					const float32 ph = 20.f;
					const NkRect rz = ctx.NextItemRect(0.f, 1.f); // la largeur de la zone
					// Les lignes de pastilles, MESUREES avant de reserver la place.
					int32 lignes = 1;
					{
						float32 x = rz.x + 4.f;
						for (int32 k = 0; k < 7; ++k) {
							const float32 w = renderer::NkTexteLargeur(c.petite, kCategories[kOrdre[k]]) + 18.f;
							if (x + w > rz.x + rz.w - 4.f && x > rz.x + 4.f) {
								x = rz.x + 4.f;
								++lignes;
							}
							x += w + 4.f;
						}
					}
					const NkRect rb = ctx.NextItemRect(0.f, 24.f + 6.f + static_cast<float32>(lignes) * (ph + 4.f) + 6.f);
					float32 y = rb.y;
					const NkRect rr{rb.x + 4.f, y, rb.w - 8.f, 24.f};
					c.ui.detailsRechercheRect = rr;
					const nkgui::NkGuiInput &in = ctx.input;
					if (in.mouseClicked[0]) {
						c.ui.detailsRechercheFocus = NkEditeurDans(rr, in.mousePos) && NkEditeurDans(corps, in.mousePos);
					}
					if (c.ui.detailsRechercheFocus && in.KeyPressed(nkgui::NkGuiKey::Escape)) {
						c.ui.detailsRecherche[0] = '\0';
						c.ui.detailsRechercheFocus = false;
						c.ui.toucheChamp = true;
					} else if (c.ui.detailsRechercheFocus && in.KeyPressed(nkgui::NkGuiKey::Enter)) {
						c.ui.detailsRechercheFocus = false;
						c.ui.toucheChamp = true;
					}
					dc.AddRectFilled(rr, c.pal.champ, 3.f);
					dc.AddRect(rr, c.ui.detailsRechercheFocus ? c.pal.accent : c.pal.bord, 1.f, 3.f);
					const float32 lx = rr.x + 12.f, ly = rr.y + rr.h * 0.5f - 1.f;
					dc.AddCircle(NkVec2{lx, ly}, 4.5f, c.pal.attenue, 1.3f);
					dc.AddLine(NkVec2{lx + 3.2f, ly + 3.2f}, NkVec2{lx + 6.5f, ly + 6.5f}, c.pal.attenue, 1.6f);
					if (c.ui.detailsRecherche[0] == '\0' && !c.ui.detailsRechercheFocus) {
						renderer::NkTexte(dc, c.petite, rr.x + 24.f, rr.y + (rr.h - lhS) * 0.5f, "Rechercher une propriété", c.pal.attenue);
					}
					editorkit::NkOverlayFieldStyle st;
					st.fond = false;
					st.bord = false;
					st.texte = c.pal.texte;
					st.utf8 = true;
					editorkit::NkOverlayTextField(ctx, dl, c.police, NkRect{rr.x + 20.f, rr.y, rr.w - 24.f, rr.h}, c.ui.detailsRecherche,
												  static_cast<int32>(sizeof(c.ui.detailsRecherche)), c.ui.detailsRechercheFocus, &st);
					y += rr.h + 6.f;
					// Les pastilles, en flux (elles passent a la ligne comme celles
					// d'Unreal) ; « Tout » en dernier.
					float32 x = rb.x + 4.f;
					for (int32 k = 0; k < 7; ++k) {
						const int32 cat = kOrdre[k];
						const float32 w = renderer::NkTexteLargeur(c.petite, kCategories[cat]) + 18.f;
						if (x + w > rb.x + rb.w - 4.f && x > rb.x + 4.f) {
							x = rb.x + 4.f;
							y += ph + 4.f;
						}
						const NkRect r{x, y, w, ph};
						c.ui.detailsPastilles[cat] = r;
						const bool sel = c.ui.detailsCategorie == cat;
						if (NkEditeurBouton(c, r, "", sel, NkEditeurDans(corps, ctx.input.mousePos), &dc)) {
							c.ui.detailsCategorie = cat;
						}
						renderer::NkTexteDansBoite(dc, c.petite, r, kCategories[cat], sel ? c.pal.surAccent : c.pal.texte);
						x += w + 4.f;
					}
				}

				NkInspecteur I{c, id, corps, Melange(c.pal.entete, c.pal.texte, 0.05f), Melange(c.pal.entete, c.pal.texte, 0.12f),
							   Melange(c.pal.panneau, c.pal.entete, 0.45f), c.pal.bord};
				// Le Transform en tete, toujours ; les autres dans l'ordre choisi
				// (Monter / Descendre du menu « ⋮ »).
				if (Montrer(NkCarteEditeur::NK_TRANSFORM)) {
					CarteTransform(I);
				}
				for (uint32 k = 0; k < static_cast<uint32>(NkCarteEditeur::NK_COUNT); ++k) {
					const NkCarteEditeur carte = static_cast<NkCarteEditeur>(c.ui.ordreCartes[k]);
					if (carte == NkCarteEditeur::NK_TRANSFORM || !NkEditeurAUneCarte(c.m, id, carte) || !Montrer(carte)) {
						continue;
					}
					ctx.PushId(NkCarteEditeurNom(carte));
					DessinerCarte(I, carte);
					ctx.PopId();
				}
				gCarte = -1;
				gCarteRepond = false;
				gRecherche = nullptr;
				// (2026-10-01, R33) RIEN HORS CADRE : l'ancrage a l'ecran, la forme 2D et
				// la collision, qui s'affichaient en blocs bruts ICI (fusion du 01/10),
				// sont des CARTES (CarteAncrage, CarteForme, CarteCollisionneur et
				// CarteCorps), rangees dans leurs pastilles (Acteur, Rendu, Physique).
				// ── « Ajouter un composant », en bas (Unity) ─────────────────
				Espace(ctx, 10.f);
				const NkRect r0 = ctx.NextItemRect(0.f, 28.f);
				const NkRect ajout{r0.x + r0.w * 0.12f, r0.y, r0.w * 0.76f, 26.f};
				const bool ouvert = c.ui.menu == NkMenuEditeur::NK_COMPOSANT;
				if (NkEditeurBouton(c, ajout, "", ouvert, NkEditeurDans(corps, ctx.input.mousePos), &ctx.DL())) {
					NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_COMPOSANT, ajout);
				}
				renderer::NkTexteDansBoite(ctx.DL(), c.police, ajout, "Ajouter un composant", ouvert ? c.pal.surAccent : c.pal.texte);
				Espace(ctx, 14.f);
				ctx.layout.itemSpacingY = espacement;
				ctx.PopId();
				nkgui::EndChild(ctx);
				// ── LA CLOISON DES NOMS, qui se TIRE (Unreal) : la colonne des
				//    libelles de toutes les cartes suit ─────────────────────────────
				{
					const nkgui::NkGuiInput &in = ctx.input;
					const float32 xc = c.ui.detailsCloisonX;
					const bool sur = xc > 0.f && NkEditeurDans(corps, in.mousePos) && in.mousePos.x >= xc - 3.f && in.mousePos.x <= xc + 3.f;
					if (sur && in.mouseClicked[0] && c.ui.menu == NkMenuEditeur::NK_AUCUN) {
						c.ui.detailsCloison = true;
					}
					if (!in.mouseDown[0]) {
						c.ui.detailsCloison = false;
					}
					if (sur || c.ui.detailsCloison) {
						ctx.wantCursor = nkgui::NkGuiCursor::ResizeEW;
					}
					if (c.ui.detailsCloison && gRangeeW > 1.f) {
						float32 f = (in.mousePos.x + 4.f - gRangeeX) / gRangeeW;
						f = f < 0.25f ? 0.25f : (f > 0.65f ? 0.65f : f);
						c.ui.detailsColonne = f;
					}
				}
			}

			void OngletMonde(NkEditeurCadre &c, const NkRect &zone) {
				NkGuiContext &ctx = c.ctx;
				NkEditeurModele &m = c.m;
				const NkRect corps{zone.x, zone.y + 4.f, zone.w, zone.h - 4.f};
				if (!nkgui::BeginChild(ctx, "details.monde", corps, false)) {
					return;
				}
				physics::NkParticules2D *p = m.scene.Particules();
				// (2026-10-01, R33 point 6) L'INTERRUPTEUR DE L'ECLAIRAGE, en tete du
				// Monde : deux boutons segmentes, « Allumé » / « Éteint » (le reglage
				// fin reste plus bas, section Éclairage 2D).
				{
					const bool actif = m.scene.Eclairage().actif;
					auto &dlm = ctx.DL();
					const NkRect r0 = ctx.NextItemRect(0.f, 34.f);
					const NkRect r{r0.x + 2.f, r0.y + 2.f, r0.w - 4.f, 30.f};
					dlm.AddRectFilled(r, c.pal.entete, 4.f);
					const float32 lw = renderer::NkTexteLargeur(c.police, "Éclairage de la scène") + 20.f;
					renderer::NkTexte(dlm, c.police, r.x + 10.f, r.y + (r.h - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f,
									  "Éclairage de la scène", c.pal.texte);
					const float32 bw = (r.w - lw - 10.f) * 0.5f;
					const NkRect on{r.x + lw, r.y + 3.f, bw - 2.f, r.h - 6.f};
					const NkRect off{r.x + lw + bw, r.y + 3.f, bw - 2.f, r.h - 6.f};
					c.ui.boutonEclairageMonde = actif ? off : on;
					const bool dedans = NkEditeurDans(corps, ctx.input.mousePos);
					if (NkEditeurBouton(c, on, "", actif, dedans, &dlm) && !actif) {
						NkEditeurExecuter(c, NK_A_ECLAIRAGE);
					}
					renderer::NkTexteDansBoite(dlm, c.petite, on, "Allumé", actif ? c.pal.surAccent : c.pal.texte);
					if (NkEditeurBouton(c, off, "", !actif, dedans, &dlm) && actif) {
						NkEditeurExecuter(c, NK_A_ECLAIRAGE);
					}
					renderer::NkTexteDansBoite(dlm, c.petite, off, "Éteint", !actif ? c.pal.surAccent : c.pal.texte);
				}
				nkgui::Text(ctx, "Propriétés de la scène");
				nkgui::Separator(ctx);
				if (p != nullptr) {
					physics::NkReglagesP2D &r = p->reglages;
					// La gravite est UNE : celle des rigides suit celle des particules.
					float32 g = r.gravite.y;
					if (nkgui::SliderFloat(ctx, "gravite (m/s2)", g, -30.f, 10.f)) {
						r.gravite.y = g;
						if (m.scene.MondePhysique() != nullptr) {
							m.scene.MondePhysique()->SetGravity(math::NkVec3f(r.gravite.x, g, 0.f));
						}
					}
					nkgui::SliderFloat(ctx, "temperature", r.temperature, 0.f, 100.f);
					nkgui::SliderFloat(ctx, "vent (m/s2)", r.vent, -20.f, 20.f);
					nkgui::SliderFloat(ctx, "frein de l'air (1/s)", r.amortAir, 0.f, 3.f);
					nkgui::InputInt(ctx, "sous-pas", r.sousPas);
					r.sousPas = r.sousPas < 1 ? 1 : (r.sousPas > 32 ? 32 : r.sousPas);
					nkgui::Checkbox(ctx, "collisions de la matiere", r.collisions);
					nkgui::Checkbox(ctx, "soudure (atomes, blobs)", r.soudure);
					nkgui::Checkbox(ctx, "boite de la matiere", r.limites.actif);
					nkgui::Separator(ctx);
					LigneMonde(ctx, "matiere",
						  NkString::Format("%u particules, %u liens, %u ruptures", static_cast<unsigned>(p->particules.Size()),
										   static_cast<unsigned>(p->LiensActifs()), static_cast<unsigned>(p->rupturesTotal)));
				} else {
					nkgui::Text(ctx, "(cette scene n'a pas de matiere)");
				}
				LigneMonde(ctx, "pas fixe",
					  NkString::Format("%.4f s (%d max par trame)", static_cast<double>(m.scene.Config().pasFixe),
									   m.scene.Config().pasMaxParTrame));
				nkgui::Separator(ctx);
				nkgui::Text(ctx, "Affichage de la matière");
				static const char *kModes[4] = {"Eclaire", "Filaire", "Contraintes", "Vitesse"};
				int32 mode = static_cast<int32>(m.rendu.mode);
				if (ChoixMonde(ctx, "mode", kModes, 4, mode)) {
					m.rendu.mode = static_cast<NkModeRenduParticules>(mode);
				}
				nkgui::Checkbox(ctx, "liens", m.rendu.liens);
				nkgui::Checkbox(ctx, "particules", m.rendu.particules);
				nkgui::Checkbox(ctx, "vitesses", m.rendu.vitesses);
				NkEditeurSectionEclairageMonde(c); // 2026-09-30 : NkEditeurLumiere.cpp
				nkgui::Separator(ctx);
				// L'appareil simule : orientation, interrupteurs, provenance, et
				// l'appareil personnalise (2026-10-01, NkEditeurAppareilsUi.cpp).
				NkEditeurSectionAppareil(c);
				// Les calques de collision (2026-10-01, NkEditeurPlacer.h).
				NkEditeurSectionCalques(c);
				nkgui::EndChild(ctx);
			}

		} // namespace

		void NkEditeurDeplacerCarte(NkEditeurCadre &c, int32 sens) {
			NkEditeurInterface &ui = c.ui;
			const int32 n = static_cast<int32>(NkCarteEditeur::NK_COUNT);
			if (!c.m.aSelection || ui.carteMenu <= 0 || ui.carteMenu >= n || (sens != -1 && sens != 1)) {
				return; // le Transform (0) reste en tete
			}
			int32 pos = -1;
			for (int32 k = 0; k < n; ++k) {
				if (ui.ordreCartes[k] == static_cast<uint8>(ui.carteMenu)) {
					pos = k;
				}
			}
			// Le voisin VISIBLE dans ce sens : echanger avec une carte que
			// l'entite n'a pas ne changerait rien a l'ecran.
			int32 j = pos + sens;
			while (j >= 0 && j < n &&
				   (ui.ordreCartes[j] == 0u || !NkEditeurAUneCarte(c.m, c.m.selection, static_cast<NkCarteEditeur>(ui.ordreCartes[j])))) {
				j += sens;
			}
			if (pos < 0 || j < 0 || j >= n) {
				return;
			}
			const uint8 t = ui.ordreCartes[pos];
			ui.ordreCartes[pos] = ui.ordreCartes[j];
			ui.ordreCartes[j] = t;
		}

		void NkEditeurDessinerDetails(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			const NkRect zone = ui.details;
			if (!ui.voirDetails || zone.w < 8.f || zone.h < 60.f) {
				return;
			}
			c.ctx.dl.AddRectFilled(zone, c.pal.panneau);
			static const char *kOnglets[2] = {"Détails", "Monde"};
			const float32 ongletsH = 26.f;
			NkEditeurOnglets(c, NkRect{zone.x, zone.y, zone.w, ongletsH}, kOnglets, 2, ui.ongletDroite);
			const NkRect contenu{zone.x, zone.y + ongletsH, zone.w, zone.h - ongletsH};
			c.ctx.dl.PushClipRect(contenu, true);
			if (ui.ongletDroite == 0) {
				OngletDetails(c, contenu);
			} else {
				OngletMonde(c, contenu);
			}
			c.ctx.dl.PopClipRect();
		}

	} // namespace editeur
} // namespace nkentseu
