//
// NkPdfRaster.cpp — rasterisation de traces, anti-aliasee (voir NkPdfRaster.h).
//
#include "NKMedia/Pdf/NkPdfRaster.h"

namespace nkentseu {
	namespace nkcode {
		namespace pdf {

			static inline double Abs(double v) { return v < 0 ? -v : v; }
			static inline double Min2(double a, double b) { return a < b ? a : b; }
			static inline double Max2(double a, double b) { return a > b ? a : b; }

			// ============================================================
			// Construction du trace
			// ============================================================

			void NkPdfPath::MoveTo(double x, double y) {
				mStarts.PushBack(static_cast<int32>(mPts.Size()));
				NkPdfPt p;
				p.x = x;
				p.y = y;
				mPts.PushBack(p);
				mCurX = mStartX = x;
				mCurY = mStartY = y;
				mHasCur = true;
			}

			void NkPdfPath::LineTo(double x, double y) {
				// Un `l` sans `m` prealable est malforme ; on le traite comme un `m`
				// plutot que d'ecrire hors du tableau des sous-traces.
				if (!mHasCur) {
					MoveTo(x, y);
					return;
				}
				NkPdfPt p;
				p.x = x;
				p.y = y;
				mPts.PushBack(p);
				mCurX = x;
				mCurY = y;
			}

			// Subdivision recursive : on coupe tant que la courbe s'ecarte de la corde
			// de plus que la tolerance. Adaptatif — une courbe presque droite ne coute
			// qu'un segment, une boucle serree en recoit autant qu'il faut.
			void NkPdfPath::Flatten(double x0, double y0, double x1, double y1, double x2, double y2, double x3,
									double y3, int32 depth) {
				if (depth > 16) { // borne dure : coordonnees aberrantes possibles
					LineTo(x3, y3);
					return;
				}
				// Distance des points de controle a la corde, mesuree sans racine
				// carree (on compare des aires, pas des longueurs).
				const double dx = x3 - x0, dy = y3 - y0;
				double d1 = Abs((x1 - x3) * dy - (y1 - y3) * dx);
				double d2 = Abs((x2 - x3) * dy - (y2 - y3) * dx);
				const double dd = (d1 + d2) * (d1 + d2);
				const double tol = FlatTolerance();
				if (dd < tol * (dx * dx + dy * dy)) {
					LineTo(x3, y3);
					return;
				}
				// Subdivision de De Casteljau au parametre 1/2.
				const double x01 = (x0 + x1) * 0.5, y01 = (y0 + y1) * 0.5;
				const double x12 = (x1 + x2) * 0.5, y12 = (y1 + y2) * 0.5;
				const double x23 = (x2 + x3) * 0.5, y23 = (y2 + y3) * 0.5;
				const double xa = (x01 + x12) * 0.5, ya = (y01 + y12) * 0.5;
				const double xb = (x12 + x23) * 0.5, yb = (y12 + y23) * 0.5;
				const double xm = (xa + xb) * 0.5, ym = (ya + yb) * 0.5;
				Flatten(x0, y0, x01, y01, xa, ya, xm, ym, depth + 1);
				Flatten(xm, ym, xb, yb, x23, y23, x3, y3, depth + 1);
			}

			void NkPdfPath::CurveTo(double x1, double y1, double x2, double y2, double x3, double y3) {
				if (!mHasCur)
					MoveTo(x1, y1);
				Flatten(mCurX, mCurY, x1, y1, x2, y2, x3, y3, 0);
				mCurX = x3;
				mCurY = y3;
			}

			void NkPdfPath::QuadTo(double cx, double cy, double x, double y) {
				// Une quadratique est exactement representable en cubique : on evite
				// d'ecrire un second aplatisseur.
				if (!mHasCur)
					MoveTo(cx, cy);
				const double c1x = mCurX + 2.0 / 3.0 * (cx - mCurX);
				const double c1y = mCurY + 2.0 / 3.0 * (cy - mCurY);
				const double c2x = x + 2.0 / 3.0 * (cx - x);
				const double c2y = y + 2.0 / 3.0 * (cy - y);
				CurveTo(c1x, c1y, c2x, c2y, x, y);
			}

			void NkPdfPath::Close() {
				if (!mHasCur)
					return;
				// Le remplissage ferme implicitement chaque sous-trace ; on ne rajoute
				// un point que s'il manque vraiment, pour ne pas creer de segment nul.
				if (Abs(mCurX - mStartX) > 1e-12 || Abs(mCurY - mStartY) > 1e-12)
					LineTo(mStartX, mStartY);
				mCurX = mStartX;
				mCurY = mStartY;
			}

			void NkPdfPath::Rect(double x, double y, double w, double h) {
				MoveTo(x, y);
				LineTo(x + w, y);
				LineTo(x + w, y + h);
				LineTo(x, y + h);
				Close();
			}

			bool NkPdfPath::Bounds(double *x0, double *y0, double *x1, double *y1) const {
				if (mPts.Empty())
					return false;
				double ax = mPts[0].x, ay = mPts[0].y, bx = ax, by = ay;
				for (usize i = 1; i < mPts.Size(); ++i) {
					ax = Min2(ax, mPts[i].x);
					ay = Min2(ay, mPts[i].y);
					bx = Max2(bx, mPts[i].x);
					by = Max2(by, mPts[i].y);
				}
				if (x0)
					*x0 = ax;
				if (y0)
					*y0 = ay;
				if (x1)
					*x1 = bx;
				if (y1)
					*y1 = by;
				return true;
			}

			// ============================================================
			// Cible de rendu
			// ============================================================

			bool NkPdfCanvas::Create(int32 w, int32 h) {
				Destroy();
				if (w <= 0 || h <= 0)
					return false;
				// Garde-fou : une page peut declarer des dimensions aberrantes. 20000 px
				// de cote couvre largement un A0 a 300 ppp.
				if (w > 20000 || h > 20000)
					return false;
				mW = w;
				mH = h;
				mPix.Resize(static_cast<usize>(w) * static_cast<usize>(h) * 4u);
				Clear(255, 255, 255, 255);
				return true;
			}

			void NkPdfCanvas::Destroy() {
				mPix.Clear();
				mClip.Clear();
				mW = mH = 0;
			}

			void NkPdfCanvas::Clear(uint8 r, uint8 g, uint8 b, uint8 a) {
				for (usize i = 0; i + 3 < mPix.Size(); i += 4) {
					mPix[i] = r;
					mPix[i + 1] = g;
					mPix[i + 2] = b;
					mPix[i + 3] = a;
				}
			}

			void NkPdfCanvas::ClearClip() { mClip.Clear(); }

			void NkPdfCanvas::PushClipState() {
				// Aucune copie ici : c'est tout l'interet. On note simplement qu'un
				// niveau s'ouvre.
				ClipLevel lv;
				lv.dirty = false;
				mClipStack.PushBack(lv);
			}

			void NkPdfCanvas::PopClipState() {
				if (mClipStack.Empty())
					return;
				const usize last = mClipStack.Size() - 1;
				if (mClipStack[last].dirty)
					mClip = mClipStack[last].saved; // restauration effective
				mClipStack.Erase(mClipStack.Begin() + last);
			}

			// ── Coeur : couverture par balayage ─────────────────────────────────────
			//
			// Pour chaque ligne de pixels on echantillonne kSub sous-lignes. Sur
			// chacune, on releve les intersections avec les aretes, on les trie, puis
			// on parcourt en accumulant l'enroulement pour decider ce qui est
			// « dedans ». La couverture horizontale est calculee de facon EXACTE aux
			// extremites de chaque segment couvert (fraction de pixel), pas seulement
			// echantillonnee : c'est ce qui donne des bords propres sans multiplier
			// les sous-echantillons horizontaux.
			// 16 sous-lignes, et non 5. La couverture verticale est QUANTIFIEE au pas
			// de 1/kSub : avec 5, un bord horizontal a une position fractionnaire est
			// arrondi a 0,2 pres — mesure a +0,20 % d'aire sur un rectangle a bords
			// fractionnaires, et surtout des barres de lettres irregulieres, car le
			// texte est plein de bords horizontaux. 16 divise cette erreur par trois.
			// Le surcout est absorbe par la LISTE D'ARETES ACTIVES ci-dessous : sans
			// elle, chaque sous-ligne reparcourait toutes les aretes de la page.
			static constexpr int32 kSub = 16; // sous-lignes par pixel

			void NkPdfCanvas::Rasterize(const NkPdfPath &path, bool evenOdd, NkVector<uint8> &cov,
										int32 *obx0, int32 *oby0, int32 *obx1, int32 *oby1) const {
				// Boite vide par defaut : l'appelant ne parcourt alors rien.
				if (obx0) *obx0 = 0;
				if (oby0) *oby0 = 0;
				if (obx1) *obx1 = 0;
				if (oby1) *oby1 = 0;
				if (cov.Size() != static_cast<usize>(mW) * static_cast<usize>(mH))
					cov.Resize(static_cast<usize>(mW) * static_cast<usize>(mH));
				if (path.Empty() || mW <= 0 || mH <= 0)
					return;

				// Boite du trace : on ne remet a zero et ne parcourt QUE cette zone.
				double px0 = 0, py0 = 0, px1 = 0, py1 = 0;
				if (!path.Bounds(&px0, &py0, &px1, &py1))
					return;
				int32 cx0 = static_cast<int32>(px0) - 1, cy0 = static_cast<int32>(py0) - 1;
				int32 cx1 = static_cast<int32>(px1) + 2, cy1 = static_cast<int32>(py1) + 2;
				if (cx0 < 0) cx0 = 0;
				if (cy0 < 0) cy0 = 0;
				if (cx1 > mW) cx1 = mW;
				if (cy1 > mH) cy1 = mH;
				if (cx0 >= cx1 || cy0 >= cy1)
					return;
				for (int32 y = cy0; y < cy1; ++y) {
					uint8 *row = cov.Data() + static_cast<usize>(y) * static_cast<usize>(mW);
					for (int32 x = cx0; x < cx1; ++x)
						row[x] = 0;
				}
				if (obx0) *obx0 = cx0;
				if (oby0) *oby0 = cy0;
				if (obx1) *obx1 = cx1;
				if (oby1) *oby1 = cy1;

				const NkVector<NkPdfPt> &pts = path.Points();
				const NkVector<int32> &starts = path.Starts();

				// Aretes : (x0,y0)-(x1,y1) avec le sens memorise pour l'enroulement.
				struct Edge {
						double x0, y0, x1, y1;
						int32 dir;
				};
				NkVector<Edge> edges;
				for (usize s = 0; s < starts.Size(); ++s) {
					const int32 beg = starts[s];
					const int32 end = (s + 1 < starts.Size()) ? starts[s + 1] : static_cast<int32>(pts.Size());
					if (end - beg < 2)
						continue;
					for (int32 i = beg; i < end; ++i) {
						const int32 j = (i + 1 < end) ? (i + 1) : beg; // fermeture implicite
						double ax = pts[static_cast<usize>(i)].x, ay = pts[static_cast<usize>(i)].y;
						double bx = pts[static_cast<usize>(j)].x, by = pts[static_cast<usize>(j)].y;
						if (ay == by)
							continue; // horizontale : ne croise aucune sous-ligne
						Edge e;
						e.dir = (ay < by) ? 1 : -1;
						if (ay > by) { // normalise : y0 < y1
							double t = ax;
							ax = bx;
							bx = t;
							t = ay;
							ay = by;
							by = t;
						}
						e.x0 = ax;
						e.y0 = ay;
						e.x1 = bx;
						e.y1 = by;
						edges.PushBack(e);
					}
				}
				if (edges.Empty())
					return;

				// Bornes verticales : deja connues par la boite du trace, calculee plus
				// haut. On les reutilise plutot que de reparcourir les aretes.
				int32 rowBeg = static_cast<int32>(py0);
				int32 rowEnd = static_cast<int32>(py1) + 1;
				if (rowBeg < cy0)
					rowBeg = cy0;
				if (rowEnd > cy1)
					rowEnd = cy1;
				if (rowBeg >= rowEnd)
					return;

				// ── Liste d'aretes ACTIVES ──
				// Sans elle, chaque sous-ligne reparcourait TOUTES les aretes de la
				// page : une page de texte en compte des dizaines de milliers, pour
				// quelques-unes seulement qui croisent une ligne donnee. On trie les
				// aretes par y de depart, puis on maintient l'ensemble de celles qui
				// chevauchent la ligne courante. Les sous-lignes etant parcourues dans
				// l'ordre croissant, l'ajout se fait par simple avancee d'un curseur.
				NkVector<int32> order;
				order.Resize(edges.Size());
				for (usize i = 0; i < edges.Size(); ++i)
					order[i] = static_cast<int32>(i);
				// Tri par insertion sur les INDEX : les aretes d'un trace arrivent
				// deja largement ordonnees, ce qui rend ce tri quasi lineaire ici.
				for (usize i = 1; i < order.Size(); ++i) {
					const int32 k = order[i];
					const double ky = edges[static_cast<usize>(k)].y0;
					usize j = i;
					while (j > 0 && edges[static_cast<usize>(order[j - 1])].y0 > ky) {
						order[j] = order[j - 1];
						--j;
					}
					order[j] = k;
				}
				usize nextEdge = 0;	   // prochaine arete a activer, dans `order`
				NkVector<int32> active; // index d'aretes chevauchant la ligne courante

				// Accumulateur de couverture d'une ligne, en 1/kSub de pixel.
				NkVector<float32> acc;
				acc.Resize(static_cast<usize>(cx1 - cx0));
				NkVector<double> xs;   // abscisses des croisements
				NkVector<int32> dirs; // sens correspondant

				const double subH = 1.0 / static_cast<double>(kSub);
				const float32 subW = static_cast<float32>(subH);

				for (int32 row = rowBeg; row < rowEnd; ++row) {
					for (usize i = 0; i < acc.Size(); ++i)
						acc[i] = 0.f;
					bool touched = false;

					for (int32 s = 0; s < kSub; ++s) {
						// Centre de la sous-ligne : evite les cas degeneres exactement
						// sur un sommet, qui compteraient deux fois.
						const double sy = static_cast<double>(row) + (static_cast<double>(s) + 0.5) * subH;

						// Active ce qui commence, retire ce qui est fini. `order` etant
						// trie par y0 et `sy` croissant, un seul curseur suffit.
						while (nextEdge < order.Size() &&
							   edges[static_cast<usize>(order[nextEdge])].y0 <= sy) {
							active.PushBack(order[nextEdge]);
							++nextEdge;
						}
						for (usize i = 0; i < active.Size();) {
							if (edges[static_cast<usize>(active[i])].y1 <= sy) {
								active[i] = active[active.Size() - 1]; // retrait en O(1)
								active.Erase(active.Begin() + (active.Size() - 1));
							} else
								++i;
						}

						xs.Clear();
						dirs.Clear();
						for (usize a = 0; a < active.Size(); ++a) {
							const Edge &ed = edges[static_cast<usize>(active[a])];
							if (sy < ed.y0 || sy >= ed.y1)
								continue;
							const double t = (sy - ed.y0) / (ed.y1 - ed.y0);
							xs.PushBack(ed.x0 + t * (ed.x1 - ed.x0));
							dirs.PushBack(ed.dir);
						}
						if (xs.Size() < 2)
							continue;
						// Tri par insertion : les listes de croisements sont courtes
						// (quelques unites), un tri sophistique n'apporterait rien.
						for (usize i = 1; i < xs.Size(); ++i) {
							const double kx = xs[i];
							const int32 kd = dirs[i];
							usize j = i;
							while (j > 0 && xs[j - 1] > kx) {
								xs[j] = xs[j - 1];
								dirs[j] = dirs[j - 1];
								--j;
							}
							xs[j] = kx;
							dirs[j] = kd;
						}

						int32 wind = 0;
						for (usize i = 0; i + 1 < xs.Size(); ++i) {
							wind += dirs[i];
							const bool inside = evenOdd ? (((i + 1) & 1) != 0) : (wind != 0);
							if (!inside)
								continue;
							double xa = xs[i], xb = xs[i + 1];
							if (xb <= 0.0 || xa >= static_cast<double>(mW))
								continue;
							if (xa < 0.0)
								xa = 0.0;
							if (xb > static_cast<double>(mW))
								xb = static_cast<double>(mW);
							if (xb <= xa)
								continue;
							touched = true;

							const int32 ia = static_cast<int32>(xa);
							const int32 ib = static_cast<int32>(xb);
							if (ia == ib) { // segment entierement dans un pixel
								if (ia >= cx0 && ia < cx1)
									acc[static_cast<usize>(ia - cx0)] +=
										static_cast<float32>(xb - xa) * subW;
								continue;
							}
							// Pixel de gauche : fraction couverte a droite de xa.
							if (ia >= cx0 && ia < cx1)
								acc[static_cast<usize>(ia - cx0)] +=
									static_cast<float32>(static_cast<double>(ia + 1) - xa) * subW;
							// Pixels pleins.
							for (int32 x = ia + 1; x < ib; ++x)
								if (x >= cx0 && x < cx1)
									acc[static_cast<usize>(x - cx0)] += subW;
							// Pixel de droite : fraction couverte a gauche de xb.
							if (ib >= cx0 && ib < cx1)
								acc[static_cast<usize>(ib - cx0)] +=
									static_cast<float32>(xb - static_cast<double>(ib)) * subW;
						}
					}

					if (!touched)
						continue;
					uint8 *dst = cov.Data() + static_cast<usize>(row) * static_cast<usize>(mW);
					for (int32 x = cx0; x < cx1; ++x) {
						float32 v = acc[static_cast<usize>(x - cx0)];
						if (v <= 0.f)
							continue;
						if (v > 1.f)
							v = 1.f; // les auto-recouvrements ne doivent pas depasser 1
						dst[x] = static_cast<uint8>(v * 255.f + 0.5f);
					}
				}
			}

			void NkPdfCanvas::FillPath(const NkPdfPath &path, bool evenOdd, uint8 r, uint8 g, uint8 b, uint8 a) {
				if (!Valid() || a == 0)
					return;
				int32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
				Rasterize(path, evenOdd, mScratch, &x0, &y0, &x1, &y1);
				if (x0 >= x1 || y0 >= y1)
					return;
				const bool clip = !mClip.Empty();
				for (int32 yy = y0; yy < y1; ++yy)
				for (int32 xx = x0; xx < x1; ++xx) {
					const usize i = static_cast<usize>(yy) * static_cast<usize>(mW) +
									static_cast<usize>(xx);
					uint32 c = mScratch[i];
					if (!c)
						continue;
					if (clip) {
						c = (c * mClip[i]) / 255u; // intersection avec le decoupage
						if (!c)
							continue;
					}
					const uint32 alpha = (c * static_cast<uint32>(a)) / 255u;
					if (!alpha)
						continue;
					uint8 *p = mPix.Data() + i * 4u;
					// Melange « source over », en entiers : dst = src*a + dst*(1-a).
					p[0] = static_cast<uint8>((r * alpha + p[0] * (255u - alpha)) / 255u);
					p[1] = static_cast<uint8>((g * alpha + p[1] * (255u - alpha)) / 255u);
					p[2] = static_cast<uint8>((b * alpha + p[2] * (255u - alpha)) / 255u);
					const uint32 na = alpha + (p[3] * (255u - alpha)) / 255u;
					p[3] = static_cast<uint8>(na > 255u ? 255u : na);
				}
			}

			// Couverture COMPLETE (hors boite = 0). Le decoupage en a besoin : il
			// doit valoir 0 partout ailleurs, pas conserver d'anciennes valeurs.
			void NkPdfCanvas::ComputeCoverageRaw(const NkPdfPath &path, bool evenOdd,
												 NkVector<uint8> &cov) const {
				cov.Resize(static_cast<usize>(mW) * static_cast<usize>(mH));
				for (usize i = 0; i < cov.Size(); ++i)
					cov[i] = 0;
				int32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
				Rasterize(path, evenOdd, cov, &x0, &y0, &x1, &y1);
			}

			void NkPdfCanvas::ComputeCoverage(const NkPdfPath &path, bool evenOdd,
											  NkVector<uint8> &cov) const {
				int32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
				Rasterize(path, evenOdd, cov, &x0, &y0, &x1, &y1);
				// Les pixels HORS boite conservent leur valeur precedente : ils ne sont
				// pas remis a zero (c'est tout l'interet). On les neutralise donc ici,
				// car l'appelant parcourt tout le tampon.
				for (usize i = 0; i < cov.Size(); ++i) {
					const int32 yy = static_cast<int32>(i / static_cast<usize>(mW));
					const int32 xx = static_cast<int32>(i % static_cast<usize>(mW));
					if (xx < x0 || xx >= x1 || yy < y0 || yy >= y1)
						cov[i] = 0;
				}
				// Applique le decoupage ICI plutot que de laisser chaque appelant y
				// penser : l'oublier une seule fois ferait deborder un degrade.
				if (!mClip.Empty())
					for (usize i = 0; i < cov.Size() && i < mClip.Size(); ++i)
						cov[i] = static_cast<uint8>((static_cast<uint32>(cov[i]) * mClip[i]) / 255u);
			}

			void NkPdfCanvas::BlendPixel(int32 x, int32 y, uint8 r, uint8 g, uint8 b, uint32 alpha) {
				if (x < 0 || y < 0 || x >= mW || y >= mH || alpha == 0)
					return;
				uint8 *p = mPix.Data() + (static_cast<usize>(y) * static_cast<usize>(mW) +
										  static_cast<usize>(x)) * 4u;
				if (alpha > 255u)
					alpha = 255u;
				p[0] = static_cast<uint8>((r * alpha + p[0] * (255u - alpha)) / 255u);
				p[1] = static_cast<uint8>((g * alpha + p[1] * (255u - alpha)) / 255u);
				p[2] = static_cast<uint8>((b * alpha + p[2] * (255u - alpha)) / 255u);
				const uint32 na = alpha + (p[3] * (255u - alpha)) / 255u;
				p[3] = static_cast<uint8>(na > 255u ? 255u : na);
			}

			void NkPdfCanvas::SetClipFromPath(const NkPdfPath &path, bool evenOdd) {
				// COPIE PARESSEUSE : on ne sauvegarde l'etat du niveau courant qu'ici,
				// au moment ou il est vraiment sur le point de changer.
				if (!mClipStack.Empty()) {
					ClipLevel &lv = mClipStack[mClipStack.Size() - 1];
					if (!lv.dirty) {
						lv.saved = mClip;
						lv.dirty = true;
					}
				}
				NkVector<uint8> cov;
				ComputeCoverageRaw(path, evenOdd, cov);
				if (mClip.Empty()) {
					mClip = cov;
					return;
				}
				// INTERSECTION, jamais remplacement : les q/Q du PDF empilent des
				// decoupages successifs, chacun restreignant le precedent.
				for (usize i = 0; i < mClip.Size() && i < cov.Size(); ++i)
					mClip[i] = static_cast<uint8>((static_cast<uint32>(mClip[i]) * cov[i]) / 255u);
			}

			// ── Contour ─────────────────────────────────────────────────────────────
			//
			// On CONSTRUIT le trace du contour (un quadrilatere par segment, plus une
			// pastille carree a chaque jonction) puis on le remplit avec la regle NON
			// NULLE. Cela evite d'ecrire un second moteur de rasterisation, au prix
			// d'un rendu de jointure approximatif — suffisant pour de la lecture, et
			// ameliorable plus tard sans toucher au reste.
			void NkPdfCanvas::StrokePath(const NkPdfPath &path, double width, uint8 r, uint8 g, uint8 b,
										 uint8 a) {
				if (!Valid() || a == 0 || path.Empty())
					return;
				// Une epaisseur nulle signifie « le trait le plus fin possible » en PDF,
				// pas « invisible » : on la ramene a un pixel.
				double w = width;
				if (w < 1.0)
					w = 1.0;
				const double h = w * 0.5;

				const NkVector<NkPdfPt> &pts = path.Points();
				const NkVector<int32> &starts = path.Starts();
				NkPdfPath quads;
				for (usize s = 0; s < starts.Size(); ++s) {
					const int32 beg = starts[s];
					const int32 end = (s + 1 < starts.Size()) ? starts[s + 1] : static_cast<int32>(pts.Size());
					for (int32 i = beg; i + 1 < end; ++i) {
						const NkPdfPt &p0 = pts[static_cast<usize>(i)];
						const NkPdfPt &p1 = pts[static_cast<usize>(i + 1)];
						double dx = p1.x - p0.x, dy = p1.y - p0.y;
						const double len = dx * dx + dy * dy;
						if (len < 1e-18)
							continue;
						// Normale unitaire, mise a la demi-epaisseur.
						double il = 1.0;
						{ // racine carree par Newton : evite <cmath> dans ce module
							double v = len, x = v;
							for (int32 k = 0; k < 20; ++k)
								x = 0.5 * (x + v / x);
							il = 1.0 / x;
						}
						const double nx = -dy * il * h, ny = dx * il * h;
						quads.MoveTo(p0.x + nx, p0.y + ny);
						quads.LineTo(p1.x + nx, p1.y + ny);
						quads.LineTo(p1.x - nx, p1.y - ny);
						quads.LineTo(p0.x - nx, p0.y - ny);
						quads.Close();
						// Jonction : petit carre centre sur le sommet, pour boucher
						// l'encoche entre deux segments d'orientations differentes.
						if (i + 2 < end)
							quads.Rect(p1.x - h, p1.y - h, w, w);
					}
				}
				FillPath(quads, false, r, g, b, a);
			}

		} // namespace pdf
	} // namespace nkcode
} // namespace nkentseu
