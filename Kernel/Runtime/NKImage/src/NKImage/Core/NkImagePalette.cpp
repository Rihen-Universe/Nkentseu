// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#include "NKImage/Core/NkImagePalette.h"
#include "NKContainers/Utilities/NkSort.h"
#include "NKMath/NkRandom.h"

namespace nkentseu {

	namespace {

		struct Rgba {
				int32 r, g, b, a;
		};

		// Lit le pixel (x, y) d'une image 8 bits en RGBA (alpha 255 s'il n'y en a pas).
		inline Rgba Lire(const NkImage &im, int32 x, int32 y) {
			const int32 ch = im.Channels();
			const uint8 *p = im.RowPtr(y) + x * ch;
			Rgba v;
			if (ch >= 3) {
				v.r = p[0];
				v.g = p[1];
				v.b = p[2];
				v.a = (ch == 4) ? p[3] : 255;
			} else {
				v.r = v.g = v.b = p[0];
				v.a = (ch == 2) ? p[1] : 255;
			}
			return v;
		}

		inline int32 Abs(int32 v) {
			return v < 0 ? -v : v;
		}

		inline bool Proche(const Rgba &p, const math::NkColor &f, int32 tol) {
			return Abs(p.r - f.r) <= tol && Abs(p.g - f.g) <= tol && Abs(p.b - f.b) <= tol;
		}

		inline bool Valide(const NkImage &im) {
			return im.IsValid() && !im.IsHDR() && im.Channels() >= 1;
		}

		inline int32 Dist2(const Rgba &p, const Rgba &c) {
			const int32 dr = p.r - c.r, dg = p.g - c.g, db = p.b - c.b;
			return dr * dr + dg * dg + db * db;
		}

	} // namespace

	// -------------------------------------------------------------------------
	bool NkImagePalette::Quantize(const NkImage &source, int32 k, NkImage &sortie, NkVector<math::NkColor> *palette,
								  uint32 graine, int32 iterations) {
		if (!Valide(source) || k < 1 || k > 64) {
			return false;
		}
		const int32 w = source.Width();
		const int32 h = source.Height();

		// Échantillon : pas régulier pour viser au plus ~65 536 pixels.
		int32 pas = 1;
		while ((w / pas) * (h / pas) > 65536) {
			++pas;
		}
		NkVector<Rgba> ech;
		for (int32 y = 0; y < h; y += pas) {
			for (int32 x = 0; x < w; x += pas) {
				const Rgba p = Lire(source, x, y);
				if (p.a >= 128) {
					ech.PushBack(p);
				}
			}
		}
		if (ech.Size() == 0) {
			return false;
		}

		// Initialisation « maximin » : premier centre au hasard (rejouable), puis le
		// pixel dont la distance au centre le plus proche est la plus grande.
		math::NkLcg32 hasard(graine);
		Rgba centre[64];
		int32 nc = 1;
		centre[0] = ech[hasard.NextUInt32(static_cast<uint32>(ech.Size()))];
		NkVector<int32> dmin;
		dmin.Resize(ech.Size());
		for (usize i = 0; i < ech.Size(); ++i) {
			dmin[i] = Dist2(ech[i], centre[0]);
		}
		while (nc < k) {
			usize best = 0;
			for (usize i = 1; i < ech.Size(); ++i) {
				if (dmin[i] > dmin[best]) {
					best = i;
				}
			}
			if (dmin[best] == 0) {
				break; // moins de k couleurs distinctes : on s'arrête
			}
			centre[nc] = ech[best];
			for (usize i = 0; i < ech.Size(); ++i) {
				const int32 d = Dist2(ech[i], centre[nc]);
				if (d < dmin[i]) {
					dmin[i] = d;
				}
			}
			++nc;
		}

		// Itérations de Lloyd : affectation puis moyenne.
		NkVector<int32> aff;
		aff.Resize(ech.Size());
		for (usize i = 0; i < aff.Size(); ++i) {
			aff[i] = -1;
		}
		for (int32 it = 0; it < iterations; ++it) {
			bool change = false;
			float64 s[64][3];
			int32 n[64];
			for (int32 j = 0; j < nc; ++j) {
				s[j][0] = s[j][1] = s[j][2] = 0.0;
				n[j] = 0;
			}
			for (usize i = 0; i < ech.Size(); ++i) {
				int32 bj = 0, bd = 0x7fffffff;
				for (int32 j = 0; j < nc; ++j) {
					const int32 d = Dist2(ech[i], centre[j]);
					if (d < bd) {
						bd = d;
						bj = j;
					}
				}
				if (aff[i] != bj) {
					aff[i] = bj;
					change = true;
				}
				s[bj][0] += ech[i].r;
				s[bj][1] += ech[i].g;
				s[bj][2] += ech[i].b;
				++n[bj];
			}
			for (int32 j = 0; j < nc; ++j) {
				if (n[j] > 0) { // un centre vide garde sa place
					centre[j].r = static_cast<int32>(s[j][0] / n[j] + 0.5);
					centre[j].g = static_cast<int32>(s[j][1] / n[j] + 0.5);
					centre[j].b = static_cast<int32>(s[j][2] / n[j] + 0.5);
				}
			}
			if (!change) {
				break;
			}
		}

		// Sortie : chaque pixel opaque prend la couleur de son centre le plus proche.
		sortie = source.Copy();
		if (!sortie.IsValid()) {
			return false;
		}
		const int32 ch = sortie.Channels();
		for (int32 y = 0; y < h; ++y) {
			uint8 *row = sortie.RowPtr(y);
			for (int32 x = 0; x < w; ++x) {
				const Rgba p = Lire(source, x, y);
				if (p.a < 128) {
					continue;
				}
				int32 bj = 0, bd = 0x7fffffff;
				for (int32 j = 0; j < nc; ++j) {
					const int32 d = Dist2(p, centre[j]);
					if (d < bd) {
						bd = d;
						bj = j;
					}
				}
				uint8 *q = row + x * ch;
				if (ch >= 3) {
					q[0] = static_cast<uint8>(centre[bj].r);
					q[1] = static_cast<uint8>(centre[bj].g);
					q[2] = static_cast<uint8>(centre[bj].b);
				} else {
					q[0] = static_cast<uint8>(centre[bj].r);
				}
			}
		}
		if (palette) {
			palette->Clear();
			for (int32 j = 0; j < nc; ++j) {
				palette->PushBack(math::NkColor(static_cast<uint8>(centre[j].r), static_cast<uint8>(centre[j].g),
												static_cast<uint8>(centre[j].b), 255));
			}
		}
		return true;
	}

	// -------------------------------------------------------------------------
	math::NkColor NkImagePalette::EstimateBg(const NkImage &image) {
		if (!Valide(image)) {
			return math::NkColor(255, 255, 255, 255);
		}
		const int32 w = image.Width();
		const int32 h = image.Height();
		NkVector<int32> rr, gg, bb;
		auto ajoute = [&](int32 x, int32 y) {
			const Rgba p = Lire(image, x, y);
			rr.PushBack(p.r);
			gg.PushBack(p.g);
			bb.PushBack(p.b);
		};
		for (int32 x = 0; x < w; ++x) {
			ajoute(x, 0);
			ajoute(x, h - 1);
		}
		for (int32 y = 1; y + 1 < h; ++y) {
			ajoute(0, y);
			ajoute(w - 1, y);
		}
		// Médiane : insensible à quelques pixels de dessin qui touchent le bord,
		// là où la moyenne serait tirée vers eux.
		NkSort(rr.Data(), rr.Data() + rr.Size());
		NkSort(gg.Data(), gg.Data() + gg.Size());
		NkSort(bb.Data(), bb.Data() + bb.Size());
		const usize m = rr.Size() / 2;
		return math::NkColor(static_cast<uint8>(rr[m]), static_cast<uint8>(gg[m]), static_cast<uint8>(bb[m]), 255);
	}

	// -------------------------------------------------------------------------
	int32 NkImagePalette::RemoveBackground(NkImage &image, const math::NkColor &fond, int32 tolerance) {
		if (!Valide(image)) {
			return 0;
		}
		if (image.Format() != NkImagePixelFormat::NK_RGBA32) {
			NkImage conv = image.Convert(NkImagePixelFormat::NK_RGBA32);
			if (!conv.IsValid()) {
				return 0;
			}
			image = static_cast<NkImage &&>(conv);
		}
		const int32 w = image.Width();
		const int32 h = image.Height();
		NkVector<uint8> vu;
		vu.Resize(static_cast<usize>(w) * static_cast<usize>(h));
		for (usize i = 0; i < vu.Size(); ++i) {
			vu[i] = 0;
		}
		NkVector<int32> pile; // indices y * w + x
		auto graine = [&](int32 x, int32 y) {
			const usize i = static_cast<usize>(y) * static_cast<usize>(w) + static_cast<usize>(x);
			if (!vu[i] && Proche(Lire(image, x, y), fond, tolerance)) {
				vu[i] = 1;
				pile.PushBack(static_cast<int32>(i));
			}
		};
		for (int32 x = 0; x < w; ++x) {
			graine(x, 0);
			graine(x, h - 1);
		}
		for (int32 y = 0; y < h; ++y) {
			graine(0, y);
			graine(w - 1, y);
		}
		int32 efface = 0;
		while (pile.Size() > 0) {
			const int32 i = pile[pile.Size() - 1];
			pile.PopBack();
			const int32 x = i % w, y = i / w;
			image.RowPtr(y)[x * 4 + 3] = 0;
			++efface;
			if (x > 0) {
				graine(x - 1, y);
			}
			if (x + 1 < w) {
				graine(x + 1, y);
			}
			if (y > 0) {
				graine(x, y - 1);
			}
			if (y + 1 < h) {
				graine(x, y + 1);
			}
		}
		return efface;
	}

	// -------------------------------------------------------------------------
	void NkImagePalette::Spans(const NkImage &image, const math::NkColor &fond, int32 tolerance, NkImageAxis axe,
							   NkVector<NkImageSpan> &sortie, int32 ecartMin, int32 tailleMin,
							   const math::NkIntRect *zone) {
		sortie.Clear();
		if (!Valide(image)) {
			return;
		}
		int32 x0 = 0, y0 = 0, x1 = image.Width(), y1 = image.Height();
		if (zone) {
			x0 = zone->left > 0 ? zone->left : 0;
			y0 = zone->top > 0 ? zone->top : 0;
			x1 = (zone->left + zone->width < x1) ? zone->left + zone->width : x1;
			y1 = (zone->top + zone->height < y1) ? zone->top + zone->height : y1;
		}
		const bool lignes = (axe == NkImageAxis::NK_ROWS);
		const int32 a0 = lignes ? y0 : x0, a1 = lignes ? y1 : x1; // axe balayé
		const int32 b0 = lignes ? x0 : y0, b1 = lignes ? x1 : y1; // axe projeté

		// Pour chaque indice de l'axe : occupé ou non.
		int32 debut = -1, dernierOccupe = -1;
		auto ferme = [&](int32 finExclu) {
			if (debut >= 0 && finExclu - debut >= tailleMin) {
				NkImageSpan s;
				s.debut = debut;
				s.fin = finExclu;
				sortie.PushBack(s);
			}
			debut = -1;
		};
		for (int32 a = a0; a < a1; ++a) {
			bool occupe = false;
			for (int32 b = b0; b < b1 && !occupe; ++b) {
				const Rgba p = lignes ? Lire(image, b, a) : Lire(image, a, b);
				occupe = (p.a >= 128) && !Proche(p, fond, tolerance);
			}
			if (occupe) {
				if (debut < 0) {
					debut = a;
				}
				dernierOccupe = a;
			} else if (debut >= 0 && a - dernierOccupe >= ecartMin) {
				ferme(dernierOccupe + 1); // l'écart atteint ecartMin : la bande s'arrête
			}
		}
		if (debut >= 0) {
			ferme(dernierOccupe + 1);
		}
	}

} // namespace nkentseu
