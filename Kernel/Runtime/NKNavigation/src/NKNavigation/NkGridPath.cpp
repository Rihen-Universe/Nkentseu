// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#include "NKNavigation/NkGridPath.h"

namespace nkentseu {
	namespace nav {

		void NkGridPath::Init(int32 largeur, int32 hauteur) {
			mW = largeur > 0 ? largeur : 0;
			mH = hauteur > 0 ? hauteur : 0;
			mBloque.Resize(static_cast<usize>(mW) * static_cast<usize>(mH));
			for (usize i = 0; i < mBloque.Size(); ++i) {
				mBloque[i] = 0;
			}
		}

		void NkGridPath::SetBlocked(int32 x, int32 y, bool bloque) {
			if (x < 0 || y < 0 || x >= mW || y >= mH) {
				return;
			}
			mBloque[static_cast<usize>(y) * static_cast<usize>(mW) + static_cast<usize>(x)] = bloque ? 1 : 0;
		}

		bool NkGridPath::IsBlocked(int32 x, int32 y) const noexcept {
			if (x < 0 || y < 0 || x >= mW || y >= mH) {
				return true;
			}
			return mBloque[static_cast<usize>(y) * static_cast<usize>(mW) + static_cast<usize>(x)] != 0;
		}

		bool NkGridPath::LineOfSight(int32 ax, int32 ay, int32 bx, int32 by) const noexcept {
			// Bresenham : e = dx - dy est l'erreur entre la droite idéale et la case courante ;
			// 2e >= -dy autorise un pas en x, 2e <= dx un pas en y.
			const int32 dx = bx > ax ? bx - ax : ax - bx;
			const int32 dy = by > ay ? by - ay : ay - by;
			const int32 sx = ax < bx ? 1 : -1;
			const int32 sy = ay < by ? 1 : -1;
			int32 e = dx - dy;
			int32 x = ax, y = ay;
			for (;;) {
				if (IsBlocked(x, y)) {
					return false;
				}
				if (x == bx && y == by) {
					return true;
				}
				const int32 e2 = 2 * e;
				const bool pasX = e2 > -dy;
				const bool pasY = e2 < dx;
				if (pasX && pasY) {
					// Pas en diagonale : les deux cases de l'angle doivent être libres.
					if (IsBlocked(x + sx, y) || IsBlocked(x, y + sy)) {
						return false;
					}
				}
				if (pasX) {
					e -= dy;
					x += sx;
				}
				if (pasY) {
					e += dx;
					y += sy;
				}
			}
		}

		bool NkGridPath::FindPath(int32 dx, int32 dy, int32 bx, int32 by, NkVector<NkGridCell> &sortie, bool lisser) const {
			sortie.Clear();
			if (IsBlocked(dx, dy) || IsBlocked(bx, by)) {
				return false;
			}
			const usize n = static_cast<usize>(mW) * static_cast<usize>(mH);
			// parent[i] = case d'où l'on est arrivé (-1 : non visitée, -2 : le départ)
			NkVector<int32> parent;
			parent.Resize(n);
			for (usize i = 0; i < n; ++i) {
				parent[i] = -1;
			}
			// File FIFO : tableau + indice de tête (chaque case y entre au plus une fois).
			NkVector<int32> file;
			file.Reserve(n);
			const int32 depart = dy * mW + dx;
			const int32 but = by * mW + bx;
			parent[static_cast<usize>(depart)] = -2;
			file.PushBack(depart);
			static const int32 kDx[4] = {1, -1, 0, 0};
			static const int32 kDy[4] = {0, 0, 1, -1};
			usize tete = 0;
			bool trouve = (depart == but);
			while (tete < file.Size() && !trouve) {
				const int32 cur = file[tete++];
				const int32 cx = cur % mW, cy = cur / mW;
				for (int32 k = 0; k < 4; ++k) {
					const int32 nx = cx + kDx[k], ny = cy + kDy[k];
					if (IsBlocked(nx, ny)) {
						continue;
					}
					const int32 ni = ny * mW + nx;
					if (parent[static_cast<usize>(ni)] != -1) {
						continue;
					}
					parent[static_cast<usize>(ni)] = cur;
					if (ni == but) {
						trouve = true;
						break;
					}
					file.PushBack(ni);
				}
			}
			if (!trouve) {
				return false;
			}

			// Remontée du but vers le départ, puis inversion.
			NkVector<NkGridCell> brut;
			for (int32 i = but; i != -2; i = parent[static_cast<usize>(i)]) {
				NkGridCell c;
				c.x = i % mW;
				c.y = i / mW;
				brut.PushBack(c);
			}
			for (usize a = 0, b = brut.Size() - 1; a < b; ++a, --b) {
				const NkGridCell t = brut[a];
				brut[a] = brut[b];
				brut[b] = t;
			}
			if (!lisser || brut.Size() <= 2) {
				sortie = brut;
				return true;
			}

			// Lissage : on garde le point visible le plus lointain depuis l'ancrage.
			usize ancre = 0;
			sortie.PushBack(brut[0]);
			while (ancre + 1 < brut.Size()) {
				usize loin = ancre + 1; // le suivant est toujours atteignable (voisin)
				for (usize j = brut.Size() - 1; j > ancre + 1; --j) {
					if (LineOfSight(brut[ancre].x, brut[ancre].y, brut[j].x, brut[j].y)) {
						loin = j;
						break;
					}
				}
				sortie.PushBack(brut[loin]);
				ancre = loin;
			}
			return true;
		}

	} // namespace nav
} // namespace nkentseu
