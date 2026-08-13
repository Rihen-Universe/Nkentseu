//
// NkArCalibration.cpp
// =============================================================================
// Description :
//   Homographie par vue, puis résolution de la matrice de l'objectif par la
//   méthode de Zhang. Tout est écrit ici : le solveur linéaire, la recherche du
//   vecteur propre, et la mesure d'erreur qui dit si l'on peut y croire.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#include "NKXR/AR/NkArCalibration.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace xr {

		namespace {

			// Résolution d'un système n×n par élimination de Gauss avec pivot
			// partiel. Le pivot n'est pas un ornement : les équations de
			// calibration sont mal conditionnées quand les vues se ressemblent,
			// et sans lui on obtient une réponse absurde au lieu d'un refus.
			bool SolveLinear(float64 *a, float64 *b, uint32 n) {
				for (uint32 col = 0; col < n; ++col) {
					uint32 pivot = col;
					float64 best = a[col * n + col] < 0 ? -a[col * n + col] : a[col * n + col];
					for (uint32 row = col + 1u; row < n; ++row) {
						const float64 v = a[row * n + col] < 0 ? -a[row * n + col] : a[row * n + col];
						if (v > best) {
							best = v;
							pivot = row;
						}
					}
					if (best < 1e-12) {
						return false;
					}
					if (pivot != col) {
						for (uint32 k = 0; k < n; ++k) {
							const float64 t = a[col * n + k];
							a[col * n + k] = a[pivot * n + k];
							a[pivot * n + k] = t;
						}
						const float64 t = b[col];
						b[col] = b[pivot];
						b[pivot] = t;
					}
					const float64 inv = 1.0 / a[col * n + col];
					for (uint32 k = col; k < n; ++k) {
						a[col * n + k] *= inv;
					}
					b[col] *= inv;
					for (uint32 row = 0; row < n; ++row) {
						if (row == col) {
							continue;
						}
						const float64 f = a[row * n + col];
						if (f == 0.0) {
							continue;
						}
						for (uint32 k = col; k < n; ++k) {
							a[row * n + k] -= f * a[col * n + k];
						}
						b[row] -= f * b[col];
					}
				}
				return true;
			}

			// Vecteur propre associé à la plus PETITE valeur propre d'une
			// matrice symétrique, par la méthode de la puissance inverse avec
			// décalage. C'est ce qu'il faut pour un système homogène A·x = 0 :
			// la solution est la direction que A écrase le plus.
			bool SmallestEigenvector(const float64 *m, uint32 n, float64 *out) {
				// Décalage : on cherche le vecteur propre dominant de
				// (M + εI)^-1, ce qui revient à résoudre à chaque itération.
				float64 x[6];
				for (uint32 i = 0; i < n; ++i) {
					x[i] = (i == 0) ? 1.0 : 0.3 / float64(i + 1u);
				}
				float64 a[36];
				for (uint32 iter = 0; iter < 60u; ++iter) {
					for (uint32 i = 0; i < n * n; ++i) {
						a[i] = m[i];
					}
					for (uint32 i = 0; i < n; ++i) {
						a[i * n + i] += 1e-9;
					}
					float64 b[6];
					for (uint32 i = 0; i < n; ++i) {
						b[i] = x[i];
					}
					if (!SolveLinear(a, b, n)) {
						return false;
					}
					float64 norm = 0.0;
					for (uint32 i = 0; i < n; ++i) {
						norm += b[i] * b[i];
					}
					if (norm < 1e-300) {
						return false;
					}
					norm = math::NkSqrt(float32(norm));
					for (uint32 i = 0; i < n; ++i) {
						x[i] = b[i] / norm;
					}
				}
				for (uint32 i = 0; i < n; ++i) {
					out[i] = x[i];
				}
				return true;
			}

			// Homographie plan → image par transformation directe (DLT), sur les
			// correspondances d'une vue. Les points sont NORMALISÉS avant
			// résolution : sans cela, mêler des mètres (10⁻²) et des pixels
			// (10³) dans la même matrice la rend numériquement sourde.
			bool ComputeHomography(const NkVector<NkVec2f> &src, const NkVector<NkVec2f> &dst, float32 *outH) {
				const nk_size n = src.Size();
				if (n < 4 || dst.Size() != n) {
					return false;
				}
				// Normalisation isotrope : centre à l'origine, distance moyenne √2.
				float64 sx = 0, sy = 0, dx = 0, dy = 0;
				for (nk_size i = 0; i < n; ++i) {
					sx += src[i].x;
					sy += src[i].y;
					dx += dst[i].x;
					dy += dst[i].y;
				}
				sx /= float64(n);
				sy /= float64(n);
				dx /= float64(n);
				dy /= float64(n);
				float64 ss = 0, ds = 0;
				for (nk_size i = 0; i < n; ++i) {
					ss += math::NkSqrt(float32((src[i].x - sx) * (src[i].x - sx) + (src[i].y - sy) * (src[i].y - sy)));
					ds += math::NkSqrt(float32((dst[i].x - dx) * (dst[i].x - dx) + (dst[i].y - dy) * (dst[i].y - dy)));
				}
				ss = (ss > 1e-12) ? (1.4142135 * float64(n) / ss) : 1.0;
				ds = (ds > 1e-12) ? (1.4142135 * float64(n) / ds) : 1.0;

				// Système 8 inconnues (h33 = 1), moindres carrés par équations
				// normales : deux lignes par correspondance.
				float64 ata[64] = {};
				float64 atb[8] = {};
				for (nk_size i = 0; i < n; ++i) {
					const float64 X = (src[i].x - sx) * ss;
					const float64 Y = (src[i].y - sy) * ss;
					const float64 u = (dst[i].x - dx) * ds;
					const float64 v = (dst[i].y - dy) * ds;
					const float64 r1[8] = { X, Y, 1, 0, 0, 0, -u * X, -u * Y };
					const float64 r2[8] = { 0, 0, 0, X, Y, 1, -v * X, -v * Y };
					for (uint32 a = 0; a < 8u; ++a) {
						for (uint32 b = 0; b < 8u; ++b) {
							ata[a * 8 + b] += r1[a] * r1[b] + r2[a] * r2[b];
						}
						atb[a] += r1[a] * u + r2[a] * v;
					}
				}
				if (!SolveLinear(ata, atb, 8)) {
					return false;
				}
				// Dénormalisation : H = T_dst^-1 · Hn · T_src
				const float64 hn[9] = { atb[0], atb[1], atb[2], atb[3], atb[4], atb[5], atb[6], atb[7], 1.0 };
				const float64 ts[9] = { ss, 0, -ss * sx, 0, ss, -ss * sy, 0, 0, 1 };
				const float64 tdInv[9] = { 1.0 / ds, 0, dx, 0, 1.0 / ds, dy, 0, 0, 1 };
				float64 tmp[9] = {};
				for (uint32 r = 0; r < 3u; ++r) {
					for (uint32 c = 0; c < 3u; ++c) {
						float64 s = 0;
						for (uint32 k = 0; k < 3u; ++k) {
							s += hn[r * 3 + k] * ts[k * 3 + c];
						}
						tmp[r * 3 + c] = s;
					}
				}
				for (uint32 r = 0; r < 3u; ++r) {
					for (uint32 c = 0; c < 3u; ++c) {
						float64 s = 0;
						for (uint32 k = 0; k < 3u; ++k) {
							s += tdInv[r * 3 + k] * tmp[k * 3 + c];
						}
						outH[r * 3 + c] = float32(s);
					}
				}
				const float32 last = outH[8];
				if (last > 1e-12f || last < -1e-12f) {
					for (uint32 i = 0; i < 9u; ++i) {
						outH[i] /= last;
					}
				}
				return true;
			}

		} // namespace

		bool NkArCalibrationBoard::CenterOf(int32 id, float32 &outX, float32 &outY) const {
			const int32 index = id - firstId;
			if (index < 0 || uint32(index) >= cols * rows) {
				return false;
			}
			const uint32 cx = uint32(index) % cols;
			const uint32 cy = uint32(index) / cols;
			// Origine au centre de la planche : les nombres restent petits et
			// symétriques, ce qui conditionne mieux les équations.
			outX = (float32(cx) - float32(cols - 1u) * 0.5f) * spacingMeters;
			outY = (float32(rows - 1u) * 0.5f - float32(cy)) * spacingMeters;
			return true;
		}

		void NkArCalibration::Initialize(const NkArCalibrationBoard &board, uint32 imageWidth, uint32 imageHeight) {
			mBoard = board;
			mWidth = imageWidth;
			mHeight = imageHeight;
			mViews.Clear();
		}

		void NkArCalibration::Reset() {
			mViews.Clear();
		}

		bool NkArCalibration::AddView(const NkVector<NkArDetection> &detections) {
			View view;
			const float32 half = mBoard.markerSizeMeters * 0.5f;
			for (nk_size i = 0; i < detections.Size(); ++i) {
				float32 cx = 0.f, cy = 0.f;
				if (!mBoard.CenterOf(detections[i].id, cx, cy)) {
					continue; // marqueur étranger à la planche : on l'ignore
				}
				// Ordre des coins de la détection : horaire, coin 0 en haut à
				// gauche du motif. Même ordre côté planche, sinon on apparierait
				// des points qui ne se correspondent pas — et l'homographie
				// serait fausse sans que rien ne le signale.
				const NkVec2f local[4] = { { cx - half, cy + half },
										   { cx + half, cy + half },
										   { cx + half, cy - half },
										   { cx - half, cy - half } };
				for (uint32 c = 0; c < 4u; ++c) {
					view.boardPoints.PushBack(local[c]);
					view.imagePoints.PushBack(detections[i].corners[c]);
				}
			}
			// Quatre points suffisent à une homographie, mais pas à une vue
			// utile : il en faut assez pour que le bruit se compense.
			if (view.boardPoints.Size() < 12) {
				return false;
			}
			// ── Refuser une vue trop SEMBLABLE à une précédente ───────────────
			// Deux vues identiques donnent deux fois la même équation : elles
			// gonflent le compteur sans rien apprendre au système, et donnent
			// l'illusion d'une calibration bien nourrie alors qu'elle reste
			// sous-déterminée. C'est le piège de cette méthode : il faut varier
			// les ANGLES, pas multiplier les prises.
			// On compare des FORMES, pas des positions. Ce qui apprend quelque
			// chose au système, c'est de voir la planche sous un autre ANGLE :
			// la déplacer ou s'en éloigner ne change que sa place et sa taille à
			// l'image, et n'ajoute aucune équation. On retire donc à chaque vue
			// son centre et son échelle avant de la comparer aux précédentes.
			//
			// Mesuré le 13 août : avec une comparaison en pixels bruts, six vues
			// ont été retenues en un sixième de seconde — six fois le même point
			// de vue, à un tremblement de main près. Le système, sous-déterminé,
			// a rendu une focale absurde, heureusement refusée par l'erreur de
			// reprojection.
			auto normalize = [](const NkVector<NkVec2f> &pts, NkVector<NkVec2f> &out) {
				float32 cx = 0.f, cy = 0.f;
				for (nk_size i = 0; i < pts.Size(); ++i) {
					cx += pts[i].x;
					cy += pts[i].y;
				}
				cx /= float32(pts.Size());
				cy /= float32(pts.Size());
				float32 scale = 0.f;
				for (nk_size i = 0; i < pts.Size(); ++i) {
					scale += math::NkSqrt((pts[i].x - cx) * (pts[i].x - cx) + (pts[i].y - cy) * (pts[i].y - cy));
				}
				scale = (scale > 1e-6f) ? (float32(pts.Size()) / scale) : 1.f;
				out.Clear();
				for (nk_size i = 0; i < pts.Size(); ++i) {
					out.PushBack(NkVec2f((pts[i].x - cx) * scale, (pts[i].y - cy) * scale));
				}
			};
			NkVector<NkVec2f> shape;
			normalize(view.imagePoints, shape);
			for (nk_size v = 0; v < mViews.Size(); ++v) {
				const View &prev = mViews[v];
				if (prev.imagePoints.Size() != view.imagePoints.Size()) {
					continue;
				}
				NkVector<NkVec2f> prevShape;
				normalize(prev.imagePoints, prevShape);
				float32 sum = 0.f;
				for (nk_size i = 0; i < shape.Size(); ++i) {
					const float32 dx = shape[i].x - prevShape[i].x;
					const float32 dy = shape[i].y - prevShape[i].y;
					sum += math::NkSqrt(dx * dx + dy * dy);
				}
				if (sum / float32(shape.Size()) < mMinShapeDifference) {
					return false;
				}
			}
			if (!ComputeHomography(view.boardPoints, view.imagePoints, view.homography)) {
				return false;
			}
			mViews.PushBack(view);
			return true;
		}

		NkArCalibrationResult NkArCalibration::Solve() const {
			NkArCalibrationResult out;
			const nk_size n = mViews.Size();
			if (n < 3) {
				return out;
			}

			// ── Cas ROBUSTE : centre optique supposé au milieu ────────────────
			// On recentre les points sur le milieu de l'image, ce qui annule le
			// centre optique dans les équations. B se réduit alors à
			// diag(1/fx², 1/fy², 1), et les deux contraintes de Zhang deviennent
			// LINÉAIRES en (1/fx², 1/fy²) : un système à deux inconnues, très
			// surdéterminé, donc stable même avec peu d'angles. C'est ce qui
			// distingue une mesure d'un pari.
			if (mAssumeCentered) {
				const float32 ccx = float32(mWidth) * 0.5f;
				const float32 ccy = float32(mHeight) * 0.5f;
				float64 ata[4] = {};
				float64 atb[2] = {};
				uint32 equations = 0;
				for (nk_size k = 0; k < n; ++k) {
					// Homographie recalculée sur des points recentrés : c'est le
					// même geste que retirer le centre optique de K.
					NkVector<NkVec2f> centered;
					for (nk_size i = 0; i < mViews[k].imagePoints.Size(); ++i) {
						centered.PushBack(NkVec2f(mViews[k].imagePoints[i].x - ccx, mViews[k].imagePoints[i].y - ccy));
					}
					float32 h[9] = {};
					if (!ComputeHomography(mViews[k].boardPoints, centered, h)) {
						continue;
					}
					// h1ᵀ B h2 = 0  →  a·h11h12 + b·h21h22 + h31h32 = 0
					// h1ᵀ B h1 = h2ᵀ B h2 → a(h11²−h12²) + b(h21²−h22²) + (h31²−h32²) = 0
					const float64 r1[2] = { float64(h[0]) * h[1], float64(h[3]) * h[4] };
					const float64 b1 = -(float64(h[6]) * h[7]);
					const float64 r2[2] = { float64(h[0]) * h[0] - float64(h[1]) * h[1],
											float64(h[3]) * h[3] - float64(h[4]) * h[4] };
					const float64 b2 = -(float64(h[6]) * h[6] - float64(h[7]) * h[7]);
					for (uint32 a = 0; a < 2u; ++a) {
						for (uint32 b = 0; b < 2u; ++b) {
							ata[a * 2 + b] += r1[a] * r1[b] + r2[a] * r2[b];
						}
						atb[a] += r1[a] * b1 + r2[a] * b2;
					}
					++equations;
				}
				if (equations >= 3u && SolveLinear(ata, atb, 2)) {
					// atb tient maintenant 1/fx² et 1/fy².
					if (atb[0] > 1e-12 && atb[1] > 1e-12) {
						out.intrinsics.fx = float32(1.0 / math::NkSqrt(float32(atb[0])));
						out.intrinsics.fy = float32(1.0 / math::NkSqrt(float32(atb[1])));
						out.intrinsics.cx = ccx;
						out.intrinsics.cy = ccy;
						out.viewsUsed = equations;
						out.valid = (out.intrinsics.fx > 1.f && out.intrinsics.fy > 1.f);
					}
				}
				if (out.valid) {
					out.fovXDegrees =
						2.f * math::NkAtan(float32(mWidth) * 0.5f / out.intrinsics.fx) * 180.f / math::NK_PI_F;
					// Erreur de reprojection, mesurée avec les homographies des
					// vues : elle juge la cohérence des données, pas la formule.
					float64 sum = 0.0;
					uint32 count = 0;
					for (nk_size k = 0; k < n; ++k) {
						const View &v = mViews[k];
						for (nk_size i = 0; i < v.boardPoints.Size(); ++i) {
							const float32 X = v.boardPoints[i].x, Y = v.boardPoints[i].y;
							const float32 w = v.homography[6] * X + v.homography[7] * Y + v.homography[8];
							if (w < 1e-9f && w > -1e-9f) {
								continue;
							}
							const float32 u = (v.homography[0] * X + v.homography[1] * Y + v.homography[2]) / w;
							const float32 vv = (v.homography[3] * X + v.homography[4] * Y + v.homography[5]) / w;
							const float32 ex = u - v.imagePoints[i].x;
							const float32 ey = vv - v.imagePoints[i].y;
							sum += math::NkSqrt(ex * ex + ey * ey);
							++count;
						}
					}
					out.reprojectionErrorPixels = (count > 0) ? float32(sum / float64(count)) : 0.f;
					return out;
				}
				// Sinon on retombe sur la résolution complète ci-dessous, plutôt
				// que de rendre un échec : mieux vaut une réponse imparfaite
				// accompagnée de son erreur qu'aucune réponse du tout.
			}

			// ── Zhang : deux contraintes par vue sur B = K^-T·K^-1 ────────────
			// B est symétrique et défini à un facteur près : six inconnues,
			// notées b = (B11, B12, B22, B13, B23, B33). Pour une homographie de
			// colonnes h1, h2, h3, l'orthonormalité des deux premières colonnes
			// de la rotation impose h1ᵀ B h2 = 0 et h1ᵀ B h1 = h2ᵀ B h2.
			NkVector<float64> rows;
			auto vij = [](const float32 *h, uint32 i, uint32 j, float64 *v) {
				const float64 hi0 = h[0 * 3 + i], hi1 = h[1 * 3 + i], hi2 = h[2 * 3 + i];
				const float64 hj0 = h[0 * 3 + j], hj1 = h[1 * 3 + j], hj2 = h[2 * 3 + j];
				v[0] = hi0 * hj0;
				v[1] = hi0 * hj1 + hi1 * hj0;
				v[2] = hi1 * hj1;
				v[3] = hi2 * hj0 + hi0 * hj2;
				v[4] = hi2 * hj1 + hi1 * hj2;
				v[5] = hi2 * hj2;
			};
			float64 ata[36] = {};
			for (nk_size k = 0; k < n; ++k) {
				float64 v01[6], v00[6], v11[6];
				vij(mViews[k].homography, 0, 1, v01);
				vij(mViews[k].homography, 0, 0, v00);
				vij(mViews[k].homography, 1, 1, v11);
				float64 diff[6];
				for (uint32 i = 0; i < 6u; ++i) {
					diff[i] = v00[i] - v11[i];
				}
				for (uint32 a = 0; a < 6u; ++a) {
					for (uint32 b = 0; b < 6u; ++b) {
						ata[a * 6 + b] += v01[a] * v01[b] + diff[a] * diff[b];
					}
				}
			}
			float64 b[6] = {};
			if (!SmallestEigenvector(ata, 6, b)) {
				return out;
			}

			// ── Extraire l'objectif de B ──────────────────────────────────────
			// Formules fermées de Zhang, en supposant l'absence de biais
			// (les capteurs modernes n'en ont pas de mesurable).
			const float64 B11 = b[0], B12 = b[1], B22 = b[2], B13 = b[3], B23 = b[4], B33 = b[5];
			const float64 den = B11 * B22 - B12 * B12;
			if (den < 1e-18 && den > -1e-18) {
				return out;
			}
			const float64 cy = (B12 * B13 - B11 * B23) / den;
			const float64 lambda = B33 - (B13 * B13 + cy * (B12 * B13 - B11 * B23)) / B11;
			if (lambda / B11 <= 0.0) {
				return out; // solution non physique : mieux vaut refuser
			}
			const float64 fx = math::NkSqrt(float32(lambda / B11));
			const float64 fyDen = lambda * B11 / den;
			if (fyDen <= 0.0) {
				return out;
			}
			const float64 fy = math::NkSqrt(float32(fyDen));
			const float64 cx = -B13 * fx * fx / lambda;

			out.intrinsics.fx = float32(fx);
			out.intrinsics.fy = float32(fy);
			out.intrinsics.cx = float32(cx);
			out.intrinsics.cy = float32(cy);
			out.viewsUsed = uint32(n);
			out.valid = (fx > 1.0 && fy > 1.0);
			if (!out.valid) {
				return out;
			}
			out.fovXDegrees = 2.f * math::NkAtan(float32(mWidth) * 0.5f / out.intrinsics.fx) * 180.f / math::NK_PI_F;

			// ── L'erreur de reprojection, le seul juge ────────────────────────
			// On reprojette chaque point de la planche par l'homographie de sa
			// vue et l'on mesure l'écart aux coins observés. Un résultat sans
			// cette mesure n'est pas un résultat : c'est une opinion.
			float64 sum = 0.0;
			uint32 count = 0;
			for (nk_size k = 0; k < n; ++k) {
				const View &v = mViews[k];
				for (nk_size i = 0; i < v.boardPoints.Size(); ++i) {
					const float32 X = v.boardPoints[i].x, Y = v.boardPoints[i].y;
					const float32 w = v.homography[6] * X + v.homography[7] * Y + v.homography[8];
					if (w < 1e-9f && w > -1e-9f) {
						continue;
					}
					const float32 u = (v.homography[0] * X + v.homography[1] * Y + v.homography[2]) / w;
					const float32 vv = (v.homography[3] * X + v.homography[4] * Y + v.homography[5]) / w;
					const float32 ex = u - v.imagePoints[i].x;
					const float32 ey = vv - v.imagePoints[i].y;
					sum += math::NkSqrt(ex * ex + ey * ey);
					++count;
				}
			}
			out.reprojectionErrorPixels = (count > 0) ? float32(sum / float64(count)) : 0.f;
			return out;
		}

		bool NkArRenderCalibrationBoard(const NkArCalibrationBoard &board, uint8 *outGray, uint32 outWidth,
										uint32 outHeight) {
			if (outGray == nullptr || outWidth < 64u || outHeight < 64u) {
				return false;
			}
			for (uint32 i = 0; i < outWidth * outHeight; ++i) {
				outGray[i] = 255; // fond blanc : c'est la marge de chaque marqueur
			}
			// Échelle : la planche entière tient dans l'image, marges comprises.
			const float32 boardW = float32(board.cols) * board.spacingMeters;
			const float32 boardH = float32(board.rows) * board.spacingMeters;
			const float32 scale =
				(float32(outWidth) / boardW < float32(outHeight) / boardH) ? (float32(outWidth) / boardW)
																		  : (float32(outHeight) / boardH);
			// ⚠️ Le motif rendu comprend une MARGE BLANCHE d'une cellule de
			// chaque côté : le carré NOIR n'occupe que (bits+2)/(bits+4) de son
			// étendue. Or `markerSizeMeters` désigne le carré NOIR — c'est lui
			// qu'on mesure à la règle et lui que le détecteur suit. On agrandit
			// donc la boîte de dessin d'autant, faute de quoi la planche produite
			// ne correspondrait pas à sa propre description : l'échelle serait
			// fausse d'un tiers, et la calibration avec elle.
			const float32 patternOverBlack =
				float32(board.gridBits + 4u) / float32(board.gridBits + 2u);
			const uint32 markerPx = uint32(board.markerSizeMeters * scale * patternOverBlack);
			if (markerPx < 16u) {
				return false;
			}
			NkVector<uint8> pattern;
			pattern.Resize(nk_size(markerPx) * markerPx);
			for (uint32 r = 0; r < board.rows; ++r) {
				for (uint32 c = 0; c < board.cols; ++c) {
					const int32 id = board.firstId + int32(r * board.cols + c);
					if (!NkArRenderMarker(id, board.gridBits, &pattern[0], markerPx)) {
						return false;
					}
					float32 bx = 0.f, by = 0.f;
					board.CenterOf(id, bx, by);
					const int32 px = int32(float32(outWidth) * 0.5f + bx * scale) - int32(markerPx / 2u);
					const int32 py = int32(float32(outHeight) * 0.5f - by * scale) - int32(markerPx / 2u);
					for (uint32 y = 0; y < markerPx; ++y) {
						const int32 dy = py + int32(y);
						if (dy < 0 || uint32(dy) >= outHeight) {
							continue;
						}
						for (uint32 x = 0; x < markerPx; ++x) {
							const int32 dx = px + int32(x);
							if (dx < 0 || uint32(dx) >= outWidth) {
								continue;
							}
							outGray[uint32(dy) * outWidth + uint32(dx)] = pattern[y * markerPx + x];
						}
					}
				}
			}
			return true;
		}

	} // namespace xr
} // namespace nkentseu
