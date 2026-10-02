// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkEditeurCapturesBlueprint.cpp
// DESCRIPTION: Les captures HORS ECRAN de l'editeur de Blueprint a la UE5
//              (`--captures-blueprint=DOSSIER`) : aucune fenetre, aucune
//              souris reelle ; la vraie trame de l'editeur est jouee par la
//              trame de banc (clics, frappes, glisser) et le rasteriseur de
//              NKGui l'ecrit en PNG.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurBancTrame.h"
#include "Script/NkBpCatalogue.h"
#include "Script/NkBpExpression.h"
#include "Script/NkEditeurBlueprint.h"
#include "Script/NkEditeurExemplePortes.h"
#include "Script/NkEditeurGraphe.h"
#include "Script/NkEditeurScripts.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKImage/Core/NkImage.h"
#include "Unkeny/Entree/NkUnkenyActionsStandard.h"
#include "Unkeny/Jeu/NkUnkenyControleurs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			bool Png(NkEditeurBancTrame &T, const char *chemin) {
				memory::NkAllocator &tas = memory::NkGetDefaultAllocator();
				nkgui::NkGuiDrawListRaster *ras = tas.New<nkgui::NkGuiDrawListRaster>();
				const int32 w = static_cast<int32>(T.W), h = static_cast<int32>(T.H);
				ras->Init(w, h);
				ras->Effacer(0x121212FFu);
				ras->PoserTexture(T.police->TexId(), T.police->pixels, T.police->atlasW, T.police->atlasH, 1);
				ras->Rasteriser(T.pctx->dl);
				ras->Rasteriser(T.pctx->dlOverlay);
				NkImage img = NkImage::Wrap(const_cast<uint8 *>(ras->Pixels()), w, h, NkImagePixelFormat::NK_RGBA32);
				const bool ok = img.SavePNG(chemin);
				std::printf("  capture %s : %s\n", chemin, ok ? "ok" : "ECHEC");
				tas.Delete(ras);
				return ok;
			}
			/// La trame rasterisee, COPIEE (RGBA8, T.W x T.H).
			void Raster(NkEditeurBancTrame &T, NkVector<uint8> &px) {
				memory::NkAllocator &tas = memory::NkGetDefaultAllocator();
				nkgui::NkGuiDrawListRaster *ras = tas.New<nkgui::NkGuiDrawListRaster>();
				const int32 w = static_cast<int32>(T.W), h = static_cast<int32>(T.H);
				ras->Init(w, h);
				ras->Effacer(0x121212FFu);
				ras->PoserTexture(T.police->TexId(), T.police->pixels, T.police->atlasW, T.police->atlasH, 1);
				ras->Rasteriser(T.pctx->dl);
				ras->Rasteriser(T.pctx->dlOverlay);
				px.Resize(static_cast<usize>(w) * static_cast<usize>(h) * 4u);
				std::memcpy(px.Data(), ras->Pixels(), px.Size());
				tas.Delete(ras);
			}
			/// Copie la region (x, y, w, h) de `src` (largeur `sw`) agrandie `k` fois
			/// (plus proche voisin : chaque pixel devient un carre k x k) dans `dst`
			/// (largeur `dw`) en (dx, dy). Rend le nombre de couleurs DISTINCTES de la region.
			uint32 Agrandir(const NkVector<uint8> &src, int32 sw, int32 x, int32 y, int32 w, int32 h, int32 k, NkVector<uint8> &dst, int32 dw,
							int32 dx, int32 dy) {
				NkVector<uint32> vues;
				for (int32 j = 0; j < h; ++j) {
					for (int32 i = 0; i < w; ++i) {
						const uint8 *p = src.Data() + (static_cast<usize>(y + j) * static_cast<usize>(sw) + static_cast<usize>(x + i)) * 4u;
						const uint32 c = (static_cast<uint32>(p[0]) << 24) | (static_cast<uint32>(p[1]) << 16) | (static_cast<uint32>(p[2]) << 8) | p[3];
						bool deja = false;
						for (uint32 q = 0; q < vues.Size() && !deja; ++q) {
							deja = vues[q] == c;
						}
						if (!deja) {
							vues.PushBack(c);
						}
						for (int32 b = 0; b < k; ++b) {
							for (int32 a = 0; a < k; ++a) {
								uint8 *d = dst.Data() + (static_cast<usize>(dy + j * k + b) * static_cast<usize>(dw) + static_cast<usize>(dx + i * k + a)) * 4u;
								d[0] = p[0];
								d[1] = p[1];
								d[2] = p[2];
								d[3] = 255u;
							}
						}
					}
				}
				return static_cast<uint32>(vues.Size());
			}
			ecs::NkEntityId ParNom(NkScene &s, const char *nom) {
				ecs::NkEntityId t = ecs::NkEntityId::Invalid();
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (!t.IsValid() && std::strcmp(e.nom, nom) == 0) {
						t = id;
					}
				});
				return t;
			}
			graph::NkNodeId Noeud(const graph::NkNodeGraph &g, const char *type, int32 rang = 0) {
				for (uint32 i = 0; i < g.RawNodeCount(); ++i) {
					const graph::NkNode *n = g.RawNodeAt(i);
					if (n != nullptr && n->alive && n->type == type && rang-- == 0) {
						return n->id;
					}
				}
				return graph::NK_NODE_INVALID;
			}
			bool Element(const NkEditeurBlueprintEtat &bp, int32 section, int32 id, nkgui::NkVec2 &p) {
				for (uint32 i = 0; i < bp.mon.rects.Size(); ++i) {
					if (bp.mon.sections[i] == section && bp.mon.ids[i] == id) {
						p = nkgui::NkVec2{bp.mon.rects[i].x + 60.f, bp.mon.rects[i].y + bp.mon.rects[i].h * 0.5f};
						return true;
					}
				}
				return false;
			}
			bool Champ(const NkEditeurBlueprintEtat &bp, int32 id, nkgui::NkVec2 &p) {
				for (uint32 i = 0; i < bp.champsIds.Size(); ++i) {
					if (bp.champsIds[i] == id) {
						p = nkgui::NkVec2{bp.champsRects[i].x + bp.champsRects[i].w * 0.5f, bp.champsRects[i].y + bp.champsRects[i].h * 0.5f};
						return true;
					}
				}
				return false;
			}
			nkgui::NkVec2 Centre(const nkgui::NkRect &r) {
				return nkgui::NkVec2{r.x + r.w * 0.5f, r.y + r.h * 0.5f};
			}
			/// Un point VRAIMENT vide de la toile (a 40 px de tout noeud ; le corps
			/// d'un cadre compte pour du vide, comme dans UE5), de preference en bas.
			nkgui::NkVec2 PointVide(NkEditeurBlueprintEtat &bp) {
				const nkgui::NkRect zt = bp.zoneToile;
				graph::NkNodeGraph &g = bp.Graphe();
				const editorkit::NkJetonsNodal &s = editorkit::NkJetonsNodalParDefaut();
				for (float32 fy = 0.9f; fy > 0.15f; fy -= 0.03f) {
					for (float32 fx = 0.55f; fx > 0.12f; fx -= 0.03f) {
						const nkgui::NkVec2 p{zt.x + zt.w * fx, zt.y + zt.h * fy};
						bool libre = true;
						for (uint32 i = 0; libre && i < g.RawNodeCount(); ++i) {
							const graph::NkNode *n = g.RawNodeAt(i);
							if (n == nullptr || !n->alive || n->type == NK_BP_COMMENTAIRE) {
								continue;
							}
							const nkgui::NkRect r = editorkit::NkCanevasRectNoeud(bp.Toile(), zt, *n, s);
							libre = !(p.x > r.x - 40.f && p.y > r.y - 40.f && p.x < r.x + r.w + 40.f && p.y < r.y + r.h + 40.f);
						}
						if (libre) {
							return p;
						}
					}
				}
				return nkgui::NkVec2{zt.x + zt.w * 0.3f, zt.y + zt.h * 0.9f};
			}
		} // namespace

		int32 NkEditeurCapturesBlueprint(const char *dossier) {
			memory::NkAllocator &al = memory::NkGetDefaultAllocator();
			NkDirectory::CreateRecursive(dossier);
			const char *tmp = std::getenv("TEMP");
			const NkString racine = NkString(tmp != nullptr ? tmp : "/tmp") + "/unkeny_captures_blueprint/";
			NkDirectory::Delete(racine.CStr(), true);
			const NkString projet = racine + "Portes/";
			NkString err;
			const NkString scene = NkEditeurEcrireExemplePortes(projet.CStr(), &err);
			if (scene.Empty()) {
				std::printf("[captures] exemple Portes : %s\n", err.CStr());
				return 1;
			}
			// Un second Blueprint : les noeuds de CODE (Expression, Si, Code).
			{
				NkDocumentBp d;
				NkBpAjouterVariable(d, "total", "entier");
				NkBpAjouterVariable(d, "score", "reel");
				const uint32 vm = NkBpAjouterVariable(d, "message", "texte");
				d.variables[vm].defaut = "prêt";
				graph::NkNodeGraph &e = d.graphes[0].graphe;
				const graph::NkNodeId debut = NkBpCreerNoeud(e, "bp.ev.debut", 0.f, 40.f);
				const graph::NkNodeId ca = NkBpCreerNoeudCode(e, NK_BP_CODE, 300.f, 0.f);
				NkBpPoserCodeNoeud(e, ca, "total = total + 3\nscore = $round((7 - 2) / 7 * 100, 1)\nmessage = \"score : \" + texte(score)\nafficher(message)");
				graph::NkNodeId si = NkBpCreerNoeudCode(e, NK_BP_SI_EXPRESSION, 740.f, 0.f);
				NkBpCodeRetirerPrise(e, si, "a", graph::NkSocketDir::Input);
				NkBpCodeAjouterPrise(e, si, "runtime", "reel", graph::NkSocketDir::Input);
				NkBpPoserCodeNoeud(e, si, "runtime > -1 && total > 2");
				NkBpPoserDefaut(e, si, "runtime", "0.5");
				const graph::NkNodeId cg = NkBpCreerNoeudCode(e, NK_BP_CODE, 1160.f, -40.f);
				NkBpPoserCodeNoeud(e, cg, "afficher(\"grand\")");
				const graph::NkNodeId ls = NkBpCreerParCle(d, e, "bp.var.get:score", 380.f, 360.f);
				graph::NkNodeId ex = NkBpCreerNoeudCode(e, NK_BP_EXPRESSION, 620.f, 330.f);
				NkBpCodeRetirerPrise(e, ex, "a", graph::NkSocketDir::Input);
				NkBpCodeRetirerPrise(e, ex, "b", graph::NkSocketDir::Input);
				NkBpCodeAjouterPrise(e, ex, "testsTotal", "reel", graph::NkSocketDir::Input);
				NkBpCodeAjouterPrise(e, ex, "testsFailed", "reel", graph::NkSocketDir::Input);
				NkBpPoserCodeNoeud(e, ex, "$round((testsTotal - testsFailed) / testsTotal * 100, 1)");
				NkBpPoserDefaut(e, ex, "testsFailed", "2");
				NkBpAjouterVariable(d, "reussite", "reel");
				const graph::NkNodeId ed = NkBpCreerParCle(d, e, "bp.var.set:reussite", 1160.f, 200.f);
				e.Connect(debut, "suite", ca, "exec");
				e.Connect(ca, "suite", si, "exec");
				e.Connect(si, "vrai", cg, "exec");
				e.Connect(cg, "suite", ed, "exec");
				e.Connect(ls, "valeur", ex, "testsTotal");
				e.Connect(ex, "résultat", ed, "valeur");
				NkErreurBp e2;
				NkBpEnregistrerDocument((projet + "Contenu/Scripts/Code.nkbp").CStr(), d, e2);
			}
			NkEditeurModele *pm = al.New<NkEditeurModele>();
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			NkEditeurNouvelleScene(m);
			m.chemin = scene;
			NkEditeurOuvrir(m);
			NkEditeurScripts *ps = al.New<NkEditeurScripts>();
			NkEditeurScripts &s = *ps;
			s.ouvrirTexteExterne = false;
			NkEditeurBancTrame *pt = al.New<NkEditeurBancTrame>(m);
			NkEditeurBancTrame &T = *pt;
			T.W = 1920.f;
			T.H = 1080.f;
			T.pctx->Init(1920, 1080);
			NkEditeurInterface &ui = T.Ui();
			unkeny::NkActions *pa = al.New<unkeny::NkActions>();
			unkeny::NkLiaisons *pl = al.New<unkeny::NkLiaisons>();
			unkeny::NkLiaisonsStandard(*pl);
			NkEditeurScriptsDemarrer(s, m, pa, pl);
			unkeny::NkAjouterControleurs2D(m.scene, pa);
			NkEditeurScriptsTrame(s, m, &ui, 1.f);
			NkEditeurScriptsSuivreCompilation(s, m, true);
			NkEditeurScriptsTrame(s, m, &ui, 1.f / 60.f);
			int32 erreurs = 0;
			auto Trames = [&](int32 n) {
				for (int32 k = 0; k < n; ++k) {
					T.Trame();
				}
			};
			auto Capture = [&](const char *nom) {
				T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
				Trames(2);
				erreurs += Png(T, NkString::Format("%s/%s", dossier, nom).CStr()) ? 0 : 1;
			};
			ui.cadrageEnAttente = true;
			Trames(3);

			// ── 01 : la vue d'ensemble -- la porte REECRITE (variables, fonction) ──
			NkEditeurOuvrirGraphe(s, m, (projet + "Contenu/Scripts/PorteBlueprint.nkbp").CStr());
			NkEditeurBlueprintEtat &bp = s.graphe;
			Trames(3);
			NkEditeurCompilerGraphe(s, m);
			bp.choix = NkChoixBp::NK_CLASSE;
			Capture("01_vue_ensemble_porte_reecrite.png");
			// ── 10 : deux fils OBLIQUES agrandis x4, AVANT (sans frange) | APRES ──
			// (retour de Rihen : « pourquoi les connecteurs ne sont pas lisses ? »)
			{
				graph::NkNodeGraph &g = bp.Graphe();
				const editorkit::NkJetonsNodal &J = editorkit::NkJetonsNodalParDefaut();
				const graph::NkNode *ev = g.Find(Noeud(g, "bp.ev.zone_entree"));
				const graph::NkNode *si = g.Find(Noeud(g, "bp.si"));
				const graph::NkNode *et2 = g.Find(Noeud(g, "bp.math.et", 1));
				nkgui::NkVec2 a0, b0, a1, b1;
				if (ev != nullptr && si != nullptr && et2 != nullptr &&
					editorkit::NkCanevasPrise(bp.Toile(), bp.zoneToile, *ev, ev->FindSocket("suite", graph::NkSocketDir::Output), J, a0) &&
					editorkit::NkCanevasPrise(bp.Toile(), bp.zoneToile, *si, si->FindSocket("exec", graph::NkSocketDir::Input), J, b0) &&
					editorkit::NkCanevasPrise(bp.Toile(), bp.zoneToile, *et2, et2->FindSocket("r", graph::NkSocketDir::Output), J, a1) &&
					editorkit::NkCanevasPrise(bp.Toile(), bp.zoneToile, *si, si->FindSocket("condition", graph::NkSocketDir::Input), J, b1)) {
					// Le milieu de chaque fil (la cubique de la toile).
					auto Milieu = [](const nkgui::NkVec2 &a, const nkgui::NkVec2 &b, float32 t) {
						const float32 dx = (b.x > a.x ? b.x - a.x : a.x - b.x) * 0.5f;
						const float32 tire = dx < 40.f ? 40.f : dx;
						return nkgui::NkGuiDrawList::PointBezier(a, nkgui::NkVec2{a.x + tire, a.y}, nkgui::NkVec2{b.x - tire, b.y}, b, t);
					};
					const nkgui::NkVec2 m0 = Milieu(a0, b0, 0.3f), m1 = Milieu(a1, b1, 0.5f);
					const int32 cw = 100, ch = 50, k = 4, ecart = 8;
					const int32 W = 2 * cw * k + ecart, H = 2 * ch * k + ecart;
					NkVector<uint8> image;
					image.Resize(static_cast<usize>(W) * static_cast<usize>(H) * 4u);
					for (usize i = 0; i < image.Size(); i += 4u) {
						image[i] = 60u;
						image[i + 1u] = 60u;
						image[i + 2u] = 66u;
						image[i + 3u] = 255u;
					}
					uint32 couleurs[2][2] = {{0, 0}, {0, 0}};
					for (int32 apres = 0; apres < 2; ++apres) {
						T.pctx->dl.traitsLisses = apres == 1;
						T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
						Trames(2);
						NkVector<uint8> px;
						Raster(T, px);
						const int32 sw = static_cast<int32>(T.W);
						couleurs[0][apres] = Agrandir(px, sw, static_cast<int32>(m0.x) - cw / 2, static_cast<int32>(m0.y) - ch / 2, cw, ch, k, image, W,
													  apres * (cw * k + ecart), 0);
						couleurs[1][apres] = Agrandir(px, sw, static_cast<int32>(m1.x) - cw / 2, static_cast<int32>(m1.y) - ch / 2, cw, ch, k, image, W,
													  apres * (cw * k + ecart), ch * k + ecart);
					}
					T.pctx->dl.traitsLisses = true;
					const NkString chemin = NkString::Format("%s/10_fil_agrandi_x4_avant_apres.png", dossier);
					NkImage img = NkImage::Wrap(image.Data(), W, H, NkImagePixelFormat::NK_RGBA32);
					const bool ok = img.SavePNG(chemin.CStr());
					std::printf("  capture %s : %s\n", chemin.CStr(), ok ? "ok" : "ECHEC");
					std::printf("    fil d'execution presque horizontal : %u couleurs avant, %u apres ; fil de valeur oblique : %u avant, %u apres\n",
								couleurs[0][0], couleurs[0][1], couleurs[1][0], couleurs[1][1]);
					erreurs += ok && couleurs[0][1] > couleurs[0][0] * 3u && couleurs[1][1] > couleurs[1][0] * 3u ? 0 : 1;
				} else {
					std::printf("  capture 10 : les prises du fil sont introuvables\n");
					++erreurs;
				}
			}
			// ── 01b : la meme, a 100 % : le FLOT d'execution (Si -> Vrai / Faux) ──
			{
				editorkit::NkEtatCanevas &t = bp.Toile();
				const float32 z0 = t.zoom, x0 = t.vueX, y0 = t.vueY;
				t.zoom = 1.f;
				t.vueX = 470.f;
				t.vueY = -110.f;
				Capture("01b_porte_reecrite_flot_a_100.png");
				t.zoom = z0;
				t.vueX = x0;
				t.vueY = y0;
			}

			// ── 02 : clic droit dans le vide -> le menu, la recherche « pos » ──
			const nkgui::NkRect zt = bp.zoneToile;
			(void)zt;
			const nkgui::NkVec2 vide = PointVide(bp);
			T.Clic(1, vide.x, vide.y);
			T.Taper("pos");
			T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
			Trames(1);
			erreurs += Png(T, NkString::Format("%s/02_menu_contextuel_recherche.png", dossier).CStr()) ? 0 : 1;
			T.Touche(nkgui::NkGuiKey::Escape);

			// ── 03 : un fil tire depuis « autre » (entite) et LACHE dans le vide ──
			{
				graph::NkNodeGraph &g = bp.Graphe();
				const graph::NkNode *ev = g.Find(Noeud(g, "bp.ev.zone_entree"));
				nkgui::NkVec2 a0;
				if (ev != nullptr &&
					editorkit::NkCanevasPrise(bp.Toile(), bp.zoneToile, *ev, ev->FindSocket("autre", graph::NkSocketDir::Output), editorkit::NkJetonsNodalParDefaut(), a0)) {
					T.Glisser(a0.x, a0.y, vide.x, vide.y, 8);
				}
				T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
				Trames(1);
				erreurs += Png(T, NkString::Format("%s/03_menu_sensible_au_contexte_depuis_une_prise.png", dossier).CStr()) ? 0 : 1;
				T.Touche(nkgui::NkGuiKey::Escape);
			}

			// ── 04 : DECLARER une variable : « + », son nom, son type, sa valeur ──
			{
				T.Trame();
				const nkgui::NkRect plus = bp.mon.rectsPlus[3];
				T.Clic(0, plus.x + plus.w * 0.5f, plus.y + plus.h * 0.5f);
				// Le renommage en place est ouvert : on remplace le nom.
				for (int32 k = 0; k < 16; ++k) {
					T.Touche(nkgui::NkGuiKey::Backspace);
				}
				T.Taper("vitesse");
				T.Touche(nkgui::NkGuiKey::Enter);
				T.Trame();
				nkgui::NkVec2 p;
				if (Champ(bp, -1000, p)) { // la liste du TYPE
					T.Clic(0, p.x, p.y);
					if (bp.popup.ouvert && bp.popup.rects.Size() > 2u) {
						const nkgui::NkVec2 c = Centre(bp.popup.rects[2]); // « Réel »
						T.Clic(0, c.x, c.y);
					}
				}
				T.Trame();
				if (Champ(bp, 2 /*CH_DEFAUT*/, p)) {
					T.Clic(0, p.x, p.y);
					T.Touche(nkgui::NkGuiKey::Backspace);
					T.Taper("4.5");
					T.Touche(nkgui::NkGuiKey::Enter);
				}
				T.Trame();
				if (Champ(bp, 4 /*CH_INFOBULLE*/, p)) {
					T.Clic(0, p.x, p.y);
					T.Taper("Vitesse de montée de la porte (m/s)");
					T.Touche(nkgui::NkGuiKey::Enter);
				}
				T.Trame();
				if (Champ(bp, 5 /*CH_CATEGORIE*/, p)) {
					T.Clic(0, p.x, p.y);
					T.Taper("Réglages");
					T.Touche(nkgui::NkGuiKey::Enter);
				}
				Capture("04_declaration_d_une_variable_details.png");
				// 04b : la liste des TYPES, ouverte (elle couvre les champs dessous).
				if (Champ(bp, -1000, p)) {
					T.Clic(0, p.x, p.y);
				}
				Trames(1);
				erreurs += Png(T, NkString::Format("%s/04b_type_d_une_variable_la_liste.png", dossier).CStr()) ? 0 : 1;
				T.Touche(nkgui::NkGuiKey::Escape);
				// La variable de la demonstration ne reste pas dans la porte.
				const int32 v = bp.doc.TrouverVariable("vitesse");
				if (v >= 0) {
					NkBpSupprimerVariable(bp.doc, static_cast<uint32>(v));
					NkEditeurBlueprintRetenir(bp);
				}
			}

			// ── 05 : double-clic sur la fonction OuvrirPorte : son graphe ──
			{
				T.Trame();
				const int32 gf = bp.doc.TrouverGraphe("OuvrirPorte", NkGenreGrapheBp::NK_FONCTION);
				nkgui::NkVec2 p;
				if (gf > 0 && Element(bp, 1, gf, p)) {
					T.DoubleClic(p.x, p.y);
				}
				Trames(3);
				Capture("05_fonction_ouverte_ses_entrees_sorties.png");
			}

			// ── 06 : une ERREUR de compilation, ecrite sur SON noeud ──
			{
				T.Clic(0, Centre(bp.rectsOnglets[0]).x, Centre(bp.rectsOnglets[0]).y);
				Trames(2);
				graph::NkNodeGraph &g = bp.Graphe();
				const graph::NkNodeId rien = Noeud(g, NK_BP_CODE);
				const NkString avant = NkBpCodeNoeud(g, *g.Find(rien));
				NkBpPoserCodeNoeud(g, rien, "afficher(\"Rien à ouvrir : \" + porte_ouverte)");
				T.Trame();
				T.Clic(0, Centre(bp.boutons[NK_BP_COMPILER]).x, Centre(bp.boutons[NK_BP_COMPILER]).y);
				Capture("06_erreur_de_compilation_sur_son_noeud.png");
				NkBpPoserCodeNoeud(g, rien, avant.CStr());
				NkEditeurCompilerGraphe(s, m);
			}

			// ── 07 : SIMULER la porte reecrite : les fils qui ont servi, x1, x0 ──
			{
				T.Clic(0, Centre(bp.boutons[NK_BP_SIMULER]).x, Centre(bp.boutons[NK_BP_SIMULER]).y);
				for (int32 k = 0; k < 150; ++k) {
					pa->NouvelleTrame();
					pa->Poser(unkeny::NK_ACTION_AVANCER, -1.f);
					NkEditeurAvancer(m, 1.f / 60.f);
					NkEditeurScriptsTrame(s, m, &ui, 1.f / 60.f);
				}
				Trames(2);
				Capture("07_simuler_porte_reecrite_trace_et_console.png");
				T.Clic(0, Centre(bp.boutons[NK_BP_SIMULER]).x, Centre(bp.boutons[NK_BP_SIMULER]).y); // Arreter
				NkEditeurScriptsTrame(s, m, &ui, 1.f / 60.f);
			}

			// ── 08 : les noeuds de CODE (la reference principale + le flot) ──
			{
				NkEditeurOuvrirGraphe(s, m, (projet + "Contenu/Scripts/Code.nkbp").CStr());
				Trames(3);
				NkEditeurCompilerGraphe(s, m);
				graph::NkNodeGraph &g = bp.Graphe();
				const graph::NkNodeId ex = Noeud(g, NK_BP_EXPRESSION);
				bp.Toile().Choisir(ex);
				bp.choix = NkChoixBp::NK_NOEUD;
				T.Trame();
				// Un clic dans son code : l'edition s'ouvre (le caret, la coloration).
				const graph::NkNode *n = g.Find(ex);
				for (uint32 i = 0; n != nullptr && i < bp.Toile().geom.Size(); ++i) {
					const editorkit::NkGeomNoeud &gg = bp.Toile().geom[i];
					if (gg.id == ex) {
						const nkgui::NkRect rn = editorkit::NkCanevasRectNoeud(bp.Toile(), bp.zoneToile, *n, editorkit::NkJetonsNodalParDefaut());
						T.Clic(0, rn.x + rn.w * 0.5f, rn.y + (gg.blocY + 22.f + 12.f) * bp.Toile().zoom);
					}
				}
				T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
				Trames(1);
				erreurs += Png(T, NkString::Format("%s/08_noeuds_de_code_expression_si_code.png", dossier).CStr()) ? 0 : 1;
				T.Touche(nkgui::NkGuiKey::Escape);
			}

			// ── 09 : la scene : Details > Scripts, les variables PAR INSTANCE ──
			{
				T.Clic(0, Centre(bp.boutons[NK_BP_FERMER]).x, Centre(bp.boutons[NK_BP_FERMER]).y);
				NkEditeurScriptsTrame(s, m, &ui, 1.f / 60.f); // les panneaux reviennent
				m.selection = ParNom(m.scene, "Zone bleue");
				m.aSelection = m.selection.IsValid();
				Trames(4);
				for (int32 k = 0; k < 14; ++k) {
					T.pctx->input.mousePos = nkgui::NkVec2{ui.details.x + ui.details.w * 0.5f, ui.details.y + ui.details.h * 0.6f};
					T.pctx->input.AddWheelDeferred(-6.f);
					T.Trame();
				}
				Capture("09_details_scripts_variables_par_instance.png");
			}

			NkEditeurScriptsArreter(s);
			al.Delete(pt);
			al.Delete(pl);
			al.Delete(pa);
			al.Delete(ps);
			al.Delete(pm);
			return erreurs == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
