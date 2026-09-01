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
				dl.AddTriangleFilled(c0, c1, c2, col);
				dl.AddTriangleFilled(c0, c2, c3, col);
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
