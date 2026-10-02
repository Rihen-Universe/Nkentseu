// -----------------------------------------------------------------------------
// @File    NKScenaInterfaceVue.cpp
// @Brief   Le VISEUR de NKScena : la barre de vue, l'image de la vue 3D (la vue
//          de Nogee, empruntee), la grille du sol, les CAMERAS de la scene et
//          leur pyramide de vision (la camera du plan en rouge « antenne »), le
//          cadre du plan quand la vue regarde par elle, le GIZMO (deplacer,
//          tourner, echelle : on pose une entite, puis une cle), la camera
//          d'edition (clic droit + souris, clic milieu, molette).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// Les GESTES sont ceux d'UE5 et de Nogee (NogeeInterfaceVue.cpp, dont la
// geometrie de l'ecran et le gizmo sont repris ici sans l'historique de scene :
// NKScena n'enregistre pas la scene, elle pose le temps dessus -- un objet
// deplace a la main revient a sa piste au prochain changement d'instant, tant
// qu'une cle ne l'a pas retenu, comme dans le Sequencer d'UE5).
// -----------------------------------------------------------------------------

#include "NKScena/NKScenaInterface.h"
#include "NKScena/NKScenaLogo.h"
#include "Nogee/Editeur/NogeeVue3D.h"

#include "Noge/ECS/Components/Core/NkTag.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "Noge/Layers/NkEngineLayer.h"

#include <cmath>

namespace nkentseu {
	namespace nkscena {

		using namespace editorkit;
		using namespace ecs;
		using math::NkMat4f;
		using math::NkVec3f;
		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			constexpr float32 PI = 3.14159265f;

			NkVec3f Normer(const NkVec3f &v) {
				const float32 l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
				return l > 1.0e-6f ? NkVec3f{v.x / l, v.y / l, v.z / l} : NkVec3f{0.f, 0.f, 0.f};
			}

			struct NkClip {
					float32 x, y, z, w;
			};

			NkClip VersClip(const NkMat4f &m, const NkVec3f &p) {
				return NkClip{m[0][0] * p.x + m[1][0] * p.y + m[2][0] * p.z + m[3][0], m[0][1] * p.x + m[1][1] * p.y + m[2][1] * p.z + m[3][1],
							  m[0][2] * p.x + m[1][2] * p.y + m[2][2] * p.z + m[3][2], m[0][3] * p.x + m[1][3] * p.y + m[2][3] * p.z + m[3][3]};
			}

			float32 DistanceSegment(const NkVec2 &p, const NkVec2 &a, const NkVec2 &b) {
				const float32 dx = b.x - a.x, dy = b.y - a.y;
				const float32 l2 = dx * dx + dy * dy;
				float32 t = l2 > 1.0e-6f ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / l2 : 0.f;
				t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
				const float32 qx = a.x + dx * t - p.x, qy = a.y + dy * t - p.y;
				return std::sqrt(qx * qx + qy * qy);
			}

			/// Un segment 3D a l'ecran, coupe au plan proche (le patron de la grille de Nogee).
			void Segment3D(nkgui::NkGuiDrawList &dl, const NkMat4f &m, const NkRect &v, const NkVec3f &a, const NkVec3f &b,
						   const NkColor &col, float32 e) {
				NkClip ca = VersClip(m, a);
				NkClip cb = VersClip(m, b);
				const float32 proche = 0.05f;
				if (ca.w < proche && cb.w < proche) {
					return;
				}
				if (ca.w < proche || cb.w < proche) {
					const float32 t = (proche - ca.w) / (cb.w - ca.w);
					const NkClip q{ca.x + (cb.x - ca.x) * t, ca.y + (cb.y - ca.y) * t, ca.z + (cb.z - ca.z) * t, proche};
					if (ca.w < proche) {
						ca = q;
					} else {
						cb = q;
					}
				}
				const NkVec2 pa{v.x + (ca.x / ca.w * 0.5f + 0.5f) * v.w, v.y + (1.f - (ca.y / ca.w * 0.5f + 0.5f)) * v.h};
				const NkVec2 pb{v.x + (cb.x / cb.w * 0.5f + 0.5f) * v.w, v.y + (1.f - (cb.y / cb.w * 0.5f + 0.5f)) * v.h};
				dl.AddLine(pa, pb, col, e);
			}
		} // namespace

		// =====================================================================
		// LA GEOMETRIE DE L'ECRAN
		// =====================================================================
		bool NkScenaInterface::Projeter(const NkVec3f &p, NkVec2 &s) const noexcept {
			if (mHote.moteur == nullptr) {
				return false;
			}
			const NkMat4f &m = mHote.moteur->GetRenderSystem().GetViewProjection();
			const NkClip c = VersClip(m, p);
			if (c.w <= 1.0e-4f) {
				return false;
			}
			const NkRect &v = plan.viseur;
			s.x = v.x + (c.x / c.w * 0.5f + 0.5f) * v.w;
			s.y = v.y + (1.f - (c.y / c.w * 0.5f + 0.5f)) * v.h;
			return true;
		}

		NkEntityId NkScenaInterface::Prendre(const NkVec2 &s) const {
			const nogee::NogeeModele &sc = M().scene;
			NkEntityId meilleur = NkEntityId::Invalid();
			float32 aire = 1.0e30f;
			for (uint32 i = 0; i < mEntites.Size(); ++i) {
				const NkEntityId id = mEntites[i];
				if (sc.EstVerrouille(id) || sc.EstCache(id) || !sc.EstActive(id)) {
					continue;
				}
				NkVec3f mini, maxi;
				if (!sc.Boite(id, mini, maxi)) {
					continue;
				}
				float32 x0 = 1.0e30f, y0 = 1.0e30f, x1 = -1.0e30f, y1 = -1.0e30f;
				bool vu = true;
				for (int32 k = 0; k < 8 && vu; ++k) {
					const NkVec3f p{(k & 1) ? maxi.x : mini.x, (k & 2) ? maxi.y : mini.y, (k & 4) ? maxi.z : mini.z};
					NkVec2 e;
					vu = Projeter(p, e);
					x0 = e.x < x0 ? e.x : x0;
					y0 = e.y < y0 ? e.y : y0;
					x1 = e.x > x1 ? e.x : x1;
					y1 = e.y > y1 ? e.y : y1;
				}
				if (!vu || s.x < x0 || s.x > x1 || s.y < y0 || s.y > y1) {
					continue;
				}
				const float32 a = (x1 - x0) * (y1 - y0);
				if (a < aire) {
					aire = a;
					meilleur = id;
				}
			}
			return meilleur;
		}

		// =====================================================================
		// LA GRILLE DU SOL
		// =====================================================================
		void NkScenaInterface::Grille(nkgui::NkGuiDrawList &dl) {
			if (mHote.moteur == nullptr) {
				return;
			}
			const NkMat4f &m = mHote.moteur->GetRenderSystem().GetViewProjection();
			const NkRect &v = plan.viseur;
			NkColor fine = pal.texte;
			fine.a = 26;
			NkColor forte = pal.texte;
			forte.a = 52;
			const int32 N = 20;
			const float32 n = static_cast<float32>(N);
			for (int32 i = -N; i <= N; ++i) {
				const float32 f = static_cast<float32>(i);
				const NkColor &col = (i % 5 == 0) ? forte : fine;
				if (i != 0) {
					Segment3D(dl, m, v, NkVec3f{f, 0.f, -n}, NkVec3f{f, 0.f, n}, col, 1.f);
					Segment3D(dl, m, v, NkVec3f{-n, 0.f, f}, NkVec3f{n, 0.f, f}, col, 1.f);
				}
			}
			NkColor ax = pal.axeX;
			ax.a = 170;
			NkColor az = pal.axeZ;
			az.a = 170;
			Segment3D(dl, m, v, NkVec3f{-n, 0.f, 0.f}, NkVec3f{n, 0.f, 0.f}, ax, 1.5f);
			Segment3D(dl, m, v, NkVec3f{0.f, 0.f, -n}, NkVec3f{0.f, 0.f, n}, az, 1.5f);
		}

		// =====================================================================
		// LES CAMERAS DE LA SCENE : la pyramide de vision, et celle du plan
		// =====================================================================
		void NkScenaInterface::Cameras(NkFamilleCtx &c, nkgui::NkGuiDrawList &dl) {
			if (mHote.moteur == nullptr) {
				return;
			}
			NkScenaModele &m = M();
			NkWorld &w = m.scene.Monde();
			const NkMat4f &vp = mHote.moteur->GetRenderSystem().GetViewProjection();
			const NkEntityId filme = mHote.moteur->GetRenderSystem().GetActiveCamera();
			const NkEntityId duPlan = m.CameraDuPlan(m.frise.cursor);
			const NkColor antenne(kNkScenaAccent);
			const float32 aspect = mHote.vue != nullptr && mHote.vue->Hauteur() > 0u
									   ? static_cast<float32>(mHote.vue->Largeur()) / static_cast<float32>(mHote.vue->Hauteur())
									   : 16.f / 9.f;
			w.Query<const NkCameraComponent, const NkTransform>().ForEach([&](NkEntityId id, const NkCameraComponent &cam,
																				 const NkTransform &tf) {
				if (id == filme || m.scene.EstEditeur(id)) {
					return; // on ne dessine pas la camera par laquelle on regarde
				}
				const bool estPlan = id == duPlan;
				const bool choisie = m.scene.SelectionValide() && m.scene.selection == id;
				NkColor col = estPlan ? antenne : (choisie ? pal.selection : pal.texte);
				col.a = estPlan || choisie ? 255 : 170;
				const NkVec3f o = tf.GetWorldPosition();
				const NkVec3f av = tf.GetWorldForward();
				const NkVec3f haut = tf.GetWorldUp();
				const NkVec3f dr = Normer(NkVec3f{av.y * haut.z - av.z * haut.y, av.z * haut.x - av.x * haut.z, av.x * haut.y - av.y * haut.x});
				// La pyramide : son sommet a la camera, sa base a 1,2 m devant.
				const float32 d = 1.2f;
				const float32 demiH = d * std::tan(cam.fovDeg * 0.5f * PI / 180.f);
				const float32 demiW = demiH * aspect;
				NkVec3f coins[4];
				for (int32 k = 0; k < 4; ++k) {
					const float32 sx = (k == 0 || k == 3) ? -1.f : 1.f;
					const float32 sy = (k < 2) ? 1.f : -1.f;
					coins[k] = NkVec3f{o.x + av.x * d + dr.x * demiW * sx + haut.x * demiH * sy,
									   o.y + av.y * d + dr.y * demiW * sx + haut.y * demiH * sy,
									   o.z + av.z * d + dr.z * demiW * sx + haut.z * demiH * sy};
				}
				const float32 e = estPlan ? 2.f : 1.3f;
				for (int32 k = 0; k < 4; ++k) {
					Segment3D(dl, vp, plan.viseur, o, coins[k], col, e);
					Segment3D(dl, vp, plan.viseur, coins[k], coins[(k + 1) % 4], col, e);
				}
				// Le petit triangle du « haut » de l'image (comme les cameras d'UE5).
				const NkVec3f h0 = NkVec3f{(coins[0].x + coins[1].x) * 0.5f, (coins[0].y + coins[1].y) * 0.5f, (coins[0].z + coins[1].z) * 0.5f};
				const NkVec3f h1 = NkVec3f{h0.x + haut.x * demiH * 0.5f, h0.y + haut.y * demiH * 0.5f, h0.z + haut.z * demiH * 0.5f};
				Segment3D(dl, vp, plan.viseur, coins[0], h1, col, e);
				Segment3D(dl, vp, plan.viseur, coins[1], h1, col, e);
				NkVec2 s;
				if (Projeter(o, s)) {
					const ecs::NkName *n = w.Get<ecs::NkName>(id);
					const NkString lib = estPlan ? NkString::Format("%s  ·  PLAN", n != nullptr ? n->value : "Caméra")
											  : NkString(n != nullptr ? n->value : "Caméra");
					NkFamilleTexte(dl, c.petite, s.x + 8.f, s.y - 14.f, lib.CStr(), col);
				}
			});
		}

		// =====================================================================
		// LE CADRE DU PLAN (la vue regarde PAR la camera du plan)
		// =====================================================================
		void NkScenaInterface::CadreDuPlan(NkFamilleCtx &c, nkgui::NkGuiDrawList &dl) {
			NkScenaModele &m = M();
			const NkRect &v = plan.viseur;
			const NkString nom = m.PlanA(m.frise.cursor);
			const bool actif = m.CameraDuPlan(m.frise.cursor).IsValid();
			const NkColor antenne(kNkScenaAccent);
			if (actif) {
				// Le LISERE ROUGE « antenne » : ce qui passe a l'image (design/02 §3.2).
				dl.AddRect(NkRect{v.x + 1.f, v.y + 1.f, v.w - 2.f, v.h - 2.f}, antenne, 2.f);
			}
			const NkString texte = actif ? NkString::Format("PLAN  ·  %s", nom.CStr())
										 : NkString("Aucun plan à cet instant : la vue reste libre");
			const float32 w = NkFamilleLargeur(c.police, texte.CStr()) + 16.f;
			const NkRect b{v.x + 10.f, v.y + 10.f, w, 22.f};
			NkColor fond = actif ? antenne : pal.entete;
			fond.a = 230;
			dl.AddRectFilled(b, fond, 2.f);
			NkFamilleTexte(dl, c.police, b.x + 8.f, b.y + 3.f, texte.CStr(), actif ? NkColor{255, 255, 255, 255} : pal.texte);
		}

		// =====================================================================
		// LE GIZMO (repris de Nogee : deplacer, tourner, echelle)
		// =====================================================================
		bool NkScenaInterface::Gizmo(NkFamilleCtx &c, nkgui::NkGuiDrawList &dl, bool souris) {
			NkScenaModele &m = M();
			nogee::NogeeModele &sc = m.scene;
			const nkgui::NkGuiInput &in = c.ctx.input;
			if (mOutil == 0 || !sc.SelectionValide() || sc.EstVerrouille(sc.selection) || m.frise.playing) {
				mAxeTenu = -1;
				return false;
			}
			NkTransform *tf = sc.Monde().Get<NkTransform>(sc.selection);
			if (tf == nullptr) {
				return false;
			}
			const NkVec3f C = tf->worldPosition;
			NkVec2 S;
			if (!Projeter(C, S)) {
				return false;
			}
			NkVec3f axes[3] = {NkVec3f{1.f, 0.f, 0.f}, NkVec3f{0.f, 1.f, 0.f}, NkVec3f{0.f, 0.f, 1.f}};
			if (mOutil == 3) {
				const NkMat4f &wm = tf->worldMatrix;
				axes[0] = Normer(NkVec3f{wm[0][0], wm[0][1], wm[0][2]});
				axes[1] = Normer(NkVec3f{wm[1][0], wm[1][1], wm[1][2]});
				axes[2] = Normer(NkVec3f{wm[2][0], wm[2][1], wm[2][2]});
			}
			const float32 L = sc.orbite.distance * 0.16f;
			const NkColor couleurs[3] = {pal.axeX, pal.axeY, pal.axeZ};
			const NkColor survolCol{255, 220, 60, 255};
			NkVec2 bouts[3];
			bool vus[3];
			NkVec2 anneaux[3][33];
			for (int32 k = 0; k < 3; ++k) {
				vus[k] = Projeter(NkVec3f{C.x + axes[k].x * L, C.y + axes[k].y * L, C.z + axes[k].z * L}, bouts[k]);
				if (mOutil == 2) {
					const NkVec3f u = axes[(k + 1) % 3];
					const NkVec3f w = axes[(k + 2) % 3];
					for (int32 j = 0; j <= 32; ++j) {
						const float32 t = 2.f * PI * static_cast<float32>(j) / 32.f;
						const float32 cs = std::cos(t) * L, sn = std::sin(t) * L;
						if (!Projeter(NkVec3f{C.x + u.x * cs + w.x * sn, C.y + u.y * cs + w.y * sn, C.z + u.z * cs + w.z * sn}, anneaux[k][j])) {
							vus[k] = false;
						}
					}
				}
			}
			int32 survole = -1;
			if (souris && mAxeTenu < 0) {
				float32 meilleur = 7.f;
				for (int32 k = 0; k < 3; ++k) {
					if (!vus[k]) {
						continue;
					}
					float32 d = 1.0e9f;
					if (mOutil == 2) {
						for (int32 j = 0; j < 32; ++j) {
							const float32 dj = DistanceSegment(in.mousePos, anneaux[k][j], anneaux[k][j + 1]);
							d = dj < d ? dj : d;
						}
					} else {
						d = DistanceSegment(in.mousePos, S, bouts[k]);
					}
					if (d < meilleur) {
						meilleur = d;
						survole = k;
					}
				}
				const float32 dc = std::sqrt((in.mousePos.x - S.x) * (in.mousePos.x - S.x) + (in.mousePos.y - S.y) * (in.mousePos.y - S.y));
				if (mOutil == 3 && dc < 8.f) {
					survole = 3;
				}
			}
			for (int32 k = 0; k < 3; ++k) {
				if (!vus[k]) {
					continue;
				}
				const bool chaud = survole == k || mAxeTenu == k;
				const NkColor col = chaud ? survolCol : couleurs[k];
				if (mOutil == 2) {
					dl.AddPolyline(anneaux[k], 33, col, chaud ? 3.f : 2.f, false);
					continue;
				}
				dl.AddLine(S, bouts[k], col, chaud ? 3.5f : 2.5f);
				const float32 dx = bouts[k].x - S.x, dy = bouts[k].y - S.y;
				const float32 l = std::sqrt(dx * dx + dy * dy);
				if (l < 1.f) {
					continue;
				}
				const NkVec2 u{dx / l, dy / l};
				const NkVec2 n{-u.y, u.x};
				if (mOutil == 1) {
					const NkVec2 pointe{bouts[k].x + u.x * 12.f, bouts[k].y + u.y * 12.f};
					dl.AddTriangleFilled(pointe, NkVec2{bouts[k].x + n.x * 5.f, bouts[k].y + n.y * 5.f},
										 NkVec2{bouts[k].x - n.x * 5.f, bouts[k].y - n.y * 5.f}, col);
				} else {
					dl.AddRectFilled(NkRect{bouts[k].x - 4.5f, bouts[k].y - 4.5f, 9.f, 9.f}, col, 1.f);
				}
			}
			dl.AddCircleFilled(S, mOutil == 3 ? 5.f : 3.5f, (survole == 3 || mAxeTenu == 3) ? survolCol : pal.texte);
			if (survole >= 0 && in.mouseClicked[0]) {
				mAxeTenu = survole;
				mDepartPos = tf->localPosition;
				mDepartEchelle = tf->localScale;
				mDepartRot = tf->localRotation;
				mCumul = 0.f;
				mSourisAvant = in.mousePos;
				return true;
			}
			if (mAxeTenu < 0) {
				return survole >= 0;
			}
			if (!in.mouseDown[0]) {
				mAxeTenu = -1;
				return true;
			}
			const NkVec2 d{in.mousePos.x - mSourisAvant.x, in.mousePos.y - mSourisAvant.y};
			const int32 k = mAxeTenu;
			if (mOutil == 2 && k < 3) {
				const float32 a0 = std::atan2(mSourisAvant.y - S.y, mSourisAvant.x - S.x);
				const float32 a1 = std::atan2(in.mousePos.y - S.y, in.mousePos.x - S.x);
				float32 da = a1 - a0;
				da = da > PI ? da - 2.f * PI : (da < -PI ? da + 2.f * PI : da);
				const NkTransform *te = sc.Monde().Get<NkTransform>(sc.cameraEditeur);
				const NkVec3f cam = te != nullptr ? te->worldPosition : NkVec3f{0.f, 0.f, 10.f};
				const NkVec3f versCam = Normer(NkVec3f{cam.x - C.x, cam.y - C.y, cam.z - C.z});
				const float32 face = versCam.x * axes[k].x + versCam.y * axes[k].y + versCam.z * axes[k].z;
				mCumul += (face >= 0.f ? -da : da) * 57.2957795f;
				const math::NkQuatf q = k == 0	 ? math::NkQuatf::RotateX(math::NkAngle(mCumul))
										: k == 1 ? math::NkQuatf::RotateY(math::NkAngle(mCumul))
												 : math::NkQuatf::RotateZ(math::NkAngle(mCumul));
				tf->localRotation = q * mDepartRot;
			} else if (k < 3) {
				const NkVec2 e{bouts[k].x - S.x, bouts[k].y - S.y};
				const float32 e2 = e.x * e.x + e.y * e.y;
				if (e2 > 1.f) {
					const float32 t = (d.x * e.x + d.y * e.y) / e2;
					if (mOutil == 1) {
						mCumul += t * L;
						tf->localPosition = NkVec3f{mDepartPos.x + axes[k].x * mCumul, mDepartPos.y + axes[k].y * mCumul,
													mDepartPos.z + axes[k].z * mCumul};
					} else {
						mCumul += t;
						NkVec3f s = mDepartEchelle;
						float32 *cible = k == 0 ? &s.x : (k == 1 ? &s.y : &s.z);
						*cible = math::NkMax(0.01f, *cible * (1.f + mCumul));
						tf->localScale = s;
					}
				}
			} else {
				mCumul += d.x * 0.01f;
				const float32 f = math::NkMax(0.01f, 1.f + mCumul);
				tf->localScale = NkVec3f{mDepartEchelle.x * f, mDepartEchelle.y * f, mDepartEchelle.z * f};
			}
			tf->worldDirty = true;
			mSourisAvant = in.mousePos;
			return true;
		}

		// =====================================================================
		// LA BARRE FLOTTANTE
		// =====================================================================
		void NkScenaInterface::BarreFlottante(NkFamilleCtx &c) {
			NkScenaModele &m = M();
			auto Outil = [&](NkFamilleIconeBarre ic, int32 o, const char *bulle) {
				NkFamilleElementBarre e;
				e.icone = ic;
				e.enfonce = mOutil == o;
				e.bulle = NkString(bulle);
				return e;
			};
			NkFamilleElementBarre el[6];
			el[0] = Outil(NkFamilleIconeBarre::Selection, 0, "Sélection (Q)");
			el[1] = Outil(NkFamilleIconeBarre::Deplacer, 1, "Déplacer (W)");
			el[2] = Outil(NkFamilleIconeBarre::Tourner, 2, "Tourner (E)");
			el[3] = Outil(NkFamilleIconeBarre::Echelle, 3, "Échelle (R)");
			el[4].icone = NkFamilleIconeBarre::Grille;
			el[4].enfonce = mVoirGrille;
			el[4].separateur = true;
			el[4].bulle = NkString("Grille du sol");
			el[5].icone = NkFamilleIconeBarre::Camera;
			el[5].enfonce = m.vueCamera;
			el[5].bulle = NkString(m.vueCamera ? "Vue par la caméra du plan (0) — cliquer : vue libre"
											   : "Vue libre (0) — cliquer : regarder par la caméra du plan");
			const NkFamilleBarreFlottanteResultat r = NkFamilleBarreFlottante(c, plan.viseur, el, 6, menus.Ouvert());
			mBarre = r.barre;
			if (r.clic >= 0 && r.clic < 4) {
				Executer(SCENA_A_OUTIL + r.clic);
			} else if (r.clic == 4) {
				Executer(SCENA_A_GRILLE);
			} else if (r.clic == 5) {
				Executer(SCENA_A_VUE_CAMERA);
			}
		}

		// =====================================================================
		// LE VISEUR
		// =====================================================================
		void NkScenaInterface::Vue(NkFamilleCtx &c) {
			NkScenaModele &m = M();
			nogee::NogeeModele &sc = m.scene;
			auto &dl = c.ctx.dl;
			nkgui::NkGuiInput &in = c.ctx.input;
			const NkRect &v = plan.viseur;
			if (plan.vue.w < 8.f || plan.vue.h < 8.f) {
				return;
			}
			m.scene.Entites(mEntites);
			const bool rendu = mHote.rendu != nullptr && mHote.rendu->actif;
			// ── La barre de vue ───────────────────────────────────────────────
			{
				const NkFamilleBoutonVue boutons[3] = {{"Cadrer", false}, {"Grille", mVoirGrille}, {"Caméra du plan", m.vueCamera}};
				const NkString plan0 = m.PlanA(m.frise.cursor);
				const NkString reperes = NkString::Format(
					"Perspective   ·   %ux%u   ·   %s", mHote.vue != nullptr ? mHote.vue->Largeur() : 0u,
					mHote.vue != nullptr ? mHote.vue->Hauteur() : 0u,
					rendu ? "RENDU EN COURS"
					: m.vueCamera && m.CameraDuPlan(m.frise.cursor).IsValid() ? NkString::Format("caméra du plan : %s", plan0.CStr()).CStr()
																			  : "caméra d'édition");
				const int32 k = NkFamilleBarreVue(c, plan.barreVue, boutons, 3, reperes.CStr());
				if (k == 0) {
					Executer(SCENA_A_CADRER);
				} else if (k == 1) {
					Executer(SCENA_A_GRILLE);
				} else if (k == 2) {
					Executer(SCENA_A_VUE_CAMERA);
				}
			}
			// ── L'image de la vue 3D (pendant un rendu : a la taille de la SORTIE,
			//    encadree dans le viseur sans la deformer) ─────────────────────
			dl.AddRectFilled(v, NkColor{33, 36, 41, 255});
			const bool vuePrete = mHote.vue != nullptr && mHote.vue->Pret() && mGenerationVue != 0u;
			NkRect image = v;
			if (vuePrete && rendu && mHote.vue->Hauteur() > 0u) {
				const float32 a = static_cast<float32>(mHote.vue->Largeur()) / static_cast<float32>(mHote.vue->Hauteur());
				const float32 w = v.h * a <= v.w ? v.h * a : v.w;
				const float32 h = w / a;
				image = NkRect{v.x + (v.w - w) * 0.5f, v.y + (v.h - h) * 0.5f, w, h};
			}
			if (vuePrete) {
				// Cible OpenGL : origine en BAS a gauche, d'ou les UV retournees (Nogee).
				dl.AddImage(kTexVue, image, NkVec2{0.f, 1.f}, NkVec2{1.f, 0.f}, NkColor{255, 255, 255, 255});
			} else {
				NkFamilleTexteCentre(dl, c.police, v.x + v.w * 0.5f, v.y + v.h * 0.45f, "La vue 3D n'est pas encore prête", pal.attenue);
			}
			dl.PushClipRect(v, true);
			const bool parLaCamera = m.vueCamera && m.CameraDuPlan(m.frise.cursor).IsValid();
			if (rendu) {
				const NkString t = NkString::Format("Rendu  %d / %d  vers  %s", mHote.rendu->image, mHote.rendu->total,
													mHote.rendu->dossier.CStr());
				const NkRect b{v.x + 10.f, v.y + 10.f, NkFamilleLargeur(c.police, t.CStr()) + 16.f, 22.f};
				dl.AddRectFilled(b, NkColor(kNkScenaAccent), 2.f);
				NkFamilleTexte(dl, c.police, b.x + 8.f, b.y + 3.f, t.CStr(), NkColor{255, 255, 255, 255});
				dl.PopClipRect();
				return;
			}
			if (mVoirGrille && !parLaCamera) {
				Grille(dl);
			}
			Cameras(c, dl);
			const bool dansVue = NkFamilleDans(v, in.mousePos) && !NkFamilleDans(mBarre, in.mousePos);
			const bool gizmo = Gizmo(c, dl, dansVue && !mOrbite && !mPan);
			if (m.vueCamera) {
				CadreDuPlan(c, dl);
			}
			const NkEntityId filme = mHote.moteur != nullptr && mHote.moteur->GetRenderSystem().GetActiveCamera().IsValid()
										 ? mHote.moteur->GetRenderSystem().GetActiveCamera()
										 : sc.cameraEditeur;
			if (const NkCameraComponent *cam = sc.Monde().Get<NkCameraComponent>(filme)) {
				const NkMat4f &vm = cam->viewMatrix;
				NkVec2 axes[3];
				for (int32 k = 0; k < 3; ++k) {
					const float32 dx = k == 0 ? 1.f : 0.f, dy = k == 1 ? 1.f : 0.f, dz = k == 2 ? 1.f : 0.f;
					const float32 vx = vm[0][0] * dx + vm[1][0] * dy + vm[2][0] * dz;
					const float32 vy = vm[0][1] * dx + vm[1][1] * dy + vm[2][1] * dz;
					axes[k] = NkVec2{vx, -vy};
				}
				NkFamilleRepereAxes(c, dl, v, axes, 3);
			}
			dl.PopClipRect();
			BarreFlottante(c);

			// ── La camera d'edition (en vue libre) ────────────────────────────
			if (!parLaCamera) {
				if (dansVue && in.wheel != 0.f) {
					sc.orbite.distance *= std::pow(0.88f, in.wheel);
				}
				if (dansVue && in.mouseClicked[1]) {
					mOrbite = true;
					mSourisAvant = in.mousePos;
					mGlisseDroit = 0.f;
				}
				if (dansVue && in.mouseClicked[2]) {
					mPan = true;
					mSourisAvant = in.mousePos;
				}
			}
			const NkVec2 d{in.mousePos.x - mSourisAvant.x, in.mousePos.y - mSourisAvant.y};
			if (mOrbite) {
				if (in.mouseDown[1]) {
					sc.orbite.lacetDeg -= d.x * 0.3f;
					sc.orbite.tangageDeg += d.y * 0.3f;
					mGlisseDroit += std::fabs(d.x) + std::fabs(d.y);
					mSourisAvant = in.mousePos;
				} else {
					mOrbite = false;
					if (mGlisseDroit < 4.f && NkFamilleDans(v, in.mousePos)) {
						const NkEntityId sous = Prendre(in.mousePos);
						if (sous.IsValid()) {
							sc.Choisir(sous);
							m.choix = NkScenaChoix::Entite;
							menus.Ouvrir(SCENA_MENU_CTX_ENTITE, NkRect{in.mousePos.x, in.mousePos.y, 0.f, 0.f});
						} else {
							mTempsMenuPlan = m.frise.cursor;
							menus.Ouvrir(SCENA_MENU_PISTE_PLUS, NkRect{in.mousePos.x, in.mousePos.y, 0.f, 0.f});
						}
					}
				}
			}
			if (mPan) {
				if (in.mouseDown[2]) {
					const float32 lacet = sc.orbite.lacetDeg * 0.01745329f;
					const float32 k = sc.orbite.distance * 0.0016f;
					const NkVec3f droite{std::cos(lacet), 0.f, -std::sin(lacet)};
					sc.orbite.pivot.x -= droite.x * d.x * k;
					sc.orbite.pivot.z -= droite.z * d.x * k;
					sc.orbite.pivot.y += d.y * k;
					mSourisAvant = in.mousePos;
				} else {
					mPan = false;
				}
			}
			if (dansVue && in.mouseClicked[0] && !gizmo && mAxeTenu < 0) {
				const NkEntityId sous = Prendre(in.mousePos);
				if (sous.IsValid()) {
					sc.Choisir(sous);
					m.choix = NkScenaChoix::Entite;
				} else {
					sc.Deselectionner();
				}
			}
			sc.PoserCameraEditeur();
		}

	} // namespace nkscena
} // namespace nkentseu
