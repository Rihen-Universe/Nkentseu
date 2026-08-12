//
// NkArFlow.cpp
// =============================================================================
// Description :
//   Sélection de points saillants, appariement par vignette, puis ajustement
//   d'une rotation de caméra sur les déplacements observés.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#include "NKXR/AR/NkArFlow.h"

namespace nkentseu {
	namespace xr {

		namespace {

			// Relief local : somme des écarts horizontaux et verticaux sur une
			// petite fenêtre. Une vignette sans relief se recolle n'importe où,
			// et une seule fausse correspondance suffit à faire pivoter tout le
			// monde — mieux vaut la refuser dès la sélection.
			uint32 GradientScore(const uint8 *img, uint32 w, uint32 h, uint32 cx, uint32 cy) {
				if (cx < 2u || cy < 2u || cx + 2u >= w || cy + 2u >= h) {
					return 0;
				}
				uint32 score = 0;
				for (int32 dy = -2; dy <= 2; ++dy) {
					const uint8 *row = img + (nk_size(cy) + nk_size(dy)) * nk_size(w) + nk_size(cx);
					for (int32 dx = -2; dx <= 2; ++dx) {
						const int32 gx = int32(row[dx + 1]) - int32(row[dx - 1]);
						const int32 gy = int32(row[dx + int32(w)]) - int32(row[dx - int32(w)]);
						score += uint32((gx < 0 ? -gx : gx) + (gy < 0 ? -gy : gy));
					}
				}
				return score;
			}

			// Somme des différences absolues entre deux vignettes. Arrêt anticipé
			// dès que le total dépasse le meilleur connu : la recherche passe le
			// plus clair de son temps sur des positions manifestement mauvaises.
			uint32 PatchSad(const uint8 *a, const uint8 *b, uint32 w, uint32 ax, uint32 ay, uint32 bx, uint32 by,
							uint32 radius, uint32 bestSoFar) {
				uint32 sum = 0;
				const int32 r = int32(radius);
				for (int32 dy = -r; dy <= r; ++dy) {
					const uint8 *ra = a + (nk_size(int32(ay) + dy)) * nk_size(w) + nk_size(int32(ax) - r);
					const uint8 *rb = b + (nk_size(int32(by) + dy)) * nk_size(w) + nk_size(int32(bx) - r);
					for (int32 dx = 0; dx <= 2 * r; ++dx) {
						const int32 d = int32(ra[dx]) - int32(rb[dx]);
						sum += uint32(d < 0 ? -d : d);
					}
					if (sum >= bestSoFar) {
						return sum; // inutile de finir : déjà pire
					}
				}
				return sum;
			}

			float32 MedianOf(NkVector<float32> &values) {
				// Tri par insertion : quelques dizaines d'éléments, la simplicité
				// prime sur l'asymptotique.
				for (nk_size i = 1; i < values.Size(); ++i) {
					const float32 key = values[i];
					nk_size j = i;
					while (j > 0 && values[j - 1] > key) {
						values[j] = values[j - 1];
						--j;
					}
					values[j] = key;
				}
				if (values.Size() == 0) {
					return 0.f;
				}
				return values[values.Size() / 2u];
			}

			// Résolution 3×3 par pivot partiel. Pas de NkMat3 ici : le système
			// vient d'équations normales, il peut être mal conditionné (image
			// pauvre), et le pivot permet de le DÉTECTER au lieu de rendre une
			// solution absurde.
			bool Solve3(float32 m[3][4], float32 out[3]) {
				for (uint32 col = 0; col < 3u; ++col) {
					uint32 pivot = col;
					float32 best = m[col][col] < 0.f ? -m[col][col] : m[col][col];
					for (uint32 row = col + 1u; row < 3u; ++row) {
						const float32 v = m[row][col] < 0.f ? -m[row][col] : m[row][col];
						if (v > best) {
							best = v;
							pivot = row;
						}
					}
					if (best < 1e-9f) {
						return false;
					}
					if (pivot != col) {
						for (uint32 k = 0; k < 4u; ++k) {
							const float32 t = m[col][k];
							m[col][k] = m[pivot][k];
							m[pivot][k] = t;
						}
					}
					const float32 inv = 1.f / m[col][col];
					for (uint32 k = col; k < 4u; ++k) {
						m[col][k] *= inv;
					}
					for (uint32 row = 0; row < 3u; ++row) {
						if (row == col) {
							continue;
						}
						const float32 f = m[row][col];
						if (f == 0.f) {
							continue;
						}
						for (uint32 k = col; k < 4u; ++k) {
							m[row][k] -= f * m[col][k];
						}
					}
				}
				out[0] = m[0][3];
				out[1] = m[1][3];
				out[2] = m[2][3];
				return true;
			}

		} // namespace

		void NkArImageFlow::Reset() {
			mPrev.Clear();
			mWidth = 0;
			mHeight = 0;
			mHasPrev = false;
		}

		NkArFlowResult NkArImageFlow::Track(const uint8 *gray, uint32 width, uint32 height,
											const NkArCameraIntrinsics &intrinsics) {
			NkArFlowResult result;
			if (gray == nullptr || width < 32u || height < 32u) {
				return result;
			}

			// Changement de format : l'image précédente n'est plus comparable.
			if (!mHasPrev || width != mWidth || height != mHeight) {
				mWidth = width;
				mHeight = height;
				mPrev.Resize(nk_size(width) * nk_size(height));
				for (nk_size i = 0; i < mPrev.Size(); ++i) {
					mPrev[i] = gray[i];
				}
				mHasPrev = true;
				return result;
			}

			const uint32 radius = mConfig.patchRadius;
			const uint32 search = mConfig.searchRadius;
			// L'affinage sort du rayon de recherche d'un pas : la marge doit
			// l'inclure, sinon une vignette lirait hors de l'image.
			const uint32 margin = radius + search + mConfig.coarseStep + 3u;
			if (width <= 2u * margin || height <= 2u * margin) {
				return result; // image trop petite pour la fenêtre demandée
			}

			// ── 1) Sélection : le point le plus contrasté de chaque case ──────
			NkVector<float32> du;
			NkVector<float32> dv;
			NkVector<float32> px;
			NkVector<float32> py;
			const uint32 cellW = (width - 2u * margin) / mConfig.cellsX;
			const uint32 cellH = (height - 2u * margin) / mConfig.cellsY;
			if (cellW < 4u || cellH < 4u) {
				return result;
			}
			NkVector<uint32> candX;
			NkVector<uint32> candY;
			NkVector<uint32> candGrad;
			for (uint32 cy = 0; cy < mConfig.cellsY; ++cy) {
				for (uint32 cx = 0; cx < mConfig.cellsX; ++cx) {
					uint32 bestScore = mConfig.minGradient;
					uint32 bx = 0;
					uint32 by = 0;
					bool found = false;
					// Un point sur trois suffit à repérer la case la plus
					// texturée : c'est un choix de coût, pas de précision — le
					// raffinement se fait ensuite à l'appariement.
					for (uint32 y = margin + cy * cellH; y < margin + (cy + 1u) * cellH; y += 3u) {
						for (uint32 x = margin + cx * cellW; x < margin + (cx + 1u) * cellW; x += 3u) {
							const uint32 score = GradientScore(&mPrev[0], width, height, x, y);
							if (score > bestScore) {
								bestScore = score;
								bx = x;
								by = y;
								found = true;
							}
						}
					}
					if (found) {
						candX.PushBack(bx);
						candY.PushBack(by);
						candGrad.PushBack(bestScore);
					}
				}
			}

			// Classement par relief DÉCROISSANT, et on ne garde que les meilleurs.
			// Choix ADAPTATIF, et c'est le point important : un seuil absolu
			// convient à une scène et affame la suivante. Une pièce aux murs
			// clairs n'offre pas ce qu'offre une bibliothèque — mais dans les deux
			// cas, ses meilleurs points sont les moins mauvais dont on dispose.
			for (nk_size i = 1; i < candGrad.Size(); ++i) {
				const uint32 g = candGrad[i];
				const uint32 x = candX[i];
				const uint32 y = candY[i];
				nk_size j = i;
				while (j > 0 && candGrad[j - 1] < g) {
					candGrad[j] = candGrad[j - 1];
					candX[j] = candX[j - 1];
					candY[j] = candY[j - 1];
					--j;
				}
				candGrad[j] = g;
				candX[j] = x;
				candY[j] = y;
			}
			const nk_size kept = (candGrad.Size() < mConfig.maxPoints) ? candGrad.Size() : nk_size(mConfig.maxPoints);
			// Relief médian des points retenus : c'est LUI qui sert de référence
			// au vote d'immobilité, et non un chiffre décidé d'avance. Par
			// construction, la moitié des points le franchit toujours — la règle
			// ne peut donc jamais tout rejeter, ce qui était le défaut de la
			// version précédente (« 16 textures, 16 ambigus, 0 retenus »).
			const uint32 medianGrad = (kept > 0) ? candGrad[kept / 2u] : 0u;

			for (nk_size ci = 0; ci < kept; ++ci) {
				{
					const uint32 bx = candX[ci];
					const uint32 by = candY[ci];
					const uint32 bestGradient = candGrad[ci];

					// ── 2) Appariement : balayage grossier puis affinage ──────
					// On garde AUSSI le meilleur concurrent situé ailleurs : c'est
					// lui qui dit si le pic est net ou si la vignette se recolle
					// un peu partout. Le seuil d'arrêt anticipé est donc le SECOND
					// et non le premier, sinon les concurrents ne seraient jamais
					// évalués exactement.
					uint32 bestSad = 0xFFFFFFFFu;
					uint32 secondSad = 0xFFFFFFFFu;
					int32 bestDx = 0;
					int32 bestDy = 0;
					bool hasBest = false;
					const int32 peak = int32(mConfig.peakRadius);
					const int32 coarse = int32(mConfig.coarseStep < 1u ? 1u : mConfig.coarseStep);
					for (int32 oy = -int32(search); oy <= int32(search); oy += coarse) {
						for (int32 ox = -int32(search); ox <= int32(search); ox += coarse) {
							const uint32 sad = PatchSad(&mPrev[0], gray, width, bx, by, uint32(int32(bx) + ox),
														uint32(int32(by) + oy), radius, secondSad);
							const int32 fx2 = ox - bestDx;
							const int32 fy2 = oy - bestDy;
							const bool far = (fx2 < -peak || fx2 > peak || fy2 < -peak || fy2 > peak);
							if (sad < bestSad) {
								if (hasBest && far) {
									secondSad = bestSad;
								}
								bestSad = sad;
								bestDx = ox;
								bestDy = oy;
								hasBest = true;
							}
							else if (sad < secondSad && far) {
								secondSad = sad;
							}
						}
					}
					// Affinage : le balayage grossier a pu manquer le fond du
					// creux de la moitié du pas, on le retrouve pixel par pixel.
					// Le centre est FIGÉ avant la boucle : s'en servir comme borne
					// alors qu'il bouge ferait glisser la fenêtre à chaque trouvaille.
					const int32 centerX = bestDx;
					const int32 centerY = bestDy;
					for (int32 oy = centerY - coarse; oy <= centerY + coarse; ++oy) {
						for (int32 ox = centerX - coarse; ox <= centerX + coarse; ++ox) {
							if (ox == centerX && oy == centerY) {
								continue;
							}
							const uint32 sad = PatchSad(&mPrev[0], gray, width, bx, by, uint32(int32(bx) + ox),
														uint32(int32(by) + oy), radius, bestSad);
							if (sad < bestSad) {
								bestSad = sad;
								bestDx = ox;
								bestDy = oy;
							}
						}
					}
					// Vignette qui ne retrouve nulle part son pareil : occultée,
					// sortie du champ, ou changement d'éclairage. L'écarter vaut
					// mieux que de la laisser voter.
					const uint32 side = 2u * radius + 1u;
					const uint32 maxSad = side * side * 40u;
					if (bestSad > maxSad) {
						continue;
					}
					++result.candidates;
					// Pic mou : la vignette s'accorde presque aussi bien ailleurs.
					// Elle ne sait pas où elle est allée — la laisser voter, c'est
					// précisément ce qui produisait une fausse « rotation nulle ».
					if (secondSad != 0xFFFFFFFFu &&
						float32(bestSad) > mConfig.ambiguityRatio * float32(secondSad)) {
						++result.ambiguous;
						continue;
					}
					// Vote « rien n'a bougé » : on ne l'accepte que d'une vignette
					// franchement texturée (voir minGradientForStillVote).
					const int32 still = int32(mConfig.stillRadiusPixels);
					const bool votesStill = (bestDx >= -still && bestDx <= still && bestDy >= -still &&
											 bestDy <= still);
					if (votesStill && bestGradient < medianGrad) {
						++result.ambiguous;
						continue;
					}
					// ── Affinage SOUS-PIXEL ───────────────────────────────────
					// Décisif, et voici pourquoi : un panoramique lent déplace
					// l'image d'un ou deux pixels par image. Arrondi à l'entier,
					// ce mouvement se perd — la moitié du temps il tombe à zéro —
					// et la rotation cumulée n'avance jamais, alors même que la
					// pièce défile sous les yeux. Mesuré sur les captures de
					// Rihen : 450 px de défilement réel, 0,3° annoncés.
					// La parabole ajustée sur les trois accords autour du fond du
					// creux rend la fraction manquante. Les accords sont
					// recalculés SANS arrêt anticipé : il faut ici des valeurs
					// exactes, pas des bornes inférieures.
					float32 subX = float32(bestDx);
					float32 subY = float32(bestDy);
					{
						const uint32 sLeft = PatchSad(&mPrev[0], gray, width, bx, by, uint32(int32(bx) + bestDx - 1),
													  uint32(int32(by) + bestDy), radius, 0xFFFFFFFFu);
						const uint32 sRight = PatchSad(&mPrev[0], gray, width, bx, by, uint32(int32(bx) + bestDx + 1),
													   uint32(int32(by) + bestDy), radius, 0xFFFFFFFFu);
						const float32 denom = float32(sLeft) + float32(sRight) - 2.f * float32(bestSad);
						if (denom > 1.f) {
							const float32 shift = 0.5f * (float32(sLeft) - float32(sRight)) / denom;
							if (shift > -1.f && shift < 1.f) {
								subX += shift;
							}
						}
					}
					{
						const uint32 sUp = PatchSad(&mPrev[0], gray, width, bx, by, uint32(int32(bx) + bestDx),
													uint32(int32(by) + bestDy - 1), radius, 0xFFFFFFFFu);
						const uint32 sDown = PatchSad(&mPrev[0], gray, width, bx, by, uint32(int32(bx) + bestDx),
													  uint32(int32(by) + bestDy + 1), radius, 0xFFFFFFFFu);
						const float32 denom = float32(sUp) + float32(sDown) - 2.f * float32(bestSad);
						if (denom > 1.f) {
							const float32 shift = 0.5f * (float32(sUp) - float32(sDown)) / denom;
							if (shift > -1.f && shift < 1.f) {
								subY += shift;
							}
						}
					}
					du.PushBack(subX);
					dv.PushBack(subY);
					px.PushBack(float32(bx) - intrinsics.cx);
					py.PushBack(float32(by) - intrinsics.cy);
				}
			}

			if (du.Size() < mConfig.minInliers) {
				// Toujours mémoriser l'image : sans cela, la comparaison suivante
				// porterait sur deux images éloignées et échouerait à son tour.
				for (nk_size i = 0; i < mPrev.Size(); ++i) {
					mPrev[i] = gray[i];
				}
				return result;
			}

			// ── 3) Rejet des intrus, autour du mouvement MÉDIAN ───────────────
			// La médiane résiste à quelques appariements faux ; une moyenne, non
			// — un seul point aberrant suffirait à entraîner toute la rotation.
			NkVector<float32> tmpU = du;
			NkVector<float32> tmpV = dv;
			const float32 medU = MedianOf(tmpU);
			const float32 medV = MedianOf(tmpV);
			NkVector<float32> shifts;
			for (nk_size i = 0; i < du.Size(); ++i) {
				const float32 sx = du[i] - medU;
				const float32 sy = dv[i] - medV;
				shifts.PushBack(math::NkSqrt(sx * sx + sy * sy));
			}
			NkVector<float32> shiftsCopy = shifts;
			result.medianShiftPixels = math::NkSqrt(medU * medU + medV * medV);
			(void)MedianOf(shiftsCopy);

			// ── 4) Ajustement d'une rotation pure ─────────────────────────────
			// Modèle, en petits angles et en pixels (u', v' comptés depuis le
			// centre optique) :
			//     du = tx − γ·v'
			//     dv = ty + γ·u'
			// avec tx = fx·lacet, ty = fy·tangage, γ = roulis. Trois inconnues,
			// deux équations par point : le système est très surdéterminé, ce
			// qui est exactement ce qu'on veut sur des mesures bruitées.
			float32 m[3][4] = {};
			uint32 used = 0;
			for (nk_size i = 0; i < du.Size(); ++i) {
				if (shifts[i] > mConfig.inlierPixels) {
					continue;
				}
				const float32 u = px[i];
				const float32 v = py[i];
				// Ligne « du » : [1, 0, −v] · (tx, ty, γ) = du
				m[0][0] += 1.f;
				m[0][2] += -v;
				m[0][3] += du[i];
				// Ligne « dv » : [0, 1, u] · (tx, ty, γ) = dv
				m[1][1] += 1.f;
				m[1][2] += u;
				m[1][3] += dv[i];
				// Ligne du paramètre γ
				m[2][0] += -v;
				m[2][1] += u;
				m[2][2] += u * u + v * v;
				m[2][3] += -du[i] * v + dv[i] * u;
				++used;
			}
			if (used < mConfig.minInliers) {
				for (nk_size i = 0; i < mPrev.Size(); ++i) {
					mPrev[i] = gray[i];
				}
				return result;
			}

			float32 sol[3] = {};
			const bool solved = Solve3(m, sol);
			if (solved) {
				const float32 fx = intrinsics.fx > 1.f ? intrinsics.fx : 1.f;
				const float32 fy = intrinsics.fy > 1.f ? intrinsics.fy : 1.f;
				// Le contenu glisse à DROITE quand la caméra tourne à GAUCHE :
				// le signe est celui-ci, il a été vérifié contre la pose donnée
				// par le marqueur lui-même (test de non-régression).
				result.yawRad = sol[0] / fx;
				result.pitchRad = sol[1] / fy;
				result.rollRad = sol[2];
				result.inliers = used;
				result.valid = true;

				// Résidu : ce que le modèle n'explique pas. Une caméra qui
				// AVANCE produit un résidu élevé (parallaxe), et c'est le seul
				// signal dont nous disposons pour dire « ne me crois pas trop ».
				float32 residual = 0.f;
				for (nk_size i = 0; i < du.Size(); ++i) {
					if (shifts[i] > mConfig.inlierPixels) {
						continue;
					}
					const float32 eu = du[i] - (sol[0] - sol[2] * py[i]);
					const float32 ev = dv[i] - (sol[1] + sol[2] * px[i]);
					residual += math::NkSqrt(eu * eu + ev * ev);
				}
				result.residualPixels = residual / float32(used);
			}

			for (nk_size i = 0; i < mPrev.Size(); ++i) {
				mPrev[i] = gray[i];
			}
			return result;
		}

	} // namespace xr
} // namespace nkentseu
