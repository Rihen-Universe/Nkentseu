// =============================================================================
// NkUE5Ui.cpp — widgets et icones de l'editeur
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Physic2D/Editeur/NkUE5Ui.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace physic2d {

		using namespace ue5;

		// =====================================================================
		// Icones
		// =====================================================================
		void NkDessinerIcone(NkGuiDrawList &dl, NkIcone ic, const NkVec2 &c, float32 t, const NkColor &col) {
			auto P = [&](float32 x, float32 y) { return NkVec2{c.x + x * t, c.y + y * t}; };
			const float32 ep = t * 0.09f > 1.2f ? t * 0.09f : 1.2f; // epaisseur de trait
			switch (ic) {
				case NkIcone::NK_SELECTION: {
					const NkVec2 pts[7] = {P(-0.28f, -0.42f), P(-0.28f, 0.30f), P(-0.10f, 0.14f), P(0.04f, 0.44f),
										   P(0.14f, 0.39f), P(0.00f, 0.10f), P(0.24f, 0.08f)};
					dl.AddTriangleFilled(pts[0], pts[1], pts[6], col);
					dl.AddTriangleFilled(pts[2], pts[3], pts[5], col);
					dl.AddTriangleFilled(pts[3], pts[4], pts[5], col);
					break;
				}
				case NkIcone::NK_SAISIR: {
					dl.AddRectFilled({c.x - t * 0.26f, c.y - t * 0.02f, t * 0.52f, t * 0.42f}, col, t * 0.14f);
					for (int32 i = 0; i < 4; ++i) {
						const float32 x = -0.26f + 0.135f * static_cast<float32>(i);
						dl.AddRectFilled({c.x + x * t, c.y - t * (i == 1 || i == 2 ? 0.42f : 0.32f), t * 0.11f, t * 0.44f},
										 col, t * 0.05f);
					}
					dl.AddRectFilled({c.x - t * 0.42f, c.y + t * 0.0f, t * 0.12f, t * 0.26f}, col, t * 0.05f);
					break;
				}
				case NkIcone::NK_COUPER: {
					dl.AddLine(P(-0.36f, 0.36f), P(-0.12f, 0.12f), col, ep * 2.4f);
					const NkVec2 lame[4] = {P(-0.12f, 0.02f), P(0.42f, -0.42f), P(0.02f, 0.14f), P(-0.02f, 0.18f)};
					dl.AddTriangleFilled(lame[0], lame[1], lame[2], col);
					dl.AddTriangleFilled(lame[0], lame[2], lame[3], col);
					break;
				}
				case NkIcone::NK_EXPLOSION: {
					for (int32 i = 0; i < 8; ++i) {
						const float32 a = static_cast<float32>(i) * 0.785398f + 0.2f;
						const float32 r1 = (i & 1) ? 0.30f : 0.44f;
						dl.AddLine(P(std::cos(a) * 0.14f, std::sin(a) * 0.14f), P(std::cos(a) * r1, std::sin(a) * r1), col, ep);
					}
					dl.AddCircleFilled(c, t * 0.13f, col, 12);
					break;
				}
				case NkIcone::NK_AIMANT: {
					NkVec2 arc[13];
					for (int32 i = 0; i <= 12; ++i) {
						const float32 a = 3.14159f * static_cast<float32>(i) / 12.f;
						arc[i] = P(std::cos(a) * 0.26f, 0.02f + std::sin(a) * 0.26f);
					}
					dl.AddPolyline(arc, 13, col, t * 0.16f);
					dl.AddLine(P(-0.26f, 0.02f), P(-0.26f, -0.30f), col, t * 0.16f);
					dl.AddLine(P(0.26f, 0.02f), P(0.26f, -0.30f), col, t * 0.16f);
					dl.AddRectFilled({c.x - t * 0.34f, c.y - t * 0.42f, t * 0.16f, t * 0.12f}, NkColor(230, 70, 60), 1.f);
					dl.AddRectFilled({c.x + t * 0.18f, c.y - t * 0.42f, t * 0.16f, t * 0.12f}, NkColor(80, 140, 240), 1.f);
					break;
				}
				case NkIcone::NK_OBSTACLE: {
					dl.AddLine(P(-0.32f, 0.26f), P(0.32f, -0.20f), col, t * 0.22f);
					dl.AddCircleFilled(P(-0.32f, 0.26f), t * 0.11f, col, 10);
					dl.AddCircleFilled(P(0.32f, -0.20f), t * 0.11f, col, 10);
					break;
				}
				case NkIcone::NK_GOMME: {
					const NkVec2 a = P(-0.38f, 0.10f), b = P(-0.02f, -0.26f), cc = P(0.30f, 0.06f), d = P(-0.06f, 0.42f);
					dl.AddTriangleFilled(a, b, cc, col);
					dl.AddTriangleFilled(a, cc, d, col);
					dl.AddLine(P(-0.20f, -0.08f), P(0.12f, 0.24f), NkColor(20, 20, 20, 200), ep);
					dl.AddLine(P(-0.30f, 0.44f), P(0.40f, 0.44f), col, ep);
					break;
				}
				case NkIcone::NK_EPINGLE: {
					dl.AddLine(P(0.0f, -0.04f), P(-0.30f, 0.40f), col, ep * 1.2f);
					dl.AddCircleFilled(P(0.12f, -0.22f), t * 0.20f, col, 14);
					break;
				}
				case NkIcone::NK_JOUER:
					dl.AddTriangleFilled(P(-0.24f, -0.34f), P(-0.24f, 0.34f), P(0.34f, 0.f), col);
					break;
				case NkIcone::NK_PAUSE:
					dl.AddRectFilled({c.x - t * 0.26f, c.y - t * 0.32f, t * 0.18f, t * 0.64f}, col, 1.f);
					dl.AddRectFilled({c.x + t * 0.08f, c.y - t * 0.32f, t * 0.18f, t * 0.64f}, col, 1.f);
					break;
				case NkIcone::NK_PAS:
					dl.AddTriangleFilled(P(-0.30f, -0.32f), P(-0.30f, 0.32f), P(0.16f, 0.f), col);
					dl.AddRectFilled({c.x + t * 0.18f, c.y - t * 0.32f, t * 0.12f, t * 0.64f}, col, 1.f);
					break;
				case NkIcone::NK_STOP:
					dl.AddRectFilled({c.x - t * 0.28f, c.y - t * 0.28f, t * 0.56f, t * 0.56f}, col, t * 0.06f);
					break;
				case NkIcone::NK_OEIL:
				case NkIcone::NK_OEIL_BARRE: {
					NkVec2 haut[9];
					NkVec2 bas[9];
					for (int32 i = 0; i <= 8; ++i) {
						const float32 x = -0.42f + 0.84f * static_cast<float32>(i) / 8.f;
						const float32 y = 0.26f * std::sqrt(NkMaxf(0.f, 1.f - (x / 0.42f) * (x / 0.42f)));
						haut[i] = P(x, -y);
						bas[i] = P(x, y);
					}
					dl.AddPolyline(haut, 9, col, ep);
					dl.AddPolyline(bas, 9, col, ep);
					dl.AddCircleFilled(c, t * 0.12f, col, 10);
					if (ic == NkIcone::NK_OEIL_BARRE) {
						dl.AddLine(P(-0.40f, 0.38f), P(0.40f, -0.38f), col, ep * 1.3f);
					}
					break;
				}
				case NkIcone::NK_FLECHE_BAS:
					dl.AddTriangleFilled(P(-0.28f, -0.14f), P(0.28f, -0.14f), P(0.f, 0.18f), col);
					break;
				case NkIcone::NK_FLECHE_DROITE:
					dl.AddTriangleFilled(P(-0.14f, -0.28f), P(-0.14f, 0.28f), P(0.18f, 0.f), col);
					break;
				case NkIcone::NK_GRILLE:
					for (int32 i = 0; i < 4; ++i) {
						const float32 v = -0.36f + 0.24f * static_cast<float32>(i);
						dl.AddLine(P(v, -0.40f), P(v, 0.40f), col, ep * 0.8f);
						dl.AddLine(P(-0.40f, v), P(0.40f, v), col, ep * 0.8f);
					}
					break;
				case NkIcone::NK_LOUPE:
					dl.AddCircle(P(-0.06f, -0.06f), t * 0.24f, col, ep * 1.2f, 16);
					dl.AddLine(P(0.12f, 0.12f), P(0.38f, 0.38f), col, ep * 2.f);
					break;
				case NkIcone::NK_RECADRER: {
					const float32 a = 0.38f, b = 0.16f;
					const float32 sx[4] = {-1.f, 1.f, 1.f, -1.f};
					const float32 sy[4] = {-1.f, -1.f, 1.f, 1.f};
					for (int32 i = 0; i < 4; ++i) {
						dl.AddLine(P(sx[i] * a, sy[i] * a), P(sx[i] * (a - b), sy[i] * a), col, ep);
						dl.AddLine(P(sx[i] * a, sy[i] * a), P(sx[i] * a, sy[i] * (a - b)), col, ep);
					}
					dl.AddCircleFilled(c, t * 0.08f, col, 8);
					break;
				}
				case NkIcone::NK_POUBELLE:
					dl.AddRectFilled({c.x - t * 0.24f, c.y - t * 0.16f, t * 0.48f, t * 0.54f}, col, t * 0.05f);
					dl.AddRectFilled({c.x - t * 0.34f, c.y - t * 0.30f, t * 0.68f, t * 0.09f}, col, 1.f);
					dl.AddRectFilled({c.x - t * 0.10f, c.y - t * 0.40f, t * 0.20f, t * 0.10f}, col, 1.f);
					break;
				case NkIcone::NK_ENGRENAGE:
					for (int32 i = 0; i < 8; ++i) {
						const float32 a = static_cast<float32>(i) * 0.785398f;
						dl.AddLine(P(std::cos(a) * 0.18f, std::sin(a) * 0.18f), P(std::cos(a) * 0.40f, std::sin(a) * 0.40f), col,
								   t * 0.14f);
					}
					dl.AddCircleFilled(c, t * 0.28f, col, 16);
					dl.AddCircleFilled(c, t * 0.11f, kBarreOutils, 10);
					break;
				case NkIcone::NK_NIVEAU:
					dl.AddRectFilled({c.x - t * 0.38f, c.y + t * 0.14f, t * 0.76f, t * 0.16f}, col, 1.f);
					dl.AddRectFilled({c.x - t * 0.28f, c.y - t * 0.08f, t * 0.56f, t * 0.14f}, col, 1.f);
					dl.AddRectFilled({c.x - t * 0.18f, c.y - t * 0.28f, t * 0.36f, t * 0.12f}, col, 1.f);
					break;
				case NkIcone::NK_CUBE_PLUS:
					dl.AddRect({c.x - t * 0.36f, c.y - t * 0.30f, t * 0.50f, t * 0.50f}, col, ep, 1.f);
					dl.AddLine(P(0.28f, 0.02f), P(0.28f, 0.42f), col, ep * 1.4f);
					dl.AddLine(P(0.08f, 0.22f), P(0.48f, 0.22f), col, ep * 1.4f);
					break;
				// --- Acteurs -----------------------------------------------------------
				case NkIcone::NK_A_BALLON:
					dl.AddCircleFilled(c, t * 0.38f, col, 20);
					dl.AddCircleFilled(P(-0.13f, -0.14f), t * 0.10f, NkColor(255, 255, 255, 110), 10);
					break;
				case NkIcone::NK_A_BLOB:
					dl.AddCircleFilled(P(-0.14f, 0.12f), t * 0.24f, col, 16);
					dl.AddCircleFilled(P(0.16f, 0.14f), t * 0.22f, col, 16);
					dl.AddCircleFilled(P(0.02f, -0.12f), t * 0.24f, col, 16);
					dl.AddRectFilled({c.x - t * 0.36f, c.y + t * 0.18f, t * 0.72f, t * 0.18f}, col, t * 0.09f);
					dl.AddCircleFilled(P(-0.06f, -0.20f), t * 0.07f, NkColor(255, 255, 255, 120), 8);
					break;
				case NkIcone::NK_A_GELEE:
					dl.AddRectFilled({c.x - t * 0.36f, c.y - t * 0.30f, t * 0.72f, t * 0.64f}, col, t * 0.18f);
					dl.AddRectFilled({c.x - t * 0.24f, c.y - t * 0.22f, t * 0.16f, t * 0.10f}, NkColor(255, 255, 255, 120), t * 0.05f);
					break;
				case NkIcone::NK_A_CAISSE:
					dl.AddRectFilled({c.x - t * 0.36f, c.y - t * 0.36f, t * 0.72f, t * 0.72f}, col, 1.f);
					dl.AddRect({c.x - t * 0.36f, c.y - t * 0.36f, t * 0.72f, t * 0.72f}, NkAssombrir(col, 0.45f), ep, 1.f);
					dl.AddLine(P(-0.30f, -0.30f), P(0.30f, 0.30f), NkAssombrir(col, 0.45f), ep);
					dl.AddLine(P(0.30f, -0.30f), P(-0.30f, 0.30f), NkAssombrir(col, 0.45f), ep);
					break;
				case NkIcone::NK_A_GOUTTE:
					dl.AddCircleFilled(P(0.f, 0.12f), t * 0.26f, col, 18);
					dl.AddTriangleFilled(P(-0.23f, 0.02f), P(0.23f, 0.02f), P(0.f, -0.42f), col);
					dl.AddCircleFilled(P(-0.08f, 0.10f), t * 0.06f, NkColor(255, 255, 255, 140), 8);
					break;
				case NkIcone::NK_A_SABLE: {
					const float32 xs[10] = {-0.30f, -0.10f, 0.10f, 0.30f, -0.20f, 0.f, 0.20f, -0.10f, 0.10f, 0.f};
					const float32 ys[10] = {0.30f, 0.30f, 0.30f, 0.30f, 0.12f, 0.12f, 0.12f, -0.06f, -0.06f, -0.24f};
					for (int32 i = 0; i < 10; ++i) {
						dl.AddCircleFilled(P(xs[i], ys[i]), t * 0.085f, col, 8);
					}
					break;
				}
				case NkIcone::NK_A_ATOMES: {
					NkVec2 pts[7];
					pts[0] = c;
					for (int32 i = 0; i < 6; ++i) {
						const float32 a = static_cast<float32>(i) * 1.0472f;
						pts[i + 1] = P(std::cos(a) * 0.32f, std::sin(a) * 0.32f);
					}
					for (int32 i = 0; i < 6; ++i) {
						dl.AddLine(pts[0], pts[i + 1], NkAvecAlpha(col, 150), ep * 0.8f);
						dl.AddLine(pts[i + 1], pts[(i + 1) % 6 + 1], NkAvecAlpha(col, 150), ep * 0.8f);
					}
					for (int32 i = 0; i < 7; ++i) {
						dl.AddCircleFilled(pts[i], t * 0.09f, col, 10);
					}
					break;
				}
				case NkIcone::NK_A_TISSU:
					for (int32 i = 0; i < 4; ++i) {
						const float32 x = -0.30f + 0.2f * static_cast<float32>(i);
						dl.AddRectFilled({c.x + x * t, c.y - t * 0.34f, t * 0.19f, t * 0.62f + (i & 1) * t * 0.08f},
										 (i & 1) ? NkEclaircir(col, 0.2f) : col, 1.f);
					}
					dl.AddLine(P(-0.40f, -0.36f), P(0.40f, -0.36f), NkColor(200, 200, 200), ep);
					break;
				case NkIcone::NK_A_CORDE:
					dl.AddLine(P(-0.30f, -0.42f), P(0.30f, -0.42f), NkColor(160, 160, 160), ep);
					for (int32 i = 0; i < 5; ++i) {
						dl.AddCircleFilled(P(0.f, -0.34f + 0.13f * static_cast<float32>(i)), t * 0.06f, col, 8);
					}
					dl.AddCircleFilled(P(0.f, 0.30f), t * 0.14f, NkColor(150, 150, 160), 12);
					break;
				case NkIcone::NK_A_PONT:
					for (int32 i = 0; i < 7; ++i) {
						const float32 x = -0.36f + 0.12f * static_cast<float32>(i);
						const float32 y = 0.02f + 0.12f * (1.f - (x / 0.36f) * (x / 0.36f));
						dl.AddRectFilled({c.x + x * t - t * 0.05f, c.y + y * t - t * 0.04f, t * 0.10f, t * 0.08f}, col, 1.f);
					}
					dl.AddRectFilled({c.x - t * 0.44f, c.y - t * 0.02f, t * 0.07f, t * 0.44f}, NkColor(140, 140, 150), 1.f);
					dl.AddRectFilled({c.x + t * 0.37f, c.y - t * 0.02f, t * 0.07f, t * 0.44f}, NkColor(140, 140, 150), 1.f);
					break;
				default:
					break;
			}
		}

		// =====================================================================
		// Cycle
		// =====================================================================
		uint32 NkUE5Ui::Hacher(const char *s) noexcept {
			uint32 h = 2166136261u;
			while (s != nullptr && *s) {
				h ^= static_cast<uint8>(*s++);
				h *= 16777619u;
			}
			return h == 0 ? 1u : h;
		}

		void NkUE5Ui::Debut(NkGuiDrawList *dl, NkGuiFont *police, float32 taillePolice, NkGuiFont *grande,
							float32 tailleGrande, NkGuiFont *mono, float32 tailleMono, const NkUE5Entree *entree,
							float32 echelle, float32 dt) noexcept {
			mDl = dl;
			mPolice = police;
			mGrande = grande;
			mMono = mono;
			mTaillePolice = taillePolice;
			mTailleGrande = tailleGrande;
			mTailleMono = tailleMono;
			mEntree = entree;
			mEchelle = echelle;
			mDt = dt;
			mBulleIdTrame = 0;
			mBulleDemandee = false;
			mModale = NkRect{0.f, 0.f, 0.f, 0.f};
		}

		void NkUE5Ui::Fin() noexcept {
			if (mBulleIdTrame == 0 || mBulleIdTrame != mBulleId) {
				mBulleId = mBulleIdTrame;
				mBulleTemps = 0.f;
			} else {
				mBulleTemps += mDt;
			}
			if (mBulleId != 0 && mBulleTemps > 0.45f && mBulleTexte[0] != '\0' && mActif == 0) {
				const float32 px = E(12.f);
				const float32 w = LargeurTexte(mBulleTexte, px) + E(16.f);
				const float32 h = E(24.f);
				float32 x = mEntree->x + E(14.f);
				float32 y = mEntree->y + E(20.f);
				const NkRect clip = mDl->CurrentClip();
				if (x + w > clip.x + clip.w) {
					x = mEntree->x - w - E(6.f);
				}
				(void)y;
				mDl->AddRectFilled({x + E(2.f), y + E(2.f), w, h}, NkColor(0, 0, 0, 120), E(3.f));
				mDl->AddRectFilled({x, y, w, h}, NkColor(20, 20, 20, 245), E(3.f));
				mDl->AddRect({x, y, w, h}, NkColor(70, 70, 70), 1.f, E(3.f));
				TexteGauche({x, y, w, h}, mBulleTexte, kTexteClair, px, E(8.f));
			}
			if (mActif != 0 && !mEntree->bas[0]) {
				mActif = 0;
			}
		}

		bool NkUE5Ui::Bloque(const NkRect &r) const noexcept {
			if (mModale.w <= 0.f) {
				return false;
			}
			// Un widget ENTIEREMENT hors de la zone modale ne recoit rien.
			return !(r.x >= mModale.x && r.y >= mModale.y && r.x + r.w <= mModale.x + mModale.w + 0.5f &&
					 r.y + r.h <= mModale.y + mModale.h + 0.5f);
		}

		// =====================================================================
		// Texte
		// =====================================================================
		NkGuiFont *NkUE5Ui::Police(float32 px, bool mono, float32 &echelle) const noexcept {
			NkGuiFont *f = mPolice;
			float32 taille = mTaillePolice;
			if (mono && mMono != nullptr && mMono->Valid()) {
				f = mMono;
				taille = mTailleMono;
			} else if (mGrande != nullptr && mGrande->Valid() && px > (mTaillePolice + mTailleGrande) * 0.5f) {
				f = mGrande;
				taille = mTailleGrande;
			}
			echelle = taille > 0.f ? px / taille : 1.f;
			if (echelle > 0.97f && echelle < 1.03f) {
				echelle = 1.f; // a l'echelle 1, NKGui cale les glyphes au pixel : texte net
			}
			return f;
		}

		float32 NkUE5Ui::LargeurTexte(const char *s, float32 px, bool mono) const noexcept {
			float32 k = 1.f;
			NkGuiFont *f = Police(px, mono, k);
			return (f != nullptr && f->Valid() && s != nullptr) ? f->MeasureWidth(s) * k : 0.f;
		}

		void NkUE5Ui::Texte(float32 x, float32 haut, const char *s, const NkColor &c, float32 px, bool mono,
							float32 maxW) noexcept {
			float32 k = 1.f;
			NkGuiFont *f = Police(px, mono, k);
			if (f == nullptr || !f->Valid() || s == nullptr) {
				return;
			}
			const NkVec2 base{std::floor(x + 0.5f), std::floor(haut + f->Ascent() * k + 0.5f)};
			if (k == 1.f) {
				mDl->AddText(f->Face(), f->TexId(), base, s, c, maxW);
			} else {
				mDl->AddTextScaled(f->Face(), f->TexId(), base, s, c, k, maxW);
			}
		}

		void NkUE5Ui::TexteGauche(const NkRect &r, const char *s, const NkColor &c, float32 px, float32 marge) noexcept {
			float32 k = 1.f;
			NkGuiFont *f = Police(px, false, k);
			if (f == nullptr || !f->Valid()) {
				return;
			}
			const float32 lh = f->LineHeight() * k;
			Texte(r.x + marge, r.y + (r.h - lh) * 0.5f, s, c, px, false, r.w - marge * 2.f);
		}

		void NkUE5Ui::TexteCentre(const NkRect &r, const char *s, const NkColor &c, float32 px) noexcept {
			float32 k = 1.f;
			NkGuiFont *f = Police(px, false, k);
			if (f == nullptr || !f->Valid()) {
				return;
			}
			const float32 lh = f->LineHeight() * k;
			const float32 w = f->MeasureWidth(s) * k;
			Texte(r.x + (r.w - w) * 0.5f, r.y + (r.h - lh) * 0.5f, s, c, px);
		}

		void NkUE5Ui::TexteDroite(const NkRect &r, const char *s, const NkColor &c, float32 px, float32 marge) noexcept {
			float32 k = 1.f;
			NkGuiFont *f = Police(px, false, k);
			if (f == nullptr || !f->Valid()) {
				return;
			}
			const float32 lh = f->LineHeight() * k;
			const float32 w = f->MeasureWidth(s) * k;
			Texte(r.x + r.w - marge - w, r.y + (r.h - lh) * 0.5f, s, c, px);
		}

		// =====================================================================
		// Widgets
		// =====================================================================
		bool NkUE5Ui::Survol(const NkRect &r) const noexcept {
			if (Bloque(r)) {
				return false;
			}
			const NkRect clip = mDl->CurrentClip();
			const bool dansClip = mEntree->x >= clip.x && mEntree->y >= clip.y && mEntree->x < clip.x + clip.w &&
								  mEntree->y < clip.y + clip.h;
			return dansClip && mEntree->Dans(r) && (mActif == 0);
		}

		bool NkUE5Ui::Clic(const NkRect &r) const noexcept {
			return Survol(r) && mEntree->presse[0];
		}

		bool NkUE5Ui::Bouton(const char *id, const NkRect &r, const char *libelle, bool actif, NkIcone ic,
							 const NkColor *teinteIcone) noexcept {
			const uint32 h = Hacher(id);
			const bool survol = Survol(r) || mActif == h;
			bool clique = false;
			if (Survol(r) && mEntree->presse[0]) {
				mActif = h;
			}
			if (mActif == h && mEntree->relache[0]) {
				clique = mEntree->Dans(r);
			}
			if (survol) {
				mDernierSurvolId = h;
			}
			NkColor fond = actif ? kAccent : (mActif == h ? kBoutonPresse : (survol ? kBoutonSurvol : kBouton));
			mDl->AddRectFilled(r, fond, E(3.f));
			if (!actif) {
				mDl->AddRect(r, NkColor(18, 18, 18), 1.f, E(3.f));
			}
			float32 x = r.x + E(8.f);
			if (ic != NkIcone::NK_AUCUNE) {
				const float32 t = r.h * 0.62f;
				NkDessinerIcone(*mDl, ic, NkVec2{x + t * 0.5f, r.y + r.h * 0.5f}, t,
								teinteIcone != nullptr ? *teinteIcone : (actif ? kTexteClair : kTexte));
				x += t + E(6.f);
			}
			if (libelle != nullptr && *libelle) {
				if (ic == NkIcone::NK_AUCUNE) {
					TexteCentre(r, libelle, actif || survol ? kTexteClair : kTexte, E(13.f));
				} else {
					TexteGauche({x, r.y, r.x + r.w - x, r.h}, libelle, actif || survol ? kTexteClair : kTexte, E(13.f), 0.f);
				}
			}
			return clique;
		}

		bool NkUE5Ui::BoutonOutil(const char *id, const NkRect &r, NkIcone ic, bool actif, const char *bulle,
								  const NkColor *teinte) noexcept {
			const uint32 h = Hacher(id);
			const bool survol = Survol(r) || mActif == h;
			bool clique = false;
			if (Survol(r) && mEntree->presse[0]) {
				mActif = h;
			}
			if (mActif == h && mEntree->relache[0]) {
				clique = mEntree->Dans(r);
			}
			if (actif) {
				mDl->AddRectFilled(r, kAccentSombre, E(3.f));
				mDl->AddRect(r, kAccent, 1.f, E(3.f));
			} else if (survol) {
				mDl->AddRectFilled(r, mActif == h ? kBoutonPresse : kBoutonSurvol, E(3.f));
			}
			const NkColor c = teinte != nullptr ? *teinte : (actif || survol ? kTexteClair : kTexte);
			NkDessinerIcone(*mDl, ic, NkVec2{r.x + r.w * 0.5f, r.y + r.h * 0.5f}, NkMinf(r.w, r.h) * 0.62f, c);
			if (survol && bulle != nullptr) {
				mBulleIdTrame = h;
				if (mBulleId != h) {
					std::snprintf(mBulleTexte, sizeof(mBulleTexte), "%s", bulle);
				}
			}
			return clique;
		}

		void NkUE5Ui::InfoBulle(const char *texte) noexcept {
			if (mDernierSurvolId == 0 || texte == nullptr) {
				return;
			}
			mBulleIdTrame = mDernierSurvolId;
			if (mBulleId != mDernierSurvolId) {
				std::snprintf(mBulleTexte, sizeof(mBulleTexte), "%s", texte);
			}
			mDernierSurvolId = 0;
		}

		bool NkUE5Ui::Case(const char *id, const NkRect &r, bool &v) noexcept {
			const uint32 h = Hacher(id);
			const float32 cote = NkMinf(r.h, E(16.f));
			const NkRect boite{r.x, r.y + (r.h - cote) * 0.5f, cote, cote};
			const bool survol = Survol(r);
			bool change = false;
			if (survol && mEntree->presse[0]) {
				mActif = h;
			}
			if (mActif == h && mEntree->relache[0] && mEntree->Dans(r)) {
				v = !v;
				change = true;
			}
			if (survol) {
				mDernierSurvolId = h;
			}
			if (v) {
				mDl->AddRectFilled(boite, survol ? kAccentClair : kAccent, E(2.f));
				const NkVec2 a{boite.x + cote * 0.22f, boite.y + cote * 0.52f};
				const NkVec2 b{boite.x + cote * 0.42f, boite.y + cote * 0.72f};
				const NkVec2 c{boite.x + cote * 0.78f, boite.y + cote * 0.28f};
				mDl->AddLine(a, b, kTexteClair, E(2.f));
				mDl->AddLine(b, c, kTexteClair, E(2.f));
			} else {
				mDl->AddRectFilled(boite, survol ? kChampSurvol : kChamp, E(2.f));
				mDl->AddRect(boite, survol ? NkColor(96, 96, 96) : kChampBord, 1.f, E(2.f));
			}
			return change;
		}

		bool NkUE5Ui::Glisseur(const char *id, const NkRect &r, float32 &v, float32 mn, float32 mx,
							   const char *fmt) noexcept {
			const uint32 h = Hacher(id);
			const bool survol = Survol(r);
			bool change = false;
			if (survol) {
				mDernierSurvolId = h;
			}
			if (survol && mEntree->presse[0]) {
				mActif = h;
				mDepartX = mEntree->x;
				mDepartValeur = v;
				mAGlisse = false;
			}
			if (mActif == h) {
				const float32 dx = mEntree->x - mDepartX;
				if (dx > E(3.f) || dx < -E(3.f)) {
					mAGlisse = true;
				}
				if (mAGlisse) {
					const float32 finesse = mEntree->shift ? 0.1f : 1.f;
					const float32 nv = NkClampf(mDepartValeur + dx / NkMaxf(r.w, 1.f) * (mx - mn) * finesse, mn, mx);
					if (nv != v) {
						v = nv;
						change = true;
					}
				}
				if (mEntree->relache[0] && !mAGlisse) {
					const float32 t = NkClampf((mEntree->x - r.x) / NkMaxf(r.w, 1.f), 0.f, 1.f);
					v = mn + (mx - mn) * t;
					change = true;
				}
			}
			const bool chaud = survol || mActif == h;
			mDl->AddRectFilled(r, chaud ? kChampSurvol : kChamp, E(3.f));
			const float32 t = mx > mn ? NkClampf((v - mn) / (mx - mn), 0.f, 1.f) : 0.f;
			if (t > 0.f) {
				mDl->AddRectFilled({r.x + 1.f, r.y + 1.f, (r.w - 2.f) * t, r.h - 2.f},
								   mActif == h ? kAccentSombre : kChampRemplissage, E(2.f));
			}
			mDl->AddRect(r, mActif == h ? kAccent : (survol ? NkColor(90, 90, 90) : kChampBord), 1.f, E(3.f));
			char buf[48];
			std::snprintf(buf, sizeof(buf), fmt != nullptr ? fmt : "%.2f", static_cast<double>(v));
			TexteGauche(r, buf, kTexteClair, E(12.5f), E(7.f));
			return change;
		}

		bool NkUE5Ui::GlisseurEntier(const char *id, const NkRect &r, int32 &v, int32 mn, int32 mx) noexcept {
			float32 f = static_cast<float32>(v);
			const bool change = Glisseur(id, r, f, static_cast<float32>(mn), static_cast<float32>(mx), "%.0f");
			const int32 nv = static_cast<int32>(f + 0.5f);
			if (nv != v) {
				v = nv;
				return true;
			}
			return change && false;
		}

		bool NkUE5Ui::Categorie(const char *id, const NkRect &r, const char *titre, bool &ouvert) noexcept {
			const uint32 h = Hacher(id);
			const bool survol = Survol(r);
			bool change = false;
			if (survol && mEntree->presse[0]) {
				mActif = h;
			}
			if (mActif == h && mEntree->relache[0] && mEntree->Dans(r)) {
				ouvert = !ouvert;
				change = true;
			}
			mDl->AddRectFilled(r, survol ? kCategorieSurvol : kCategorie, 0.f);
			mDl->AddRectFilled({r.x, r.y + r.h - 1.f, r.w, 1.f}, kSeparateur, 0.f);
			const float32 t = r.h * 0.5f;
			NkDessinerIcone(*mDl, ouvert ? NkIcone::NK_FLECHE_BAS : NkIcone::NK_FLECHE_DROITE,
							NkVec2{r.x + E(12.f), r.y + r.h * 0.5f}, t, kTexte);
			TexteGauche({r.x + E(22.f), r.y, r.w - E(22.f), r.h}, titre, kTexteClair, E(13.f), 0.f);
			return change;
		}

		bool NkUE5Ui::Onglet(const NkRect &r, const char *libelle, bool actif, NkIcone ic) noexcept {
			const bool survol = Survol(r);
			if (actif) {
				mDl->AddRectFilled(r, kOngletActif, 0.f);
				mDl->AddRectFilled({r.x, r.y, r.w, E(2.f)}, kAccent, 0.f);
			} else if (survol) {
				mDl->AddRectFilled(r, kOngletInactif, 0.f);
			}
			float32 x = r.x + E(10.f);
			if (ic != NkIcone::NK_AUCUNE) {
				const float32 t = r.h * 0.5f;
				NkDessinerIcone(*mDl, ic, NkVec2{x + t * 0.5f, r.y + r.h * 0.5f}, t, actif ? kTexteClair : kTexteFaible);
				x += t + E(6.f);
			}
			TexteGauche({x, r.y, r.x + r.w - x, r.h}, libelle, actif ? kTexteClair : (survol ? kTexte : kTexteFaible),
						E(12.5f), 0.f);
			return survol && mEntree->presse[0];
		}

		bool NkUE5Ui::Ligne(const char *id, const NkRect &r, bool selectionnee, bool impaire) noexcept {
			const uint32 h = Hacher(id);
			const bool survol = Survol(r);
			if (selectionnee) {
				mDl->AddRectFilled(r, kSelectionLigne, 0.f);
			} else if (survol) {
				mDl->AddRectFilled(r, NkColor(58, 58, 58), 0.f);
			} else if (impaire) {
				mDl->AddRectFilled(r, kLigneImpaire, 0.f);
			}
			if (survol) {
				mDernierSurvolId = h;
			}
			return survol && mEntree->presse[0];
		}

	} // namespace physic2d
} // namespace nkentseu
