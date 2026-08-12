//
// NkArMarker.cpp
// =============================================================================
// Description :
//   La chaîne de vision complète, écrite de zéro : Otsu → contours de Moore →
//   quadrilatères → homographie → lecture du code → pose plane.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#include "NKXR/AR/NkArMarker.h"
#include "NKMemory/NkAllocator.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace xr {

		namespace {

			// ── Seuil d'Otsu ─────────────────────────────────────────────────
			// Le seuil qui MAXIMISE la variance entre les deux populations de
			// gris : aucun réglage à deviner, l'image décide elle-même.
			uint8 OtsuThreshold(const uint8 *gray, uint32 count) {
				uint32 histogram[256] = {};
				for (uint32 i = 0; i < count; ++i) {
					++histogram[gray[i]];
				}
				float64 total = float64(count);
				float64 sumAll = 0.0;
				for (uint32 i = 0; i < 256u; ++i) {
					sumAll += float64(i) * float64(histogram[i]);
				}
				float64 sumBack = 0.0;
				float64 weightBack = 0.0;
				float64 bestVariance = -1.0;
				// Le maximum d'Otsu est souvent un PLATEAU (aucun pixel n'a de
				// valeur entre les deux modes : tous ces seuils séparent
				// pareil). Prendre le premier donnerait ici 0 — et « plus
				// sombre que 0 » n'est jamais vrai, donc AUCUN pixel sombre :
				// le détecteur rendait zéro sans rien pouvoir dire. On garde
				// donc les deux bornes du plateau et on rend son MILIEU.
				uint32 plateauFirst = 128, plateauLast = 128;
				for (uint32 t = 0; t < 256u; ++t) {
					weightBack += float64(histogram[t]);
					if (weightBack == 0.0) {
						continue;
					}
					const float64 weightFore = total - weightBack;
					if (weightFore == 0.0) {
						break;
					}
					sumBack += float64(t) * float64(histogram[t]);
					const float64 meanBack = sumBack / weightBack;
					const float64 meanFore = (sumAll - sumBack) / weightFore;
					const float64 delta = meanBack - meanFore;
					const float64 variance = weightBack * weightFore * delta * delta;
					if (variance > bestVariance) {
						bestVariance = variance;
						plateauFirst = t;
						plateauLast = t;
					}
					else if (variance == bestVariance) {
						plateauLast = t;
					}
				}
				return uint8((plateauFirst + plateauLast) / 2u);
			}

			// ── Seuillage ADAPTATIF par image intégrale ─────────────────────
			// Le seuil de chaque pixel est la moyenne de son voisinage, moins
			// une marge. L'image intégrale (somme des préfixes) donne la somme
			// de n'importe quelle fenêtre en 4 lectures : le coût ne dépend
			// donc PAS de la taille de fenêtre — sans elle, une fenêtre 41×41
			// coûterait 1681 lectures par pixel et la caméra ramerait.
			void AdaptiveThreshold(const uint8 *gray, uint32 width, uint32 height, uint32 window, int32 bias,
								   uint8 *outMask, memory::NkAllocator &allocator) {
				const nk_size count = nk_size(width) * height;
				uint64 *integral = static_cast<uint64 *>(allocator.Allocate(count * sizeof(uint64), alignof(uint64)));
				if (integral == nullptr) {
					// Repli honnête : sans mémoire, seuil fixe plutôt que rien.
					for (nk_size i = 0; i < count; ++i) {
						outMask[i] = (gray[i] < 128u) ? 1u : 0u;
					}
					return;
				}
				for (uint32 y = 0; y < height; ++y) {
					uint64 rowSum = 0;
					for (uint32 x = 0; x < width; ++x) {
						rowSum += gray[nk_size(y) * width + x];
						integral[nk_size(y) * width + x] = rowSum + ((y > 0u) ? integral[nk_size(y - 1u) * width + x] : 0u);
					}
				}
				const int32 half = int32(window / 2u);
				for (uint32 y = 0; y < height; ++y) {
					for (uint32 x = 0; x < width; ++x) {
						const int32 x0 = (int32(x) - half > 0) ? (int32(x) - half - 1) : -1;
						const int32 y0 = (int32(y) - half > 0) ? (int32(y) - half - 1) : -1;
						const int32 x1 = (int32(x) + half < int32(width)) ? (int32(x) + half) : int32(width) - 1;
						const int32 y1 = (int32(y) + half < int32(height)) ? (int32(y) + half) : int32(height) - 1;
						const uint64 a = (x0 >= 0 && y0 >= 0) ? integral[nk_size(y0) * width + uint32(x0)] : 0u;
						const uint64 b = (y0 >= 0) ? integral[nk_size(y0) * width + uint32(x1)] : 0u;
						const uint64 c = (x0 >= 0) ? integral[nk_size(y1) * width + uint32(x0)] : 0u;
						const uint64 d = integral[nk_size(y1) * width + uint32(x1)];
						const uint64 sum = d + a - b - c;
						const int64 area = int64(x1 - x0) * int64(y1 - y0);
						const int32 mean = (area > 0) ? int32(sum / uint64(area)) : 128;
						outMask[nk_size(y) * width + x] = (int32(gray[nk_size(y) * width + x]) < mean - bias) ? 1u : 0u;
					}
				}
				allocator.Deallocate(integral);
			}

			// ── Résolution d'un système linéaire n×n (Gauss, pivot partiel) ──
			bool SolveLinear(float64 *a, float64 *b, uint32 n) {
				for (uint32 col = 0; col < n; ++col) {
					uint32 pivot = col;
					float64 best = a[col * n + col] < 0.0 ? -a[col * n + col] : a[col * n + col];
					for (uint32 row = col + 1u; row < n; ++row) {
						const float64 value = a[row * n + col] < 0.0 ? -a[row * n + col] : a[row * n + col];
						if (value > best) {
							best = value;
							pivot = row;
						}
					}
					if (best < 1e-12) {
						return false; // système dégénéré : 4 points alignés
					}
					if (pivot != col) {
						for (uint32 k = 0; k < n; ++k) {
							const float64 tmp = a[col * n + k];
							a[col * n + k] = a[pivot * n + k];
							a[pivot * n + k] = tmp;
						}
						const float64 tmp = b[col];
						b[col] = b[pivot];
						b[pivot] = tmp;
					}
					for (uint32 row = 0; row < n; ++row) {
						if (row == col) {
							continue;
						}
						const float64 factor = a[row * n + col] / a[col * n + col];
						if (factor == 0.0) {
							continue;
						}
						for (uint32 k = col; k < n; ++k) {
							a[row * n + k] -= factor * a[col * n + k];
						}
						b[row] -= factor * b[col];
					}
				}
				for (uint32 i = 0; i < n; ++i) {
					b[i] /= a[i * n + i];
				}
				return true;
			}

			// ── Homographie par DLT sur 4 correspondances ────────────────────
			// h33 = 1 (l'homographie est définie à un facteur près) : il reste
			// 8 inconnues pour 8 équations — un système carré, pas de moindres
			// carrés à faire.
			bool ComputeHomography(const NkVec2f *from, const NkVec2f *to, float64 *outH) {
				float64 a[64] = {};
				float64 b[8] = {};
				for (uint32 i = 0; i < 4u; ++i) {
					const float64 x = float64(from[i].x);
					const float64 y = float64(from[i].y);
					const float64 u = float64(to[i].x);
					const float64 v = float64(to[i].y);
					float64 *row0 = &a[(i * 2u) * 8u];
					row0[0] = x;  row0[1] = y;  row0[2] = 1.0;
					row0[3] = 0.0; row0[4] = 0.0; row0[5] = 0.0;
					row0[6] = -u * x; row0[7] = -u * y;
					b[i * 2u] = u;
					float64 *row1 = &a[(i * 2u + 1u) * 8u];
					row1[0] = 0.0; row1[1] = 0.0; row1[2] = 0.0;
					row1[3] = x;  row1[4] = y;  row1[5] = 1.0;
					row1[6] = -v * x; row1[7] = -v * y;
					b[i * 2u + 1u] = v;
				}
				if (!SolveLinear(a, b, 8u)) {
					return false;
				}
				for (uint32 i = 0; i < 8u; ++i) {
					outH[i] = b[i];
				}
				outH[8] = 1.0;
				return true;
			}

			NkVec2f ApplyHomography(const float64 *h, float32 x, float32 y) {
				const float64 w = h[6] * float64(x) + h[7] * float64(y) + h[8];
				const float64 inv = (w != 0.0) ? (1.0 / w) : 0.0;
				NkVec2f out;
				out.x = float32((h[0] * float64(x) + h[1] * float64(y) + h[2]) * inv);
				out.y = float32((h[3] * float64(x) + h[4] * float64(y) + h[5]) * inv);
				return out;
			}

			// Aire signée d'un polygone : son SIGNE donne le sens de parcours.
			float32 SignedArea(const NkVec2f *points, uint32 count) {
				float32 area = 0.f;
				for (uint32 i = 0; i < count; ++i) {
					const NkVec2f &a = points[i];
					const NkVec2f &b = points[(i + 1u) % count];
					area += a.x * b.y - b.x * a.y;
				}
				return area * 0.5f;
			}

			float32 PointSegmentDistance(const NkVec2f &p, const NkVec2f &a, const NkVec2f &b) {
				const float32 dx = b.x - a.x;
				const float32 dy = b.y - a.y;
				const float32 lenSq = dx * dx + dy * dy;
				if (lenSq < 1e-6f) {
					const float32 ex = p.x - a.x;
					const float32 ey = p.y - a.y;
					return math::NkSqrt(ex * ex + ey * ey);
				}
				float32 t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq;
				t = math::NkClamp(t, 0.f, 1.f);
				const float32 px = a.x + t * dx - p.x;
				const float32 py = a.y + t * dy - p.y;
				return math::NkSqrt(px * px + py * py);
			}

		} // namespace

			// ── Les quatre coins de la grille sont RÉSERVÉS ─────────────────
			// (0,0) est BLANC, les trois autres NOIRS : une seule rotation
			// satisfait cette signature, donc l'orientation du marqueur est
			// DÉTERMINÉE — et avec elle l'identifiant ET la pose.
			// Sans cette marque, la lecture retenait « la première rotation qui
			// donne un code non nul » : l'identifiant dépendait alors de
			// l'ordre des coins trouvés, donc du seuillage. Ça marchait par
			// chance jusqu'à ce qu'on change de seuillage.
			inline bool NkArCellIsOrientation(uint32 gx, uint32 gy, uint32 gridBits) {
				(void)gridBits;
				return gx == 0u && gy == 0u;
			}

			inline bool NkArCellIsReserved(uint32 gx, uint32 gy, uint32 gridBits) {
				const uint32 last = gridBits - 1u;
				return (gx == 0u || gx == last) && (gy == 0u || gy == last);
			}

			// Index du bit d'identifiant porté par une cellule utile : les
			// cellules réservées sont sautées, la numérotation reste dense.
			inline uint32 NkArBitIndexOf(uint32 gx, uint32 gy, uint32 gridBits) {
				uint32 index = 0;
				for (uint32 y = 0; y < gridBits; ++y) {
					for (uint32 x = 0; x < gridBits; ++x) {
						if (NkArCellIsReserved(x, y, gridBits)) {
							continue;
						}
						if (x == gx && y == gy) {
							return index;
						}
						++index;
					}
				}
				return 0xFFFFFFFFu;
			}

			inline bool NkArIdBitAt(uint32 id, uint32 gx, uint32 gy, uint32 gridBits) {
				const uint32 bit = NkArBitIndexOf(gx, gy, gridBits);
				return (bit < 32u) && (((id >> bit) & 1u) != 0u);
			}

		// ── Fabrication d'un marqueur ────────────────────────────────────────

		bool NkArRenderMarker(int32 id, uint32 gridBits, uint8 *outGray, uint32 size) {
			if (outGray == nullptr || gridBits == 0u || size == 0u) {
				return false;
			}
			// MARGE BLANCHE (« zone de silence ») d'une cellule tout autour,
			// puis la bordure noire, puis les cellules utiles.
			// Elle n'est pas décorative : sans elle, un marqueur affiché sur un
			// fond SOMBRE (visionneuse en thème noir, écran éteint autour) voit
			// sa bordure noire se fondre dans le fond — il n'y a plus de
			// contour fermé à suivre, et rien n'est détecté. Constaté sur
			// l'écran de Rihen, image de diagnostic à l'appui. Tous les
			// systèmes de marqueurs (ArUco, AprilTag, QR) l'imposent.
			const uint32 cells = gridBits + 4u; // marge + bordure de chaque côté
			for (uint32 y = 0; y < size; ++y) {
				for (uint32 x = 0; x < size; ++x) {
					const uint32 cellX = (x * cells) / size;
					const uint32 cellY = (y * cells) / size;
					// Marge extérieure : BLANCHE.
					if (cellX == 0u || cellY == 0u || cellX == cells - 1u || cellY == cells - 1u) {
						outGray[y * size + x] = 255;
						continue;
					}
					uint8 value = 0; // bordure : noire
					if (cellX >= 2u && cellX <= gridBits + 1u && cellY >= 2u && cellY <= gridBits + 1u) {
						const uint32 gx = cellX - 2u;
						const uint32 gy = cellY - 2u;
						value = NkArCellIsOrientation(gx, gy, gridBits)
									? 255
									: (NkArCellIsReserved(gx, gy, gridBits)
										   ? 0
										   : (NkArIdBitAt(uint32(id), gx, gy, gridBits) ? 255 : 0));
					}
					outGray[y * size + x] = value;
				}
			}
			return true;
		}

		// ── Détection ────────────────────────────────────────────────────────

		uint32 NkArDetectMarkers(const uint8 *gray, uint32 width, uint32 height,
								 const NkArDetectorConfig &config, NkVector<NkArDetection> &outDetections,
								 uint8 *outMask) {
			outDetections.Clear();
			if (gray == nullptr || width < 8u || height < 8u) {
				return 0;
			}
			const uint32 pixelCount = width * height;
			const uint8 threshold = config.useOtsu ? OtsuThreshold(gray, pixelCount) : config.fixedThreshold;
			// Dire OÙ la chaîne abandonne : un détecteur muet qui rend zéro est
			// indébogable, chaque rejet a une raison. Champ de configuration
			// et non variable d'environnement (principe moteur n°4).
			const bool debug = config.debugCounters;
			uint32 dbgContours = 0, dbgTooShort = 0, dbgNotQuad = 0, dbgTooSmall = 0, dbgBorder = 0, dbgNoCode = 0;
			if (debug) {
				logger.Infof("[NkAr] seuil = %u\n", uint32(threshold));
			}

			// Masque binaire : 1 = SOMBRE (le marqueur a une bordure noire, ce
			// sont donc les zones sombres que l'on suit).
			auto &allocator = memory::NkGetDefaultAllocator();
			uint8 *mask = static_cast<uint8 *>(allocator.Allocate(pixelCount, 1));
			uint8 *visited = static_cast<uint8 *>(allocator.Allocate(pixelCount, 1));
			if (mask == nullptr || visited == nullptr) {
				if (mask != nullptr) {
					allocator.Deallocate(mask);
				}
				if (visited != nullptr) {
					allocator.Deallocate(visited);
				}
				return 0;
			}
			if (config.adaptive) {
				const uint32 window = (config.adaptiveWindow | 1u); // impair obligatoire
				AdaptiveThreshold(gray, width, height, window, config.adaptiveBias, mask, allocator);
			}
			else {
				for (uint32 i = 0; i < pixelCount; ++i) {
					mask[i] = (gray[i] < threshold) ? 1u : 0u;
				}
			}
			for (uint32 i = 0; i < pixelCount; ++i) {
				visited[i] = 0u;
			}
			if (outMask != nullptr) {
				for (uint32 i = 0; i < pixelCount; ++i) {
					outMask[i] = mask[i] ? 255u : 0u;
				}
			}

			NkVector<NkVec2f> contour;
			NkVector<NkVec2f> polygon;

			// Balayage : on part d'un pixel sombre dont le voisin de gauche est
			// clair — c'est un bord gauche de composante, donc un départ sûr.
			for (uint32 y = 1; y + 1u < height; ++y) {
				for (uint32 x = 1; x + 1u < width; ++x) {
					const uint32 index = y * width + x;
					if (mask[index] == 0u || visited[index] != 0u || mask[index - 1u] != 0u) {
						continue;
					}

					// ── Suivi de contour de Moore ────────────────────────────
					contour.Clear();
					int32 startX = int32(x);
					int32 startY = int32(y);
					int32 curX = startX;
					int32 curY = startY;
					// Voisinage 8, sens horaire, en partant de l'ouest.
					const int32 dx8[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
					const int32 dy8[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };
					uint32 dir = 0;
					uint32 steps = 0;
					const uint32 maxSteps = 4u * (width + height);
					bool closed = false;
					do {
						visited[uint32(curY) * width + uint32(curX)] = 1u;
						contour.PushBack(NkVec2f(float32(curX), float32(curY)));
						bool moved = false;
						for (uint32 k = 0; k < 8u; ++k) {
							const uint32 probe = (dir + k) % 8u;
							const int32 nx = curX + dx8[probe];
							const int32 ny = curY + dy8[probe];
							if (nx < 0 || ny < 0 || uint32(nx) >= width || uint32(ny) >= height) {
								continue;
							}
							if (mask[uint32(ny) * width + uint32(nx)] == 0u) {
								continue;
							}
							curX = nx;
							curY = ny;
							// Repartir « en arrière » du sens d'arrivée : la
							// règle de Moore qui garantit de longer le bord.
							dir = (probe + 6u) % 8u;
							moved = true;
							break;
						}
						if (!moved) {
							break; // pixel isolé
						}
						++steps;
						if (curX == startX && curY == startY) {
							closed = true;
							break;
						}
					} while (steps < maxSteps);

					if (!closed || contour.Size() < 16u) {
						++dbgTooShort;
						continue;
					}
					++dbgContours;

					// ── Réduction en quadrilatère ────────────────────────────
					// Le point le plus éloigné du premier, puis le plus éloigné
					// de ces deux-là, etc. : quatre coins extraits sans seuil
					// arbitraire (variante robuste de Douglas-Peucker fermé).
					polygon.Clear();
					const uint32 n = uint32(contour.Size());
					uint32 idx0 = 0;
					float32 bestDist = -1.f;
					for (uint32 i = 1; i < n; ++i) {
						const float32 ddx = contour[i].x - contour[0].x;
						const float32 ddy = contour[i].y - contour[0].y;
						const float32 d = ddx * ddx + ddy * ddy;
						if (d > bestDist) {
							bestDist = d;
							idx0 = i;
						}
					}
					uint32 idx1 = 0;
					bestDist = -1.f;
					for (uint32 i = 0; i < n; ++i) {
						const float32 ddx = contour[i].x - contour[idx0].x;
						const float32 ddy = contour[i].y - contour[idx0].y;
						const float32 d = ddx * ddx + ddy * ddy;
						if (d > bestDist) {
							bestDist = d;
							idx1 = i;
						}
					}
					// Deux coins opposés connus : les deux autres sont les plus
					// éloignés de la droite qui les joint, de part et d'autre.
					uint32 idx2 = 0;
					uint32 idx3 = 0;
					float32 bestA = -1.f;
					float32 bestB = -1.f;
					const NkVec2f &pa = contour[idx0];
					const NkVec2f &pb = contour[idx1];
					for (uint32 i = 0; i < n; ++i) {
						const float32 cross = (pb.x - pa.x) * (contour[i].y - pa.y) -
											  (pb.y - pa.y) * (contour[i].x - pa.x);
						const float32 dist = PointSegmentDistance(contour[i], pa, pb);
						if (cross > 0.f && dist > bestA) {
							bestA = dist;
							idx2 = i;
						}
						else if (cross < 0.f && dist > bestB) {
							bestB = dist;
							idx3 = i;
						}
					}
					if (bestA < 3.f || bestB < 3.f) {
						++dbgNotQuad;
						continue; // pas un quadrilatère : trop plat
					}
					// Ordonner les quatre coins par indice le long du contour :
					// ils sont alors dans l'ordre de parcours, donc cohérents.
					uint32 corners[4] = { idx0, idx1, idx2, idx3 };
					for (uint32 i = 0; i < 4u; ++i) {
						for (uint32 j = i + 1u; j < 4u; ++j) {
							if (corners[j] < corners[i]) {
								const uint32 tmp = corners[i];
								corners[i] = corners[j];
								corners[j] = tmp;
							}
						}
					}
					NkVec2f quad[4];
					for (uint32 i = 0; i < 4u; ++i) {
						quad[i] = contour[corners[i]];
					}
					// Sens horaire imposé (repère de lecture stable).
					if (SignedArea(quad, 4u) < 0.f) {
						const NkVec2f tmp = quad[1];
						quad[1] = quad[3];
						quad[3] = tmp;
					}

					float32 perimeter = 0.f;
					for (uint32 i = 0; i < 4u; ++i) {
						const NkVec2f &a = quad[i];
						const NkVec2f &b = quad[(i + 1u) % 4u];
						perimeter += math::NkSqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
					}
					const float32 edge = perimeter * 0.25f;
					if (edge < float32(config.minEdgePixels)) {
						++dbgTooSmall;
						continue;
					}

					// ── Redressement + lecture du code ───────────────────────
					const uint32 cells = config.gridBits + 2u;
					const NkVec2f unit[4] = { NkVec2f(0.f, 0.f), NkVec2f(float32(cells), 0.f),
											  NkVec2f(float32(cells), float32(cells)),
											  NkVec2f(0.f, float32(cells)) };
					float64 h[9] = {};
					if (!ComputeHomography(unit, quad, h)) {
						continue;
					}
					// La bordure DOIT être sombre : c'est ce qui distingue un
					// marqueur d'un rectangle quelconque de la scène.
					bool borderOk = true;
					for (uint32 c = 0; c < cells && borderOk; ++c) {
						const uint32 checks[4][2] = { { c, 0u }, { c, cells - 1u }, { 0u, c }, { cells - 1u, c } };
						for (uint32 k = 0; k < 4u; ++k) {
							const NkVec2f p = ApplyHomography(h, float32(checks[k][0]) + 0.5f,
															  float32(checks[k][1]) + 0.5f);
							const int32 px = int32(p.x);
							const int32 py = int32(p.y);
							// Lire le MASQUE (donc le seuil local) et non le seuil
							// global : sinon un marqueur bien seuillé localement
							// serait rejeté par une luminosité d'ensemble.
							if (px < 0 || py < 0 || uint32(px) >= width || uint32(py) >= height ||
								mask[uint32(py) * width + uint32(px)] == 0u) {
								borderOk = false;
								break;
							}
						}
					}
					if (!borderOk) {
						++dbgBorder;
						continue;
					}

					// Grille de bits (1 = clair). Échantillon central de chaque
					// cellule : le plus loin des bords, donc le plus sûr.
					uint32 bits[64] = {};
					const uint32 gridBits = config.gridBits;
					bool readOk = (gridBits * gridBits) <= 64u;
					for (uint32 gy = 0; gy < gridBits && readOk; ++gy) {
						for (uint32 gx = 0; gx < gridBits; ++gx) {
							const NkVec2f p = ApplyHomography(h, float32(gx + 1u) + 0.5f, float32(gy + 1u) + 0.5f);
							const int32 px = int32(p.x);
							const int32 py = int32(p.y);
							if (px < 0 || py < 0 || uint32(px) >= width || uint32(py) >= height) {
								readOk = false;
								break;
							}
							bits[gy * gridBits + gx] = (mask[uint32(py) * width + uint32(px)] == 0u) ? 1u : 0u;
						}
					}
					if (!readOk) {
						continue;
					}

					// Quatre rotations : celle qui donne un code valide fixe
					// aussi l'orientation du marqueur.
					NkArDetection detection;
					detection.edgeLength = edge;
					for (uint32 rot = 0; rot < 4u; ++rot) {
						// Vérifier D'ABORD la signature des coins : une seule
						// rotation la satisfait, ce qui lève toute ambiguïté
						// sur l'orientation — donc sur l'identifiant et la pose.
						bool signatureOk = true;
						for (uint32 gy = 0; gy < gridBits && signatureOk; ++gy) {
							for (uint32 gx = 0; gx < gridBits; ++gx) {
								if (!NkArCellIsReserved(gx, gy, gridBits)) {
									continue;
								}
								uint32 sx = gx;
								uint32 sy = gy;
								for (uint32 r = 0; r < rot; ++r) {
									const uint32 tx = sx;
									sx = gridBits - 1u - sy;
									sy = tx;
								}
								const bool light = (bits[sy * gridBits + sx] != 0u);
								if (light != NkArCellIsOrientation(gx, gy, gridBits)) {
									signatureOk = false;
									break;
								}
							}
						}
						if (!signatureOk) {
							continue;
						}
						uint32 value = 0;
						for (uint32 gy = 0; gy < gridBits; ++gy) {
							for (uint32 gx = 0; gx < gridBits; ++gx) {
								if (NkArCellIsReserved(gx, gy, gridBits)) {
									continue;
								}
								uint32 sx = gx;
								uint32 sy = gy;
								for (uint32 r = 0; r < rot; ++r) {
									const uint32 tx = sx;
									sx = gridBits - 1u - sy;
									sy = tx;
								}
								const uint32 bitIndex = NkArBitIndexOf(gx, gy, gridBits);
								if (bitIndex < 32u && bits[sy * gridBits + sx] != 0u) {
									value |= (1u << bitIndex);
								}
							}
						}
						detection.id = int32(value);
						for (uint32 i = 0; i < 4u; ++i) {
							detection.corners[i] = quad[(i + rot) % 4u];
						}
						break;
					}
					if (detection.id >= 0) {
						outDetections.PushBack(detection);
					}
					else {
						++dbgNoCode;
					}
				}
			}

			if (debug) {
				logger.Infof("[NkAr] contours=%u trop-courts=%u non-quad=%u trop-petits=%u bordure-KO=%u sans-code=%u -> %u\n",
							 dbgContours, dbgTooShort, dbgNotQuad, dbgTooSmall, dbgBorder, dbgNoCode,
							 uint32(outDetections.Size()));
			}
			allocator.Deallocate(mask);
			allocator.Deallocate(visited);
			return uint32(outDetections.Size());
		}

		// ── Pose plane depuis l'homographie ──────────────────────────────────

		bool NkArPoseFromDetection(const NkArDetection &detection, float32 sizeMeters,
								   const NkArCameraIntrinsics &intrinsics, NkXrPose &outPose) {
			if (sizeMeters <= 0.f || intrinsics.fx <= 0.f || intrinsics.fy <= 0.f) {
				return false;
			}
			// Le marqueur : carré centré, dans le plan z = 0, en mètres.
			const float32 half = sizeMeters * 0.5f;
			const NkVec2f object[4] = { NkVec2f(-half, half), NkVec2f(half, half), NkVec2f(half, -half),
										NkVec2f(-half, -half) };
			float64 h[9] = {};
			if (!ComputeHomography(object, detection.corners, h)) {
				return false;
			}
			// K^-1 * H : on retire l'effet de la caméra pour ne garder que la
			// pose. K^-1 = [[1/fx, 0, -cx/fx], [0, 1/fy, -cy/fy], [0, 0, 1]].
			const float64 invFx = 1.0 / float64(intrinsics.fx);
			const float64 invFy = 1.0 / float64(intrinsics.fy);
			float64 m[9];
			for (uint32 col = 0; col < 3u; ++col) {
				m[0 * 3u + col] = (h[0 * 3u + col] - float64(intrinsics.cx) * h[2 * 3u + col]) * invFx;
				m[1 * 3u + col] = (h[1 * 3u + col] - float64(intrinsics.cy) * h[2 * 3u + col]) * invFy;
				m[2 * 3u + col] = h[2 * 3u + col];
			}
			// Les deux premières colonnes sont deux axes de rotation à un
			// facteur d'échelle près — commun aux deux, donc moyenné.
			const float64 n1 = math::NkSqrt(m[0] * m[0] + m[3] * m[3] + m[6] * m[6]);
			const float64 n2 = math::NkSqrt(m[1] * m[1] + m[4] * m[4] + m[7] * m[7]);
			if (n1 < 1e-9 || n2 < 1e-9) {
				return false;
			}
			const float64 scale = 2.0 / (n1 + n2);
			NkVec3f r1(float32(m[0] * scale), float32(m[3] * scale), float32(m[6] * scale));
			NkVec3f r2(float32(m[1] * scale), float32(m[4] * scale), float32(m[7] * scale));
			NkVec3f t(float32(m[2] * scale), float32(m[5] * scale), float32(m[8] * scale));
			// Ici on est encore dans le repère PINHOLE STANDARD (Y vers le
			// BAS, +Z devant) : c'est celui qu'impose la formule des pixels.
			// Le marqueur est devant, donc t.z doit y être POSITIF — sinon
			// c'est la solution miroir que l'homographie admet aussi.
			if (t.z < 0.f) {
				r1 = r1 * -1.f;
				r2 = r2 * -1.f;
				t = t * -1.f;
			}
			// Réorthonormalisation : le bruit rend r1 et r2 non exactement
			// perpendiculaires ; sans cette étape le quaternion serait faux.
			NkVec3f x = r1.Normalized();
			NkVec3f y = (r2 - x * x.Dot(r2)).Normalized();
			NkVec3f z = x.Cross(y);

			// ── Passage au repère du moteur ─────────────────────────────────
			// NKXR/OpenXR : Y vers le HAUT, avant = -Z. Le changement de base
			// est diag(1,-1,-1), appliqué À LA FOIS à la translation et aux
			// axes de la rotation. Sans lui, un marqueur vu de face rendait
			// une orientation à 180° et un décalage X de signe faux — les
			// distances, elles, restaient justes, ce qui rendait le défaut
			// discret : c'est exactement ce que le test a attrapé.
			x = NkVec3f(x.x, -x.y, -x.z);
			y = NkVec3f(y.x, -y.y, -y.z);
			z = NkVec3f(z.x, -z.y, -z.z);
			t = NkVec3f(t.x, -t.y, -t.z);

			NkMat4f rotation = NkMat4f::Identity();
			rotation.mat[0][0] = x.x;  rotation.mat[0][1] = x.y;  rotation.mat[0][2] = x.z;
			rotation.mat[1][0] = y.x;  rotation.mat[1][1] = y.y;  rotation.mat[1][2] = y.z;
			rotation.mat[2][0] = z.x;  rotation.mat[2][1] = z.y;  rotation.mat[2][2] = z.z;

			outPose.position = t;
			outPose.orientation = NkQuatf(rotation).Normalized();
			return true;
		}

	} // namespace xr
} // namespace nkentseu
