// -----------------------------------------------------------------------------
// @File    NkFamillePlacer.cpp
// @Brief   « Placer des acteurs » de la famille (voir NkFamillePlacer.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamillePlacer.h"

#include <cmath>
#include <cstring>

namespace nkentseu {
	namespace editorkit {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			constexpr float32 ONGLETS_L = 54.f;	   ///< la colonne des onglets verticaux
			constexpr float32 ONGLET_H = 52.f;
			constexpr float32 RANGEE_H = 30.f;	   ///< une rangee de la liste
			constexpr float32 SEUIL_GLISSER = 5.f; ///< px : en deca, c'est un clic

			/// Minuscules et sans accents (UTF-8 latin-1 courant).
			void Plier(const char *s, char *out, uint32 cap) {
				uint32 n = 0;
				for (const unsigned char *p = reinterpret_cast<const unsigned char *>(s); *p != 0u && n + 1u < cap; ++p) {
					unsigned char ch = *p;
					if (ch == 0xC3u && p[1] != 0u) {
						const unsigned char d = p[1];
						++p;
						if ((d >= 0x80u && d <= 0x85u) || (d >= 0xA0u && d <= 0xA5u)) {
							ch = 'a';
						} else if (d == 0x87u || d == 0xA7u) {
							ch = 'c';
						} else if ((d >= 0x88u && d <= 0x8Bu) || (d >= 0xA8u && d <= 0xABu)) {
							ch = 'e';
						} else if ((d >= 0x8Cu && d <= 0x8Fu) || (d >= 0xACu && d <= 0xAFu)) {
							ch = 'i';
						} else if ((d >= 0x92u && d <= 0x96u) || (d >= 0xB2u && d <= 0xB6u)) {
							ch = 'o';
						} else if ((d >= 0x99u && d <= 0x9Cu) || (d >= 0xB9u && d <= 0xBCu)) {
							ch = 'u';
						} else {
							continue;
						}
					} else if (ch >= 'A' && ch <= 'Z') {
						ch = static_cast<unsigned char>(ch - 'A' + 'a');
					}
					out[n++] = static_cast<char>(ch);
				}
				out[n] = '\0';
			}

			bool DansOnglet(const NkFamillePlacer &p, NkFamilleOngletPlacer o, int32 k, const NkFamilleElementPlacer &el) {
				if (o == NkFamilleOngletPlacer::Tout) {
					return true;
				}
				if (o == NkFamilleOngletPlacer::Favoris) {
					return k < 64 && (p.favoris & (1ull << static_cast<uint64>(k))) != 0u;
				}
				if (o == NkFamilleOngletPlacer::Recents) {
					for (uint32 i = 0; i < p.recents.Size(); ++i) {
						if (p.recents[i] == k) {
							return true;
						}
					}
					return false;
				}
				return (el.onglets & NkFamilleBitOnglet(o)) != 0u;
			}
		} // namespace

		bool NkFamilleContientPlie(const char *texte, const char *motif) noexcept {
			if (motif == nullptr || motif[0] == '\0') {
				return true;
			}
			if (texte == nullptr) {
				return false;
			}
			char a[128];
			char b[64];
			Plier(texte, a, sizeof(a));
			Plier(motif, b, sizeof(b));
			return std::strstr(a, b) != nullptr;
		}

		const char *NkFamilleNomOngletPlacer(NkFamilleOngletPlacer o) noexcept {
			switch (o) {
				case NkFamilleOngletPlacer::Favoris:  return "Favoris";
				case NkFamilleOngletPlacer::Recents:  return "Récents";
				case NkFamilleOngletPlacer::Base:     return "Base";
				case NkFamilleOngletPlacer::Lumieres: return "Lumières";
				case NkFamilleOngletPlacer::Formes:   return "Formes";
				case NkFamilleOngletPlacer::Effets:   return "Effets";
				case NkFamilleOngletPlacer::Volumes:  return "Volumes";
				default:                              return "Tout";
			}
		}

		void NkFamillePlacer::Retenir(int32 k) {
			for (uint32 i = 0; i < recents.Size(); ++i) {
				if (recents[i] == k) {
					recents.RemoveAt(i);
					break;
				}
			}
			recents.Insert(recents.Begin(), k);
			while (recents.Size() > 10u) {
				recents.PopBack();
			}
		}

		void NkFamilleIconeOngletPlacer(nkgui::NkGuiDrawList &dl, NkFamilleOngletPlacer o, float32 cx, float32 cy,
										const NkColor &col) noexcept {
			auto P = [](float32 x, float32 y) { return NkVec2{x, y}; };
			switch (o) {
				case NkFamilleOngletPlacer::Favoris: {
					NkVec2 pts[10];
					for (int32 k = 0; k < 10; ++k) {
						const float32 a = -1.5707963f + 3.14159265f * static_cast<float32>(k) / 5.f;
						const float32 r = (k % 2 == 0) ? 9.f : 4.f;
						pts[k] = P(cx + r * std::cos(a), cy + r * std::sin(a));
					}
					for (int32 k = 0; k < 10; ++k) {
						dl.AddTriangleFilled(P(cx, cy), pts[k], pts[(k + 1) % 10], col);
					}
					break;
				}
				case NkFamilleOngletPlacer::Recents:
					dl.AddCircle(P(cx, cy), 8.f, col, 1.6f);
					dl.AddLine(P(cx, cy), P(cx, cy - 5.5f), col, 1.6f);
					dl.AddLine(P(cx, cy), P(cx + 4.f, cy + 2.f), col, 1.6f);
					break;
				case NkFamilleOngletPlacer::Base:
					// Un petit bonhomme (un acteur) : tete et epaules.
					dl.AddCircleFilled(P(cx, cy - 4.f), 4.f, col);
					dl.AddTriangleFilled(P(cx - 7.f, cy + 9.f), P(cx + 7.f, cy + 9.f), P(cx, cy + 1.f), col);
					break;
				case NkFamilleOngletPlacer::Lumieres:
					dl.AddCircleFilled(P(cx, cy - 2.f), 6.f, col);
					dl.AddRectFilled(NkRect{cx - 3.f, cy + 5.f, 6.f, 4.f}, col);
					break;
				case NkFamilleOngletPlacer::Formes:
					dl.AddCircleFilled(P(cx - 3.f, cy - 2.f), 5.f, col);
					dl.AddTriangleFilled(P(cx + 1.f, cy + 9.f), P(cx + 10.f, cy + 9.f), P(cx + 5.5f, cy), col);
					dl.AddRect(NkRect{cx - 9.f, cy + 2.f, 8.f, 7.f}, col, 1.4f);
					break;
				case NkFamilleOngletPlacer::Effets: {
					const NkVec2 f[4] = {P(cx, cy - 9.f), P(cx + 6.f, cy + 2.f), P(cx, cy + 9.f), P(cx - 6.f, cy + 2.f)};
					dl.AddTriangleFilled(f[0], f[1], f[2], col);
					dl.AddTriangleFilled(f[0], f[2], f[3], col);
					break;
				}
				case NkFamilleOngletPlacer::Volumes:
					for (int32 k = 0; k < 4; ++k) {
						const float32 t = -8.f + 4.5f * static_cast<float32>(k);
						dl.AddLine(P(cx + t, cy - 8.f), P(cx + t + 2.5f, cy - 8.f), col, 1.5f);
						dl.AddLine(P(cx + t, cy + 8.f), P(cx + t + 2.5f, cy + 8.f), col, 1.5f);
						dl.AddLine(P(cx - 8.f, cy + t), P(cx - 8.f, cy + t + 2.5f), col, 1.5f);
						dl.AddLine(P(cx + 8.f, cy + t), P(cx + 8.f, cy + t + 2.5f), col, 1.5f);
					}
					break;
				default:
					for (int32 i = 0; i < 2; ++i) {
						for (int32 j = 0; j < 2; ++j) {
							dl.AddRectFilled(NkRect{cx - 8.f + 9.f * static_cast<float32>(i), cy - 8.f + 9.f * static_cast<float32>(j), 7.f, 7.f},
											 col, 1.f);
						}
					}
					break;
			}
		}

		NkFamillePlacerResultat NkFamilleDessinerPlacer(NkFamilleCtx &c, const NkRect &zone, NkFamillePlacer &p,
														const NkFamilleElementPlacer *cat, int32 n,
														NkFamilleIconePlacerFn icone, void *user, const NkRect &cible) {
			NkFamillePlacerResultat res;
			if (zone.w < 40.f || zone.h < 80.f) {
				p.appui = -1;
				p.glisse = false;
				return res;
			}
			nkgui::NkGuiContext &ctx = c.ctx;
			const nkgui::NkGuiInput &in = ctx.input;
			auto &dl = ctx.dl;
			res.aLaSouris = NkFamilleDans(zone, in.mousePos);
			dl.AddRectFilled(zone, c.pal.panneau);

			// ── L'onglet du panneau (« Place Actors » d'UE5) ────────────────
			static const char *kTitre[1] = {"Placer des acteurs"};
			int32 seul = 0;
			const float32 titreH = NkFamilleCotes::kOngletPanneau;
			NkFamilleOnglets(c, NkRect{zone.x, zone.y, zone.w, titreH}, kTitre, 1, seul);

			// ── La recherche ────────────────────────────────────────────────
			const NkRect recherche{zone.x + 6.f, zone.y + titreH + 5.f, zone.w - 12.f, 22.f};
			NkFamilleRecherche(c, recherche, p.filtre, static_cast<int32>(sizeof(p.filtre)), p.filtreFocus,
							   "Rechercher des acteurs", true, c.petite);

			const float32 haut = recherche.y + recherche.h + 6.f;
			const float32 aideH = 34.f;
			const NkRect corps{zone.x, haut, zone.w, zone.y + zone.h - haut - aideH};

			// ── Les onglets verticaux ───────────────────────────────────────
			const int32 nbOnglets = static_cast<int32>(NkFamilleOngletPlacer::Count);
			const NkRect colonne{corps.x, corps.y, ONGLETS_L, corps.h};
			dl.AddRectFilled(colonne, c.pal.fond);
			// Les huit onglets tiennent TOUJOURS dans la colonne : ils se tassent
			// (et perdent leur libelle sous 40 px), jamais ne disparaissent.
			float32 ongletH = (colonne.h - 8.f) / static_cast<float32>(nbOnglets);
			ongletH = ongletH > ONGLET_H ? ONGLET_H : (ongletH < 22.f ? 22.f : ongletH);
			const bool libelles = ongletH >= 40.f;
			for (int32 o = 0; o < nbOnglets; ++o) {
				const NkRect r{colonne.x + 3.f, colonne.y + 4.f + static_cast<float32>(o) * ongletH, ONGLETS_L - 6.f, ongletH - 4.f};
				p.ongletsRects[o] = r;
				const bool actif = p.onglet == o;
				const bool survol = NkFamilleDans(r, in.mousePos);
				if (actif) {
					dl.AddRectFilled(r, c.pal.accent, 4.f);
				} else if (survol) {
					dl.AddRectFilled(r, c.pal.boutonSurvol, 4.f);
				}
				const NkColor col = actif ? c.pal.surAccent : c.pal.texte;
				NkFamilleIconeOngletPlacer(dl, static_cast<NkFamilleOngletPlacer>(o), r.x + r.w * 0.5f,
										   libelles ? r.y + 16.f : r.y + r.h * 0.5f, col);
				if (libelles) {
					NkFamilleTexteCentre(dl, c.petite, r.x + r.w * 0.5f, r.y + 30.f,
										 NkFamilleNomOngletPlacer(static_cast<NkFamilleOngletPlacer>(o)), col);
				}
				if (survol && in.mouseClicked[0]) {
					p.onglet = o;
					p.defil = 0.f;
				}
			}

			// ── La liste ────────────────────────────────────────────────────
			p.rects.Resize(static_cast<usize>(n));
			for (int32 k = 0; k < n; ++k) {
				p.rects[static_cast<uint32>(k)] = NkRect{0.f, 0.f, 0.f, 0.f};
			}
			const NkRect liste{colonne.x + colonne.w + 4.f, corps.y + 2.f, corps.w - colonne.w - 8.f, corps.h - 4.f};
			const NkFamilleOngletPlacer onglet = static_cast<NkFamilleOngletPlacer>(p.onglet);
			NkVector<int32> ordre;
			if (onglet == NkFamilleOngletPlacer::Recents) {
				for (uint32 i = 0; i < p.recents.Size(); ++i) {
					const int32 k = p.recents[i];
					if (k >= 0 && k < n && NkFamilleContientPlie(cat[k].nom, p.filtre)) {
						ordre.PushBack(k);
					}
				}
			} else {
				// Une recherche cherche dans TOUT, comme celle d'UE5.
				const NkFamilleOngletPlacer o = p.filtre[0] != '\0' ? NkFamilleOngletPlacer::Tout : onglet;
				for (int32 k = 0; k < n; ++k) {
					if (DansOnglet(p, o, k, cat[k]) && NkFamilleContientPlie(cat[k].nom, p.filtre)) {
						ordre.PushBack(k);
					}
				}
			}
			const float32 total = static_cast<float32>(ordre.Size()) * RANGEE_H;
			if (NkFamilleDans(liste, in.mousePos) && in.wheel != 0.f) {
				p.defil -= in.wheel * RANGEE_H * 2.f;
			}
			const float32 defilMax = total - liste.h > 0.f ? total - liste.h : 0.f;
			p.defil = p.defil < 0.f ? 0.f : (p.defil > defilMax ? defilMax : p.defil);
			dl.PushClipRect(liste, true);
			int32 survole = -1;
			for (uint32 i = 0; i < ordre.Size(); ++i) {
				const int32 k = ordre[i];
				const NkFamilleElementPlacer &el = cat[k];
				const NkRect r{liste.x, liste.y + static_cast<float32>(i) * RANGEE_H - p.defil, liste.w, RANGEE_H - 2.f};
				if (r.y + r.h < liste.y || r.y > liste.y + liste.h) {
					continue;
				}
				p.rects[static_cast<uint32>(k)] = r;
				const bool survol = NkFamilleDans(r, in.mousePos) && NkFamilleDans(liste, in.mousePos);
				if (survol) {
					survole = k;
				}
				const NkRect etoile{r.x + r.w - 20.f, r.y + (r.h - 16.f) * 0.5f, 16.f, 16.f};
				const bool favori = k < 64 && (p.favoris & (1ull << static_cast<uint64>(k))) != 0u;
				dl.AddRectFilled(r, p.appui == k ? c.pal.boutonSurvol : (survol ? c.pal.entete : c.pal.bouton), 3.f);
				const NkRect rIcone{r.x + 4.f, r.y + 3.f, r.h - 6.f, r.h - 6.f};
				dl.AddRectFilled(rIcone, c.pal.fond, 3.f);
				if (icone != nullptr) {
					icone(user, dl, k, rIcone, c.pal.texte);
				}
				const float32 ty = r.y + (r.h - NkFamilleHauteurLigne(c.police, 16.f)) * 0.5f;
				dl.PushClipRect(NkRect{rIcone.x + rIcone.w + 6.f, r.y, r.w - rIcone.w - 34.f, r.h}, true);
				NkFamilleTexte(dl, c.police, rIcone.x + rIcone.w + 8.f, ty, el.nom, c.pal.texte);
				dl.PopClipRect();
				if (survol || favori) {
					// L'etoile des Favoris (UE5) : pleine si favori.
					NkVec2 pts[10];
					const float32 ex = etoile.x + 8.f, ey = etoile.y + 8.f;
					for (int32 j = 0; j < 10; ++j) {
						const float32 a = -1.5707963f + 3.14159265f * static_cast<float32>(j) / 5.f;
						const float32 rr = (j % 2 == 0) ? 7.f : 3.f;
						pts[j] = NkVec2{ex + rr * std::cos(a), ey + rr * std::sin(a)};
					}
					if (favori) {
						for (int32 j = 0; j < 10; ++j) {
							dl.AddTriangleFilled(NkVec2{ex, ey}, pts[j], pts[(j + 1) % 10], c.pal.selection);
						}
					} else {
						dl.AddPolyline(pts, 10, c.pal.attenue, 1.2f, true);
					}
				}
				if (survol && in.mouseClicked[0] && p.appui < 0) {
					if (NkFamilleDans(etoile, in.mousePos) && k < 64) {
						p.favoris ^= (1ull << static_cast<uint64>(k));
					} else {
						p.appui = k;
						p.glisse = false;
						p.depart = in.mousePos;
					}
				}
			}
			dl.PopClipRect();
			if (ordre.Empty()) {
				const char *vide = onglet == NkFamilleOngletPlacer::Favoris  ? "Aucun favori : l'étoile d'une ligne l'y met."
								   : onglet == NkFamilleOngletPlacer::Recents ? "Rien de posé pour l'instant."
																		 : "Aucun résultat.";
				NkFamilleTexte(dl, c.petite, liste.x + 4.f, liste.y + 6.f, vide, c.pal.attenue);
			}

			// ── L'aide, en bas : ce que fait le geste, et l'element survole ─
			{
				const NkRect aide{zone.x + 6.f, zone.y + zone.h - aideH + 2.f, zone.w - 12.f, aideH - 4.f};
				dl.AddRectFilled(NkRect{zone.x, aide.y - 3.f, zone.w, 1.f}, c.pal.bord);
				dl.PushClipRect(aide, true);
				const bool aideElement = survole >= 0 && cat[survole].aide != nullptr && cat[survole].aide[0] != '\0';
				NkFamilleTexte(dl, c.petite, aide.x, aide.y,
							   aideElement ? cat[survole].aide : "Glisser dans la vue : poser au point lâché.", c.pal.attenue);
				NkFamilleTexte(dl, c.petite, aide.x, aide.y + 14.f,
							   survole >= 0 ? "Clic : au centre de la vue · glisser : où vous voulez" : "Clic : au centre de la vue.",
							   c.pal.attenue);
				dl.PopClipRect();
			}

			// ── Le geste : clic = au centre ; glisser = la ou on lache ──────
			if (p.appui >= 0 && p.appui < n) {
				const int32 k = p.appui;
				const float32 dx = in.mousePos.x - p.depart.x, dy = in.mousePos.y - p.depart.y;
				if (!p.glisse && dx * dx + dy * dy > SEUIL_GLISSER * SEUIL_GLISSER) {
					p.glisse = true;
				}
				if (in.mouseReleased[0] || !in.mouseDown[0]) {
					if (!p.glisse) {
						res.clic = k;
					} else if (NkFamilleDans(cible, in.mousePos)) {
						res.lache = k;
						res.lachePos = in.mousePos;
					}
					p.appui = -1;
					p.glisse = false;
				} else if (p.glisse) {
					res.glisse = k;
					// Le FANTOME : l'icone et le nom sous le curseur ; sur la vue, la
					// croix du point de pose.
					auto &over = ctx.dlOverlay;
					const char *nom = cat[k].nom;
					const float32 w = NkFamilleLargeur(c.police, nom) + 40.f;
					const NkRect g{in.mousePos.x + 14.f, in.mousePos.y + 10.f, w, 26.f};
					over.AddRectFilled(g, c.pal.entete, 3.f);
					const bool surVue = NkFamilleDans(cible, in.mousePos);
					over.AddRect(g, surVue ? c.pal.selection : c.pal.accent, 1.f, 3.f);
					if (icone != nullptr) {
						icone(user, over, k, NkRect{g.x + 3.f, g.y + 3.f, 20.f, 20.f}, c.pal.texte);
					}
					NkFamilleTexte(over, c.police, g.x + 28.f, g.y + (g.h - NkFamilleHauteurLigne(c.police, 16.f)) * 0.5f, nom,
								   c.pal.texte);
					if (surVue) {
						const NkVec2 q{in.mousePos.x, in.mousePos.y};
						over.AddLine(NkVec2{q.x - 8.f, q.y}, NkVec2{q.x + 8.f, q.y}, c.pal.selection, 1.5f);
						over.AddLine(NkVec2{q.x, q.y - 8.f}, NkVec2{q.x, q.y + 8.f}, c.pal.selection, 1.5f);
					}
				}
			}
			if (res.clic >= 0) {
				p.Retenir(res.clic);
			}
			if (res.lache >= 0) {
				p.Retenir(res.lache);
			}
			return res;
		}

	} // namespace editorkit
} // namespace nkentseu
