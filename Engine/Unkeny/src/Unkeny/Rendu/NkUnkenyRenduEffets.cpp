//
// NkUnkenyRenduEffets.cpp
// =============================================================================
// Description :
//   Le dessin des particules visuelles. Il LIT NkEffets2D, il n'y ecrit rien :
//   dessiner deux fois ne change pas l'effet.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Rendu/NkUnkenyRenduEffets.h"

namespace nkentseu {
	namespace unkeny {

		namespace {
			using nkgui::NkColor;
			using nkgui::NkGuiBlend;

			constexpr float32 PI = 3.14159265f;
			constexpr int32 SEGMENTS_DOUX = 8; ///< un disque fondu : huit triangles, assez a 20 px

			uint32 Melange(uint32 a, uint32 b, float32 t) noexcept {
				uint32 o = 0u;
				for (int32 k = 0; k < 32; k += 8) {
					const float32 x = static_cast<float32>((a >> k) & 0xFFu);
					const float32 y = static_cast<float32>((b >> k) & 0xFFu);
					const uint32 v = static_cast<uint32>(x + (y - x) * t + 0.5f);
					o |= (v > 255u ? 255u : v) << k;
				}
				return o;
			}

			/// RGBA -> NkColor ; `premul` : la couleur est multipliee par l'alpha
			/// (le mode additif n'en lit pas l'alpha).
			NkColor Couleur(uint32 rgba, bool premul) noexcept {
				const uint32 a = rgba & 0xFFu;
				uint32 r = (rgba >> 24) & 0xFFu;
				uint32 g = (rgba >> 16) & 0xFFu;
				uint32 b = (rgba >> 8) & 0xFFu;
				if (premul) {
					r = r * a / 255u;
					g = g * a / 255u;
					b = b * a / 255u;
					return NkColor(static_cast<uint8>(r), static_cast<uint8>(g), static_cast<uint8>(b), 255);
				}
				return NkColor(static_cast<uint8>(r), static_cast<uint8>(g), static_cast<uint8>(b), static_cast<uint8>(a));
			}

			void Douce(nkgui::NkGuiDrawList &dl, const NkVec2f &c, float32 r, const NkColor &centre, bool additif) {
				// Le bord : transparent en alpha (meme teinte), NOIR en additif.
				const NkColor bord = additif ? NkColor(0, 0, 0, 255) : NkColor(centre.r, centre.g, centre.b, 0);
				for (int32 k = 0; k < SEGMENTS_DOUX; ++k) {
					const float32 a0 = 2.f * PI * static_cast<float32>(k) / static_cast<float32>(SEGMENTS_DOUX);
					const float32 a1 = 2.f * PI * static_cast<float32>(k + 1) / static_cast<float32>(SEGMENTS_DOUX);
					dl.AddTriangleMultiColor(c, NkVec2f(c.x + r * math::NkCos(a0), c.y + r * math::NkSin(a0)),
											 NkVec2f(c.x + r * math::NkCos(a1), c.y + r * math::NkSin(a1)), centre, bord, bord);
				}
			}

			int32 Passe(nkgui::NkGuiDrawList &dl, NkScene &scene, bool additifs) {
				const NkVector<NkParticuleEffet2D> &ps = scene.Effets().Particules();
				bool aucune = true;
				for (uint32 i = 0; i < ps.Size() && aucune; ++i) {
					aucune = ps[i].additif != additifs;
				}
				// Rien a dessiner : AUCUN changement d'etat dans la liste.
				if (aucune) {
					return 0;
				}
				const NkVue2D &cam = scene.Camera();
				const nkgui::NkRect vis = cam.ZoneVisible();
				if (additifs) {
					dl.PushBlend(NkGuiBlend::PlusLighter);
				}
				int32 n = 0;
				for (uint32 i = 0; i < ps.Size(); ++i) {
					const NkParticuleEffet2D &p = ps[i];
					if (p.additif != additifs) {
						continue;
					}
					const float32 t = p.vie > 0.f ? p.age / p.vie : 1.f;
					const float32 taille = p.taille0 + (p.taille1 - p.taille0) * t;
					const float32 r = taille * 0.5f;
					if (p.pos.x + r < vis.x || p.pos.x - r > vis.x + vis.w || p.pos.y + r < vis.y ||
						p.pos.y - r > vis.y + vis.h) {
						continue;
					}
					const uint32 rgba = NkCouleurSurLaVie(p.c0, p.c1, p.c2, t);
					if ((rgba & 0xFFu) == 0u) {
						continue; // entierement transparente : rien a ajouter
					}
					const NkColor c = Couleur(rgba, additifs);
					const NkVec2f e = cam.MondeVersEcran(p.pos);
					const float32 re = cam.LongueurVersEcran(r);
					switch (p.forme) {
						case NkFormeParticule2D::NK_TRAIT: {
							// Le trait suit la vitesse sur 1/30 s : ce que l'oeil garde
							// d'une goutte qui tombe.
							const NkVec2f queue = cam.MondeVersEcran(NkVec2f(p.pos.x - p.vit.x / 30.f, p.pos.y - p.vit.y / 30.f));
							dl.AddLine(queue, e, c, re * 2.f > 1.f ? re * 2.f : 1.f);
							break;
						}
						case NkFormeParticule2D::NK_PLEINE:
							dl.AddCircleFilled(e, re > 0.5f ? re : 0.5f, c, 8);
							break;
						default:
							Douce(dl, e, re > 0.75f ? re : 0.75f, c, additifs);
							break;
					}
					++n;
				}
				if (additifs) {
					dl.PopBlend();
				}
				return n;
			}
		} // namespace

		uint32 NkCouleurSurLaVie(uint32 debut, uint32 milieu, uint32 fin, float32 t) noexcept {
			t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
			return t < 0.5f ? Melange(debut, milieu, t * 2.f) : Melange(milieu, fin, (t - 0.5f) * 2.f);
		}

		int32 NkDessinerEffets(nkgui::NkGuiDrawList &dl, NkScene &scene, NkPasseEffets2D passe) {
			int32 n = 0;
			if (passe != NkPasseEffets2D::NK_EFFETS_EMISSIFS) {
				n += Passe(dl, scene, false);
			}
			if (passe != NkPasseEffets2D::NK_EFFETS_ECLAIRES) {
				n += Passe(dl, scene, true);
			}
			return n;
		}

	} // namespace unkeny
} // namespace nkentseu
