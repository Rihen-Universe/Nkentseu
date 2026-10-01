//
// NkUnkenyZoneSure.cpp
// =============================================================================
// Description :
//   La zone sure offerte au jeu : conversion, ancrage, surimpression.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
#include "Unkeny/Partie/NkUnkenyZoneSure.h"

namespace nkentseu {
	namespace unkeny {

		NkEcranDuJeu NkEcranDepuisLayout(const renderer::NkLayoutInfo &layout, const nkgui::NkRect &rect, bool simule) noexcept {
			NkEcranDuJeu e;
			e.simule = simule;
			const float32 lw = static_cast<float32>(layout.width);
			const float32 lh = static_cast<float32>(layout.height);
			e.rect = (rect.w > 0.f && rect.h > 0.f) ? rect : nkgui::NkRect{0.f, 0.f, lw, lh};
			// L'echelle de l'affichage : 1 dans le joueur (la fenetre EST le
			// layout), < 1 dans l'editeur (l'appareil reduit dans le viseur).
			const float32 kx = lw > 0.f ? e.rect.w / lw : 1.f;
			const float32 ky = lh > 0.f ? e.rect.h / lh : 1.f;
			e.marges = NkSafeAreaInsets(layout.safeArea.top * ky, layout.safeArea.bottom * ky, layout.safeArea.left * kx,
										layout.safeArea.right * kx);
			e.densite = (layout.density > 0.f ? layout.density : 1.f) * kx;
			return e;
		}

		bool NkZoneSureMonde(const NkScene &scene, NkVec2f &minimum, NkVec2f &maximum) noexcept {
			const NkEcranDuJeu &e = scene.Ecran();
			if (!e.Valide()) {
				return false;
			}
			const nkgui::NkRect z = NkZoneSure(e);
			// Le haut de l'ecran est le HAUT du monde (Y s'inverse dans la vue).
			const NkVec2f a = scene.Camera().EcranVersMonde(NkVec2f(z.x, z.y + z.h));
			const NkVec2f b = scene.Camera().EcranVersMonde(NkVec2f(z.x + z.w, z.y));
			minimum = NkVec2f(a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y);
			maximum = NkVec2f(a.x < b.x ? b.x : a.x, a.y < b.y ? b.y : a.y);
			return true;
		}

		int32 NkAppliquerAncrages(NkScene &scene, NkVector<NkPositionAncree> *avant) {
			const NkEcranDuJeu &e = scene.Ecran();
			if (!e.Valide()) {
				return 0;
			}
			const nkgui::NkRect sure = NkZoneSure(e);
			const NkVue2D &vue = scene.Camera();
			int32 n = 0;
			// RELEVER, PUIS POSER : les positions d'avant sont gardees AVANT
			// toute ecriture (l'editeur les rend), et le parcours ne modifie rien.
			struct NkPose {
					ecs::NkEntityId id;
					NkVec2f position;
			};
			NkVector<NkPose> poses;
			scene.Monde().Query<NkAncrageEcran2D, NkTransform2D>().ForEach(
				[&](ecs::NkEntityId id, NkAncrageEcran2D &a, NkTransform2D &t) {
					if (!scene.EstActive(id)) {
						return;
					}
					// La taille a l'ecran : celle du sprite, echelle et zoom compris.
					float32 w = 0.f;
					float32 h = 0.f;
					NkVec2f pivot(0.5f, 0.5f);
					if (const NkSprite2D *s = scene.Monde().Get<NkSprite2D>(id)) {
						w = vue.LongueurVersEcran(s->taille.x * (t.echelle.x < 0.f ? -t.echelle.x : t.echelle.x));
						h = vue.LongueurVersEcran(s->taille.y * (t.echelle.y < 0.f ? -t.echelle.y : t.echelle.y));
						pivot = s->pivot;
					}
					const nkgui::NkRect zone = a.zoneSure ? sure : e.rect;
					const nkgui::NkRect r = NkAncrer(zone, static_cast<NkAncre>(a.ancre), w, h, a.decalage.x * e.densite,
													 a.decalage.y * e.densite);
					NkPose p;
					p.id = id;
					p.position = vue.EcranVersMonde(NkVec2f(r.x + pivot.x * r.w, r.y + pivot.y * r.h));
					poses.PushBack(p);
				});
			for (usize i = 0; i < poses.Size(); ++i) {
				NkTransform2D *t = scene.Monde().Get<NkTransform2D>(poses[i].id);
				if (t == nullptr) {
					continue;
				}
				if (avant != nullptr) {
					NkPositionAncree pa;
					pa.id = poses[i].id;
					pa.avant = t->position;
					avant->PushBack(pa);
				}
				t->position = poses[i].position;
				++n;
			}
			return n;
		}

		void NkRendreAncrages(NkScene &scene, const NkVector<NkPositionAncree> &avant) {
			for (usize i = 0; i < avant.Size(); ++i) {
				if (NkTransform2D *t = scene.Monde().Get<NkTransform2D>(avant[i].id)) {
					t->position = avant[i].avant;
				}
			}
		}

		void NkDessinerZoneSure(nkgui::NkGuiDrawList &dl, const NkEcranDuJeu &e) {
			if (!e.Valide()) {
				return;
			}
			const nkgui::NkRect z = NkZoneSure(e);
			const nkgui::NkColor voile(255, 170, 40, 40);
			const nkgui::NkColor trait(255, 190, 60, 230);
			const nkgui::NkRect &r = e.rect;
			// Les bandes hors zone : voile et hachures (on les reconnait sans
			// couleur, sur une capture en niveaux de gris comme sur un ecran).
			const nkgui::NkRect bandes[4] = {
				{r.x, r.y, r.w, z.y - r.y},
				{r.x, z.y + z.h, r.w, r.y + r.h - z.y - z.h},
				{r.x, z.y, z.x - r.x, z.h},
				{z.x + z.w, z.y, r.x + r.w - z.x - z.w, z.h},
			};
			for (const nkgui::NkRect &b : bandes) {
				if (b.w <= 0.5f || b.h <= 0.5f) {
					continue;
				}
				dl.AddRectFilled(b, voile);
				dl.PushClipRect(b, true);
				const float32 pas = 10.f;
				for (float32 k = -b.h; k < b.w; k += pas) {
					dl.AddLine(NkVec2f(b.x + k, b.y + b.h), NkVec2f(b.x + k + b.h, b.y), nkgui::NkColor(255, 180, 60, 70), 1.f);
				}
				dl.PopClipRect();
			}
			dl.AddRect(z, trait, 1.5f);
			// Les coins du rectangle sur, appuyes : c'est la que vont les boutons.
			const float32 c = 10.f;
			dl.AddLine(NkVec2f(z.x, z.y), NkVec2f(z.x + c, z.y), trait, 3.f);
			dl.AddLine(NkVec2f(z.x, z.y), NkVec2f(z.x, z.y + c), trait, 3.f);
			dl.AddLine(NkVec2f(z.x + z.w, z.y + z.h), NkVec2f(z.x + z.w - c, z.y + z.h), trait, 3.f);
			dl.AddLine(NkVec2f(z.x + z.w, z.y + z.h), NkVec2f(z.x + z.w, z.y + z.h - c), trait, 3.f);
		}

	} // namespace unkeny
} // namespace nkentseu
