// -----------------------------------------------------------------------------
// FICHIER: NKEditorKit/Components/NkMenuNoeuds.cpp
// DESCRIPTION: Le menu des noeuds de la toile (NkMenuNoeuds.h) : recherche,
//              categories repliables, contexte ecrit et retirable, clavier.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NKEditorKit/Components/NkMenuNoeuds.h"

#include "NKEditorKit/NkEditorTextField.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editorkit {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		NkString NkReplierTexte(const char *texte) {
			NkString r;
			if (texte == nullptr) {
				return r;
			}
			const unsigned char *p = reinterpret_cast<const unsigned char *>(texte);
			while (*p != 0u) {
				const unsigned char c = *p;
				if (c < 0x80u) {
					r.Append(static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c));
					++p;
					continue;
				}
				if (c == 0xC3u && p[1] != 0u) {
					// Latin-1 : les lettres accentuees les plus courantes du francais.
					const unsigned char d = p[1];
					char b = '?';
					if ((d >= 0x80u && d <= 0x85u) || (d >= 0xA0u && d <= 0xA5u)) {
						b = 'a';
					} else if (d == 0x87u || d == 0xA7u) {
						b = 'c';
					} else if ((d >= 0x88u && d <= 0x8Bu) || (d >= 0xA8u && d <= 0xABu)) {
						b = 'e';
					} else if ((d >= 0x8Cu && d <= 0x8Fu) || (d >= 0xACu && d <= 0xAFu)) {
						b = 'i';
					} else if ((d >= 0x92u && d <= 0x96u) || (d >= 0xB2u && d <= 0xB6u)) {
						b = 'o';
					} else if ((d >= 0x99u && d <= 0x9Cu) || (d >= 0xB9u && d <= 0xBCu)) {
						b = 'u';
					}
					r.Append(b);
					p += 2;
					continue;
				}
				// Un autre caractere multi-octets : recopie tel quel.
				r.Append(static_cast<char>(c));
				++p;
			}
			return r;
		}

		bool NkContientReplie(const char *texte, const char *motif) {
			if (motif == nullptr || motif[0] == '\0') {
				return true;
			}
			const NkString a = NkReplierTexte(texte), b = NkReplierTexte(motif);
			return std::strstr(a.CStr(), b.CStr()) != nullptr;
		}

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
			bool Repliee(const NkEtatMenuNoeuds &e, const NkString &c) {
				for (uint32 i = 0; i < e.replies.Size(); ++i) {
					if (e.replies[i] == c) {
						return true;
					}
				}
				return false;
			}
			/// Une rangee affichee : une categorie (titre) ou une entree.
			struct Rangee {
					int32 entree = -1; ///< -1 : une categorie
					NkString categorie;
					uint32 niveau = 0;
			};
			/// La categorie de niveau `n` (« A|B|C », n = 1 -> « A|B »).
			NkString Prefixe(const NkString &c, uint32 n) {
				uint32 vus = 0;
				for (usize i = 0; i < c.Length(); ++i) {
					if (c.CStr()[i] == '|') {
						if (++vus == n) {
							return NkString(c.CStr(), i);
						}
					}
				}
				return c;
			}
			uint32 Profondeur(const NkString &c) {
				uint32 n = 1;
				for (usize i = 0; i < c.Length(); ++i) {
					n += c.CStr()[i] == '|' ? 1u : 0u;
				}
				return c.Empty() ? 0u : n;
			}
			const char *Dernier(const NkString &c) {
				const char *p = std::strrchr(c.CStr(), '|');
				return p != nullptr ? p + 1 : c.CStr();
			}
		} // namespace

		int32 NkMenuNoeuds(nkgui::NkGuiContext &ctx, nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *police, NkEtatMenuNoeuds &e,
						   const NkVector<NkEntreeMenuNoeud> &entrees, const NkJetonsNodal &s, const char *titre) {
			if (!e.ouvert) {
				return NK_MENU_RIEN;
			}
			nkgui::NkGuiInput &in = ctx.input;
			const float32 W = s.menuLargeur, H = s.menuHauteur;
			// Dans l'ecran (le contexte connait sa taille par son dessin : on borne
			// sur la zone de decoupe courante).
			const NkRect ecran = dl.CurrentClip();
			float32 x = e.x, y = e.y;
			if (ecran.w > 0.f) {
				x = x + W > ecran.x + ecran.w ? ecran.x + ecran.w - W - 4.f : x;
				y = y + H > ecran.y + ecran.h ? ecran.y + ecran.h - H - 4.f : y;
				x = x < ecran.x + 2.f ? ecran.x + 2.f : x;
				y = y < ecran.y + 2.f ? ecran.y + 2.f : y;
			}
			const NkRect r{x, y, W, H};
			e.rect = r;
			const NkVec2 souris = in.mousePos;
			int32 resultat = NK_MENU_RIEN;
			// Un clic DEHORS ferme (et ne traverse pas).
			if ((in.mouseClicked[0] || in.mouseClicked[1]) && !Dans(r, souris)) {
				e.ouvert = false;
				in.mouseClicked[0] = false;
				in.mouseClicked[1] = false;
				return NK_MENU_FERME;
			}
			const float32 lh = Ligne(police);
			// L'ombre, le fond.
			dl.AddRectFilled(NkRect{r.x + 4.f, r.y + 6.f, r.w, r.h}, s.menuOmbre, 6.f);
			dl.AddRectFilled(r, s.menuFond, 6.f);
			dl.AddRect(r, s.menuBord, 1.f, 6.f);
			// L'en-tete : le titre, et la case « Sensible au contexte ».
			float32 cy = r.y + 8.f;
			Texte(dl, police, r.x + 10.f, cy, titre != nullptr ? titre : "", s.texteEntete, 0.9f, r.w - 150.f);
			{
				const char *lib = "Sensible au contexte";
				const float32 lw = Largeur(police, lib, 0.82f);
				const NkRect c{r.x + r.w - lw - 30.f, cy + 1.f, 12.f, 12.f};
				e.rectCase = NkRect{c.x, c.y - 2.f, lw + 22.f, 16.f};
				dl.AddRectFilled(c, e.sensibleContexte ? s.menuCategorie : s.champ, 2.f);
				dl.AddRect(c, s.champBord, 1.f, 2.f);
				if (e.sensibleContexte) {
					dl.AddLine(NkVec2{c.x + 2.5f, c.y + 6.f}, NkVec2{c.x + 5.f, c.y + 9.f}, NkColor(30, 20, 8, 255), 1.6f);
					dl.AddLine(NkVec2{c.x + 5.f, c.y + 9.f}, NkVec2{c.x + 10.f, c.y + 3.f}, NkColor(30, 20, 8, 255), 1.6f);
				}
				Texte(dl, police, c.x + 18.f, cy, lib, s.attenue, 0.82f);
				if (in.mouseClicked[0] && Dans(e.rectCase, souris)) {
					e.sensibleContexte = !e.sensibleContexte;
					in.mouseClicked[0] = false;
					resultat = NK_MENU_CONTEXTE;
				}
			}
			cy += lh + 6.f;
			// La recherche, qui a le clavier.
			e.rectRecherche = NkRect{r.x + 8.f, cy, r.w - 16.f, 24.f};
			dl.AddRectFilled(e.rectRecherche, s.champ, 3.f);
			dl.AddRect(e.rectRecherche, e.filtreFocus ? s.menuCategorie : s.champBord, 1.f, 3.f);
			if (in.mouseClicked[0] && Dans(e.rectRecherche, souris)) {
				e.filtreFocus = true;
			}
			if (e.filtre[0] == '\0') {
				Texte(dl, police, e.rectRecherche.x + 8.f, e.rectRecherche.y + (24.f - lh * 0.9f) * 0.5f, "Rechercher un nœud…", s.attenue, 0.9f);
			}
			NkOverlayFieldStyle st;
			st.fond = false;
			st.bord = false;
			st.texte = s.texteEntete;
			st.utf8 = true;
			char avant[64];
			std::snprintf(avant, sizeof(avant), "%s", e.filtre);
			NkOverlayTextField(ctx, dl, police, NkRect{e.rectRecherche.x + 4.f, e.rectRecherche.y, e.rectRecherche.w - 8.f, e.rectRecherche.h},
							   e.filtre, static_cast<int32>(sizeof(e.filtre)), e.filtreFocus, &st);
			if (std::strcmp(avant, e.filtre) != 0) {
				e.clavier = 0; // une frappe : la premiere entree est surlignee
				e.defil = 0.f;
			}
			cy += 30.f;
			// Le FILTRE de contexte, ECRIT et RETIRABLE (planche 07).
			if (!e.contexte.Empty() && e.sensibleContexte) {
				Texte(dl, police, r.x + 10.f, cy + 2.f, e.contexte.CStr(), s.attenue, 0.82f, r.w - 60.f);
				const float32 lw = Largeur(police, e.contexte.CStr(), 0.82f);
				if (!e.contexteGlyphe.Empty()) {
					const float32 gw = Largeur(police, e.contexteGlyphe.CStr(), 0.72f) + 8.f;
					const NkRect p{r.x + 16.f + lw, cy + 2.f, gw, 13.f};
					dl.AddRectFilled(p, NkAlphaNodal(e.contexteCouleur, s.pastilleAlpha), 2.f);
					Texte(dl, police, p.x + 4.f, p.y + (13.f - lh * 0.72f) * 0.5f, e.contexteGlyphe.CStr(), e.contexteCouleur, 0.72f);
				}
				e.rectContexte = NkRect{r.x + r.w - 26.f, cy, 18.f, 18.f};
				const NkColor cc = Dans(e.rectContexte, souris) ? s.texteEntete : s.attenue;
				dl.AddLine(NkVec2{e.rectContexte.x + 5.f, e.rectContexte.y + 5.f}, NkVec2{e.rectContexte.x + 13.f, e.rectContexte.y + 13.f}, cc, 1.4f);
				dl.AddLine(NkVec2{e.rectContexte.x + 13.f, e.rectContexte.y + 5.f}, NkVec2{e.rectContexte.x + 5.f, e.rectContexte.y + 13.f}, cc, 1.4f);
				if (in.mouseClicked[0] && Dans(e.rectContexte, souris)) {
					e.sensibleContexte = false;
					in.mouseClicked[0] = false;
					resultat = NK_MENU_CONTEXTE;
				}
				cy += 22.f;
			} else {
				e.rectContexte = NkRect{0.f, 0.f, 0.f, 0.f};
			}
			// ── La LISTE : categories puis entrees ──
			const bool recherche = e.filtre[0] != '\0';
			NkVector<Rangee> rangees;
			{
				NkString derniere;
				bool premiere = true;
				for (uint32 i = 0; i < entrees.Size(); ++i) {
					const NkEntreeMenuNoeud &x2 = entrees[i];
					if (recherche && !NkContientReplie(x2.libelle.CStr(), e.filtre) && !NkContientReplie(x2.categorie.CStr(), e.filtre)) {
						continue;
					}
					// Les titres de categorie qui changent (A, A|B...).
					const uint32 prof = Profondeur(x2.categorie);
					for (uint32 n = 1; n <= prof; ++n) {
						const NkString c = Prefixe(x2.categorie, n);
						if (premiere || !(Prefixe(derniere, n) == c) || Profondeur(derniere) < n) {
							// Une categorie dont un ancetre est replie ne s'affiche pas.
							bool cache = false;
							for (uint32 m = 1; m < n && !recherche; ++m) {
								cache = cache || Repliee(e, Prefixe(x2.categorie, m));
							}
							if (!cache) {
								Rangee rg;
								rg.categorie = c;
								rg.niveau = n - 1u;
								rangees.PushBack(rg);
							}
						}
					}
					premiere = false;
					derniere = x2.categorie;
					bool cache = false;
					for (uint32 n = 1; n <= prof && !recherche; ++n) {
						cache = cache || Repliee(e, Prefixe(x2.categorie, n));
					}
					if (!cache) {
						Rangee rg;
						rg.entree = static_cast<int32>(i);
						rg.niveau = prof;
						rangees.PushBack(rg);
					}
				}
			}
			const float32 bas = 46.f; // l'aide de l'entree survolee
			const NkRect liste{r.x + 4.f, cy, r.w - 8.f, r.y + r.h - cy - bas};
			const float32 rh = s.menuRangee;
			// Le clavier : Haut / Bas sur les ENTREES, Entree choisit, Echap ferme.
			NkVector<uint32> visibles; // indices de rangees qui sont des entrees
			for (uint32 i = 0; i < rangees.Size(); ++i) {
				if (rangees[i].entree >= 0) {
					visibles.PushBack(i);
				}
			}
			e.nbVisibles = static_cast<uint32>(visibles.Size());
			if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
				e.ouvert = false;
				in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Escape)] = false;
				return NK_MENU_FERME;
			}
			if (in.KeyPressedRepeat(nkgui::NkGuiKey::Down) && !visibles.Empty()) {
				e.clavier = e.clavier + 1 < static_cast<int32>(visibles.Size()) ? e.clavier + 1 : static_cast<int32>(visibles.Size()) - 1;
			}
			if (in.KeyPressedRepeat(nkgui::NkGuiKey::Up) && !visibles.Empty()) {
				e.clavier = e.clavier > 0 ? e.clavier - 1 : 0;
			}
			if (e.clavier >= static_cast<int32>(visibles.Size())) {
				e.clavier = static_cast<int32>(visibles.Size()) - 1;
			}
			if (in.KeyPressed(nkgui::NkGuiKey::Enter)) {
				in.keyInit[static_cast<int32>(nkgui::NkGuiKey::Enter)] = false;
				const int32 k = e.clavier >= 0 ? e.clavier : (visibles.Size() == 1u ? 0 : -1);
				if (k >= 0 && static_cast<uint32>(k) < visibles.Size()) {
					e.ouvert = false;
					return entrees[static_cast<uint32>(rangees[visibles[static_cast<uint32>(k)]].entree)].id;
				}
			}
			// Le defilement (molette ; le surligne reste visible).
			const float32 total = static_cast<float32>(rangees.Size()) * rh;
			if (Dans(liste, souris) && in.wheel != 0.f) {
				e.defil -= in.wheel * rh * 3.f;
				in.wheel = 0.f;
			}
			if (e.clavier >= 0 && static_cast<uint32>(e.clavier) < visibles.Size()) {
				const float32 yk = static_cast<float32>(visibles[static_cast<uint32>(e.clavier)]) * rh;
				e.defil = yk < e.defil ? yk : (yk + rh > e.defil + liste.h ? yk + rh - liste.h : e.defil);
			}
			const float32 maxDefil = total - liste.h > 0.f ? total - liste.h : 0.f;
			e.defil = e.defil < 0.f ? 0.f : (e.defil > maxDefil ? maxDefil : e.defil);
			e.rects.Clear();
			e.ids.Clear();
			dl.PushClipRect(liste);
			int32 survol = -1;
			for (uint32 i = 0; i < rangees.Size(); ++i) {
				const float32 yy = liste.y + static_cast<float32>(i) * rh - e.defil;
				if (yy + rh < liste.y || yy > liste.y + liste.h) {
					continue;
				}
				const Rangee &rg = rangees[i];
				const NkRect rr{liste.x, yy, liste.w, rh};
				const float32 indent = 8.f + static_cast<float32>(rg.niveau) * 14.f;
				const bool dessus = Dans(rr, souris) && Dans(liste, souris);
				if (rg.entree < 0) {
					// Une CATEGORIE : ▾ / ▸, en orange (les titres de la charte).
					const bool rep = !recherche && Repliee(e, rg.categorie);
					const float32 tx = rr.x + indent, ty = yy + rh * 0.5f;
					if (rep) {
						dl.AddTriangleFilled(NkVec2{tx, ty - 4.f}, NkVec2{tx + 5.f, ty}, NkVec2{tx, ty + 4.f}, s.menuCategorie);
					} else {
						dl.AddTriangleFilled(NkVec2{tx - 1.f, ty - 3.f}, NkVec2{tx + 7.f, ty - 3.f}, NkVec2{tx + 3.f, ty + 2.f}, s.menuCategorie);
					}
					Texte(dl, police, tx + 12.f, yy + (rh - lh * 0.9f) * 0.5f, Dernier(rg.categorie), s.menuCategorie, 0.9f);
					if (dessus && in.mouseClicked[0] && !recherche) {
						if (rep) {
							for (uint32 q = 0; q < e.replies.Size(); ++q) {
								if (e.replies[q] == rg.categorie) {
									e.replies.Erase(e.replies.Begin() + q);
									break;
								}
							}
						} else {
							e.replies.PushBack(rg.categorie);
						}
						in.mouseClicked[0] = false;
					}
					continue;
				}
				const NkEntreeMenuNoeud &x2 = entrees[static_cast<uint32>(rg.entree)];
				int32 rangVisible = -1;
				for (uint32 q = 0; q < visibles.Size(); ++q) {
					if (visibles[q] == i) {
						rangVisible = static_cast<int32>(q);
					}
				}
				if (dessus) {
					survol = rg.entree;
				}
				if (dessus || rangVisible == e.clavier) {
					dl.AddRectFilled(rr, s.menuSurvol, 3.f);
				}
				// La pastille : la categorie du noeud (sa couleur d'en-tete).
				dl.AddRectFilled(NkRect{rr.x + indent, yy + rh * 0.5f - 5.f, 4.f, 10.f}, x2.couleur, 1.f);
				Texte(dl, police, rr.x + indent + 12.f, yy + (rh - lh) * 0.5f, x2.libelle.CStr(), s.texte, 1.f, rr.w - indent - 18.f);
				e.rects.PushBack(rr);
				e.ids.PushBack(x2.id);
				if (dessus && in.mouseClicked[0]) {
					e.ouvert = false;
					in.mouseClicked[0] = false;
					dl.PopClipRect();
					return x2.id;
				}
			}
			dl.PopClipRect();
			// Rien : le resultat DIT son perimetre.
			if (visibles.Empty()) {
				const NkString msg = recherche ? NkString::Format("Aucun nœud ne correspond à « %s »", e.filtre) : NkString("Aucun nœud");
				Texte(dl, police, liste.x + 10.f, liste.y + 8.f, msg.CStr(), s.texte, 0.9f, liste.w - 20.f);
				if (!e.contexte.Empty() && e.sensibleContexte) {
					Texte(dl, police, liste.x + 10.f, liste.y + 8.f + lh, (e.contexte + " " + e.contexteGlyphe).CStr(), s.attenue, 0.82f, liste.w - 20.f);
				}
			}
			// L'aide de l'entree survolee (ou surlignee).
			int32 aide = survol;
			if (aide < 0 && e.clavier >= 0 && static_cast<uint32>(e.clavier) < visibles.Size()) {
				aide = rangees[visibles[static_cast<uint32>(e.clavier)]].entree;
			}
			dl.AddLine(NkVec2{r.x + 6.f, r.y + r.h - bas}, NkVec2{r.x + r.w - 6.f, r.y + r.h - bas}, s.menuBord, 1.f);
			if (aide >= 0) {
				const NkEntreeMenuNoeud &x2 = entrees[static_cast<uint32>(aide)];
				Texte(dl, police, r.x + 10.f, r.y + r.h - bas + 6.f, x2.libelle.CStr(), s.texteEntete, 0.86f, r.w - 20.f);
				Texte(dl, police, r.x + 10.f, r.y + r.h - bas + 6.f + lh * 0.9f, x2.aide.Empty() ? x2.categorie.CStr() : x2.aide.CStr(), s.attenue, 0.8f,
					  r.w - 20.f);
			} else {
				Texte(dl, police, r.x + 10.f, r.y + r.h - bas + 8.f, "Haut / Bas, Entrée : poser · Échap : fermer", s.attenue, 0.8f, r.w - 20.f);
			}
			// Le menu garde la souris : rien ne traverse.
			if (Dans(r, souris)) {
				in.mouseClicked[0] = false;
				in.mouseClicked[1] = false;
				in.wheel = 0.f;
			}
			return resultat;
		}

	} // namespace editorkit
} // namespace nkentseu
