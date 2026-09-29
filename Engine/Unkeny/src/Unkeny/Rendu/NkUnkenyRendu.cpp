// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Rendu/NkUnkenyRendu.cpp
// DESCRIPTION: Le dessin d'une scene 2D. Ne modifie aucun composant.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Rendu/NkUnkenyRendu.h"

#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace unkeny {

		namespace {
			using nkgui::NkColor;
			using nkgui::NkRect;

			NkColor Couleur(uint32 rgba) noexcept {
				return NkColor(static_cast<uint8>((rgba >> 24) & 0xFFu), static_cast<uint8>((rgba >> 16) & 0xFFu),
							   static_cast<uint8>((rgba >> 8) & 0xFFu), static_cast<uint8>(rgba & 0xFFu));
			}

			/// Ce qu'il faut retenir d'une entite pour la dessiner, une fois la
			/// requete finie. On ne garde PAS de pointeur vers les composants :
			/// une requete NKECS peut deplacer les donnees entre archetypes, et
			/// un pointeur retenu pointerait alors ailleurs.
			struct Aplat {
					NkTransform2D t;
					NkSprite2D s;
			};

			bool Chevauche(const NkRect &a, const NkRect &b) noexcept {
				return !(a.x + a.w < b.x || b.x + b.w < a.x || a.y + a.h < b.y || b.y + b.h < a.y);
			}
		} // namespace

		// =====================================================================
		NkStatsRendu NkDessinerScene(nkgui::NkGuiDrawList &dl, NkScene &scene) {
			NkStatsRendu stats;
			const NkVue2D &cam = scene.Camera();
			const NkRect visible = cam.ZoneVisible();

			NkVector<Aplat> aplats;
			scene.Monde().Query<NkTransform2D, NkSprite2D>().ForEach(
				[&](ecs::NkEntityId, NkTransform2D &t, NkSprite2D &s) {
					if (!s.visible) {
						return;
					}
					++stats.entitesVues;

					// Hors-champ, teste en MONDE. Le rayon englobant est genereux
					// (la diagonale), parce qu'une entite tournee deborde de sa
					// taille : un test trop serre fait disparaitre des objets au
					// bord de l'ecran quand ils pivotent — defaut qui ne se voit
					// qu'en mouvement.
					const float32 dx = s.taille.x * math::NkAbs(t.echelle.x);
					const float32 dy = s.taille.y * math::NkAbs(t.echelle.y);
					const float32 r = math::NkSqrt(dx * dx + dy * dy) * 0.5f;
					const NkRect boite{t.position.x - r, t.position.y - r, r * 2.f, r * 2.f};
					if (!Chevauche(boite, visible)) {
						return;
					}
					Aplat a;
					a.t = t;
					a.s = s;
					aplats.PushBack(a);
				});

			// ⚠️ TRI PAR COUCHE. NKECS ne garantit aucun ordre d'iteration ; sans
			// ce tri, l'empilement change quand on ajoute un composant a une
			// entite, et le defaut se presente comme « le personnage passe
			// parfois derriere le decor ». Tri par insertion : les scenes 2D ont
			// des centaines d'entites visibles, pas des millions, et un tri
			// stable garde l'ordre relatif a couche egale.
			for (uint32 i = 1; i < aplats.Size(); ++i) {
				const Aplat cle = aplats[i];
				int32 j = static_cast<int32>(i) - 1;
				while (j >= 0 && aplats[static_cast<uint32>(j)].s.couche > cle.s.couche) {
					aplats[static_cast<uint32>(j + 1)] = aplats[static_cast<uint32>(j)];
					--j;
				}
				aplats[static_cast<uint32>(j + 1)] = cle;
			}

			for (uint32 i = 0; i < aplats.Size(); ++i) {
				const NkTransform2D &t = aplats[i].t;
				const NkSprite2D &s = aplats[i].s;

				// Les quatre coins, en LOCAL puis en monde puis en ecran. Passer
				// par les coins — et non par un rectangle ecran — est ce qui rend
				// la rotation et l'echelle justes du premier coup.
				const float32 w = s.taille.x;
				const float32 h = s.taille.y;
				const float32 ox = -s.pivot.x * w;
				const float32 oy = -s.pivot.y * h;
				const NkVec2f c0 = cam.MondeVersEcran(t.VersMonde(NkVec2f(ox, oy + h)));
				const NkVec2f c1 = cam.MondeVersEcran(t.VersMonde(NkVec2f(ox + w, oy + h)));
				const NkVec2f c2 = cam.MondeVersEcran(t.VersMonde(NkVec2f(ox + w, oy)));
				const NkVec2f c3 = cam.MondeVersEcran(t.VersMonde(NkVec2f(ox, oy)));

				const NkColor col = Couleur(s.couleur);
				// Deux triangles : c'est la seule primitive qui accepte un quad
				// TOURNE. AddRectFilled resterait aligne aux axes et la rotation
				// serait perdue en silence.
				if (s.texId != 0u) {
					// TEXTURE (ajoute le 2026-09-29 : `texId` existait, rien ne le
					// lisait). La couleur TEINTE l'image, blanc = telle quelle.
					// c0 est le coin HAUT-gauche a l'ecran, et le v des UV descend
					// comme les lignes d'une image : (uv0.x, uv0.y) va donc en c0.
					// Un miroir se fait par l'echelle negative du transform, pas
					// en inversant les UV.
					const NkVec2f pts[4] = {c0, c1, c2, c3};
					const NkVec2f uvs[4] = {NkVec2f(s.uv0.x, s.uv0.y), NkVec2f(s.uv1.x, s.uv0.y),
											NkVec2f(s.uv1.x, s.uv1.y), NkVec2f(s.uv0.x, s.uv1.y)};
					dl.AddImagePolygon(s.texId, pts, uvs, 4, col);
				} else {
					dl.AddTriangleFilled(c0, c1, c2, col);
					dl.AddTriangleFilled(c0, c2, c3, col);
				}
				++stats.entitesDessinees;
			}
			return stats;
		}

		// =====================================================================
		void NkDessinerCollisionneurs(nkgui::NkGuiDrawList &dl, NkScene &scene, uint32 couleur) {
			const NkVue2D &cam = scene.Camera();
			const NkColor col = Couleur(couleur);

			scene.Monde().Query<NkTransform2D, NkCollisionneur2D>().ForEach(
				[&](ecs::NkEntityId, NkTransform2D &t, NkCollisionneur2D &c) {
					const NkVec2f centre(t.position.x + c.decalage.x, t.position.y + c.decalage.y);
					const NkVec2f e = cam.MondeVersEcran(centre);
					switch (c.forme) {
						case NkForme2D::NK_CERCLE:
							dl.AddCircle(e, cam.LongueurVersEcran(c.rayon), col, 1.5f);
							break;
						case NkForme2D::NK_CAPSULE: {
							const float32 r = cam.LongueurVersEcran(c.rayon);
							const float32 d = cam.LongueurVersEcran(c.demiTaille.x);
							dl.AddCircle(NkVec2f(e.x - d, e.y), r, col, 1.5f);
							dl.AddCircle(NkVec2f(e.x + d, e.y), r, col, 1.5f);
							dl.AddLine(NkVec2f(e.x - d, e.y - r), NkVec2f(e.x + d, e.y - r), col, 1.5f);
							dl.AddLine(NkVec2f(e.x - d, e.y + r), NkVec2f(e.x + d, e.y + r), col, 1.5f);
							break;
						}
						default: {
							const float32 hw = cam.LongueurVersEcran(c.demiTaille.x);
							const float32 hh = cam.LongueurVersEcran(c.demiTaille.y);
							dl.AddRect(NkRect{e.x - hw, e.y - hh, hw * 2.f, hh * 2.f}, col, 1.5f);
							break;
						}
					}
				});
		}

		// =====================================================================
		void NkDessinerFormes(nkgui::NkGuiDrawList &dl, NkScene &scene, const NkOptionsFormes &o) {
			const NkVue2D &cam = scene.Camera();
			ecs::NkWorld &monde = scene.Monde();
			auto Mix = [](const NkColor &a, const NkColor &b, float32 t) {
				auto m = [t](uint8 x, uint8 y) {
					return static_cast<uint8>(static_cast<float32>(x) + (static_cast<float32>(y) - static_cast<float32>(x)) * t);
				};
				return NkColor(m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a));
			};
			auto Max1 = [](float32 v) { return v > 1.f ? v : 1.f; };
			monde.Query<NkTransform2D, NkCollisionneur2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &t, NkCollisionneur2D &c) {
				const NkCorps2D *corps = monde.Get<NkCorps2D>(id);
				const NkSprite2D *sp = monde.Get<NkSprite2D>(id);
				const bool rigide = corps != nullptr && corps->type == NkTypeCorps::NK_DYNAMIQUE;
				if (sp != nullptr && sp->visible && sp->texId != 0u) {
					return; // texture : c'est NkDessinerScene qui le dessine
				}
				NkColor fond, bord;
				if (rigide) {
					fond = sp != nullptr ? Couleur(sp->couleur) : NkColor(180, 180, 190);
					bord = Mix(fond, NkColor(0, 0, 0), 0.55f);
				} else {
					const uint32 propre = o.couleurDecor != nullptr ? o.couleurDecor(monde, id, o.donnees) : 0u;
					fond = Couleur(propre != 0u ? propre : o.decor);
					bord = propre != 0u ? Mix(fond, NkColor(255, 255, 255), 0.25f) : Couleur(o.decorBord);
				}
				auto E = [&](float32 lx, float32 ly) {
					return cam.MondeVersEcran(t.VersMonde(NkVec2f(lx + c.decalage.x, ly + c.decalage.y)));
				};
				switch (c.forme) {
					case NkForme2D::NK_CERCLE: {
						const NkVec2f e = E(0.f, 0.f);
						const float32 r = cam.LongueurVersEcran(c.rayon);
						dl.AddCircleFilled(e, r, fond);
						if (rigide) {
							// Un reflet et un repere : sans eux, une balle qui roule
							// ne se distingue pas d'une balle qui glisse.
							dl.AddCircleFilled(NkVec2f(e.x - r * 0.3f, e.y - r * 0.3f), r * 0.35f, Mix(fond, NkColor(255, 255, 255), 0.45f));
							dl.AddLine(e, E(c.rayon * 0.85f, 0.f), bord, Max1(r * 0.12f));
						}
						dl.AddCircle(e, r, bord, Max1(r * 0.08f));
						break;
					}
					case NkForme2D::NK_CAPSULE: {
						const NkVec2f a = E(-c.demiTaille.x, 0.f), b = E(c.demiTaille.x, 0.f);
						const float32 r = cam.LongueurVersEcran(c.rayon);
						const NkVec2f d = b - a;
						const float32 l = math::NkSqrt(d.x * d.x + d.y * d.y);
						const NkVec2f n = l > 1.0e-4f ? NkVec2f(-d.y / l, d.x / l) : NkVec2f(0.f, 1.f);
						NkVec2f q[4] = {a + n * r, b + n * r, b - n * r, a - n * r};
						dl.AddConvexPolyFilled(q, 4, fond);
						dl.AddCircleFilled(a, r, fond);
						dl.AddCircleFilled(b, r, fond);
						dl.AddLine(q[0], q[1], bord, 1.2f); // l'arete eclairee
						break;
					}
					default: {
						const float32 hx = c.demiTaille.x, hy = c.demiTaille.y;
						NkVec2f q[4] = {E(-hx, -hy), E(hx, -hy), E(hx, hy), E(-hx, hy)};
						dl.AddConvexPolyFilled(q, 4, fond);
						if (rigide) {
							// Une caisse : un cadre et une traverse.
							const float32 k = 0.78f;
							NkVec2f in[4] = {E(-hx * k, -hy * k), E(hx * k, -hy * k), E(hx * k, hy * k), E(-hx * k, hy * k)};
							const float32 e = Max1(cam.LongueurVersEcran(hx) * 0.08f);
							dl.AddPolyline(in, 4, bord, e, true);
							dl.AddLine(in[0], in[2], bord, e);
							dl.AddPolyline(q, 4, bord, e * 1.2f, true);
						} else {
							dl.AddLine(q[3], q[2], bord, 1.5f); // le dessus
						}
						break;
					}
				}
			});
		}

		// =====================================================================
		void NkDessinerGrille(nkgui::NkGuiDrawList &dl, const NkVue2D &camera, float32 pas, uint32 couleur,
							  uint32 couleurAxes) {
			if (pas <= 0.f) {
				return;
			}
			const NkRect visible = camera.ZoneVisible();
			const NkRect viseur = camera.Viseur();
			const NkColor col = Couleur(couleur);
			const NkColor axes = Couleur(couleurAxes);

			// ⚠️ On borne le nombre de lignes. Degzoome, une grille au pas de 1
			// demanderait des dizaines de milliers de traits : la trame tombe a
			// une image par seconde et on accuse le rendu de la scene.
			const int32 kMaxLignes = 200;
			float32 p = pas;
			while (visible.w / p > static_cast<float32>(kMaxLignes)) {
				p *= 2.f; // on double le pas plutot que de renoncer a la grille
			}

			const float32 x0 = math::NkFloor(visible.x / p) * p;
			for (float32 x = x0; x <= visible.x + visible.w; x += p) {
				const float32 ex = camera.MondeVersEcran(NkVec2f(x, 0.f)).x;
				dl.AddLine(NkVec2f(ex, viseur.y), NkVec2f(ex, viseur.y + viseur.h),
						   math::NkAbs(x) < p * 0.01f ? axes : col, 1.f);
			}
			const float32 y0 = math::NkFloor(visible.y / p) * p;
			for (float32 y = y0; y <= visible.y + visible.h; y += p) {
				const float32 ey = camera.MondeVersEcran(NkVec2f(0.f, y)).y;
				dl.AddLine(NkVec2f(viseur.x, ey), NkVec2f(viseur.x + viseur.w, ey),
						   math::NkAbs(y) < p * 0.01f ? axes : col, 1.f);
			}
		}

	} // namespace unkeny
} // namespace nkentseu
