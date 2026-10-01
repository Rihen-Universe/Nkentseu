// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurFormesUi.cpp
// DESCRIPTION: Les blocs des Details pour la FORME 2D et la COLLISION d'une
//              entite, et la section « Calques de collision » de l'onglet Monde
//              (voir NkEditeurPlacer.h).
//
// ⚠️ UN BLOC, PAS UNE CARTE, DELIBEREMENT
//   Les cartes (NkCarteEditeur) sont celles de NkEditeurDetails.cpp, refaites
//   par un autre chantier le meme jour. Ces blocs-ci suivent le patron du bloc
//   d'ancrage (NkEditeurBlocAncrage) : appeles en fin de liste, ecrits avec les
//   widgets de NKGui, ils ne touchent ni a l'ordre ni au dessin des cartes.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurPlacer.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKGui/Widgets/NkGuiWidgets.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkGuiContext;
		using nkgui::NkRect;

		namespace {
			/// Le ColorEdit4 de NKGui sur une couleur 0xRRGGBBAA : les conversions
			/// sont celles de math::NkColor / NkColorF (NKMath/NkColor.h).
			bool Teinte(NkGuiContext &ctx, const char *libelle, uint32 &rgba) {
				const math::NkColorF cf = math::NkColor(rgba).ToColorF();
				float32 col[4] = {cf.r, cf.g, cf.b, cf.a};
				if (!nkgui::ColorEdit4(ctx, libelle, col)) {
					return false;
				}
				rgba = math::NkColor(math::NkColorF(col[0], col[1], col[2], col[3])).ToUint32A();
				return true;
			}

			/// Une liste de POINTS (ligne, polygone libre, sommets d'un collisionneur),
			/// REPLIABLE (« points (n) ») : une rangee par point, son numero puis x et
			/// y cote a cote ; puis « + » et « - ». Rend vrai si elle change.
			bool Points(NkEditeurCadre &c, const char *cle, const char *titre, NkVec2f *pts, uint8 &nb, uint32 min, uint32 max) {
				NkGuiContext &ctx = c.ctx;
				bool change = false;
				ctx.PushId(cle);
				const NkString texte = NkString::Format("%s (%u)", titre, static_cast<unsigned>(nb));
				if (!nkgui::TreeNodeEx(ctx, "liste", texte.CStr())) {
					ctx.PopId();
					return false;
				}
				for (uint32 i = 0; i < nb; ++i) {
					const NkRect r = ctx.NextItemRect(0.f, 20.f);
					char num[8];
					std::snprintf(num, sizeof(num), "%u", static_cast<unsigned>(i + 1u));
					renderer::NkTexte(ctx.DL(), c.petite, r.x + 2.f, r.y + 3.f, num, c.pal.attenue);
					const float32 w = (r.w - 30.f) * 0.5f;
					ctx.PushId(num);
					ctx.SetNextItemRect(NkRect{r.x + 24.f, r.y, w - 2.f, r.h});
					change |= nkgui::DragFloat(ctx, "##x", pts[i].x, 0.01f);
					ctx.SetNextItemRect(NkRect{r.x + 26.f + w, r.y, w - 2.f, r.h});
					change |= nkgui::DragFloat(ctx, "##y", pts[i].y, 0.01f);
					ctx.PopId();
				}
				ctx.BeginDisabled(nb >= max);
				if (nkgui::Button(ctx, "+ point")) {
					// Au milieu du dernier cote : la forme ne saute pas.
					const NkVec2f a = nb > 0u ? pts[nb - 1u] : NkVec2f(0.f, 0.f);
					const NkVec2f b = nb > 0u ? pts[0] : NkVec2f(0.5f, 0.f);
					pts[nb] = NkVec2f((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f + (nb > 1u ? 0.f : 0.5f));
					++nb;
					change = true;
				}
				ctx.EndDisabled();
				ctx.SameLine();
				ctx.BeginDisabled(nb <= min);
				if (nkgui::Button(ctx, "- point")) {
					--nb;
					change = true;
				}
				ctx.EndDisabled();
				nkgui::TreePop(ctx);
				ctx.PopId();
				return change;
			}

			int32 PremierCalque(uint32 couche) {
				for (uint32 i = 0; i < NK_CALQUES_COLLISION; ++i) {
					if ((couche & (1u << i)) != 0u) {
						return static_cast<int32>(i);
					}
				}
				return -1;
			}

			const char *NomCalque(const NkCalquesCollision2D &k, int32 i, char *tampon, uint32 taille) {
				if (i < 0) {
					return "(aucun)";
				}
				if (k.noms[i][0] != '\0') {
					return k.noms[i];
				}
				std::snprintf(tampon, taille, i == 0 ? "Défaut" : "Calque %d", i);
				return tampon;
			}
		} // namespace

		// =====================================================================
		void NkEditeurBlocForme(NkEditeurCadre &c, ecs::NkEntityId id) {
			NkGuiContext &ctx = c.ctx;
			NkEditeurModele &m = c.m;
			NkRenduForme2D *f = m.scene.Monde().Get<NkRenduForme2D>(id);
			if (f == nullptr) {
				return;
			}
			ctx.PushId("forme2d");
			nkgui::Separator(ctx);
			nkgui::Text(ctx, NkString::Format("Forme 2D : %s", NkNomGenreForme2D(f->genre)).CStr());
			bool change = false;
			// Le GENRE, en neuf boutons : on le change comme on le voit.
			static const char *kGenres[9] = {"Rectangle", "Cercle", "Ellipse", "Triangle", "Étoile", "Polygone", "Capsule", "Ligne", "Libre"};
			for (int32 g = 0; g < 9; ++g) {
				if (g % 3 != 0) {
					ctx.SameLine();
				}
				ctx.BeginDisabled(static_cast<int32>(f->genre) == g);
				if (nkgui::Button(ctx, kGenres[g])) {
					const NkGenreForme2D genre = static_cast<NkGenreForme2D>(g);
					// Les points d'exemple d'une ligne ou d'un polygone libre, s'il
					// n'en a pas : sans eux, la forme n'aurait que sa boite.
					if ((genre == NkGenreForme2D::NK_LIGNE && f->nbPoints < 2u) ||
						(genre == NkGenreForme2D::NK_POLYGONE_LIBRE && f->nbPoints < 3u)) {
						const NkRenduForme2D d = NkFormeParDefaut(genre);
						f->nbPoints = d.nbPoints;
						for (uint32 i = 0; i < NK_FORME_POINTS_MAX; ++i) {
							f->points[i] = d.points[i];
						}
					}
					f->genre = genre;
					change = true;
				}
				ctx.EndDisabled();
			}
			const bool aPoints = f->genre == NkGenreForme2D::NK_LIGNE || f->genre == NkGenreForme2D::NK_POLYGONE_LIBRE;
			if (!aPoints) {
				change |= nkgui::DragFloat(ctx, "largeur (m)", f->taille.x, 0.01f, 0.02f, 200.f);
				change |= nkgui::DragFloat(ctx, "hauteur (m)", f->taille.y, 0.01f, 0.02f, 200.f);
			}
			if (f->genre == NkGenreForme2D::NK_ETOILE) {
				int32 b = f->branches;
				if (nkgui::InputInt(ctx, "branches", b, 1, 3, 16)) {
					f->branches = static_cast<uint8>(b < 3 ? 3 : (b > 16 ? 16 : b));
					change = true;
				}
				change |= nkgui::SliderFloat(ctx, "rayon intérieur", f->rayonInterieur, 0.1f, 0.95f);
			}
			if (f->genre == NkGenreForme2D::NK_POLYGONE_REGULIER) {
				int32 n = f->cotes;
				if (nkgui::InputInt(ctx, "côtés", n, 1, 3, 32)) {
					f->cotes = static_cast<uint8>(n < 3 ? 3 : (n > 32 ? 32 : n));
					change = true;
				}
			}
			if (f->genre == NkGenreForme2D::NK_LIGNE) {
				change |= nkgui::SliderFloat(ctx, "épaisseur du trait (m)", f->epaisseur, 0.01f, 1.f);
			}
			if (aPoints) {
				change |= Points(c, "pts", f->genre == NkGenreForme2D::NK_LIGNE ? "points de la ligne (m)" : "points du contour (m)", f->points,
								 f->nbPoints, f->genre == NkGenreForme2D::NK_LIGNE ? 2u : 3u, NK_FORME_POINTS_MAX);
			}
			nkgui::Checkbox(ctx, "remplie", f->rempli);
			Teinte(ctx, "remplissage", f->remplissage);
			Teinte(ctx, "contour", f->couleurContour);
			nkgui::SliderFloat(ctx, "épaisseur du contour (m)", f->epaisseurContour, 0.f, 0.5f);
			const bool aCoins = f->genre == NkGenreForme2D::NK_RECTANGLE || f->genre == NkGenreForme2D::NK_TRIANGLE ||
								f->genre == NkGenreForme2D::NK_ETOILE || f->genre == NkGenreForme2D::NK_POLYGONE_REGULIER ||
								f->genre == NkGenreForme2D::NK_POLYGONE_LIBRE;
			if (aCoins) {
				nkgui::SliderFloat(ctx, "arrondi des coins (m)", f->arrondi, 0.f, 1.f);
			}
			nkgui::SliderFloat(ctx, "opacité", f->opacite, 0.f, 1.f);
			nkgui::InputInt(ctx, "couche (ordre de dessin)", f->couche);
			nkgui::Checkbox(ctx, "visible", f->visible);
			if (nkgui::Checkbox(ctx, "le collisionneur suit la forme", f->collisionSuit) && f->collisionSuit) {
				change = true;
			}
			if (change) {
				NkEditeurFormeChangee(m, id);
			}
			ctx.PopId();
		}

		// =====================================================================
		void NkEditeurBlocCollision(NkEditeurCadre &c, ecs::NkEntityId id) {
			NkGuiContext &ctx = c.ctx;
			NkEditeurModele &m = c.m;
			NkScene &s = m.scene;
			if (s.Monde().Has<NkCorpsMou2D>(id)) {
				return; // la matiere touche deja tout, sans collisionneur rigide
			}
			NkCollisionneur2D *col = s.Monde().Get<NkCollisionneur2D>(id);
			ctx.PushId("collision2d");
			nkgui::Separator(ctx);
			nkgui::Text(ctx, "Collision");
			if (col == nullptr) {
				nkgui::TextWrapped(ctx, "Aucun collisionneur : rien ne la touche. En ajouter un :");
				for (int32 k = 0; k < static_cast<int32>(NkCollisionEditeur::NK_COUNT); ++k) {
					const NkCollisionEditeur ke = static_cast<NkCollisionEditeur>(k);
					if (ke == NkCollisionEditeur::NK_DEPUIS_FORME && !s.Monde().Has<NkRenduForme2D>(id)) {
						continue;
					}
					if (ke == NkCollisionEditeur::NK_DEPUIS_SPRITE) {
						const NkSprite2D *sp = s.Monde().Get<NkSprite2D>(id);
						if (sp == nullptr || sp->texId == 0u) {
							continue;
						}
					}
					if (k % 2 != 0) {
						ctx.SameLine();
					}
					if (nkgui::Button(ctx, NkNomCollisionEditeur(ke))) {
						NkEditeurAjouterCollision(m, id, ke);
					}
				}
				ctx.PopId();
				return;
			}
			bool change = false;
			// ── Le calque, et ce qu'il touche (la ligne de la matrice) ─────
			NkCalquesCollision2D &kc = s.Calques();
			char tampon[32];
			int32 calque = PremierCalque(col->couche);
			nkgui::Text(ctx, NkString::Format("calque : %s", NomCalque(kc, calque, tampon, sizeof(tampon))).CStr());
			ctx.SameLine();
			if (nkgui::Button(ctx, "<##calque") && calque > 0) {
				col->couche = (col->couche & 0xFFFF0000u) | (1u << static_cast<uint32>(calque - 1));
				change = true;
			}
			ctx.SameLine();
			if (nkgui::Button(ctx, ">##calque") && calque < static_cast<int32>(NK_CALQUES_COLLISION) - 1) {
				col->couche = (col->couche & 0xFFFF0000u) | (1u << static_cast<uint32>(calque + 1));
				change = true;
			}
			{
				// « touche : » les calques NOMMES (et Defaut) que sa ligne laisse passer.
				calque = PremierCalque(col->couche);
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
					touche.Append(NomCalque(kc, static_cast<int32>(j), tampon, sizeof(tampon)));
				}
				nkgui::TextWrapped(ctx, NkString::Format("touche : %s%s", touche.Empty() ? "rien" : touche.CStr(),
														 refuses > 0 ? "  (Monde > Calques de collision)" : "")
											.CStr());
				if (nkgui::Button(ctx, "Calques et matrice (réglages du projet)")) {
					c.ui.reglagesCollision = true;
				}
			}
			// ── Solide ou declencheur ──────────────────────────────────────
			change |= nkgui::Checkbox(ctx, "déclencheur (détecte sans repousser)", col->declencheur);
			// ── La rotation propre ─────────────────────────────────────────
			float32 deg = col->rotation * 57.2957795f;
			if (nkgui::DragFloat(ctx, "rotation propre (°)", deg, 0.5f, -360.f, 360.f)) {
				col->rotation = deg / 57.2957795f;
				change = true;
			}
			// ── Polygone, chaine : les sommets ─────────────────────────────
			if (col->forme == NkForme2D::NK_POLYGONE || col->forme == NkForme2D::NK_CHAINE) {
				const bool polygone = col->forme == NkForme2D::NK_POLYGONE;
				if (!polygone) {
					change |= nkgui::Checkbox(ctx, "chaîne fermée (contour)", col->boucle);
				}
				change |= Points(c, "sommets", polygone ? "sommets (polygone CONVEXE, 8 au plus)" : "sommets de la chaîne (décor, 32 au plus)",
								 col->sommets, col->nbSommets, polygone ? 3u : 2u, polygone ? NK_POLYGONE_CONVEXE_MAX : NK_COLLISION_SOMMETS_MAX);
				if (polygone && col->nbSommets >= 3u && !NkEstConvexe2D(col->sommets, col->nbSommets)) {
					nkgui::TextWrapped(ctx, "Ce polygone est CONCAVE : le solveur ne le touche pas juste. Passez-le en « Chaîne » "
											"(décor) ou prenez « Depuis la forme ».");
				}
			}
			// ── Generer ────────────────────────────────────────────────────
			const NkSprite2D *sp = s.Monde().Get<NkSprite2D>(id);
			const bool aForme = s.Monde().Has<NkRenduForme2D>(id);
			const bool aImage = sp != nullptr && sp->texId != 0u;
			if (aForme && nkgui::Button(ctx, "Depuis la forme")) {
				NkEditeurAjouterCollision(m, id, NkCollisionEditeur::NK_DEPUIS_FORME);
				col = s.Monde().Get<NkCollisionneur2D>(id);
			}
			if (aImage) {
				if (aForme) {
					ctx.SameLine();
				}
				if (nkgui::Button(ctx, "Depuis le contour du sprite")) {
					NkEditeurAjouterCollision(m, id, NkCollisionEditeur::NK_DEPUIS_SPRITE);
					col = s.Monde().Get<NkCollisionneur2D>(id);
				}
			}
			// ── L'edition dans la vue ──────────────────────────────────────
			nkgui::Checkbox(ctx, "Éditer dans la vue (poignées)", c.ui.editionCollision);
			// ── Le corps et son materiau ───────────────────────────────────
			NkCorps2D *b = s.Monde().Get<NkCorps2D>(id);
			if (b != nullptr) {
				static const char *kTypes[3] = {"statique", "cinématique", "dynamique"};
				nkgui::Text(ctx, NkString::Format("corps : %s", kTypes[static_cast<int32>(b->type) % 3]).CStr());
				bool corps = false;
				for (int32 t = 0; t < 3; ++t) {
					if (t > 0) {
						ctx.SameLine();
					}
					ctx.BeginDisabled(static_cast<int32>(b->type) == t);
					if (nkgui::Button(ctx, kTypes[t])) {
						b->type = static_cast<NkTypeCorps>(t);
						corps = true;
					}
					ctx.EndDisabled();
				}
				corps |= nkgui::SliderFloat(ctx, "friction", b->friction, 0.f, 1.5f);
				corps |= nkgui::SliderFloat(ctx, "rebond", b->rebond, 0.f, 1.f);
				if (corps) {
					// Un corps qui devient dynamique : un contour concave devient son
					// enveloppe (le solveur ne fait pas tomber un concave).
					NkEditeurFormeChangee(m, id);
					change = true;
				}
			} else if (s.PhysiqueActive()) {
				nkgui::TextWrapped(ctx, "Sans corps, il ne touche rien en jeu (seulement la prise et les ombres).");
				int32 type = -1;
				if (nkgui::Button(ctx, "Rendre solide (statique)")) {
					type = static_cast<int32>(NkTypeCorps::NK_STATIQUE);
				}
				ctx.SameLine();
				if (nkgui::Button(ctx, "Le faire tomber (dynamique)")) {
					type = static_cast<int32>(NkTypeCorps::NK_DYNAMIQUE);
				}
				if (type >= 0) {
					NkEditeurRetenir(m);
					NkCorps2D nb;
					nb.type = static_cast<NkTypeCorps>(type);
					// Dynamique : un contour concave devient son enveloppe convexe.
					if (aForme) {
						s.AjouterCorps(id, nb);
						NkEditeurFormeChangee(m, id);
					} else {
						s.AjouterCorps(id, nb);
					}
					col = s.Monde().Get<NkCollisionneur2D>(id);
				}
			}
			if (change && col != nullptr) {
				// Regle a la main : la forme ne le refait plus.
				if (NkRenduForme2D *f = s.Monde().Get<NkRenduForme2D>(id)) {
					f->collisionSuit = false;
				}
				if (s.Monde().Has<NkCorps2D>(id)) {
					s.ActualiserCorps(id);
				}
			}
			ctx.PopId();
		}

		// =====================================================================
		namespace {
			/// Les calques : noms et matrice. `large` (la fenetre des reglages) : la
			/// ligne de la matrice porte le NOM du calque, pas seulement son numero.
			void Calques(NkEditeurCadre &c, bool large) {
				NkGuiContext &ctx = c.ctx;
				NkScene &s = c.m.scene;
				NkCalquesCollision2D &kc = s.Calques();
				ctx.PushId(large ? "calques.reglages" : "calques");
				if (!large) {
					nkgui::Separator(ctx);
					nkgui::Text(ctx, "Calques de collision (qui touche qui)");
				}
				nkgui::TextWrapped(ctx, "Chaque collisionneur a un calque (Détails > Collision). Une case vide : ces deux calques "
										"se traversent. Gardé dans la scène et dans le jeu construit.");
				nkgui::Checkbox(ctx, "les 16 calques", c.ui.calquesTous);
				const uint32 n = c.ui.calquesTous ? NK_CALQUES_COLLISION : 8u;
				// Les noms.
				for (uint32 i = 0; i < n; ++i) {
					char lib[24];
					std::snprintf(lib, sizeof(lib), "calque %u##nom%u", static_cast<unsigned>(i), static_cast<unsigned>(i));
					nkgui::InputText(ctx, lib, kc.noms[i], static_cast<int32>(sizeof(kc.noms[i])));
				}
				// La MATRICE, en triangle comme celle d'Unity : la ligne i, les colonnes
				// j <= i. Une case par paire (la matrice est symetrique).
				const float32 cel = large ? 22.f : 16.f;
				const float32 marge = large ? 132.f : 26.f;
				const float32 haut = 22.f;
				const NkRect zone = ctx.NextItemRect(0.f, haut + cel * static_cast<float32>(n) + 6.f);
				auto &dl = ctx.DL();
				const nkgui::NkGuiInput &in = ctx.input;
				bool change = false;
				for (uint32 i = 0; i < n; ++i) {
					char num[32];
					std::snprintf(num, sizeof(num), "%u", static_cast<unsigned>(i));
					char nom[32];
					const char *ligne = large ? NomCalque(kc, static_cast<int32>(i), nom, sizeof(nom)) : num;
					const NkRect lib{zone.x + 2.f, zone.y + haut + cel * static_cast<float32>(i), marge - 6.f, cel};
					dl.PushClipRect(lib, true);
					renderer::NkTexte(dl, c.petite, lib.x, lib.y + 2.f, ligne, c.pal.attenue);
					dl.PopClipRect();
					renderer::NkTexte(dl, c.petite, zone.x + marge + cel * static_cast<float32>(i) + 3.f, zone.y + 4.f, num, c.pal.attenue);
					for (uint32 j = 0; j <= i; ++j) {
						const NkRect r{zone.x + marge + cel * static_cast<float32>(j), zone.y + haut + cel * static_cast<float32>(i), cel - 2.f,
									   cel - 2.f};
						const bool oui = kc.Touche(i, j);
						const bool survol = NkEditeurDans(r, in.mousePos);
						dl.AddRectFilled(r, oui ? c.pal.accent : c.pal.champ, 2.f);
						dl.AddRect(r, survol ? c.pal.texte : c.pal.bord, 1.f, 2.f);
						if (survol && in.mouseClicked[0]) {
							NkEditeurRetenir(c.m);
							kc.Poser(i, j, !oui);
							change = true;
						}
					}
				}
				if (nkgui::Button(ctx, "Tout touche tout")) {
					for (uint32 i = 0; i < NK_CALQUES_COLLISION; ++i) {
						kc.matrice[i] = 0xFFFFu;
					}
					change = true;
				}
				if (change) {
					// Les corps naissent avec la matrice : on les refait (vitesses gardees).
					const uint32 refaits = s.AppliquerCalques();
					NkEditeurAnnoncer(c.m, NkString::Format("Calques de collision appliqués (%u corps)", refaits).CStr());
				}
				ctx.PopId();
			}
		} // namespace

		void NkEditeurSectionCalques(NkEditeurCadre &c) {
			Calques(c, false);
		}

		// =====================================================================
		void NkEditeurDessinerReglagesCollision(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			if (!ui.reglagesCollision) {
				ui.reglagesCollisionRect = NkRect{0.f, 0.f, 0.f, 0.f};
				return;
			}
			auto &dl = c.ctx.dl;
			const float32 w = ui.ecran.w - 80.f < 600.f ? ui.ecran.w - 80.f : 600.f;
			const float32 h = ui.ecran.h - 120.f < 700.f ? ui.ecran.h - 120.f : 700.f;
			const NkRect r{ui.ecran.x + (ui.ecran.w - w) * 0.5f, ui.ecran.y + (ui.ecran.h - h) * 0.5f, w, h};
			ui.reglagesCollisionRect = r;
			const float32 enteteH = 34.f;
			dl.AddRectFilled(r, c.pal.panneau, 4.f);
			dl.AddRect(r, c.pal.accent, 1.f, 4.f);
			dl.AddRectFilled(NkRect{r.x, r.y, r.w, enteteH}, c.pal.entete, 4.f);
			renderer::NkTexte(dl, c.police, r.x + 12.f, r.y + 8.f, "Réglages du projet — calques de collision", c.pal.texte);
			// La croix (comme celle du panneau Entrees).
			const NkRect fermer{r.x + r.w - 30.f, r.y + 6.f, 22.f, 22.f};
			const bool actif = ui.menu == NkMenuEditeur::NK_AUCUN && ui.confirmation == NK_A_AUCUNE;
			if (NkEditeurBouton(c, fermer, "", false, actif)) {
				ui.reglagesCollision = false;
			}
			dl.AddLine(nkgui::NkVec2{fermer.x + 6.f, fermer.y + 6.f}, nkgui::NkVec2{fermer.x + 16.f, fermer.y + 16.f}, c.pal.texte, 1.6f);
			dl.AddLine(nkgui::NkVec2{fermer.x + 16.f, fermer.y + 6.f}, nkgui::NkVec2{fermer.x + 6.f, fermer.y + 16.f}, c.pal.texte, 1.6f);
			const NkRect corps{r.x + 8.f, r.y + enteteH + 6.f, r.w - 16.f, r.h - enteteH - 12.f};
			if (nkgui::BeginChild(c.ctx, "reglages.collision", corps, false)) {
				Calques(c, true);
				nkgui::EndChild(c.ctx);
			}
		}

	} // namespace editeur
} // namespace nkentseu
