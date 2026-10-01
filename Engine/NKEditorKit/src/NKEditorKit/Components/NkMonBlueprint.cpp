// -----------------------------------------------------------------------------
// FICHIER: NKEditorKit/Components/NkMonBlueprint.cpp
// DESCRIPTION: Le panneau « Mon Blueprint » generique (NkMonBlueprint.h).
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NKEditorKit/Components/NkMonBlueprint.h"

#include "NKEditorKit/Components/NkMenuNoeuds.h"
#include "NKEditorKit/NkEditorTextField.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editorkit {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			bool Dans(const NkRect &r, const NkVec2 &p) {
				return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h;
			}
			void Texte(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, float32 x, float32 top, const char *s, const NkColor &c,
					   float32 echelle = 1.f, float32 maxW = -1.f) {
				if (f == nullptr || f->Face() == nullptr || s == nullptr || s[0] == '\0') {
					return;
				}
				const NkVec2 base{x, top + f->Ascent() * echelle};
				if (echelle > 0.97f && echelle < 1.03f) {
					dl.AddText(f->Face(), f->TexId(), base, s, c, maxW);
				} else {
					dl.AddTextScaled(f->Face(), f->TexId(), base, s, c, echelle, maxW);
				}
			}
			float32 Largeur(nkgui::NkGuiFont *f, const char *s, float32 e = 1.f) {
				return f != nullptr && f->Face() != nullptr && s != nullptr ? f->MeasureWidth(s) * e : 0.f;
			}
			float32 Ligne(nkgui::NkGuiFont *f) {
				return f != nullptr && f->Face() != nullptr ? f->LineHeight() : 14.f;
			}
			bool Repliee(const NkEtatMonBlueprint &e, const NkString &t) {
				for (uint32 i = 0; i < e.replies.Size(); ++i) {
					if (e.replies[i] == t) {
						return true;
					}
				}
				return false;
			}
			void Plus(nkgui::NkGuiDrawList &dl, const NkRect &r, const NkColor &c) {
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
				dl.AddLine(NkVec2{cx - 5.f, cy}, NkVec2{cx + 5.f, cy}, c, 1.6f);
				dl.AddLine(NkVec2{cx, cy - 5.f}, NkVec2{cx, cy + 5.f}, c, 1.6f);
			}
		} // namespace

		void NkMonBlueprintRenommer(NkEtatMonBlueprint &e, int32 section, int32 id, const char *nom) {
			e.renommeSection = section;
			e.renommeId = id;
			std::snprintf(e.renomme, sizeof(e.renomme), "%s", nom != nullptr ? nom : "");
		}

		NkResultatMonBlueprint NkMonBlueprint(nkgui::NkGuiContext &ctx, nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *police, const NkRect &r,
											  const NkVector<NkSectionPanneau> &sections, NkEtatMonBlueprint &e, const NkJetonsNodal &s,
											  const char *titre) {
			NkResultatMonBlueprint res;
			nkgui::NkGuiInput &in = ctx.input;
			const NkVec2 souris = in.mousePos;
			const bool dessus = Dans(r, souris);
			const float32 lh = Ligne(police);
			e.ageChoix += in.dt > 0.f ? in.dt : 1.f / 60.f;
			dl.PushClipRect(r);
			dl.AddRectFilled(r, s.panneauFond);
			// ── L'en-tete : le titre et « + Ajouter » ──
			float32 y = r.y + 6.f;
			Texte(dl, police, r.x + 10.f, y + 3.f, titre != nullptr ? titre : "Mon Blueprint", s.sectionTexte, 0.95f);
			e.rectAjouter = NkRect{r.x + r.w - 92.f, y, 84.f, 22.f};
			{
				const bool sur = Dans(e.rectAjouter, souris);
				dl.AddRectFilled(e.rectAjouter, sur ? s.menuSurvol : s.sectionFond, 3.f);
				dl.AddRect(e.rectAjouter, s.bord, 1.f, 3.f);
				Plus(dl, NkRect{e.rectAjouter.x + 2.f, e.rectAjouter.y, 18.f, 22.f}, s.exec);
				Texte(dl, police, e.rectAjouter.x + 20.f, e.rectAjouter.y + (22.f - lh * 0.86f) * 0.5f, "Ajouter", s.texte, 0.86f);
				const float32 cx = e.rectAjouter.x + e.rectAjouter.w - 10.f, cy = e.rectAjouter.y + 11.f;
				dl.AddTriangleFilled(NkVec2{cx - 3.5f, cy - 2.f}, NkVec2{cx + 3.5f, cy - 2.f}, NkVec2{cx, cy + 2.5f}, s.attenue);
				if (sur && in.mouseClicked[0]) {
					e.menuAjouter = !e.menuAjouter;
					in.mouseClicked[0] = false;
				}
			}
			y += 28.f;
			// Le menu « + Ajouter » ouvert passe AU-DESSUS de la liste : la souris
			// sur lui n'est pas a la liste.
			uint32 nbPlus = 0;
			for (uint32 si = 0; si < sections.Size(); ++si) {
				nbPlus += sections[si].plus ? 1u : 0u;
			}
			const NkRect menuRect{e.rectAjouter.x + e.rectAjouter.w - 190.f, e.rectAjouter.y + e.rectAjouter.h + 2.f, 190.f,
								  static_cast<float32>(nbPlus) * 24.f + 8.f};
			const bool surMenu = e.menuAjouter && Dans(menuRect, souris);
			// ── La recherche ──
			const NkRect rech{r.x + 8.f, y, r.w - 16.f, 22.f};
			dl.AddRectFilled(rech, s.champ, 3.f);
			dl.AddRect(rech, e.filtreFocus ? s.exec : s.champBord, 1.f, 3.f);
			if (in.mouseClicked[0]) {
				e.filtreFocus = Dans(rech, souris);
			}
			if (e.filtre[0] == '\0' && !e.filtreFocus) {
				Texte(dl, police, rech.x + 8.f, rech.y + (22.f - lh * 0.86f) * 0.5f, "Rechercher", s.attenue, 0.86f);
			}
			NkOverlayFieldStyle st;
			st.fond = false;
			st.bord = false;
			st.texte = s.texteEntete;
			st.utf8 = true;
			NkOverlayTextField(ctx, dl, police, NkRect{rech.x + 4.f, rech.y, rech.w - 8.f, rech.h}, e.filtre, static_cast<int32>(sizeof(e.filtre)),
							   e.filtreFocus, &st);
			if (e.filtreFocus) {
				res.clavierPris = true;
				if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
					e.filtre[0] = '\0';
					e.filtreFocus = false;
				}
			}
			y += 28.f;
			// ── Les sections ──
			const NkRect liste{r.x, y, r.w, r.y + r.h - y};
			if (dessus && Dans(liste, souris) && in.wheel != 0.f) {
				e.defil -= in.wheel * 40.f;
				in.wheel = 0.f;
			}
			e.defil = e.defil < 0.f ? 0.f : e.defil;
			e.rects.Clear();
			e.sections.Clear();
			e.ids.Clear();
			e.rectsPlus.Clear();
			dl.PushClipRect(liste);
			float32 yy = liste.y - e.defil;
			const float32 hs = s.hauteurSection, he = s.hauteurElement;
			for (uint32 si = 0; si < sections.Size(); ++si) {
				const NkSectionPanneau &sec = sections[si];
				const bool rep = Repliee(e, sec.titre) && e.filtre[0] == '\0';
				const NkRect rs{r.x, yy, r.w, hs};
				dl.AddRectFilled(rs, s.sectionFond);
				dl.AddLine(NkVec2{rs.x, rs.y + rs.h - 0.5f}, NkVec2{rs.x + rs.w, rs.y + rs.h - 0.5f}, s.bord, 1.f);
				const float32 tx = rs.x + 10.f, ty = rs.y + hs * 0.5f;
				if (rep) {
					dl.AddTriangleFilled(NkVec2{tx, ty - 4.f}, NkVec2{tx + 5.f, ty}, NkVec2{tx, ty + 4.f}, s.triangleTitre);
				} else {
					dl.AddTriangleFilled(NkVec2{tx - 1.f, ty - 3.f}, NkVec2{tx + 7.f, ty - 3.f}, NkVec2{tx + 3.f, ty + 2.f}, s.triangleTitre);
				}
				char t[96];
				std::snprintf(t, sizeof(t), "%s", sec.titre.CStr());
				Texte(dl, police, tx + 14.f, rs.y + (hs - lh * 0.9f) * 0.5f, t, s.sectionTexte, 0.9f, r.w - 70.f);
				if (!sec.elements.Empty()) {
					char n[16];
					std::snprintf(n, sizeof(n), "%u", static_cast<unsigned>(sec.elements.Size()));
					Texte(dl, police, tx + 20.f + Largeur(police, t, 0.9f), rs.y + (hs - lh * 0.8f) * 0.5f, n, s.attenue, 0.8f);
				}
				NkRect rp{0.f, 0.f, 0.f, 0.f};
				if (sec.plus) {
					rp = NkRect{rs.x + rs.w - 26.f, rs.y + 3.f, 20.f, hs - 6.f};
					const bool sur = Dans(rp, souris) && Dans(liste, souris) && !surMenu;
					if (sur) {
						dl.AddRectFilled(rp, s.menuSurvol, 3.f);
					}
					Plus(dl, rp, sur ? s.exec : s.texte);
					if (sur && in.mouseClicked[0]) {
						res.ajouter = static_cast<int32>(si);
						in.mouseClicked[0] = false;
					}
				}
				e.rectsPlus.PushBack(rp);
				if (Dans(rs, souris) && Dans(liste, souris) && in.mouseClicked[0] && !Dans(rp, souris) && !surMenu) {
					if (rep) {
						for (uint32 q = 0; q < e.replies.Size(); ++q) {
							if (e.replies[q] == sec.titre) {
								e.replies.Erase(e.replies.Begin() + q);
								break;
							}
						}
					} else {
						e.replies.PushBack(sec.titre);
					}
					in.mouseClicked[0] = false;
				}
				yy += hs;
				if (rep) {
					continue;
				}
				for (uint32 k = 0; k < sec.elements.Size(); ++k) {
					const NkElementPanneau &el = sec.elements[k];
					if (e.filtre[0] != '\0' && !NkContientReplie(el.libelle.CStr(), e.filtre)) {
						continue;
					}
					const NkRect re{r.x, yy, r.w, he};
					const bool choisi = e.choixSection == static_cast<int32>(si) && e.choixId == el.id;
					const bool sur = Dans(re, souris) && Dans(liste, souris) && !surMenu;
					if (choisi) {
						dl.AddRectFilled(re, s.elementChoisi);
						dl.AddRectFilled(NkRect{re.x, re.y, 2.f, re.h}, s.elementChoisiBord);
					} else if (sur) {
						dl.AddRectFilled(re, s.elementSurvol);
					}
					float32 x = re.x + 24.f;
					if (el.pastille) {
						// La CAPSULE du type (UE5) : la couleur de la famille, et son glyphe.
						const float32 pw = el.glyphe.Empty() ? 18.f : Largeur(police, el.glyphe.CStr(), 0.7f) + 10.f;
						const NkRect p{x, re.y + (he - 12.f) * 0.5f, pw, 12.f};
						dl.AddRectFilled(p, el.glyphe.Empty() ? el.couleur : NkAlphaNodal(el.couleur, 0.26f), 6.f);
						if (!el.glyphe.Empty()) {
							Texte(dl, police, p.x + 5.f, p.y + (12.f - lh * 0.7f) * 0.5f, el.glyphe.CStr(), el.couleur, 0.7f);
						}
						x += pw + 8.f;
					}
					const bool enRenommage = e.renommeSection == static_cast<int32>(si) && e.renommeId == el.id;
					if (enRenommage) {
						const NkRect rr{x - 2.f, re.y + 2.f, re.x + re.w - x - 6.f, he - 4.f};
						dl.AddRectFilled(rr, s.champ, 2.f);
						dl.AddRect(rr, s.exec, 1.f, 2.f);
						NkOverlayFieldStyle st2;
						st2.fond = false;
						st2.bord = false;
						st2.texte = s.texteEntete;
						st2.utf8 = true;
						NkOverlayTextField(ctx, dl, police, rr, e.renomme, static_cast<int32>(sizeof(e.renomme)), true, &st2);
						res.clavierPris = true;
						if (in.KeyPressed(nkgui::NkGuiKey::Enter) || (in.mouseClicked[0] && !Dans(rr, souris))) {
							res.renomme = true;
							res.choisiSection = static_cast<int32>(si);
							res.choisiId = el.id;
							res.nouveauNom = e.renomme;
							e.renommeSection = e.renommeId = -1;
							in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Enter)] = false;
						} else if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
							e.renommeSection = e.renommeId = -1;
							in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Escape)] = false;
						}
					} else {
						const float32 sw = Largeur(police, el.sousTexte.CStr(), 0.8f);
						Texte(dl, police, x, re.y + (he - lh) * 0.5f, el.libelle.CStr(), s.texte, 1.f, re.x + re.w - x - sw - 16.f);
						Texte(dl, police, re.x + re.w - sw - 10.f, re.y + (he - lh * 0.8f) * 0.5f, el.sousTexte.CStr(), s.attenue, 0.8f);
					}
					e.rects.PushBack(re);
					e.sections.PushBack(static_cast<int32>(si));
					e.ids.PushBack(el.id);
					if (sur && !enRenommage) {
						if (in.mouseClicked[0]) {
							if (in.mouseDoubleClicked[0]) {
								res.doubleClic = true;
							} else if (choisi && e.ageChoix > 0.5f && e.ageChoix < 1.6f) {
								// Le clic LENT sur l'element deja choisi : renommer.
								NkMonBlueprintRenommer(e, static_cast<int32>(si), el.id, el.libelle.CStr());
							}
							e.choixSection = static_cast<int32>(si);
							e.choixId = el.id;
							if (!choisi) {
								e.ageChoix = 0.f;
							}
							res.choisiSection = e.choixSection;
							res.choisiId = e.choixId;
							e.appui = true;
							e.appuiSection = static_cast<int32>(si);
							e.appuiId = el.id;
							e.appuiX = souris.x;
							e.appuiY = souris.y;
							in.mouseClicked[0] = false;
						} else if (in.mouseClicked[1]) {
							e.choixSection = static_cast<int32>(si);
							e.choixId = el.id;
							res.choisiSection = e.choixSection;
							res.choisiId = e.choixId;
							res.menu = true;
							res.menuX = souris.x;
							res.menuY = souris.y;
							in.mouseClicked[1] = false;
						}
					}
					yy += he;
				}
				yy += 4.f;
			}
			dl.PopClipRect();
			// ── Le menu « + Ajouter » ──
			e.rectsMenuAjouter.Clear();
			if (e.menuAjouter) {
				const NkRect m = menuRect;
				float32 my = m.y;
				dl.AddRectFilled(NkRect{m.x + 3.f, m.y + 4.f, m.w, m.h}, s.menuOmbre, 4.f);
				dl.AddRectFilled(m, s.menuFond, 4.f);
				dl.AddRect(m, s.menuBord, 1.f, 4.f);
				my += 4.f;
				for (uint32 si = 0; si < sections.Size(); ++si) {
					if (!sections[si].plus) {
						e.rectsMenuAjouter.PushBack(NkRect{0.f, 0.f, 0.f, 0.f});
						continue;
					}
					const NkRect ri{m.x + 4.f, my, m.w - 8.f, 24.f};
					const bool sur = Dans(ri, souris);
					if (sur) {
						dl.AddRectFilled(ri, s.menuSurvol, 3.f);
					}
					Plus(dl, NkRect{ri.x + 2.f, ri.y, 18.f, 24.f}, s.exec);
					Texte(dl, police, ri.x + 24.f, ri.y + (24.f - lh) * 0.5f, sections[si].titre.CStr(), s.texte);
					e.rectsMenuAjouter.PushBack(ri);
					if (sur && in.mouseClicked[0]) {
						res.ajouter = static_cast<int32>(si);
						e.menuAjouter = false;
						in.mouseClicked[0] = false;
					}
					my += 24.f;
				}
				if (in.mouseClicked[0] && !Dans(m, souris) && !Dans(e.rectAjouter, souris)) {
					e.menuAjouter = false;
				}
			}
			// ── Le GLISSER : un element tire hors de sa place ──
			if (e.appui) {
				if (!in.mouseDown[0]) {
					if (e.glisse && !Dans(r, souris)) {
						res.depose = true;
						res.glisseSection = e.appuiSection;
						res.glisseId = e.appuiId;
						res.x = souris.x;
						res.y = souris.y;
					}
					e.appui = false;
					e.glisse = false;
				} else {
					const float32 dx = souris.x - e.appuiX, dy = souris.y - e.appuiY;
					e.glisse = e.glisse || dx * dx + dy * dy > 36.f;
					if (e.glisse) {
						res.glisseEnCours = true;
						res.glisseSection = e.appuiSection;
						res.glisseId = e.appuiId;
						res.x = souris.x;
						res.y = souris.y;
					}
				}
			}
			// ── Le clavier : F2 renomme, Suppr supprime (la souris sur le panneau) ──
			if (dessus && !e.filtreFocus && e.renommeSection < 0 && e.choixSection >= 0) {
				if (in.KeyPressed(nkgui::NkGuiKey::F2)) {
					for (uint32 k = 0; k < static_cast<uint32>(sections.Size()); ++k) {
						for (uint32 q = 0; static_cast<int32>(k) == e.choixSection && q < sections[k].elements.Size(); ++q) {
							if (sections[k].elements[q].id == e.choixId) {
								NkMonBlueprintRenommer(e, e.choixSection, e.choixId, sections[k].elements[q].libelle.CStr());
							}
						}
					}
					in.keyInit[static_cast<int32>(nkgui::NkGuiKey::F2)] = false;
					res.clavierPris = true;
				} else if (in.KeyPressed(nkgui::NkGuiKey::Delete)) {
					res.supprimer = true;
					res.choisiSection = e.choixSection;
					res.choisiId = e.choixId;
					in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Delete)] = false;
					res.clavierPris = true;
				}
			}
			dl.PopClipRect();
			if (dessus) {
				in.mouseClicked[0] = false;
				in.mouseClicked[1] = false;
			}
			return res;
		}

	} // namespace editorkit
} // namespace nkentseu
