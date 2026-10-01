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

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/NkEditorTextField.h"
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

			/// La colonne des LIBELLES, en fraction de la largeur d'une carte.
			constexpr float32 LIBELLE_FRAC = 0.40f;
			constexpr float32 RANG_H = 22.f;   ///< une rangee
			constexpr float32 ENTETE_H = 26.f; ///< l'en-tete d'une carte

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
				renderer::NkTexte(dl, I.c.police, x, ty, NkCarteEditeurNom(carte), texte);
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
			void Rangee(NkInspecteur &I, const char *libelle, float32 h, NkRect &champ, NkRect &lib) {
				NkGuiContext &ctx = I.c.ctx;
				auto &dl = ctx.DL();
				const NkRect r0 = ctx.NextItemRect(0.f, h);
				const NkRect r{r0.x + 4.f, r0.y, r0.w - 8.f, r0.h};
				dl.AddRectFilled(r, I.fondCorps);
				dl.AddRectFilled(NkRect{r.x, r.y, 1.f, r.h}, I.bord);
				dl.AddRectFilled(NkRect{r.x + r.w - 1.f, r.y, 1.f, r.h}, I.bord);
				const float32 colonne = r.w * LIBELLE_FRAC;
				lib = NkRect{r.x + 12.f, r.y, colonne - 16.f, r.h};
				champ = NkRect{r.x + colonne, r.y + 2.f, r.w - colonne - 8.f, r.h - 4.f};
				if (libelle != nullptr && libelle[0] != '\0') {
					const float32 ty = r.y + (r.h - renderer::NkTexteHauteurLigne(I.c.police, 16.f)) * 0.5f;
					dl.PushClipRect(lib, true);
					renderer::NkTexte(dl, I.c.police, lib.x, ty, libelle, I.c.pal.texte);
					dl.PopClipRect();
				}
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
				Rangee(I, libelle, RANG_H, champ, lib);
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
				Rangee(I, libelle, RANG_H, champ, lib);
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
				Rangee(I, libelle, RANG_H, champ, lib);
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
				Rangee(I, libelle, RANG_H, champ, lib);
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
				Rangee(I, libelle, RANG_H, champ, lib);
				float32 col[4] = {static_cast<float32>((rgba >> 24) & 0xFFu) / 255.f, static_cast<float32>((rgba >> 16) & 0xFFu) / 255.f,
								  static_cast<float32>((rgba >> 8) & 0xFFu) / 255.f, static_cast<float32>(rgba & 0xFFu) / 255.f};
				ctx.PushId(libelle);
				ctx.SetNextItemRect(champ);
				const bool change = nkgui::ColorEdit4(ctx, "##v", col);
				ctx.PopId();
				if (!change) {
					return false;
				}
				auto o = [](float32 v) { return static_cast<uint32>((v < 0.f ? 0.f : (v > 1.f ? 1.f : v)) * 255.f + 0.5f); };
				rgba = (o(col[0]) << 24) | (o(col[1]) << 16) | (o(col[2]) << 8) | o(col[3]);
				return true;
			}

			/// Deux nombres sur une rangee (position, taille...), chacun avec sa
			/// lettre -- rouge X, verte Y, comme les axes -- qui se TIRE elle aussi.
			/// Le bouton « remettre » au bout d'une rangee (la fleche d'Unreal) ; il
			/// prend sa place a DROITE du champ, qui retrecit d'autant.
			bool Remettre(NkInspecteur &I, NkRect &champ) {
				const NkRect r{champ.x + champ.w - 20.f, champ.y, 20.f, champ.h};
				champ.w -= 24.f;
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

			bool Paire(NkInspecteur &I, const char *libelle, float32 &a, float32 &b, float32 pas, float32 vmin = -1.0e9f,
					   float32 vmax = 1.0e9f, const char *la = "X", const char *lb = "Y", bool *remise = nullptr) {
				NkGuiContext &ctx = I.c.ctx;
				auto &dl = ctx.DL();
				NkRect champ, lib;
				Rangee(I, libelle, RANG_H, champ, lib);
				if (remise != nullptr) {
					*remise = Remettre(I, champ);
				}
				ctx.PushId(libelle);
				bool change = false;
				const float32 moitie = (champ.w - 4.f) * 0.5f;
				for (int32 k = 0; k < 2; ++k) {
					float32 &v = k == 0 ? a : b;
					const NkRect cellule{champ.x + static_cast<float32>(k) * (moitie + 4.f), champ.y, moitie, champ.h};
					const NkRect lettre{cellule.x, cellule.y, 14.f, cellule.h};
					const NkColor col = k == 0 ? NkColor{220, 95, 95, 255} : NkColor{105, 200, 115, 255};
					renderer::NkTexteDansBoite(dl, I.c.petite, lettre, k == 0 ? la : lb, col);
					ctx.PushId(k == 0 ? "a" : "b");
					change |= Frotter(I, lettre, v, pas, vmin, vmax);
					ctx.SetNextItemRect(NkRect{cellule.x + 15.f, cellule.y, cellule.w - 15.f, cellule.h});
					change |= nkgui::DragFloat(ctx, "##v", v, pas, vmin, vmax);
					ctx.PopId();
				}
				ctx.PopId();
				return change;
			}

			/// UNE composante avec sa lettre coloree -- la rotation d'un Transform :
			/// Z, bleue, comme l'axe qui sort de l'ecran.
			bool Unique(NkInspecteur &I, const char *libelle, float32 &v, float32 pas, float32 vmin, float32 vmax, const char *lettre,
						const NkColor &couleur, bool *remise) {
				NkGuiContext &ctx = I.c.ctx;
				NkRect champ, lib;
				Rangee(I, libelle, RANG_H, champ, lib);
				if (remise != nullptr) {
					*remise = Remettre(I, champ);
				}
				ctx.PushId(libelle);
				const NkRect rl{champ.x, champ.y, 14.f, champ.h};
				renderer::NkTexteDansBoite(ctx.DL(), I.c.petite, rl, lettre, couleur);
				bool change = Frotter(I, rl, v, pas, vmin, vmax);
				ctx.SetNextItemRect(NkRect{champ.x + 15.f, champ.y, champ.w - 15.f, champ.h});
				change |= nkgui::DragFloat(ctx, "##v", v, pas, vmin, vmax);
				ctx.PopId();
				return change;
			}

			/// Un choix parmi quelques valeurs, SEGMENTE (UI_SPEC §3.1).
			bool Choix(NkInspecteur &I, const char *libelle, const char *const *noms, int32 n, int32 &valeur) {
				NkRect champ, lib;
				Rangee(I, libelle, RANG_H, champ, lib);
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
				Rangee(I, libelle, RANG_H, champ, lib);
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
				Rangee(I, "", RANG_H - 4.f, champ, lib);
				auto &dl = I.c.ctx.DL();
				const float32 ty = lib.y + (lib.h - renderer::NkTexteHauteurLigne(I.c.petite, 12.f)) * 0.5f;
				renderer::NkTexte(dl, I.c.petite, lib.x, ty, texte, vif ? I.c.pal.texte : I.c.pal.attenue);
			}

			/// Un ou deux boutons sur la largeur de la carte ; rend celui clique
			/// (0, 1) ou -1.
			int32 Boutons(NkInspecteur &I, const char *a, const char *b = nullptr, bool actifA = true, bool actifB = true) {
				NkRect champ, lib;
				Rangee(I, "", RANG_H + 4.f, champ, lib);
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
				if (Paire(I, "Position (m)", x, y, 0.02f, -1.0e9f, 1.0e9f, "X", "Y", &remise)) {
					NkEditeurDeplacer(m, NkVec2f(x, y));
				}
				if (remise) {
					NkEditeurDeplacer(m, NkVec2f(0.f, 0.f));
				}
				t = m.scene.Monde().Get<NkTransform2D>(I.id);
				if (!m.scene.Monde().Has<NkCorpsMou2D>(I.id)) {
					float32 rot = t->rotation * 57.2957795f;
					remise = false;
					const bool tourne = Unique(I, "Rotation (°)", rot, 0.5f, -360.f, 360.f, "Z", NkColor{95, 150, 235, 255}, &remise);
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
				remise = false;
				if (Paire(I, "Échelle", e.x, e.y, 0.01f, 0.05f, 100.f, "X", "Y", &remise)) {
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
				Info(I, "Texture", s->texId != 0u ? I.c.m.textures.Nom(s->texId) : "(aucune)");
				Fin(I);
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
				bool change = false;
				static const char *kFormes[3] = {"Cercle", "Boîte", "Capsule"};
				int32 f = static_cast<int32>(col->forme);
				if (Choix(I, "Forme", kFormes, 3, f)) {
					col->forme = static_cast<NkForme2D>(f);
					change = true;
				}
				if (col->forme == NkForme2D::NK_BOITE) {
					change |= Paire(I, "Demi-taille (m)", col->demiTaille.x, col->demiTaille.y, 0.01f, 0.02f, 50.f, "L", "H");
				} else {
					change |= Nombre(I, "Rayon (m)", col->rayon, 0.01f, 0.02f, 20.f);
					if (col->forme == NkForme2D::NK_CAPSULE) {
						change |= Nombre(I, "Demi-longueur (m)", col->demiTaille.x, 0.01f, 0.f, 50.f);
					}
				}
				change |= Paire(I, "Décalage (m)", col->decalage.x, col->decalage.y, 0.01f);
				change |= Case(I, "Déclencheur (zone)", col->declencheur);
				// ⚠️ La forme vit AUSSI dans le solveur : on refait le corps.
				if (change && m.scene.Monde().Has<NkCorps2D>(I.id)) {
					m.scene.ActualiserCorps(I.id);
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
					Ligne(I, "(modèle non enregistré : NkEnregistrerModeleAnimateur)");
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
						Rangee(I, p.nom, RANG_H, champ, lib);
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

				// ── La case « active », le nom ; la nature dessous ───────────
				// « Ajouter » est en BAS des cartes, comme dans Unity : on ajoute
				// apres avoir vu ce que l'entite porte deja.
				const float32 rangeeH = 24.f;
				// (2026-10-01) LA CASE « ACTIVE », a gauche du nom (GameObject.active
				// de Unity) : decochee, l'entite et sa descendance ne sont ni rendues,
				// ni simulees, ni animees -- en edition comme en jeu, sauve dans la
				// scene et les prefabs (NkUnkenyActif.h). Ctrl+Z l'annule.
				// ⚠️ CE N'EST PAS L'OEIL DE L'OUTLINER : l'oeil ne cache qu'en EDITION et
				//    le jeu montre tout ; la case, elle, change LE JEU. Les deux restent.
				const NkRect caseR{zone.x + 8.f, zone.y + 6.f + (rangeeH - 16.f) * 0.5f, 16.f, 16.f};
				c.ui.caseActif = caseR;
				{
					const bool soi = c.m.scene.EstActiveSoi(id);
					const bool enEffet = c.m.scene.EstActive(id);
					const bool survol = NkEditeurDans(caseR, ctx.input.mousePos);
					dl.AddRectFilled(caseR, c.pal.champ, 2.f);
					dl.AddRect(caseR, survol ? c.pal.accent : c.pal.bord, 1.f, 2.f);
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
				const NkRect nomR{caseR.x + caseR.w + 6.f, zone.y + 6.f, zone.x + zone.w - 6.f - (caseR.x + caseR.w + 6.f), rangeeH};
				ChampNom(c, id, nomR);
				NkString type = NkString::Format("type : %s", NkEditeurTypeDe(c.m.scene, id));
				if (!c.m.scene.EstActiveSoi(id)) {
					type.Append("  —  désactivée (ni rendue, ni simulée)");
				} else if (!c.m.scene.EstActive(id)) {
					type.Append("  —  désactivée par un parent");
				}
				renderer::NkTexte(dl, c.petite, zone.x + 8.f, nomR.y + rangeeH + 5.f, type.CStr(), c.pal.attenue);

				// ── Les cartes, dans une zone defilable ──────────────────────
				const float32 haut = nomR.y + rangeeH + 22.f;
				const NkRect corps{zone.x, haut, zone.w, zone.y + zone.h - haut};
				if (!nkgui::BeginChild(ctx, "details.composants", corps, false)) {
					return;
				}
				// Chaque entite garde ses propres identifiants de champs : sans
				// cette cle, un glisser commence sur une caisse finirait sur une autre.
				char cle[24];
				std::snprintf(cle, sizeof(cle), "e%llu", static_cast<unsigned long long>(id.Pack()));
				ctx.PushId(cle);
				// Les rangees d'une carte se TOUCHENT : l'espacement vertical de NKGui
				// y ouvrirait des fentes (et casserait le bord des cartes).
				const float32 espacement = ctx.layout.itemSpacingY;
				ctx.layout.itemSpacingY = 0.f;
				NkInspecteur I{c, id, corps, c.pal.entete, Melange(c.pal.entete, c.pal.texte, 0.08f),
							   Melange(c.pal.panneau, c.pal.entete, 0.45f), c.pal.bord};
				// Le Transform en tete, toujours ; les autres dans l'ordre choisi
				// (Monter / Descendre du menu « ⋮ »).
				CarteTransform(I);
				for (uint32 k = 0; k < static_cast<uint32>(NkCarteEditeur::NK_COUNT); ++k) {
					const NkCarteEditeur carte = static_cast<NkCarteEditeur>(c.ui.ordreCartes[k]);
					if (carte == NkCarteEditeur::NK_TRANSFORM || !NkEditeurAUneCarte(c.m, id, carte)) {
						continue;
					}
					ctx.PushId(NkCarteEditeurNom(carte));
					DessinerCarte(I, carte);
					ctx.PopId();
				}
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
			}

			void OngletMonde(NkEditeurCadre &c, const NkRect &zone) {
				NkGuiContext &ctx = c.ctx;
				NkEditeurModele &m = c.m;
				const NkRect corps{zone.x, zone.y + 4.f, zone.w, zone.h - 4.f};
				if (!nkgui::BeginChild(ctx, "details.monde", corps, false)) {
					return;
				}
				physics::NkParticules2D *p = m.scene.Particules();
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
