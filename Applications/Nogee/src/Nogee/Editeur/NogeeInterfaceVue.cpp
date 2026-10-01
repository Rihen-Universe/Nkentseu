// -----------------------------------------------------------------------------
// @File    NogeeInterfaceVue.cpp
// @Brief   Le VISEUR de Nogee : la barre de vue, l'image de la vue 3D, la grille
//          du sol, la selection (prendre a la souris), le GIZMO (deplacer,
//          tourner, echelle), la camera d'edition (clic droit + souris, clic
//          milieu, molette), la barre flottante et le repere d'axes.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// Les GESTES sont ceux d'UE5 et d'UnkenyEditor : clic gauche = choisir, glisser
// une poignee du gizmo = transformer, clic droit tenu + souris = tourner autour,
// clic droit simple = menu contextuel, clic milieu = glisser la vue, molette =
// avancer / reculer, F = cadrer. Le lisere ORANGE de la selection est rendu
// par le moteur lui-meme (NkRender3D::SubmitSelection, NogeeEditeurApp).
// -----------------------------------------------------------------------------

#include "Nogee/Editeur/NogeeInterface.h"
#include "Nogee/Editeur/NogeeVue3D.h"

#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "Noge/Layers/NkEngineLayer.h"

#include <cmath>

namespace nkentseu {
	namespace nogee {

		using namespace editorkit;
		using namespace ecs;
		using math::NkVec3f;
		using math::NkMat4f;
		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			constexpr float32 PI = 3.14159265f;

			NkVec3f Normer(const NkVec3f &v) {
				const float32 l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
				return l > 1.0e-6f ? NkVec3f{v.x / l, v.y / l, v.z / l} : NkVec3f{0.f, 0.f, 0.f};
			}

			float32 Accrocher(float32 v, float32 pas, bool actif) {
				return actif && pas > 0.f ? std::floor(v / pas + 0.5f) * pas : v;
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
		} // namespace

		// =====================================================================
		// LA GEOMETRIE DE L'ECRAN
		// =====================================================================
		bool NogeeInterface::Projeter(const NkVec3f &p, NkVec2 &s) const noexcept {
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

		bool NogeeInterface::RayonSouris(const NkVec2 &s, NkVec3f &o, NkVec3f &d) const noexcept {
			if (mHote.moteur == nullptr || plan.viseur.w < 2.f || plan.viseur.h < 2.f) {
				return false;
			}
			const NkMat4f inv = mHote.moteur->GetRenderSystem().GetViewProjection().Inverse();
			const NkRect &v = plan.viseur;
			const float32 nx = (s.x - v.x) / v.w * 2.f - 1.f;
			const float32 ny = 1.f - (s.y - v.y) / v.h * 2.f;
			// Deux points de profondeur 0 et 1 : sur le rayon quelle que soit la
			// convention de profondeur (OpenGL [-1, 1] ou [0, 1]).
			auto Point = [&](float32 z) {
				const NkClip c = VersClip(inv, NkVec3f{nx, ny, z});
				const float32 w = std::fabs(c.w) > 1.0e-7f ? c.w : 1.0e-7f;
				return NkVec3f{c.x / w, c.y / w, c.z / w};
			};
			const NkVec3f a = Point(0.f);
			const NkVec3f b = Point(1.f);
			o = a;
			d = Normer(NkVec3f{b.x - a.x, b.y - a.y, b.z - a.z});
			return d.x != 0.f || d.y != 0.f || d.z != 0.f;
		}

		NkVec3f NogeeInterface::PointAuSol(const NkVec2 &s) const noexcept {
			NkVec3f o, d;
			const NogeeModele &m = M();
			if (RayonSouris(s, o, d) && d.y < -1.0e-4f) {
				const float32 t = -o.y / d.y;
				if (t > 0.f && t < 500.f) {
					return NkVec3f{o.x + d.x * t, 0.f, o.z + d.z * t};
				}
			}
			return NkVec3f{m.orbite.pivot.x, 0.f, m.orbite.pivot.z};
		}

		NkVec3f NogeeInterface::PointDePose(int32 element, bool auCentre, const NkVec2 &) const noexcept {
			const NogeeModele &m = M();
			const NkRect &v = plan.viseur;
			NkVec3f p = auCentre ? PointAuSol(NkVec2{v.x + v.w * 0.5f, v.y + v.h * 0.5f}) : mPointMenu;
			p.x = Accrocher(p.x, m.pasGrille, m.accrocheGrille);
			p.z = Accrocher(p.z, m.pasGrille, m.accrocheGrille);
			int32 n = 0;
			const NogeeElement *cat = NogeeCatalogue(n);
			const NogeeGenre g = element >= 0 && element < n ? cat[element].genre : NogeeGenre::ActeurVide;
			switch (g) {
				case NogeeGenre::LumiereDirectionnelle:
				case NogeeGenre::LumierePonctuelle:
				case NogeeGenre::LumiereProjecteur: p.y = 3.f; break;
				case NogeeGenre::Camera:            p.y = 2.f; break;
				case NogeeGenre::Sol:               p.y = -0.5f; break;
				case NogeeGenre::Plan:              p.y = 0.01f; break;
				case NogeeGenre::SpherePhysique:    p.y = 3.f; break;
				case NogeeGenre::CubePhysique:      p.y = 2.f; break;
				default:                            p.y = 0.5f; break;
			}
			return p;
		}

		NkEntityId NogeeInterface::Prendre(const NkVec2 &s) const {
			const NogeeModele &m = M();
			NkEntityId meilleur = NkEntityId::Invalid();
			float32 aire = 1.0e30f;
			for (uint32 i = 0; i < mArbreEntites.Size(); ++i) {
				const NkEntityId id = mArbreEntites[i];
				if (m.EstVerrouille(id) || m.EstCache(id) || !m.EstActive(id)) {
					continue;
				}
				NkVec3f mini, maxi;
				if (!m.Boite(id, mini, maxi)) {
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
				// La plus PETITE boite a l'ecran gagne : un cube pose sur le sol se
				// prend avant le sol qui le contient.
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
		void NogeeInterface::Grille(nkgui::NkGuiDrawList &dl) {
			if (mHote.moteur == nullptr) {
				return;
			}
			const NkMat4f &m = mHote.moteur->GetRenderSystem().GetViewProjection();
			const NkRect &v = plan.viseur;
			auto Segment = [&](const NkVec3f &a, const NkVec3f &b, const NkColor &col, float32 e) {
				NkClip ca = VersClip(m, a);
				NkClip cb = VersClip(m, b);
				const float32 proche = 0.05f;
				if (ca.w < proche && cb.w < proche) {
					return;
				}
				// La coupe au plan proche, en coordonnees homogenes.
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
			};
			NkColor fine = pal.texte;
			fine.a = 26;
			NkColor forte = pal.texte;
			forte.a = 52;
			const int32 N = 20;
			for (int32 i = -N; i <= N; ++i) {
				const float32 f = static_cast<float32>(i);
				const NkColor &col = (i % 5 == 0) ? forte : fine;
				if (i != 0) {
					Segment(NkVec3f{f, 0.f, -static_cast<float32>(N)}, NkVec3f{f, 0.f, static_cast<float32>(N)}, col, 1.f);
					Segment(NkVec3f{-static_cast<float32>(N), 0.f, f}, NkVec3f{static_cast<float32>(N), 0.f, f}, col, 1.f);
				}
			}
			// Les axes du monde sur le sol : X rouge, Z bleu (Unreal).
			NkColor ax = pal.axeX;
			ax.a = 170;
			NkColor az = pal.axeZ;
			az.a = 170;
			Segment(NkVec3f{-static_cast<float32>(N), 0.f, 0.f}, NkVec3f{static_cast<float32>(N), 0.f, 0.f}, ax, 1.5f);
			Segment(NkVec3f{0.f, 0.f, -static_cast<float32>(N)}, NkVec3f{0.f, 0.f, static_cast<float32>(N)}, az, 1.5f);
		}

		// =====================================================================
		// LE GIZMO
		// =====================================================================
		bool NogeeInterface::Gizmo(NkFamilleCtx &c, nkgui::NkGuiDrawList &dl, bool souris) {
			NogeeModele &m = M();
			const nkgui::NkGuiInput &in = c.ctx.input;
			if (m.outil == NogeeOutil::Selection || !m.SelectionValide() || m.etat != NogeeEtatJeu::Edition ||
				m.EstVerrouille(m.selection)) {
				mAxeTenu = -1;
				return false;
			}
			NkTransform *tf = m.Monde().Get<NkTransform>(m.selection);
			if (tf == nullptr) {
				return false;
			}
			const NkVec3f C = tf->worldPosition;
			NkVec2 S;
			if (!Projeter(C, S)) {
				return false;
			}
			// Les axes : ceux du monde, ou ceux de l'entite (l'echelle est toujours locale).
			NkVec3f axes[3] = {NkVec3f{1.f, 0.f, 0.f}, NkVec3f{0.f, 1.f, 0.f}, NkVec3f{0.f, 0.f, 1.f}};
			if (m.repereLocal || m.outil == NogeeOutil::Echelle) {
				const NkMat4f &w = tf->worldMatrix;
				axes[0] = Normer(NkVec3f{w[0][0], w[0][1], w[0][2]});
				axes[1] = Normer(NkVec3f{w[1][0], w[1][1], w[1][2]});
				axes[2] = Normer(NkVec3f{w[2][0], w[2][1], w[2][2]});
			}
			const float32 L = m.orbite.distance * 0.16f;
			const NkColor couleurs[3] = {pal.axeX, pal.axeY, pal.axeZ};
			const NkColor survolCol{255, 220, 60, 255};

			// ── Les poignees a l'ecran ─────────────────────────────────────────
			NkVec2 bouts[3];
			bool vus[3];
			NkVec2 anneaux[3][33];
			for (int32 k = 0; k < 3; ++k) {
				vus[k] = Projeter(NkVec3f{C.x + axes[k].x * L, C.y + axes[k].y * L, C.z + axes[k].z * L}, bouts[k]);
				if (m.outil == NogeeOutil::Tourner) {
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
			// ── Le survol ──────────────────────────────────────────────────────
			int32 survole = -1;
			if (souris && mAxeTenu < 0) {
				float32 meilleur = 7.f;
				for (int32 k = 0; k < 3; ++k) {
					if (!vus[k]) {
						continue;
					}
					float32 d = 1.0e9f;
					if (m.outil == NogeeOutil::Tourner) {
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
				if (m.outil == NogeeOutil::Echelle && dc < 8.f) {
					survole = 3; // le centre : l'echelle uniforme
				}
			}
			// ── Le dessin ──────────────────────────────────────────────────────
			for (int32 k = 0; k < 3; ++k) {
				if (!vus[k]) {
					continue;
				}
				const bool chaud = survole == k || mAxeTenu == k;
				const NkColor col = chaud ? survolCol : couleurs[k];
				if (m.outil == NogeeOutil::Tourner) {
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
				if (m.outil == NogeeOutil::Deplacer) {
					const NkVec2 pointe{bouts[k].x + u.x * 12.f, bouts[k].y + u.y * 12.f};
					dl.AddTriangleFilled(pointe, NkVec2{bouts[k].x + n.x * 5.f, bouts[k].y + n.y * 5.f},
										 NkVec2{bouts[k].x - n.x * 5.f, bouts[k].y - n.y * 5.f}, col);
				} else {
					dl.AddRectFilled(NkRect{bouts[k].x - 4.5f, bouts[k].y - 4.5f, 9.f, 9.f}, col, 1.f);
				}
			}
			dl.AddCircleFilled(S, m.outil == NogeeOutil::Echelle ? 5.f : 3.5f,
							   (survole == 3 || mAxeTenu == 3) ? survolCol : pal.texte);

			// ── Le geste ───────────────────────────────────────────────────────
			if (survole >= 0 && in.mouseClicked[0]) {
				m.Retenir();
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
			if (m.outil == NogeeOutil::Tourner && k < 3) {
				// L'angle de la souris autour du centre, d'une image a l'autre.
				const float32 a0 = std::atan2(mSourisAvant.y - S.y, mSourisAvant.x - S.x);
				const float32 a1 = std::atan2(in.mousePos.y - S.y, in.mousePos.x - S.x);
				float32 da = a1 - a0;
				da = da > PI ? da - 2.f * PI : (da < -PI ? da + 2.f * PI : da);
				// Le sens : l'axe vient-il vers la camera ? (l'ecran a y vers le bas)
				NkVec2 b;
				const NkVec3f cam = m.Monde().Get<NkTransform>(m.cameraEditeur) != nullptr
										? m.Monde().Get<NkTransform>(m.cameraEditeur)->worldPosition
										: NkVec3f{0.f, 0.f, 10.f};
				const NkVec3f versCam = Normer(NkVec3f{cam.x - C.x, cam.y - C.y, cam.z - C.z});
				const float32 face = versCam.x * axes[k].x + versCam.y * axes[k].y + versCam.z * axes[k].z;
				(void)b;
				mCumul += (face >= 0.f ? -da : da) * 57.2957795f;
				const float32 deg = Accrocher(mCumul, m.pasAngle, m.accrocheAngle != in.ctrlDown);
				const math::NkQuatf q = k == 0 ? math::NkQuatf::RotateX(math::NkAngle(deg))
										: k == 1 ? math::NkQuatf::RotateY(math::NkAngle(deg))
												 : math::NkQuatf::RotateZ(math::NkAngle(deg));
				tf->localRotation = (m.repereLocal ? mDepartRot * q : q * mDepartRot);
			} else if (k < 3) {
				const NkVec2 e{bouts[k].x - S.x, bouts[k].y - S.y};
				const float32 e2 = e.x * e.x + e.y * e.y;
				if (e2 > 1.f) {
					const float32 t = (d.x * e.x + d.y * e.y) / e2; // en longueurs de poignee
					if (m.outil == NogeeOutil::Deplacer) {
						mCumul += t * L;
						const float32 pas = Accrocher(mCumul, m.pasGrille, m.accrocheGrille != in.ctrlDown);
						tf->localPosition = NkVec3f{mDepartPos.x + axes[k].x * pas, mDepartPos.y + axes[k].y * pas,
													mDepartPos.z + axes[k].z * pas};
					} else {
						mCumul += t;
						const float32 f = 1.f + Accrocher(mCumul, m.pasEchelle, m.accrocheEchelle != in.ctrlDown);
						NkVec3f s = mDepartEchelle;
						float32 *cible = k == 0 ? &s.x : (k == 1 ? &s.y : &s.z);
						*cible = math::NkMax(0.01f, *cible * f);
						tf->localScale = s;
					}
				}
			} else {
				// Le centre de l'echelle : uniforme, en tirant vers la droite.
				mCumul += d.x * 0.01f;
				const float32 f = math::NkMax(0.01f, 1.f + Accrocher(mCumul, m.pasEchelle, m.accrocheEchelle != in.ctrlDown));
				tf->localScale = NkVec3f{mDepartEchelle.x * f, mDepartEchelle.y * f, mDepartEchelle.z * f};
			}
			tf->worldDirty = true;
			m.modifie = true;
			mSourisAvant = in.mousePos;
			return true;
		}

		// =====================================================================
		// LA BARRE FLOTTANTE
		// =====================================================================
		void NogeeInterface::BarreFlottante(NkFamilleCtx &c) {
			NogeeModele &m = M();
			auto Outil = [&](NkFamilleIconeBarre ic, NogeeOutil o, const char *bulle) {
				NkFamilleElementBarre e;
				e.icone = ic;
				e.enfonce = m.outil == o;
				e.bulle = NkString(bulle);
				return e;
			};
			NkFamilleElementBarre el[11];
			el[0] = Outil(NkFamilleIconeBarre::Selection, NogeeOutil::Selection, "Sélection (Q)");
			el[1] = Outil(NkFamilleIconeBarre::Deplacer, NogeeOutil::Deplacer, "Déplacer (W)");
			el[2] = Outil(NkFamilleIconeBarre::Tourner, NogeeOutil::Tourner, "Tourner (E)");
			el[3] = Outil(NkFamilleIconeBarre::Echelle, NogeeOutil::Echelle, "Échelle (R)");
			el[4].icone = m.repereLocal ? NkFamilleIconeBarre::Local : NkFamilleIconeBarre::Monde;
			el[4].separateur = true;
			el[4].bulle = NkString(m.repereLocal ? "Axes de l'entité (local) — cliquer : monde" : "Axes du monde — cliquer : local");
			el[5].icone = NkFamilleIconeBarre::Grille;
			el[5].enfonce = m.accrocheGrille;
			el[5].separateur = true;
			el[5].bulle = NkString("Accrochage des déplacements (Ctrl l'inverse)");
			el[6].texte = NkString::Format("%g m", static_cast<double>(m.pasGrille));
			el[6].enfonce = menus.menu == NOGEE_MENU_PAS_GRILLE;
			el[6].eteint = !m.accrocheGrille;
			el[6].bulle = NkString("Pas de la grille");
			el[7].icone = NkFamilleIconeBarre::Angle;
			el[7].enfonce = m.accrocheAngle;
			el[7].bulle = NkString("Accrochage des rotations (Ctrl l'inverse)");
			el[8].texte = NkString::Format("%g°", static_cast<double>(m.pasAngle));
			el[8].enfonce = menus.menu == NOGEE_MENU_PAS_ANGLE;
			el[8].eteint = !m.accrocheAngle;
			el[8].bulle = NkString("Pas des angles");
			el[9].icone = NkFamilleIconeBarre::PasEchelle;
			el[9].enfonce = m.accrocheEchelle;
			el[9].bulle = NkString("Accrochage des échelles (Ctrl l'inverse)");
			el[10].texte = NkString::Format("x%g", static_cast<double>(m.pasEchelle));
			el[10].enfonce = menus.menu == NOGEE_MENU_PAS_ECHELLE;
			el[10].eteint = !m.accrocheEchelle;
			el[10].bulle = NkString("Pas de l'échelle");
			const NkFamilleBarreFlottanteResultat r = NkFamilleBarreFlottante(c, plan.viseur, el, 11, menus.Ouvert());
			mBarre = r.barre;
			switch (r.clic) {
				case 0: case 1: case 2: case 3: Executer(NOGEE_A_OUTIL + r.clic); break;
				case 4:  Executer(NOGEE_A_REPERE_LOCAL); break;
				case 5:  Executer(NOGEE_A_ACCROCHE_GRILLE); break;
				case 6:  menus.Ouvrir(NOGEE_MENU_PAS_GRILLE, r.rect); break;
				case 7:  Executer(NOGEE_A_ACCROCHE_ANGLE); break;
				case 8:  menus.Ouvrir(NOGEE_MENU_PAS_ANGLE, r.rect); break;
				case 9:  Executer(NOGEE_A_ACCROCHE_ECHELLE); break;
				case 10: menus.Ouvrir(NOGEE_MENU_PAS_ECHELLE, r.rect); break;
				default: break;
			}
		}

		// =====================================================================
		// LE VISEUR
		// =====================================================================
		void NogeeInterface::Vue(NkFamilleCtx &c) {
			NogeeModele &m = M();
			auto &dl = c.ctx.dl;
			nkgui::NkGuiInput &in = c.ctx.input;
			const NkRect &v = plan.viseur;
			if (plan.vue.w < 8.f || plan.vue.h < 8.f) {
				return;
			}
			// ── La barre de vue ───────────────────────────────────────────────
			{
				const NkFamilleBoutonVue boutons[3] = {{"Cadrer", false}, {"Grille", m.voirGrille}, {"Filaire", mFilaire}};
				const NkString reperes = NkString::Format("Perspective   ·   %ux%u   ·   %s",
														  mHote.vue != nullptr ? mHote.vue->Largeur() : 0u,
														  mHote.vue != nullptr ? mHote.vue->Hauteur() : 0u,
														  m.etat == NogeeEtatJeu::Edition ? "caméra d'édition" : "caméra du jeu");
				const int32 k = NkFamilleBarreVue(c, plan.barreVue, boutons, 3, reperes.CStr());
				if (k == 0) {
					Executer(NOGEE_A_CADRER);
				} else if (k == 1) {
					Executer(NOGEE_A_GRILLE);
				} else if (k == 2) {
					Executer(NOGEE_A_FILAIRE);
				}
			}
			// ── L'image de la vue 3D ──────────────────────────────────────────
			dl.AddRectFilled(v, NkColor{33, 36, 41, 255});
			const bool vuePrete = mHote.vue != nullptr && mHote.vue->Pret() && mGenerationVue != 0u;
			if (vuePrete) {
				// Cible OpenGL : origine en BAS a gauche, d'ou les UV retournees
				// (NkAnimaEditor/Panels.h, NKCraft NkModelerUI.h).
				dl.AddImage(kTexVue, v, NkVec2{0.f, 1.f}, NkVec2{1.f, 0.f}, NkColor{255, 255, 255, 255});
			} else {
				NkFamilleTexteCentre(dl, c.police, v.x + v.w * 0.5f, v.y + v.h * 0.45f, "La vue 3D n'est pas encore prête",
									 pal.attenue);
			}
			dl.PushClipRect(v, true);
			if (m.voirGrille && m.etat == NogeeEtatJeu::Edition) {
				Grille(dl);
			}
			const bool dansVue = NkFamilleDans(v, in.mousePos) && !NkFamilleDans(mBarre, in.mousePos);
			const bool gizmo = Gizmo(c, dl, dansVue && !mOrbite && !mPan);
			// Le repere d'axes du coin bas gauche, depuis la camera qui filme.
			const NkEntityId filme = mHote.moteur != nullptr && mHote.moteur->GetRenderSystem().GetActiveCamera().IsValid()
										 ? mHote.moteur->GetRenderSystem().GetActiveCamera()
										 : m.cameraEditeur;
			if (const NkCameraComponent *cam = m.Monde().Get<NkCameraComponent>(filme)) {
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

			// ── La camera d'edition : clic droit tenu + souris, clic milieu, molette ─
			if (dansVue && in.wheel != 0.f) {
				m.orbite.distance *= std::pow(0.88f, in.wheel);
			}
			if (dansVue && in.mouseClicked[1]) {
				mOrbite = true;
				mSourisAvant = in.mousePos;
				mPointMenu = PointAuSol(in.mousePos);
				mGlisseDroit = 0.f;
			}
			if (dansVue && in.mouseClicked[2]) {
				mPan = true;
				mSourisAvant = in.mousePos;
			}
			const NkVec2 d{in.mousePos.x - mSourisAvant.x, in.mousePos.y - mSourisAvant.y};
			if (mOrbite) {
				if (in.mouseDown[1]) {
					m.orbite.lacetDeg -= d.x * 0.3f;
					m.orbite.tangageDeg += d.y * 0.3f;
					mGlisseDroit += std::fabs(d.x) + std::fabs(d.y);
					mSourisAvant = in.mousePos;
				} else {
					mOrbite = false;
					// Un clic droit SANS glisser : le menu de ce qui est dessous.
					if (mGlisseDroit < 4.f && NkFamilleDans(v, in.mousePos)) {
						const NkEntityId sous = Prendre(in.mousePos);
						if (sous.IsValid()) {
							m.Choisir(sous);
							menus.Ouvrir(NOGEE_MENU_CTX_ENTITE, NkRect{in.mousePos.x, in.mousePos.y, 0.f, 0.f});
						} else {
							menus.Ouvrir(NOGEE_MENU_CTX_VIDE, NkRect{in.mousePos.x, in.mousePos.y, 0.f, 0.f});
						}
					}
				}
			}
			if (mPan) {
				if (in.mouseDown[2]) {
					const float32 lacet = m.orbite.lacetDeg * 0.01745329f;
					const float32 k = m.orbite.distance * 0.0016f;
					const NkVec3f droite{std::cos(lacet), 0.f, -std::sin(lacet)};
					m.orbite.pivot.x -= droite.x * d.x * k;
					m.orbite.pivot.z -= droite.z * d.x * k;
					m.orbite.pivot.y += d.y * k;
					mSourisAvant = in.mousePos;
				} else {
					mPan = false;
				}
			}
			// ── Choisir : un clic gauche qui n'est pas pour le gizmo ──────────
			if (dansVue && in.mouseClicked[0] && !gizmo && mAxeTenu < 0) {
				const NkEntityId sous = Prendre(in.mousePos);
				if (sous.IsValid()) {
					m.Choisir(sous);
				} else {
					m.Deselectionner();
				}
			}
			m.PoserCameraEditeur();
		}

	} // namespace nogee
} // namespace nkentseu
