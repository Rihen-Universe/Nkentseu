// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#include "NKCanvas/Renderer/Resources/NkSpriteSheet.h"
#include "NKImage/Core/NkImagePalette.h"

namespace nkentseu {
	namespace renderer {

		namespace {

			// Copie bornee d'une chaine C (sans bibliotheque standard).
			void CopierNom(char *dst, int32 cap, const char *src) {
				int32 i = 0;
				if (src != nullptr) {
					for (; src[i] != 0 && i < cap - 1; ++i) {
						dst[i] = src[i];
					}
				}
				dst[i] = 0;
			}

			bool Egaux(const char *a, const char *b) {
				while (*a != 0 && *a == *b) {
					++a;
					++b;
				}
				return *a == *b;
			}

			// Ecrit « case_<n> » dans dst.
			void NomParDefaut(char *dst, int32 cap, int32 n) {
				const char pre[] = "case_";
				int32 i = 0;
				for (; pre[i] != 0 && i < cap - 1; ++i) {
					dst[i] = pre[i];
				}
				char chiffres[12];
				int32 nc = 0;
				if (n == 0) {
					chiffres[nc++] = '0';
				}
				while (n > 0 && nc < 11) {
					chiffres[nc++] = static_cast<char>('0' + n % 10);
					n /= 10;
				}
				while (nc > 0 && i < cap - 1) {
					dst[i++] = chiffres[--nc];
				}
				dst[i] = 0;
			}

		} // namespace

		void NkSpriteSheet::NommerParDefaut() {
			for (usize i = 0; i < mCases.Size(); ++i) {
				NomParDefaut(mCases[i].nom, 48, static_cast<int32>(i));
			}
		}

		int32 NkSpriteSheet::BuildGrid(int32 largeurImage, int32 hauteurImage, const NkSheetGrid &g) {
			mCases.Clear();
			if (g.colonnes < 1 || g.lignes < 1 || largeurImage <= 0 || hauteurImage <= 0) {
				return 0;
			}
			const int32 l = (largeurImage - g.margeGauche - g.margeDroite - (g.colonnes - 1) * g.ecartX) / g.colonnes;
			const int32 h = (hauteurImage - g.margeHaut - g.margeBas - (g.lignes - 1) * g.ecartY) / g.lignes;
			if (l <= 0 || h <= 0) {
				return 0;
			}
			for (int32 r = 0; r < g.lignes; ++r) {
				for (int32 c = 0; c < g.colonnes; ++c) {
					const int32 x = g.margeGauche + c * (l + g.ecartX);
					const int32 y = g.margeHaut + r * (h + g.ecartY);
					NkSheetCell cell;
					cell.rect = math::NkRect2i(x + static_cast<int32>(g.utileX * static_cast<float32>(l)),
											   y + static_cast<int32>(g.utileY * static_cast<float32>(h)),
											   static_cast<int32>(g.utileL * static_cast<float32>(l)),
											   static_cast<int32>(g.utileH * static_cast<float32>(h)));
					mCases.PushBack(cell);
				}
			}
			NommerParDefaut();
			return static_cast<int32>(mCases.Size());
		}

		int32 NkSpriteSheet::BuildAuto(const NkImage &image, const math::NkColor &fond, int32 tolerance, int32 ecartMin,
									   int32 tailleMin) {
			mCases.Clear();
			if (!image.IsValid()) {
				return 0;
			}
			NkVector<NkImageSpan> bandes;
			NkImagePalette::Spans(image, fond, tolerance, NkImageAxis::NK_ROWS, bandes, ecartMin, tailleMin);
			for (usize b = 0; b < bandes.Size(); ++b) {
				const math::NkIntRect zoneBande(0, bandes[b].debut, image.Width(), bandes[b].fin - bandes[b].debut);
				NkVector<NkImageSpan> colonnes;
				NkImagePalette::Spans(image, fond, tolerance, NkImageAxis::NK_COLUMNS, colonnes, ecartMin, tailleMin,
									  &zoneBande);
				for (usize c = 0; c < colonnes.Size(); ++c) {
					// On resserre la hauteur sur ce qui est reellement occupe dans la colonne.
					const math::NkIntRect zoneCase(colonnes[c].debut, bandes[b].debut, colonnes[c].fin - colonnes[c].debut,
												   bandes[b].fin - bandes[b].debut);
					NkVector<NkImageSpan> hauteurs;
					NkImagePalette::Spans(image, fond, tolerance, NkImageAxis::NK_ROWS, hauteurs, 1000000, 1, &zoneCase);
					int32 y0 = bandes[b].debut, y1 = bandes[b].fin;
					if (hauteurs.Size() > 0) {
						y0 = hauteurs[0].debut;
						y1 = hauteurs[hauteurs.Size() - 1].fin;
					}
					NkSheetCell cell;
					cell.rect = math::NkRect2i(colonnes[c].debut, y0, colonnes[c].fin - colonnes[c].debut, y1 - y0);
					mCases.PushBack(cell);
				}
			}
			NommerParDefaut();
			return static_cast<int32>(mCases.Size());
		}

		void NkSpriteSheet::Tighten(const NkImage &image, const math::NkColor &fond, int32 tolerance) {
			for (usize i = 0; i < mCases.Size(); ++i) {
				const math::NkRect2i r = mCases[i].rect;
				NkVector<NkImageSpan> lig, col;
				NkImagePalette::Spans(image, fond, tolerance, NkImageAxis::NK_ROWS, lig, 1000000, 1, &r);
				NkImagePalette::Spans(image, fond, tolerance, NkImageAxis::NK_COLUMNS, col, 1000000, 1, &r);
				if (lig.Size() == 0 || col.Size() == 0) {
					continue;
				}
				mCases[i].rect = math::NkRect2i(col[0].debut, lig[0].debut, col[0].fin - col[0].debut,
												lig[0].fin - lig[0].debut);
			}
		}

		void NkSpriteSheet::SetNames(const char *const *noms, int32 n) {
			for (int32 i = 0; i < n && i < Count(); ++i) {
				CopierNom(mCases[static_cast<usize>(i)].nom, 48, noms[i]);
			}
		}

		void NkSpriteSheet::Rename(int32 indice, const char *nom) {
			if (indice >= 0 && indice < Count()) {
				CopierNom(mCases[static_cast<usize>(indice)].nom, 48, nom);
			}
		}

		const NkSheetCell *NkSpriteSheet::Get(int32 indice) const noexcept {
			return (indice >= 0 && indice < Count()) ? &mCases[static_cast<usize>(indice)] : nullptr;
		}

		const math::NkRect2i *NkSpriteSheet::Find(const char *nom) const noexcept {
			if (nom == nullptr) {
				return nullptr;
			}
			for (usize i = 0; i < mCases.Size(); ++i) {
				if (Egaux(mCases[i].nom, nom)) {
					return &mCases[i].rect;
				}
			}
			return nullptr;
		}

	} // namespace renderer
} // namespace nkentseu
