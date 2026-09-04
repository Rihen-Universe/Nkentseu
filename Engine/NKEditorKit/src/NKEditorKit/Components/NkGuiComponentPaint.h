#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiComponentPaint.h
// @Brief   Implementation MINCE de `NkComponentPaint` sur la liste d'affichage
//          de NKGui — le strict necessaire pour qu'un composant s'affiche.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ CE FICHIER N'EST PLUS PROVISOIRE — IL EST L'IMPLEMENTATION DU MONDE NKGUI.
// =============================================================================
//  Ecrit comme depannage « en attendant le peintre de NK3DModeler », il a
//  change de statut le 2026-08-30, quand ce peintre est arrive
//  (`NkModelerComponentPaint.h`, 147 l., commit `6f62e114`) : la mesure a
//  montre qu'il ne pouvait PAS remplacer ce fichier — il exige un
//  `NkModelerPainter`, que NkUIDesign n'a pas. Deux mondes de rendu, deux
//  implementations perennes (plus `NkRecordingPaint` pour les essais).
//
//  ⚠️ ET RIEN N'A MAIGRI, PARCE QUE LA MESURE L'A REFUSE. Ce qu'il porte est
//     soit le RESPECT DU CONTRAT (l'ellipse, le centrage — les retirer ferait
//     mentir la signature), soit une DEPENDANCE NOMMEE vers NKGui (l'atlas
//     d'icones, le contour arrondi — elles disparaitront quand NKGui saura).
//     Un doublon dont une moitie porte une connaissance que l'autre n'a pas ne
//     se retire pas ; ici il n'y a meme pas doublon, il y a deux mondes.
//
//  🚧 REGLE POUR CELUI QUI PASSERA APRES MOI : **il ne doit pas grossir.**
//     Chaque methode se contente de traduire un appel vers `NkGuiDrawList`.
//     Aucune geometrie nouvelle, aucune decision de rendu, aucun cas
//     particulier. Une decision de rendu se prend dans le COMPOSANT — c'est ce
//     qui garantit que les trois peintres rendent la MEME chose.
//
// CE QU'IL FAIT QUAND MEME, ET POURQUOI IL LE FAUT :
//   - **l'ellipse** (« mon_tres_long_fichier... ») : le contrat de
//     `NkComponentPaint::Text` l'exige, et `NkGuiDrawList::AddText(maxWidth)`
//     coupe au glyphe sans points de suite. Sans elle, la signature mentirait —
//     et c'est exactement le defaut « un parametre qui n'est pas honore ». Ce
//     n'est donc pas une extension : c'est le respect du contrat.
//   - **le centrage** : meme raison. L'aide existe dans NKGui mais elle est
//     INTERNE a un `.cpp`, non exposee (mesure du 18/08).
//
// ⚠️ CE QU'IL NE SAIT PAS FAIRE, ET C'EST DIT PLUTOT QUE DECOUVERT :
//   - **`Icon` NE DESSINE AUCUNE ICONE.** Il n'existe aucune notion d'icone
//     dans NKGui, et les 193 glyphes du depot sont definis DEUX FOIS (102 en
//     SVG chez NK3DModeler, 91 en PNG chez NKCode), a raison d'une texture GPU
//     par glyphe. L'atlas appartient a l'agent NKGui et figure dans sa liste.
//     En attendant, `Icon` peint un **carre plein du role demande** : la place
//     est prise, la couleur est juste, le glyphe manque. Un composant qui
//     l'appelle est donc deja correct le jour ou l'atlas arrive.
//     **Ce n'est pas un oubli — c'est une dependance nommee.**
//   - **`Outline` et le contour arrondi** restent le contournement a deux
//     rectangles, parce que `AddRect` ne sait pas arrondir. Meme proprietaire,
//     meme liste.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkComponentPaint.h"
#include "NKEditorKit/NkTheme.h"
#include "NKGui/NKGui.h"
#include "NKMath/NkEarcut.h" // le triangulateur, descendu de NKFont le 2026-09-01

namespace nkentseu {
	namespace editorkit {

		class NkGuiComponentPaint : public NkComponentPaint {
			public:
				NkGuiComponentPaint(nkgui::NkGuiContext &ctx, const NkTheme &theme) noexcept
					: mCtx(ctx), mTheme(theme) {}

				// ── Theme et metrologie ─────────────────────────────────────────
				uint32 ColorOf(uint16 role) const override {
					return mTheme.Get(role);
				}
				// ⚠️ AUCUN `Scale()` — arbitrage du 18/08 : l'echelle appartient a
				//    la SURFACE, pas au peintre, parce que la disposition et les
				//    tables de metrique la lisent aussi et qu'elles ne peignent
				//    pas. Elle voyage dans `NkComponentInput::surfaceScale`.
				//    Ce peintre ne l'a jamais vue et n'en a pas besoin.
				float32 LineHeight() const override {
					return mCtx.font ? mCtx.font->LineHeight() : 16.f;
				}
				float32 TextWidth(const char *s) const override {
					return (mCtx.font && s) ? mCtx.font->MeasureWidth(s) : 0.f;
				}

				// ── Primitives ──────────────────────────────────────────────────
				void Fill(const NkPaintRect &r, uint16 role, float32 rounding) override {
					RectRempli(r, C(role), rounding);
				}
				void FillColor(const NkPaintRect &r, uint32 rgba, float32 rounding) override {
					RectRempli(r, Unpack(rgba), rounding);
				}
				void Outline(const NkPaintRect &r, uint16 border, uint16 inner,
							 float32 rounding) override {
					// Contournement assume : `AddRect` ne sait pas arrondir. Plein
					// puis creusement d'un pixel. Meme geste que `NkModelerPainter`,
					// et il disparait le jour ou NKGui sait arrondir un contour.
					RectRempli(r, C(border), rounding);
					RectRempli({r.x + 1.f, r.y + 1.f, r.w - 2.f, r.h - 2.f}, C(inner),
							   rounding > 1.f ? rounding - 1.f : 0.f);
				}
				void OutlineSharp(const NkPaintRect &r, uint16 role) override {
					if (const NkPaintTransform *m = TransformeActive()) {
						nkgui::NkVec2 p[4];
						Coins(*m, r, p);
						mCtx.DL().AddPolyline(p, 4, C(role), 1.f, true);
						return;
					}
					mCtx.DL().AddRect(R(r), C(role), 1.f);
				}
				void HLine(float32 x, float32 y, float32 w, uint16 role) override {
					if (const NkPaintTransform *m = TransformeActive()) {
						mCtx.DL().AddLine(T(*m, x, y), T(*m, x + w, y), C(role), 1.f);
						return;
					}
					mCtx.DL().AddRectFilled({Px(x), Px(y), Px(x + w) - Px(x), 1.f}, C(role));
				}
				void VLine(float32 x, float32 y, float32 h, uint16 role) override {
					if (const NkPaintTransform *m = TransformeActive()) {
						mCtx.DL().AddLine(T(*m, x, y), T(*m, x, y + h), C(role), 1.f);
						return;
					}
					mCtx.DL().AddRectFilled({Px(x), Px(y), 1.f, Px(y + h) - Px(y)}, C(role));
				}

				void Text(const NkPaintRect &r, const char *s, uint16 role, NkTextAlign align) override;
				void Icon(const NkPaintRect &r, uint16 iconHandle, uint16 role) override;

				// Traductions pures (2026-08-30) — la regle « il ne doit pas
				// grossir » tient : l'ellipse parametrique vit dans NKGui
				// (`AddEllipseFilled`), la ligne dans `AddLine`.
				bool Ellipse(const NkPaintRect &r, uint16 role) override {
					if (const NkPaintTransform *m = TransformeActive()) {
						// 48 points : la meme finesse que l'ellipse native pour un rayon
						// de l'ordre de la boite, transformes un a un.
						enum { kN = 48 };
						nkgui::NkVec2 p[kN];
						const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
						for (int32 i = 0; i < kN; ++i) {
							const float32 ang = 6.2831853f * (float32)i / (float32)kN;
							p[i] = T(*m, cx + r.w * 0.5f * NkCosApprox(ang), cy + r.h * 0.5f * NkSinApprox(ang));
						}
						mCtx.DL().AddConvexPolyFilled(p, kN, C(role));
						return true;
					}
					mCtx.DL().AddEllipseFilled({r.x + r.w * 0.5f, r.y + r.h * 0.5f}, r.w * 0.5f,
											   r.h * 0.5f, C(role));
					return true;
				}
				bool Line(float32 x1, float32 y1, float32 x2, float32 y2, uint16 role,
						  float32 thickness) override {
					if (const NkPaintTransform *m = TransformeActive()) {
						mCtx.DL().AddLine(T(*m, x1, y1), T(*m, x2, y2), C(role), thickness);
						return true;
					}
					mCtx.DL().AddLine({x1, y1}, {x2, y2}, C(role), thickness);
					return true;
				}
				/// Polygone plein, CONCAVE COMPRIS — triangulation par
				/// ear-clipping SANS ALLOCATION (`NKMath/NkEarcut.h`).
				///
				/// 🔴 CE QU'IL FAISAIT AVANT, ET POURQUOI C'ÉTAIT UN DÉFAUT LIVRÉ :
				///    un ÉVENTAIL DEPUIS LE CENTROÏDE. Juste pour un convexe, juste
				///    pour une étoile, FAUX dès que le contour est concave — les
				///    triangles de l'éventail traversent le creux et le
				///    remplissage déborde. Tant que l'application ne posait que des
				///    rectangles et des ellipses, personne ne pouvait le voir ; les
				///    poignées de Bézier ont rendu le défaut atteignable, donc réel.
				///
				/// 🔴 ET LA PREMIÈRE TENTATIVE DE RÉPARATION A FAIT PLANTER
				///    L'APPLICATION — corruption de tas, `0xC0000374`, en trois
				///    secondes. La cause n'était pas ici : `NkEarcut` (la porte qui
				///    ALLOUE) libère chaque oreille découpée, puis libère la liste
				///    une seconde fois depuis sa tête, laquelle a presque toujours
				///    été découpée. Une DOUBLE LIBÉRATION, présente depuis le
				///    début, que seuls des milliers d'appels par seconde rendaient
				///    visible. On passe donc par `NkEarcutVers`, la porte SANS
				///    ALLOCATION : pas de tas, donc pas de libération à équilibrer,
				///    donc pas de double libération possible.
				///
				/// ⚠️ LES TAMPONS SONT SUR LA PILE, ET LEUR TAILLE EST EXACTE : un
				///    polygone simple à N sommets donne toujours N-2 triangles.
				///    `NkContourDe` plafonne à 128 points, d'où 128 nœuds et
				///    3*(128-2) indices. Aucune allocation dans la boucle de
				///    dessin — c'était l'autre moitié du problème.
				///
				/// ⚠️ REPLI EXPLICITE PLUTÔT QUE RIEN : si la triangulation ne rend
				///    aucun triangle (contour dégénéré, points alignés, contour qui
				///    se croise), on repeint l'éventail d'avant. Il est faux sur un
				///    concave, mais il montre QUELQUE CHOSE — et une forme qui
				///    disparaît est pire qu'une forme mal remplie : elle fait croire
				///    à une suppression.
				bool PolygonHex(const float32 *xy, int32 count, uint32 rgba) override {
					if (const NkPaintTransform *m = TransformeActive()) {
						if (!xy || count < 3 || count > 128)
							return false;
						float32 tmp[256];
						for (int32 i = 0; i < count; ++i) {
							const nkgui::NkVec2 q = T(*m, xy[i * 2], xy[i * 2 + 1]);
							tmp[i * 2] = q.x;
							tmp[i * 2 + 1] = q.y;
						}
						return PolygonHexBrut(tmp, count, rgba);
					}
					return PolygonHexBrut(xy, count, rgba);
				}
				bool PolygonHexBrut(const float32 *xy, int32 count, uint32 rgba) {
					if (!xy || count < 3)
						return false;
					const nkgui::NkColor col = {(uint8)((rgba >> 24) & 0xFFu),
												(uint8)((rgba >> 16) & 0xFFu),
												(uint8)((rgba >> 8) & 0xFFu),
												(uint8)(rgba & 0xFFu)};
					enum { kMaxPts = 128 };
					if (count <= (int32)kMaxPts) {
						math::NkVec2f pts[kMaxPts];
						::nkentseu::detail::NkEarcutNode<float32> noeuds[kMaxPts];
						uint32 idx[(kMaxPts - 2) * 3];
						for (int32 i = 0; i < count; ++i)
							pts[i] = math::NkVec2f(xy[i * 2], xy[i * 2 + 1]);
						// ⚠️ LE SENS N'EST PAS NOTRE AFFAIRE : `NkEarcutVers` mesure
						//    l'aire signée et se retourne tout seul. Le peintre reçoit
						//    des contours dans un sens quelconque (une forme dont on a
						//    tiré les sommets peut s'être retournée) et n'a pas à le
						//    savoir.
						const uint32 nbTri = ::nkentseu::NkEarcutVers<float32>(
							pts, (uint32)count, noeuds, kMaxPts, idx, (kMaxPts - 2) * 3);
						if (nbTri > 0) {
							for (uint32 t = 0; t < nbTri; ++t) {
								const math::NkVec2f &a = pts[idx[t * 3 + 0]];
								const math::NkVec2f &b = pts[idx[t * 3 + 1]];
								const math::NkVec2f &c = pts[idx[t * 3 + 2]];
								mCtx.DL().AddTriangleFilled({a.x, a.y}, {b.x, b.y}, {c.x, c.y},
															col);
							}
							return true;
						}
					}
					// repli : l'éventail d'avant (voir l'avertissement ci-dessus)
					float32 cx = 0.f, cy = 0.f;
					for (int32 i = 0; i < count; ++i) {
						cx += xy[i * 2];
						cy += xy[i * 2 + 1];
					}
					cx /= (float32)count;
					cy /= (float32)count;
					for (int32 i = 0; i < count; ++i) {
						const int32 j = (i + 1) % count;
						mCtx.DL().AddTriangleFilled({xy[i * 2], xy[i * 2 + 1]},
													{xy[j * 2], xy[j * 2 + 1]}, {cx, cy}, col);
					}
					return true;
				}

				void PushClip(const NkPaintRect &r) override {
					if (const NkPaintTransform *m = TransformeActive()) {
						// ⚠️ UNE DECOUPE TOURNEE N'EST PAS UN RECTANGLE. On prend l'englobant
						//    du rectangle transforme : il ne coupe jamais ce qui doit se
						//    voir, il peut en laisser un peu plus. Dit ici, pas cache.
						nkgui::NkVec2 p[4];
						Coins(*m, r, p);
						float32 x0 = p[0].x, y0 = p[0].y, x1 = p[0].x, y1 = p[0].y;
						for (int32 i = 1; i < 4; ++i) {
							if (p[i].x < x0) x0 = p[i].x;
							if (p[i].x > x1) x1 = p[i].x;
							if (p[i].y < y0) y0 = p[i].y;
							if (p[i].y > y1) y1 = p[i].y;
						}
						mCtx.DL().PushClipRect({x0, y0, x1 - x0, y1 - y0}, true);
						return;
					}
					mCtx.DL().PushClipRect(R(r), true);
				}
				void PopClip() override {
					mCtx.DL().PopClipRect();
				}

			private:
				/// ALIGNEMENT AU PIXEL. C'est une lecon deja payee ailleurs (le flou
				/// des glyphes poses a mi-pixel) : `Px` est repris tel quel de
				/// `NkModelerUI.h:59`, et c'est l'une des choses que la reception du
				/// peintre doit GARDER, pas « nettoyer ».
			public:
				// ── LA TRANSFORMEE : la pile et ses aides ──────────────────────
				// `protected` : un peintre derive (celui d'une application, avec
				// ses propres polices) doit lire la matrice en vigueur pour la
				// respecter dans SES primitives. Le kit ne devine pas les siennes.
				void PushTransform(const NkPaintTransform &m) override {
					if (mSpT < kProfT) {
						// Composee avec celle du dessus : un enfant sous un parent
						// transforme ajoute la sienne, il ne la remplace pas.
						const NkPaintTransform *h = TransformeActive();
						mPileT[mSpT] = h ? Composer(*h, m) : m;
					}
					++mSpT; // au-dela de la profondeur : on compte, on n'applique pas
				}
				void PopTransform() override {
					if (mSpT > 0)
						--mSpT;
				}
				// ── LE MODE DE MELANGE (2026-09-04) : la liste de dessin le porte par commande ──
				void PushBlend(NkPaintBlend b) override {
					mCtx.DL().PushBlend((nkgui::NkGuiBlend)(uint8)b);
				}
				void PopBlend() override {
					mCtx.DL().PopBlend();
				}
			protected:
				/// La transformee en vigueur, ou nullptr a l'identite / hors pile.
				const NkPaintTransform *TransformeActive() const noexcept {
					if (mSpT == 0 || mSpT > kProfT)
						return nullptr;
					const NkPaintTransform &m = mPileT[mSpT - 1];
					return m.Identite() ? nullptr : &m;
				}
				static nkgui::NkVec2 T(const NkPaintTransform &m, float32 x, float32 y) noexcept {
					return {m.a * x + m.c * y + m.e, m.b * x + m.d * y + m.f};
				}
				/// h o m : d'abord m (la nouvelle), puis h (celle du dessus).
				static NkPaintTransform Composer(const NkPaintTransform &h,
												 const NkPaintTransform &m) noexcept {
					NkPaintTransform r;
					r.a = h.a * m.a + h.c * m.b;
					r.b = h.b * m.a + h.d * m.b;
					r.c = h.a * m.c + h.c * m.d;
					r.d = h.b * m.c + h.d * m.d;
					r.e = h.a * m.e + h.c * m.f + h.e;
					r.f = h.b * m.e + h.d * m.f + h.f;
					return r;
				}
				static void Coins(const NkPaintTransform &m, const NkPaintRect &r,
								  nkgui::NkVec2 *p) noexcept {
					p[0] = T(m, r.x, r.y);
					p[1] = T(m, r.x + r.w, r.y);
					p[2] = T(m, r.x + r.w, r.y + r.h);
					p[3] = T(m, r.x, r.y + r.h);
				}
				// sin/cos sans <cmath> dans l'en-tete : polynomes suffisants pour des
				// arcs de dessin (erreur < 1e-3), le meme parti que `NkSinCosDeg` du document.
				static float32 NkSinApprox(float32 x) noexcept {
					while (x > 3.14159265f) x -= 6.2831853f;
					while (x < -3.14159265f) x += 6.2831853f;
					const float32 x2 = x * x;
					return x * (1.f - x2 / 6.f * (1.f - x2 / 20.f * (1.f - x2 / 42.f * (1.f - x2 / 72.f))));
				}
				static float32 NkCosApprox(float32 x) noexcept { return NkSinApprox(x + 1.57079633f); }
				/// Un rectangle (arrondi ou non) rempli, transforme s'il le faut.
				void RectRempli(const NkPaintRect &r, const nkgui::NkColor &col, float32 rounding) {
					const NkPaintTransform *m = TransformeActive();
					if (!m) {
						mCtx.DL().AddRectFilled(R(r), col, rounding);
						return;
					}
					if (r.w <= 0.f || r.h <= 0.f)
						return;
					float32 ro = rounding;
					const float32 demi = (r.w < r.h ? r.w : r.h) * 0.5f;
					if (ro > demi) ro = demi;
					if (ro <= 0.5f) {
						nkgui::NkVec2 p[4];
						Coins(*m, r, p);
						mCtx.DL().AddConvexPolyFilled(p, 4, col);
						return;
					}
					// quatre arcs de kS segments, dans le repere propre, PUIS transformes :
					// l'arrondi se calcule droit, comme pour les formes du document.
					enum { kS = 6 };
					nkgui::NkVec2 p[4 * (kS + 1)];
					int32 n = 0;
					const float32 cxs[4] = {r.x + r.w - ro, r.x + r.w - ro, r.x + ro, r.x + ro};
					const float32 cys[4] = {r.y + ro, r.y + r.h - ro, r.y + r.h - ro, r.y + ro};
					const float32 a0s[4] = {-1.57079633f, 0.f, 1.57079633f, 3.14159265f};
					for (int32 k = 0; k < 4; ++k)
						for (int32 s = 0; s <= kS; ++s) {
							const float32 ang = a0s[k] + 1.57079633f * (float32)s / (float32)kS;
							p[n++] = T(*m, cxs[k] + ro * NkCosApprox(ang), cys[k] + ro * NkSinApprox(ang));
						}
					mCtx.DL().AddConvexPolyFilled(p, n, col);
				}
				enum { kProfT = 16 };
				NkPaintTransform mPileT[kProfT];
				int32 mSpT = 0;

			private:
				static float32 Px(float32 v) noexcept {
					return (float32)(int32)(v + 0.5f);
				}
				static nkgui::NkRect R(const NkPaintRect &r) noexcept {
					const float32 x = Px(r.x), y = Px(r.y);
					return {x, y, Px(r.x + r.w) - x, Px(r.y + r.h) - y};
				}
				static nkgui::NkColor Unpack(uint32 c) noexcept {
					return {(uint8)((c >> 24) & 0xFFu), (uint8)((c >> 16) & 0xFFu),
							(uint8)((c >> 8) & 0xFFu), (uint8)(c & 0xFFu)};
				}
				nkgui::NkColor C(uint16 role) const noexcept {
					return Unpack(mTheme.Get(role));
				}

				nkgui::NkGuiContext &mCtx;
				const NkTheme &mTheme;
		};

	} // namespace editorkit
} // namespace nkentseu
