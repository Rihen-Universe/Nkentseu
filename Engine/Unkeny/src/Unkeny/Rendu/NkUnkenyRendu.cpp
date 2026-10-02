// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Rendu/NkUnkenyRendu.cpp
// DESCRIPTION: Le dessin d'une scene 2D. Ne modifie aucun composant.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Rendu/NkUnkenyRendu.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkEarcut.h"
#include "Unkeny/Maillage/NkUnkenyMaillagePhysique.h"

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
			/// (2026-10-01) Un aplat est un sprite OU une forme : un seul tri.
			/// (2026-10-02) OU un MAILLAGE 2D : son entite (relue apres le tri) et
			/// la place de ses sommets en monde dans `sommets`.
			struct Aplat {
					NkTransform2D t;
					NkSprite2D s;
					int32 forme = -1; ///< indice dans la liste des formes, -1 = sprite
					int32 couche = 0;
					ecs::NkEntityId maillage;	///< valide : un maillage 2D
					uint32 premierSommet = 0u; ///< dans la liste des sommets en monde
			};

			/// La couleur `a` multipliee par `b` (0xRRGGBBAA, canal par canal).
			uint32 Multiplier(uint32 a, uint32 b) noexcept {
				uint32 r = 0u;
				for (int32 k = 0; k < 4; ++k) {
					const int32 s = 24 - 8 * k;
					const uint32 x = ((a >> s) & 0xFFu) * ((b >> s) & 0xFFu) + 127u;
					r |= ((x / 255u) & 0xFFu) << s;
				}
				return r;
			}

			NkColor Opacite(const NkColor &c, float32 o) noexcept {
				const float32 k = o < 0.f ? 0.f : (o > 1.f ? 1.f : o);
				return NkColor(c.r, c.g, c.b, static_cast<uint8>(static_cast<float32>(c.a) * k + 0.5f));
			}

			/// Le dessin d'une forme dont le contour est DEJA en pixels.
			/// `pxParM` : les pixels d'un metre (epaisseurs du trait et du contour).
			void DessinerContourForme(nkgui::NkGuiDrawList &dl, const NkRenduForme2D &f, const NkVec2f *e, uint32 n,
									  bool ferme, float32 pxParM) {
				if (n < 2u) {
					return;
				}
				// 0xRRGGBBAA : le constructeur de math::NkColor (NKMath/NkColor.h).
				const NkColor fond = Opacite(NkColor(f.remplissage), f.opacite);
				const NkColor bord = Opacite(NkColor(f.couleurContour), f.opacite);
				const float32 contour = f.epaisseurContour > 0.f ? math::NkMax(f.epaisseurContour * pxParM, 1.f) : 0.f;
				if (!ferme) {
					// Une LIGNE : son contour d'abord (plus large), puis le trait ;
					// des disques aux points font les bouts et les jointures ronds.
					const float32 trait = math::NkMax(f.epaisseur * pxParM, 1.f);
					for (int32 passe = 0; passe < 2; ++passe) {
						if (passe == 0 && contour <= 0.f) {
							continue;
						}
						const NkColor col = passe == 0 ? bord : fond;
						const float32 ep = passe == 0 ? trait + 2.f * contour : trait;
						if (passe == 1 && !f.rempli) {
							continue;
						}
						dl.AddPolyline(e, static_cast<int32>(n), col, ep, false);
						for (uint32 i = 0; i < n; ++i) {
							dl.AddCircleFilled(e[i], ep * 0.5f, col);
						}
					}
					return;
				}
				if (f.rempli && n >= 3u) {
					if (!NkFormePeutEtreConcave(f)) {
						dl.AddConvexPolyFilled(e, static_cast<int32>(n), fond);
					} else {
						// CONCAVE (etoile, polygone libre) : l'eventail d'un convexe
						// deborderait dans les creux. La porte SANS allocation de NKMath.
						detail::NkEarcutNode<float32> noeuds[NK_FORME_CONTOUR_MAX];
						uint32 idx[(NK_FORME_CONTOUR_MAX - 2u) * 3u];
						const uint32 nbTri = NkEarcutVers<float32>(e, n, noeuds, NK_FORME_CONTOUR_MAX, idx,
																	(NK_FORME_CONTOUR_MAX - 2u) * 3u);
						for (uint32 k = 0; k < nbTri; ++k) {
							dl.AddTriangleFilled(e[idx[k * 3u]], e[idx[k * 3u + 1u]], e[idx[k * 3u + 2u]], fond);
						}
					}
				}
				if (contour > 0.f) {
					dl.AddPolyline(e, static_cast<int32>(n), bord, contour, true);
				}
			}

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
			NkVector<NkRenduForme2D> formes;
			scene.Monde().Query<NkTransform2D, NkSprite2D>().ForEach(
				[&](ecs::NkEntityId id, NkTransform2D &t, NkSprite2D &s) {
					// Une entite ETEINTE (NkUnkenyActif.h) n'est pas rendue.
					if (!s.visible || !scene.EstActive(id)) {
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
					a.couche = s.couche;
					aplats.PushBack(a);
				});
			// Les FORMES 2D (2026-10-01), meme hors-champ, meme tri.
			scene.Monde().Query<NkTransform2D, NkRenduForme2D>().ForEach(
				[&](ecs::NkEntityId id, NkTransform2D &t, NkRenduForme2D &f) {
					if (!f.visible || !scene.EstActive(id)) {
						return;
					}
					++stats.entitesVues;
					const NkVec2f d = NkDemiBoiteForme2D(f);
					const float32 dx = d.x * math::NkAbs(t.echelle.x);
					const float32 dy = d.y * math::NkAbs(t.echelle.y);
					const float32 r = math::NkSqrt(dx * dx + dy * dy);
					const NkRect boite{t.position.x - r, t.position.y - r, r * 2.f, r * 2.f};
					if (!Chevauche(boite, visible)) {
						return;
					}
					Aplat a;
					a.t = t;
					a.forme = static_cast<int32>(formes.Size());
					a.couche = f.couche;
					formes.PushBack(f);
					aplats.PushBack(a);
				});
			// Les MAILLAGES 2D (2026-10-02, R31), meme tri. Le hors-champ se juge sur
			// leurs sommets EN MONDE : en jeu, un eclat parti loin de son entite se
			// dessine toujours la ou il est.
			NkVector<NkVec2f> sommets;
			scene.Monde().Query<NkTransform2D, NkMaillage2D>().ForEach(
				[&](ecs::NkEntityId id, NkTransform2D &t, NkMaillage2D &m) {
					if (!m.visible || m.nbTriangles == 0u || !scene.EstActive(id)) {
						return;
					}
					++stats.entitesVues;
					const uint32 premier = static_cast<uint32>(sommets.Size());
					sommets.Resize(premier + m.nbSommets);
					NkMaillagePositionsMonde(scene, t, m, sommets.Data() + premier);
					NkVec2f mn = sommets[premier], mx = sommets[premier];
					for (uint32 i = 1; i < m.nbSommets; ++i) {
						const NkVec2f &p = sommets[premier + i];
						mn = NkVec2f(math::NkMin(mn.x, p.x), math::NkMin(mn.y, p.y));
						mx = NkVec2f(math::NkMax(mx.x, p.x), math::NkMax(mx.y, p.y));
					}
					if (!Chevauche(NkRect{mn.x, mn.y, mx.x - mn.x, mx.y - mn.y}, visible)) {
						sommets.Resize(premier);
						return;
					}
					Aplat a;
					a.t = t;
					a.couche = m.couche;
					a.maillage = id;
					a.premierSommet = premier;
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
				while (j >= 0 && aplats[static_cast<uint32>(j)].couche > cle.couche) {
					aplats[static_cast<uint32>(j + 1)] = aplats[static_cast<uint32>(j)];
					--j;
				}
				aplats[static_cast<uint32>(j + 1)] = cle;
			}

			for (uint32 i = 0; i < aplats.Size(); ++i) {
				const NkTransform2D &t = aplats[i].t;
				const NkSprite2D &s = aplats[i].s;
				if (aplats[i].forme >= 0) {
					NkDessinerRenduForme2D(dl, cam, t, formes[static_cast<uint32>(aplats[i].forme)]);
					++stats.entitesDessinees;
					continue;
				}
				if (aplats[i].maillage.IsValid()) {
					// Relu APRES le tri (aucune requete ne tourne plus) : le pointeur tient.
					if (const NkMaillage2D *m = scene.Monde().Get<NkMaillage2D>(aplats[i].maillage)) {
						NkDessinerMaillage2D(dl, cam, *m, sommets.Data() + aplats[i].premierSommet);
						++stats.entitesDessinees;
					}
					continue;
				}

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
		void NkDessinerMaillage2D(nkgui::NkGuiDrawList &dl, const NkVue2D &cam, const NkMaillage2D &m, const NkVec2f *monde) {
			if (monde == nullptr || m.nbTriangles == 0u || m.nbSommets < 3u) {
				return;
			}
			NkVec2f ecran[NK_MAILLAGE2D_SOMMETS_MAX];
			NkColor cols[NK_MAILLAGE2D_SOMMETS_MAX];
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				ecran[i] = cam.MondeVersEcran(monde[i]);
				cols[i] = Couleur(Multiplier(m.couleurs[i], m.teinte));
			}
			// L'ORDRE DE DESSIN DES PARTIES : la plus grande `ordre` passe devant ; a
			// egalite, l'ordre des triangles (tri par insertion : stable).
			uint32 idx[NK_MAILLAGE2D_TRIANGLES_MAX * 3u];
			uint32 ordreTri[NK_MAILLAGE2D_TRIANGLES_MAX];
			for (uint32 t = 0; t < m.nbTriangles; ++t) {
				ordreTri[t] = t;
			}
			auto Ordre = [&m](uint32 t) {
				const uint32 p = m.partieTriangle[t];
				return p < m.nbParties ? m.parties[p].ordre : 0;
			};
			for (uint32 i = 1; i < m.nbTriangles; ++i) {
				const uint32 cle = ordreTri[i];
				int32 j = static_cast<int32>(i) - 1;
				while (j >= 0 && Ordre(ordreTri[static_cast<uint32>(j)]) > Ordre(cle)) {
					ordreTri[static_cast<uint32>(j + 1)] = ordreTri[static_cast<uint32>(j)];
					--j;
				}
				ordreTri[static_cast<uint32>(j + 1)] = cle;
			}
			for (uint32 i = 0; i < m.nbTriangles; ++i) {
				const uint32 t = ordreTri[i];
				for (uint32 k = 0; k < 3u; ++k) {
					idx[i * 3u + k] = m.triangles[t * 3u + k];
				}
			}
			// UN appel : la texture (ou les couleurs seules) deformee par les sommets,
			// une couleur par sommet (NkGuiDrawList::AddMesh).
			dl.AddMesh(m.texId, ecran, m.uvs, cols, static_cast<int32>(m.nbSommets), idx, static_cast<int32>(m.nbTriangles) * 3);
		}

		// =====================================================================
		void NkDessinerRenduForme2D(nkgui::NkGuiDrawList &dl, const NkVue2D &cam, const NkTransform2D &t,
									const NkRenduForme2D &f) {
			NkVec2f pts[NK_FORME_CONTOUR_MAX];
			bool ferme = true;
			const uint32 n = NkContourForme2D(f, pts, NK_FORME_CONTOUR_MAX, ferme);
			// Local -> monde (rotation ET echelle, comme un sprite) -> ecran.
			for (uint32 i = 0; i < n; ++i) {
				pts[i] = cam.MondeVersEcran(t.VersMonde(pts[i]));
			}
			const float32 echelle = (math::NkAbs(t.echelle.x) + math::NkAbs(t.echelle.y)) * 0.5f;
			DessinerContourForme(dl, f, pts, n, ferme, cam.LongueurVersEcran(1.f) * echelle);
		}

		void NkDessinerFormeVignette(nkgui::NkGuiDrawList &dl, const NkRenduForme2D &f, const NkRect &r) {
			NkVec2f pts[NK_FORME_CONTOUR_MAX];
			bool ferme = true;
			const uint32 n = NkContourForme2D(f, pts, NK_FORME_CONTOUR_MAX, ferme);
			if (n < 2u) {
				return;
			}
			// La boite du contour, ramenee au rectangle (marge comprise), y vers le bas.
			NkVec2f mn = pts[0], mx = pts[0];
			for (uint32 i = 1; i < n; ++i) {
				mn = NkVec2f(math::NkMin(mn.x, pts[i].x), math::NkMin(mn.y, pts[i].y));
				mx = NkVec2f(math::NkMax(mx.x, pts[i].x), math::NkMax(mx.y, pts[i].y));
			}
			const float32 bord = (ferme ? f.epaisseurContour : f.epaisseur) * 0.5f;
			const float32 lw = math::NkMax(mx.x - mn.x + 2.f * bord, 1.0e-3f);
			const float32 lh = math::NkMax(mx.y - mn.y + 2.f * bord, 1.0e-3f);
			const float32 k = math::NkMin(r.w / lw, r.h / lh);
			const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
			const float32 mx0 = (mn.x + mx.x) * 0.5f, my0 = (mn.y + mx.y) * 0.5f;
			for (uint32 i = 0; i < n; ++i) {
				pts[i] = NkVec2f(cx + (pts[i].x - mx0) * k, cy - (pts[i].y - my0) * k);
			}
			DessinerContourForme(dl, f, pts, n, ferme, k);
		}

		// =====================================================================
		void NkDessinerCollisionneurs(nkgui::NkGuiDrawList &dl, NkScene &scene, uint32 couleur) {
			const NkVue2D &cam = scene.Camera();
			const NkColor col = Couleur(couleur);

			// (2026-10-01) Le contour EXACT de chaque collisionneur, ROTATIONS
			// comprises (celle de l'entite et la sienne) : avant, une boite et une
			// capsule etaient tracees droites quel que soit leur angle. Polygones et
			// chaines aussi, leurs sommets marques.
			scene.Monde().Query<NkTransform2D, NkCollisionneur2D>().ForEach(
				[&](ecs::NkEntityId id, NkTransform2D &t, NkCollisionneur2D &c) {
					if (!scene.EstActive(id)) {
						return; // eteint : son collisionneur ne touche rien, il ne se montre pas
					}
					NkVec2f pts[NK_COLLISION_SOMMETS_MAX + 72u];
					bool ferme = true;
					const uint32 n = NkContourCollisionneur2D(t, c, pts, NK_COLLISION_SOMMETS_MAX + 72u, ferme);
					for (uint32 i = 0; i < n; ++i) {
						pts[i] = cam.MondeVersEcran(pts[i]);
					}
					if (n >= 2u) {
						dl.AddPolyline(pts, static_cast<int32>(n), col, 1.5f, ferme);
					}
					if (c.forme == NkForme2D::NK_POLYGONE || c.forme == NkForme2D::NK_CHAINE) {
						for (uint32 i = 0; i < n; ++i) {
							dl.AddCircleFilled(pts[i], 2.5f, col);
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
				if (!NkEntiteActive(monde, id)) {
					return; // une entite ETEINTE n'est pas rendue (NkUnkenyActif.h)
				}
				const NkCorps2D *corps = monde.Get<NkCorps2D>(id);
				const NkSprite2D *sp = monde.Get<NkSprite2D>(id);
				const bool rigide = corps != nullptr && corps->type == NkTypeCorps::NK_DYNAMIQUE;
				if (sp != nullptr && sp->visible && sp->texId != 0u) {
					return; // texture : c'est NkDessinerScene qui le dessine
				}
				// (2026-10-01) Une FORME 2D se dessine elle-meme (NkDessinerScene),
				// a ses couleurs : son collisionneur ne la double pas. Cachee, elle
				// ne laisse rien voir non plus.
				if (monde.Has<NkRenduForme2D>(id)) {
					return;
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
				// (2026-10-01) La rotation PROPRE du collisionneur y est (0 avant).
				const float32 cr = math::NkCos(c.rotation), sr = math::NkSin(c.rotation);
				auto E = [&](float32 lx, float32 ly) {
					return cam.MondeVersEcran(t.VersMonde(NkVec2f(c.decalage.x + lx * cr - ly * sr, c.decalage.y + lx * sr + ly * cr)));
				};
				switch (c.forme) {
					case NkForme2D::NK_POLYGONE:
					case NkForme2D::NK_CHAINE: {
						// Le decor fait a la main (une colline, une pente) : rempli s'il
						// est ferme, un trait sinon.
						NkVec2f q[NK_COLLISION_SOMMETS_MAX];
						const uint32 n = NkNbSommetsCollision2D(c);
						for (uint32 i = 0; i < n; ++i) {
							q[i] = E(c.sommets[i].x, c.sommets[i].y);
						}
						const bool ferme = c.forme == NkForme2D::NK_POLYGONE || c.boucle;
						if (ferme && n >= 3u) {
							detail::NkEarcutNode<float32> noeuds[NK_COLLISION_SOMMETS_MAX];
							uint32 idx[(NK_COLLISION_SOMMETS_MAX - 2u) * 3u];
							const uint32 nbTri = NkEarcutVers<float32>(q, n, noeuds, NK_COLLISION_SOMMETS_MAX, idx,
																		(NK_COLLISION_SOMMETS_MAX - 2u) * 3u);
							for (uint32 k = 0; k < nbTri; ++k) {
								dl.AddTriangleFilled(q[idx[k * 3u]], q[idx[k * 3u + 1u]], q[idx[k * 3u + 2u]], fond);
							}
							dl.AddPolyline(q, static_cast<int32>(n), bord, 1.5f, true);
						} else if (n >= 2u) {
							dl.AddPolyline(q, static_cast<int32>(n), fond, 3.f, false);
						}
						break;
					}
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
