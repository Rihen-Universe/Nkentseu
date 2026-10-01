// -----------------------------------------------------------------------------
// @File    NkFamilleOutliner.cpp
// @Brief   L'Outliner d'Unreal 5 de la famille (voir NkFamilleOutliner.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleOutliner.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKEditorKit/NkEditorScrollbar.h"
#include "NKEditorKit/NkEditorTextField.h"

#include <cmath>
#include <cstdio>

namespace nkentseu {
	namespace editorkit {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {

			// Les poignees d'icones : l'oeil et le cadenas ont CHACUN une seule
			// poignee (l'etat est porte par l'icone de nature, voir le peintre).
			constexpr uint16 ICONE_OEIL = 0x10u;
			constexpr uint16 ICONE_CADENAS = 0x11u;
			constexpr uint16 ICONE_NATURE = 0x100u;
			/// Le delai du clic lent (UE5 : environ une demi-seconde).
			constexpr float32 CLIC_LENT_DELAI = 0.5f;
			constexpr float32 PI = 3.14159265f;

			/// Peint les icones de l'Outliner : oeil, cadenas, nature.
			///
			/// ⚠️ L'OEIL ET LE CADENAS SONT PEINTS A L'APPEL DE L'ICONE DE NATURE,
			///    pas au leur : le kit appelle, dans chaque ligne, l'oeil, le cadenas
			///    puis la nature ; le peintre retient les deux rectangles, et la
			///    poignee de nature, qui porte les drapeaux, les peint dans le bon etat.
			class NkPeintreOutliner : public NkGuiComponentPaint {
				public:
					NkPeintreOutliner(nkgui::NkGuiContext &ctx, const NkTheme &theme, const char *tamponRenommage) noexcept
						: NkGuiComponentPaint(ctx, theme), mCtxO(ctx), mTampon(tamponRenommage) {
					}

					void Icon(const NkPaintRect &r, uint16 poignee, uint16 role) override {
						if (poignee == ICONE_OEIL) {
							mOeil = r;
							mAOeil = true;
							return;
						}
						if (poignee == ICONE_CADENAS) {
							mCadenas = r;
							mACadenas = true;
							return;
						}
						if (poignee >= ICONE_NATURE) {
							if (mAOeil) {
								Oeil(mOeil, (poignee & 2u) != 0u, (poignee & 8u) != 0u);
							}
							if (mACadenas) {
								Cadenas(mCadenas, (poignee & 1u) != 0u, (poignee & 4u) != 0u);
							}
							Nature(r, static_cast<uint16>((poignee >> 4) & 0xFu), role);
							// Le NOM commence apres cette icone : le clic lent le demande.
							const NkVec2 s = mCtxO.input.mousePos;
							if (s.y >= r.y && s.y < r.y + r.h) {
								mLibelleX = r.x + r.w;
							}
						}
						// La racine (poignee 0) n'a ni oeil ni cadenas.
						mAOeil = false;
						mACadenas = false;
					}

					/// Le kit dessine le texte en cours de renommage avec LE tampon du
					/// modele : son rectangle est celui ou poser le champ de saisie.
					void Text(const NkPaintRect &r, const char *s, uint16 role, NkTextAlign align) override {
						if (s != nullptr && s == mTampon) {
							mSaisie = r;
							mASaisie = true;
						}
						NkGuiComponentPaint::Text(r, s, role, align);
					}

					float32 LibelleX() const noexcept {
						return mLibelleX;
					}
					bool Saisie(NkRect &r) const noexcept {
						r = NkRect{mSaisie.x, mSaisie.y, mSaisie.w, mSaisie.h};
						return mASaisie;
					}

				private:
					NkColor Couleur(NkRole role, uint8 alpha = 255) const {
						NkColor c = NkThemeUnpack(ColorOf(static_cast<uint16>(role)));
						c.a = static_cast<uint8>((static_cast<uint32>(c.a) * alpha) / 255u);
						return c;
					}

					/// Ouvert : discret. Ferme : une paupiere et trois cils, plus VIFS.
					/// Ferme par un PARENT (`herite`) : la paupiere, ATTENUEE.
					void Oeil(const NkPaintRect &r, bool cache, bool herite) {
						nkgui::NkGuiDrawList &dl = mCtxO.DL();
						const float32 cx = r.x + r.w * 0.5f;
						const float32 cy = r.y + r.h * 0.5f;
						const float32 a = 6.f;
						const float32 b = 3.4f;
						if (!cache && !herite) {
							const NkColor col = Couleur(NkRole::TextMuted, 200);
							NkVec2 pts[18];
							for (int32 k = 0; k < 9; ++k) {
								const float32 t = static_cast<float32>(k) / 8.f;
								pts[k] = NkVec2{cx - a + 2.f * a * t, cy - b * std::sin(PI * t)};
								pts[9 + k] = NkVec2{cx + a - 2.f * a * t, cy + b * std::sin(PI * t)};
							}
							dl.AddPolyline(pts, 18, col, 1.2f, true);
							dl.AddCircleFilled(NkVec2{cx, cy}, 1.8f, col);
							return;
						}
						const NkColor col = cache ? Couleur(NkRole::Text) : Couleur(NkRole::TextMuted, 150);
						NkVec2 paupiere[9];
						for (int32 k = 0; k < 9; ++k) {
							const float32 t = static_cast<float32>(k) / 8.f;
							paupiere[k] = NkVec2{cx - a + 2.f * a * t, cy - 1.f + b * 0.7f * std::sin(PI * t)};
						}
						dl.AddPolyline(paupiere, 9, col, 1.4f, false);
						for (int32 k = 1; k <= 3; ++k) {
							const float32 t = static_cast<float32>(k) / 4.f;
							const float32 x = cx - a + 2.f * a * t;
							const float32 y = cy - 1.f + b * 0.7f * std::sin(PI * t);
							dl.AddLine(NkVec2{x, y}, NkVec2{x + (t - 0.5f) * 2.f, y + 2.6f}, col, 1.2f);
						}
					}

					/// Ouvert : l'anse levee, discret. Ferme : le corps plein, vif ;
					/// ferme par un parent (`herite`) : plein, mais ATTENUE.
					void Cadenas(const NkPaintRect &r, bool propre, bool herite) {
						nkgui::NkGuiDrawList &dl = mCtxO.DL();
						const float32 cx = r.x + r.w * 0.5f;
						const float32 cy = r.y + r.h * 0.5f;
						const bool verrou = propre || herite;
						const NkColor col = propre ? Couleur(NkRole::Text) : Couleur(NkRole::TextMuted, 150);
						const NkRect corps{cx - 4.f, cy - 0.5f, 8.f, 6.f};
						const float32 leve = verrou ? 0.f : 2.5f;
						const float32 ra = 2.6f;
						const float32 haut = cy - 0.5f - leve;
						NkVec2 anse[9];
						for (int32 k = 0; k < 9; ++k) {
							const float32 ang = PI * static_cast<float32>(k) / 8.f;
							anse[k] = NkVec2{cx - ra * std::cos(ang), haut - ra * std::sin(ang) - 0.5f};
						}
						dl.AddPolyline(anse, 9, col, 1.3f, false);
						dl.AddLine(NkVec2{cx - ra, haut - 0.5f}, NkVec2{cx - ra, cy - 0.5f}, col, 1.3f);
						if (verrou) {
							dl.AddLine(NkVec2{cx + ra, haut - 0.5f}, NkVec2{cx + ra, cy - 0.5f}, col, 1.3f);
							dl.AddRectFilled(corps, col, 1.5f);
						} else {
							dl.AddRect(corps, col, 1.f, 1.5f);
						}
					}

					void Nature(const NkPaintRect &r, uint16 nature, uint16 role) {
						nkgui::NkGuiDrawList &dl = mCtxO.DL();
						const float32 cx = r.x + r.w * 0.5f;
						const float32 cy = r.y + r.h * 0.5f;
						const NkColor col = NkThemeUnpack(ColorOf(role));
						switch (static_cast<NkFamilleNature>(nature)) {
							case NkFamilleNature::Mou:
								dl.AddCircleFilled(NkVec2{cx - 1.f, cy + 1.f}, 4.f, col);
								dl.AddCircleFilled(NkVec2{cx + 2.4f, cy - 1.8f}, 2.4f, col);
								break;
							case NkFamilleNature::Rigide:
								dl.AddRectFilled(NkRect{cx - 4.f, cy - 4.f, 8.f, 8.f}, col, 1.5f);
								break;
							case NkFamilleNature::Lumiere:
								for (int32 k = 0; k < 8; ++k) {
									const float32 a = 0.785398f * static_cast<float32>(k);
									dl.AddLine(NkVec2{cx + std::cos(a) * 3.5f, cy + std::sin(a) * 3.5f},
											   NkVec2{cx + std::cos(a) * 6.f, cy + std::sin(a) * 6.f}, col, 1.1f);
								}
								dl.AddCircleFilled(NkVec2{cx, cy}, 2.6f, col);
								break;
							case NkFamilleNature::Emetteur: {
								const NkVec2 f[4] = {NkVec2{cx, cy - 6.f}, NkVec2{cx + 4.f, cy + 1.f}, NkVec2{cx, cy + 5.f},
													 NkVec2{cx - 4.f, cy + 1.f}};
								dl.AddTriangleFilled(f[0], f[1], f[2], col);
								dl.AddTriangleFilled(f[0], f[2], f[3], col);
								break;
							}
							case NkFamilleNature::Decor:
								dl.AddRectFilled(NkRect{cx - 5.5f, cy + 0.5f, 11.f, 3.5f}, col, 1.f);
								dl.AddLine(NkVec2{cx - 4.f, cy - 2.5f}, NkVec2{cx + 4.f, cy - 2.5f}, col, 1.f);
								break;
							case NkFamilleNature::Camera:
								// Un boitier et son objectif (un trapeze vers la droite).
								dl.AddRectFilled(NkRect{cx - 6.f, cy - 3.5f, 8.f, 7.f}, col, 1.f);
								dl.AddTriangleFilled(NkVec2{cx + 2.f, cy}, NkVec2{cx + 6.f, cy - 3.5f}, NkVec2{cx + 6.f, cy + 3.5f}, col);
								break;
							case NkFamilleNature::Maillage: {
								// Un cube en perspective cavaliere, au trait.
								const NkRect f{cx - 5.f, cy - 2.f, 7.f, 7.f};
								dl.AddRect(f, col, 1.1f);
								dl.AddLine(NkVec2{f.x, f.y}, NkVec2{f.x + 3.f, f.y - 3.f}, col, 1.1f);
								dl.AddLine(NkVec2{f.x + f.w, f.y}, NkVec2{f.x + f.w + 3.f, f.y - 3.f}, col, 1.1f);
								dl.AddLine(NkVec2{f.x + f.w, f.y + f.h}, NkVec2{f.x + f.w + 3.f, f.y + f.h - 3.f}, col, 1.1f);
								dl.AddLine(NkVec2{f.x + 3.f, f.y - 3.f}, NkVec2{f.x + f.w + 3.f, f.y - 3.f}, col, 1.1f);
								dl.AddLine(NkVec2{f.x + f.w + 3.f, f.y - 3.f}, NkVec2{f.x + f.w + 3.f, f.y + f.h - 3.f}, col, 1.1f);
								break;
							}
							case NkFamilleNature::Son:
								dl.AddRectFilled(NkRect{cx - 5.f, cy - 2.f, 3.f, 4.f}, col);
								dl.AddTriangleFilled(NkVec2{cx - 2.f, cy - 2.f}, NkVec2{cx + 2.f, cy - 5.f}, NkVec2{cx + 2.f, cy + 5.f}, col);
								dl.AddTriangleFilled(NkVec2{cx - 2.f, cy - 2.f}, NkVec2{cx + 2.f, cy + 5.f}, NkVec2{cx - 2.f, cy + 2.f}, col);
								dl.AddLine(NkVec2{cx + 4.f, cy - 3.f}, NkVec2{cx + 5.5f, cy}, col, 1.1f);
								dl.AddLine(NkVec2{cx + 5.5f, cy}, NkVec2{cx + 4.f, cy + 3.f}, col, 1.1f);
								break;
							default: {
								const NkVec2 pts[4] = {NkVec2{cx, cy - 4.5f}, NkVec2{cx + 4.5f, cy}, NkVec2{cx, cy + 4.5f},
													   NkVec2{cx - 4.5f, cy}};
								dl.AddPolyline(pts, 4, col, 1.3f, true);
								break;
							}
						}
					}

					nkgui::NkGuiContext &mCtxO;
					const char *mTampon;
					NkPaintRect mOeil{};
					NkPaintRect mCadenas{};
					NkPaintRect mSaisie{};
					bool mAOeil = false;
					bool mACadenas = false;
					bool mASaisie = false;
					float32 mLibelleX = 1.0e9f;
			};

			// ── Les rappels du kit -> ceux de l'application ─────────────────────
			struct NkPont {
					const NkFamilleOutlinerRappels *r;
					NkFamilleOutliner *o;
			};

			void SurActivation(void *user, int32 index, const char *) {
				NkPont &p = *static_cast<NkPont *>(user);
				if (p.r->activer != nullptr && index > 0) {
					p.r->activer(p.r->user, index);
				}
			}

			void SurDrapeau(void *user, int32 index, const char *, uint8 drapeau, bool) {
				NkPont &p = *static_cast<NkPont *>(user);
				if (p.r->drapeau != nullptr && index > 0) {
					p.r->drapeau(p.r->user, index, drapeau == static_cast<uint8>(NkTreeFlag::Visible));
				}
			}

			void SurRenommage(void *user, int32 index, const char *, const char *, const char *nouveau) {
				NkPont &p = *static_cast<NkPont *>(user);
				// Un nom VIDE n'est pas un nom.
				if (p.r->renommer != nullptr && index > 0 && nouveau != nullptr && nouveau[0] != '\0') {
					p.r->renommer(p.r->user, index, nouveau);
				}
			}

			void SurMenu(void *user, int32 index, float32 x, float32 y) {
				NkPont &p = *static_cast<NkPont *>(user);
				if (p.r->menu != nullptr) {
					p.r->menu(p.r->user, index > 0 ? index : 0, x, y);
				}
			}

			void Preparer(NkFamilleOutliner &o) {
				if (o.pret) {
					return;
				}
				o.pret = true;
				o.reglages.Bind(NkTreeViewDecl());
				// L'en-tete, la recherche et le pied sont peints par le panneau :
				// ceux du composant disent « Arbre » et « noeud(s) ».
				o.reglages.SetParam("show_header", 0.f);
				o.reglages.SetParam("show_search", 0.f);
				o.reglages.SetParam("show_footer", 0.f);
				o.reglages.SetParam("show_visibility", 1.f);
				o.reglages.SetParam("show_lock", 1.f);
				o.reglages.SetParam("show_type", 1.f);
				o.reglages.SetParam("indent_guides", 0.f);
				o.reglages.SetParam("multi_select", 0.f);
				o.reglages.SetParam("range_select", 0.f);
				// Le double-clic CADRE (UE5) ; le renommage est F2, le menu, ou le clic lent.
				o.reglages.SetParam("activate_on_double_click", 1.f);
				// Deposer, c'est RATTACHER (les freres n'ont pas d'ordre a la main).
				o.reglages.SetParam("drop_into_only", 1.f);
				o.reglages.SetMetric("row_h", 22.f);
			}

			/// Ouvre la saisie en place sur le noeud, tout le nom choisi.
			void OuvrirRenommage(NkFamilleCtx &c, NkFamilleOutliner &o, nk_uint64 noeud) {
				const int32 k = o.arbre.IndexOf(noeud);
				if (k <= 0) {
					return;
				}
				o.arbre.renaming = noeud;
				std::snprintf(o.arbre.renameBuf, sizeof(o.arbre.renameBuf), "%s",
							  o.arbre.nodes[static_cast<uint32>(k)].label.CStr());
				o.arbre.renameCommit = false;
				o.arbre.renameCancel = false;
				o.arbre.renameEatClick = false;
				c.ctx.input.wantSelectAll = true;
				o.filtreFocus = false;
			}

		} // namespace

		uint16 NkFamilleIconeNoeud(NkFamilleNature nature, bool cache, bool verrou, bool cacheHerite,
								   bool verrouHerite) noexcept {
			return static_cast<uint16>(ICONE_NATURE | ((static_cast<uint16>(nature) & 0xFu) << 4) | (cacheHerite ? 8u : 0u) |
									   (verrouHerite ? 4u : 0u) | (cache ? 2u : 0u) | (verrou ? 1u : 0u));
		}

		NkFamilleOutlinerResultat NkFamilleDessinerOutliner(NkFamilleCtx &c, const NkRect &zone, NkFamilleOutliner &o,
															const NkFamilleOutlinerRappels &rappels, const char *pied,
															float32 dt, const char *titre, const char *plus) {
			NkFamilleOutlinerResultat res;
			if (zone.w < 8.f || zone.h < 60.f) {
				return res;
			}
			auto &dl = c.ctx.dl;
			nkgui::NkGuiInput &in = c.ctx.input;
			dl.AddRectFilled(zone, c.pal.panneau);
			Preparer(o);
			res.aLaSouris = NkFamilleDans(zone, in.mousePos);

			if (o.renommerDemande) {
				o.renommerDemande = false;
				if (o.arbre.active != 0 && o.arbre.active != kNkFamilleRacine) {
					OuvrirRenommage(c, o, o.arbre.active);
				}
			}
			const nk_uint64 activeAvant = o.arbre.active;

			// ── L'en-tete : le nom du panneau, et « + Entité » ────────────────
			const float32 enteteH = NkFamilleCotes::kOngletPanneau;
			const NkRect entete{zone.x, zone.y, zone.w, enteteH};
			dl.AddRectFilled(entete, c.pal.entete);
			NkFamilleTexte(dl, c.police, entete.x + 8.f, entete.y + (enteteH - NkFamilleHauteurLigne(c.police, 16.f)) * 0.5f,
						   titre, c.pal.texte);
			if (plus != nullptr && plus[0] != '\0') {
				const float32 bw = NkFamilleLargeur(c.petite, plus) + 14.f;
				const NkRect rPlus{entete.x + entete.w - bw - 4.f, entete.y + 3.f, bw, enteteH - 6.f};
				if (NkFamilleBouton(c, rPlus, plus, false, true, nullptr, c.petite) && rappels.nouvelle != nullptr) {
					rappels.nouvelle(rappels.user);
				}
			}

			// ── La recherche ──────────────────────────────────────────────────
			const NkRect recherche{zone.x + 4.f, zone.y + enteteH + 3.f, zone.w - 8.f, 22.f};
			NkFamilleRecherche(c, recherche, o.arbre.filter, static_cast<int32>(sizeof(o.arbre.filter)), o.filtreFocus,
							   "Rechercher…", false, c.police);

			// ── Les colonnes ──────────────────────────────────────────────────
			const float32 colonnesY = recherche.y + recherche.h + 3.f;
			const float32 colonnesH = 20.f;
			dl.AddRectFilled(NkRect{zone.x, colonnesY, zone.w, colonnesH}, c.pal.entete);
			dl.AddRectFilled(NkRect{zone.x, colonnesY + colonnesH - 1.f, zone.w, 1.f}, c.pal.bord);
			const float32 cty = colonnesY + (colonnesH - NkFamilleHauteurLigne(c.petite, 12.f)) * 0.5f;
			NkFamilleTexte(dl, c.petite, zone.x + 10.f, cty, "Nom", c.pal.attenue);
			NkFamilleTexteADroite(dl, c.petite, zone.x + zone.w - 22.f, cty, "Type", c.pal.attenue);

			// ── L'arbre du kit ────────────────────────────────────────────────
			const float32 piedH = 22.f;
			const float32 arbreY = colonnesY + colonnesH;
			const NkRect arbreR{zone.x, arbreY, zone.w, zone.y + zone.h - piedH - arbreY};
			NkTreeViewStyle s;
			s.values = &o.reglages;
			s.panelBg = static_cast<uint16>(NkRole::PanelBg);
			s.headerBg = static_cast<uint16>(NkRole::PanelHeader);
			s.border = static_cast<uint16>(NkRole::Border);
			s.text = static_cast<uint16>(NkRole::Text);
			s.textMuted = static_cast<uint16>(NkRole::TextMuted);
			s.rowHover = static_cast<uint16>(NkRole::InputBg);
			// Le BLEU dit la selection dans une LISTE ; l'ambre est reserve a la
			// selection dans la scene, dans le viseur.
			s.activeMark = static_cast<uint16>(NkRole::AccentUi);
			s.activeText = static_cast<uint16>(NkRole::TextOnAccent);
			s.chosenMark = static_cast<uint16>(NkRole::AccentUi);
			s.guide = static_cast<uint16>(NkRole::Border);
			s.dropMark = static_cast<uint16>(NkRole::AccentUi);
			s.iconTint = static_cast<uint16>(NkRole::TextMuted);
			s.dimTint = static_cast<uint16>(NkRole::TextMuted);
			s.icons.eyeOpen = ICONE_OEIL;
			s.icons.eyeClosed = ICONE_OEIL;
			s.icons.lockOpen = ICONE_CADENAS;
			s.icons.lockClosed = ICONE_CADENAS;
			NkPont pont{&rappels, &o};
			NkTreeViewHooks hooks;
			hooks.user = &pont;
			hooks.onActivate = &SurActivation;
			hooks.onToggleFlag = &SurDrapeau;
			hooks.onRename = &SurRenommage;
			hooks.onContextMenu = &SurMenu;

			NkComponentInput ci = NkFamilleEntreeComposant(c.ctx);
			// ── Le glisser d'une ligne : il ne commence qu'au-dela de 4 px et
			//    seulement si l'appui etait DANS l'arbre ─────────────────────────
			if (in.mouseClicked[0]) {
				o.appui = NkFamilleDans(arbreR, in.mousePos);
				o.depart = in.mousePos;
				o.glisse = false;
			}
			if (in.mouseDown[0] && o.appui && !o.glisse && o.arbre.dragSource != 0) {
				const float32 dx = in.mousePos.x - o.depart.x;
				const float32 dy = in.mousePos.y - o.depart.y;
				o.glisse = dx * dx + dy * dy > 16.f;
			}
			if (o.glisse) {
				ci.dragType = "famille.entite";
				ci.dragReleased = in.mouseReleased[0];
			}
			NkPeintreOutliner peintre(c.ctx, c.theme, o.arbre.renameBuf);
			const NkTreeViewResult r =
				NkDrawTreeView(peintre, ci, NkPaintRect{arbreR.x, arbreR.y, arbreR.w, arbreR.h}, o.arbre, s, hooks);

			// Le depot : APRES le dessin, jamais pendant.
			if (r.dropAccepted && o.glisse && rappels.rattacher != nullptr) {
				const int32 source = o.arbre.IndexOf(r.dropSource);
				const int32 cible = o.arbre.IndexOf(r.dropTarget);
				if (source > 0) {
					rappels.rattacher(rappels.user, source, cible > 0 ? cible : 0);
				}
			}
			if (in.mouseReleased[0]) {
				o.glisse = false;
				o.appui = false;
				o.arbre.dragSource = 0;
			}
			if (r.selectionChanged) {
				res.selectionChangee = true;
				res.actif = o.arbre.IndexOf(o.arbre.active);
			}
			if (r.defilContenu > r.defilVue && r.defilW > 0.f && r.defilH > 0.f) {
				NkVScrollbar(c.ctx, dl, NkRect{r.defilX, r.defilY, r.defilW, r.defilH}, o.arbre.scroll, r.defilContenu,
							 r.defilVue, c.ctx.GetId("famille.outliner.defil"), r.defilPas);
			}

			// ── Le renommage en place : la SAISIE, par-dessus la boite du kit ──
			if (o.arbre.renaming != 0) {
				NkRect saisie;
				if (peintre.Saisie(saisie)) {
					NkOverlayFieldStyle st;
					st.texte = c.pal.texte;
					st.utf8 = true;
					const NkRect champ{saisie.x - 5.f, saisie.y + 2.f, saisie.w + 10.f, saisie.h - 4.f};
					NkOverlayTextField(c.ctx, dl, c.police, champ, o.arbre.renameBuf, 32, true, &st);
				}
				if (in.KeyPressed(nkgui::NkGuiKey::Enter)) {
					o.arbre.renameCommit = true;
				} else if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
					o.arbre.renameCancel = true;
				}
			}

			// ── Le clic LENT : un clic sur le NOM d'une ligne deja choisie ─────
			const NkVec2 souris = in.mousePos;
			if (in.mouseDoubleClicked[0]) {
				o.clicLentNoeud = 0;
			} else if (in.mouseClicked[0]) {
				const bool surNom = r.survoleIndex > 0 && o.arbre.renaming == 0 && activeAvant != 0 &&
									o.arbre.nodes[static_cast<uint32>(r.survoleIndex)].id == activeAvant &&
									souris.x >= peintre.LibelleX();
				o.clicLentNoeud = surNom ? activeAvant : 0;
				o.clicLentAge = 0.f;
				o.clicLentPos = souris;
			}
			if (o.clicLentNoeud != 0) {
				o.clicLentAge += dt;
				const float32 dx = souris.x - o.clicLentPos.x;
				const float32 dy = souris.y - o.clicLentPos.y;
				if (dx * dx + dy * dy > 16.f || o.arbre.active != o.clicLentNoeud || o.arbre.renaming != 0) {
					o.clicLentNoeud = 0;
				} else if (o.clicLentAge >= CLIC_LENT_DELAI && !in.mouseDown[0]) {
					o.renommerDemande = true; // a la trame suivante, AVANT le dessin, comme F2
					o.clicLentNoeud = 0;
				}
			}

			// ── Le pied : le compte, et la selection ──────────────────────────
			const NkRect rPied{zone.x, zone.y + zone.h - piedH, zone.w, piedH};
			dl.AddRectFilled(rPied, c.pal.entete);
			dl.AddRectFilled(NkRect{rPied.x, rPied.y, rPied.w, 1.f}, c.pal.bord);
			if (pied != nullptr) {
				NkFamilleTexte(dl, c.petite, rPied.x + 8.f, rPied.y + (piedH - NkFamilleHauteurLigne(c.petite, 12.f)) * 0.5f,
							   pied, c.pal.attenue);
			}
			if (res.actif < 0) {
				res.actif = o.arbre.IndexOf(o.arbre.active);
			}
			return res;
		}

	} // namespace editorkit
} // namespace nkentseu
