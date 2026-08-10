//
// NkPdfShading.cpp — voir NkPdfShading.h.
//
#include "NKMedia/Pdf/NkPdfShading.h"

namespace nkentseu {
	namespace nkcode {
		namespace pdf {

			static inline double Clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
			static double Pow(double b, double e) {
				// Puissance a exposant reel positif, sans <cmath> : exp(e * ln b).
				if (b <= 0.0)
					return 0.0;
				if (e == 1.0)
					return b;
				// ln par serie sur (b-1)/(b+1), convergente pour b > 0.
				double z = (b - 1.0) / (b + 1.0), z2 = z * z, s = 0.0, term = z;
				for (int32 k = 0; k < 40; ++k) {
					s += term / static_cast<double>(2 * k + 1);
					term *= z2;
				}
				const double ln = 2.0 * s;
				// exp par serie.
				double x = e * ln, sum = 1.0, t = 1.0;
				for (int32 k = 1; k < 40; ++k) {
					t *= x / static_cast<double>(k);
					sum += t;
				}
				return sum;
			}

			// ============================================================
			// Fonctions
			// ============================================================

			bool NkPdfFunction::Load(const NkPdfDoc &doc, const NkPdfVal &fn) {
				if (!fn.IsDictLike())
					return false;
				const NkPdfVal ft = doc.DictGet(fn, "FunctionType");
				if (!ft.IsNum())
					return false;
				mType = static_cast<int32>(ft.num);

				const NkPdfVal dom = doc.DictGet(fn, "Domain");
				if (dom.kind == NK_PDF_ARRAY && dom.b >= 2) {
					mD0 = doc.Num(doc.ArrayAt(dom, 0), 0.0);
					mD1 = doc.Num(doc.ArrayAt(dom, 1), 1.0);
				}

				if (mType == 2) {
					const NkPdfVal c0 = doc.DictGet(fn, "C0");
					const NkPdfVal c1 = doc.DictGet(fn, "C1");
					mOutN = 1;
					if (c0.kind == NK_PDF_ARRAY) {
						mOutN = c0.b < 8 ? c0.b : 8;
						for (int32 i = 0; i < mOutN; ++i)
							mC0[i] = doc.Num(doc.ArrayAt(c0, i), 0.0);
					} else
						mC0[0] = 0.0;
					if (c1.kind == NK_PDF_ARRAY) {
						const int32 n = c1.b < 8 ? c1.b : 8;
						if (n > mOutN)
							mOutN = n;
						for (int32 i = 0; i < n; ++i)
							mC1[i] = doc.Num(doc.ArrayAt(c1, i), 1.0);
					} else
						mC1[0] = 1.0;
					mN = doc.Num(doc.DictGet(fn, "N"), 1.0);
					return true;
				}

				if (mType == 3) {
					const NkPdfVal fns = doc.DictGet(fn, "Functions");
					if (fns.kind != NK_PDF_ARRAY)
						return false;
					for (int32 i = 0; i < fns.b; ++i) {
						NkPdfFunction f;
						if (f.Load(doc, doc.ArrayAt(fns, i))) {
							if (f.OutCount() > mOutN)
								mOutN = f.OutCount();
							mSubs.PushBack(f);
						}
					}
					const NkPdfVal bd = doc.DictGet(fn, "Bounds");
					for (int32 i = 0; bd.kind == NK_PDF_ARRAY && i < bd.b; ++i)
						mBounds.PushBack(doc.Num(doc.ArrayAt(bd, i), 0.0));
					const NkPdfVal en = doc.DictGet(fn, "Encode");
					for (int32 i = 0; en.kind == NK_PDF_ARRAY && i < en.b; ++i)
						mEncode.PushBack(doc.Num(doc.ArrayAt(en, i), 0.0));
					return !mSubs.Empty();
				}

				if (mType == 0) {
					if (fn.kind != NK_PDF_STREAM)
						return false;
					const NkPdfVal sz = doc.DictGet(fn, "Size");
					if (sz.kind != NK_PDF_ARRAY || sz.b < 1)
						return false;
					mSize = static_cast<int32>(doc.Num(doc.ArrayAt(sz, 0), 0.0));
					if (mSize < 2 || mSize > 100000)
						return false;
					const int32 bps = static_cast<int32>(doc.Num(doc.DictGet(fn, "BitsPerSample"), 8.0));
					const NkPdfVal rg = doc.DictGet(fn, "Range");
					if (rg.kind != NK_PDF_ARRAY || rg.b < 2)
						return false;
					mOutN = rg.b / 2;
					for (int32 i = 0; i < rg.b; ++i)
						mRange.PushBack(doc.Num(doc.ArrayAt(rg, i), 0.0));

					NkVector<uint8> data;
					if (!doc.DecodeStream(fn, data))
						return false;
					// Lecture bit a bit : BitsPerSample vaut 1, 2, 4, 8, 12, 16, 24 ou 32.
					const int64 total = static_cast<int64>(mSize) * mOutN;
					const double maxV = (bps >= 32) ? 4294967295.0 : (static_cast<double>(1u << bps) - 1.0);
					usize bit = 0;
					for (int64 i = 0; i < total; ++i) {
						uint32 v = 0;
						for (int32 k = 0; k < bps; ++k) {
							const usize byteIdx = (bit + static_cast<usize>(k)) / 8u;
							if (byteIdx >= data.Size()) {
								v <<= (bps - k);
								break;
							}
							const uint32 b = (data[byteIdx] >> (7u - ((bit + static_cast<usize>(k)) % 8u))) & 1u;
							v = (v << 1) | b;
						}
						bit += static_cast<usize>(bps);
						mSamples.PushBack(static_cast<double>(v) / maxV);
					}
					return !mSamples.Empty();
				}

				// Type 4 : petit langage PostScript. Non interprete — l'appelant le
				// signale plutot que d'inventer une couleur.
				mType = -1;
				return false;
			}

			int32 NkPdfFunction::Eval(double t, double *out, int32 maxOut) const {
				if (mType < 0 || !out || maxOut <= 0)
					return 0;
				// Le domaine borne l'entree : au-dela, la valeur est celle du bord.
				if (t < mD0)
					t = mD0;
				if (t > mD1)
					t = mD1;

				if (mType == 2) {
					const double k = (mN == 1.0) ? t : Pow(t, mN);
					const int32 n = mOutN < maxOut ? mOutN : maxOut;
					for (int32 i = 0; i < n; ++i)
						out[i] = mC0[i] + k * (mC1[i] - mC0[i]);
					return n;
				}

				if (mType == 3) {
					// Trouve le sous-intervalle, puis re-encode t dans le domaine de la
					// sous-fonction.
					const usize nsub = mSubs.Size();
					usize k = 0;
					double lo = mD0, hi = mD1;
					for (; k < mBounds.Size() && k < nsub - 1; ++k) {
						if (t < mBounds[k]) {
							hi = mBounds[k];
							break;
						}
						lo = mBounds[k];
					}
					if (k >= nsub)
						k = nsub - 1;
					if (k < mBounds.Size())
						hi = mBounds[k];
					if (k == nsub - 1)
						hi = mD1;
					double e0 = 0.0, e1 = 1.0;
					if (mEncode.Size() >= (k + 1) * 2) {
						e0 = mEncode[k * 2];
						e1 = mEncode[k * 2 + 1];
					}
					const double u = (hi > lo) ? ((t - lo) / (hi - lo)) : 0.0;
					return mSubs[k].Eval(e0 + u * (e1 - e0), out, maxOut);
				}

				if (mType == 0) {
					const double u = (mD1 > mD0) ? ((t - mD0) / (mD1 - mD0)) : 0.0;
					const double pos = u * static_cast<double>(mSize - 1);
					int32 i0 = static_cast<int32>(pos);
					if (i0 < 0)
						i0 = 0;
					if (i0 > mSize - 2)
						i0 = mSize - 2;
					const double frac = pos - static_cast<double>(i0);
					const int32 n = mOutN < maxOut ? mOutN : maxOut;
					for (int32 c = 0; c < n; ++c) {
						const usize a = static_cast<usize>(i0) * static_cast<usize>(mOutN) +
										static_cast<usize>(c);
						const usize b = a + static_cast<usize>(mOutN);
						if (b >= mSamples.Size())
							break;
						// Interpolation LINEAIRE : sans elle, un degre sur 32 echantillons
						// ressort en bandes visibles.
						const double s = mSamples[a] + frac * (mSamples[b] - mSamples[a]);
						const double r0 = (mRange.Size() > static_cast<usize>(c * 2)) ? mRange[c * 2] : 0.0;
						const double r1 = (mRange.Size() > static_cast<usize>(c * 2 + 1)) ? mRange[c * 2 + 1] : 1.0;
						out[c] = r0 + s * (r1 - r0);
					}
					return n;
				}
				return 0;
			}

			// ============================================================
			// Degrades
			// ============================================================

			bool NkPdfShading::Load(const NkPdfDoc &doc, const NkPdfVal &sh) {
				if (!sh.IsDictLike())
					return false;
				mType = static_cast<int32>(doc.Num(doc.DictGet(sh, "ShadingType"), 0.0));
				if (mType != 2 && mType != 3)
					return false;

				const NkPdfVal co = doc.DictGet(sh, "Coords");
				const int32 need = (mType == 2) ? 4 : 6;
				if (co.kind != NK_PDF_ARRAY || co.b < need)
					return false;
				for (int32 i = 0; i < need; ++i)
					mCoords[i] = doc.Num(doc.ArrayAt(co, i), 0.0);

				const NkPdfVal dm = doc.DictGet(sh, "Domain");
				if (dm.kind == NK_PDF_ARRAY && dm.b >= 2) {
					mT0 = doc.Num(doc.ArrayAt(dm, 0), 0.0);
					mT1 = doc.Num(doc.ArrayAt(dm, 1), 1.0);
				}
				const NkPdfVal ex = doc.DictGet(sh, "Extend");
				if (ex.kind == NK_PDF_ARRAY && ex.b >= 2) {
					mExtend0 = doc.ArrayAt(ex, 0).num != 0.0;
					mExtend1 = doc.ArrayAt(ex, 1).num != 0.0;
				}

				const NkPdfVal cs = doc.DictGet(sh, "ColorSpace");
				if (doc.NameIs(cs, "DeviceGray"))
					mComps = 1;
				else if (doc.NameIs(cs, "DeviceCMYK"))
					mComps = 4;
				else
					mComps = 3;

				const NkPdfVal fn = doc.DictGet(sh, "Function");
				if (fn.kind == NK_PDF_ARRAY) {
					// Un tableau de fonctions : une par composante.
					for (int32 i = 0; i < fn.b; ++i) {
						NkPdfFunction f;
						if (f.Load(doc, doc.ArrayAt(fn, i)))
							mFns.PushBack(f);
					}
				} else {
					NkPdfFunction f;
					if (f.Load(doc, fn))
						mFns.PushBack(f);
				}
				return !mFns.Empty();
			}

			void NkPdfShading::FnColor(double t, double *r, double *g, double *b) const {
				double c[8] = {0, 0, 0, 0, 0, 0, 0, 0};
				int32 n = 0;
				if (mFns.Size() == 1)
					n = mFns[0].Eval(t, c, 8);
				else {
					// Une fonction par composante.
					for (usize i = 0; i < mFns.Size() && i < 8; ++i) {
						double one = 0.0;
						if (mFns[i].Eval(t, &one, 1) > 0)
							c[i] = one;
						n = static_cast<int32>(i) + 1;
					}
				}
				if (n <= 0) {
					*r = *g = *b = 0.0;
					return;
				}
				if (mComps == 1 || n == 1) {
					*r = *g = *b = Clamp01(c[0]);
				} else if (mComps == 4 || n >= 4) {
					*r = Clamp01((1.0 - c[0]) * (1.0 - c[3]));
					*g = Clamp01((1.0 - c[1]) * (1.0 - c[3]));
					*b = Clamp01((1.0 - c[2]) * (1.0 - c[3]));
				} else {
					*r = Clamp01(c[0]);
					*g = Clamp01(c[1]);
					*b = Clamp01(c[2]);
				}
			}

			bool NkPdfShading::ColorAt(double x, double y, double *r, double *g, double *b) const {
				double s = 0.0;
				if (mType == 2) {
					// AXIAL : projection orthogonale sur l'axe.
					const double x0 = mCoords[0], y0 = mCoords[1];
					const double x1 = mCoords[2], y1 = mCoords[3];
					const double dx = x1 - x0, dy = y1 - y0;
					const double den = dx * dx + dy * dy;
					if (den < 1e-12)
						return false;
					s = ((x - x0) * dx + (y - y0) * dy) / den;
				} else {
					// RADIAL : plus grand s tel que le point soit sur le cercle
					// interpole. On resout l'equation du second degre.
					const double x0 = mCoords[0], y0 = mCoords[1], r0 = mCoords[2];
					const double x1 = mCoords[3], y1 = mCoords[4], r1 = mCoords[5];
					const double cdx = x1 - x0, cdy = y1 - y0, dr = r1 - r0;
					const double pdx = x - x0, pdy = y - y0;
					const double a = cdx * cdx + cdy * cdy - dr * dr;
					const double bq = pdx * cdx + pdy * cdy + r0 * dr;
					const double cq = pdx * pdx + pdy * pdy - r0 * r0;
					if (a > -1e-12 && a < 1e-12) {
						if (bq > -1e-12 && bq < 1e-12)
							return false;
						s = cq / (2.0 * bq);
					} else {
						const double disc = bq * bq - a * cq;
						if (disc < 0.0)
							return false;
						// Racine carree par Newton (pas de <cmath> dans ce module).
						double sq = disc > 1.0 ? disc : 1.0;
						for (int32 k = 0; k < 30; ++k)
							sq = 0.5 * (sq + disc / sq);
						const double s1 = (bq + sq) / a, s2 = (bq - sq) / a;
						// On prend la plus grande racine dont le rayon reste positif.
						s = s1;
						if (r0 + s1 * dr < 0.0)
							s = s2;
						if (r0 + s * dr < 0.0)
							return false;
					}
				}

				// Hors [0,1] : peint seulement si le degrade est ETENDU de ce cote.
				if (s < 0.0) {
					if (!mExtend0)
						return false;
					s = 0.0;
				}
				if (s > 1.0) {
					if (!mExtend1)
						return false;
					s = 1.0;
				}
				FnColor(mT0 + s * (mT1 - mT0), r, g, b);
				return true;
			}

		} // namespace pdf
	} // namespace nkcode
} // namespace nkentseu
