// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Maillage/NkUnkenyMaillagePhysique.cpp
// DESCRIPTION: La physique par partie d'un maillage 2D, et la place de ses
//              sommets en jeu (voir NkUnkenyMaillagePhysique.h).
//
// ⚠️ AUCUN PONT 2D <-> 3D ICI. Les corps rigides passent par NkScene
//    (CreerCorpsLibre, PoseCorps, LierCorps...), le seul endroit qui convertit ;
//    la matiere par NkParticules2D, qui est deja plane.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Maillage/NkUnkenyMaillagePhysique.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKLogger/NkLog.h"
#include "NKPhysics/NkParticules2DFabrique.h"
#include "Unkeny/Scene/NkUnkenyFormes.h"
#include "Unkeny/Scene/NkUnkenyScene.h"
#include "Unkeny/Squelette/NkUnkenySquelette.h"

namespace nkentseu {
	namespace unkeny {

		namespace {
			using physics::NkBodyId;

			NkPhysiquePartie2D Physique(const NkMaillage2D &m, uint32 k) noexcept {
				return k < m.nbParties ? static_cast<NkPhysiquePartie2D>(m.parties[k].physique) : NkPhysiquePartie2D::NK_AUCUNE;
			}

			bool AUnePhysique(const NkMaillage2D &m) noexcept {
				for (uint32 k = 0; k < m.nbParties; ++k) {
					if (Physique(m, k) != NkPhysiquePartie2D::NK_AUCUNE) {
						return true;
					}
				}
				return false;
			}

			/// Le point `local` d'un corps de pose (p, a), en monde.
			NkVec2f VersMondeCorps(const NkVec2f &p, float32 a, const NkVec2f &local) noexcept {
				const float32 c = math::NkCos(a), s = math::NkSin(a);
				return NkVec2f(p.x + local.x * c - local.y * s, p.y + local.x * s + local.y * c);
			}
			/// L'inverse : un point monde, dans le repere du corps.
			NkVec2f VersCorps(const NkVec2f &p, float32 a, const NkVec2f &monde) noexcept {
				const float32 c = math::NkCos(-a), s = math::NkSin(-a);
				const float32 x = monde.x - p.x, y = monde.y - p.y;
				return NkVec2f(x * c - y * s, x * s + y * c);
			}

			/// Retire du contour, un a un, le sommet qui emporte le moins d'aire
			/// (Visvalingam), jusqu'a `max` : une chaine de decor tient en 32 points.
			uint32 Reduire(NkVec2f *pts, uint32 n, uint32 max) noexcept {
				while (n > max && n > 3u) {
					uint32 iMin = 0;
					float32 aMin = 1.0e30f;
					for (uint32 i = 0; i < n; ++i) {
						const NkVec2f &a = pts[(i + n - 1u) % n], &b = pts[i], &c = pts[(i + 1u) % n];
						const float32 aire = math::NkAbs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
						if (aire < aMin) {
							aMin = aire;
							iMin = i;
						}
					}
					for (uint32 i = iMin; i + 1u < n; ++i) {
						pts[i] = pts[i + 1u];
					}
					--n;
				}
				return n;
			}

			/// Le corps qui TIENT une partie AUCUNE : celui de l'entite (si elle a un
			/// corps rigide), sinon l'ANCRE cinematique du maillage (creee ici).
			NkBodyId Tenant(NkScene &scene, ecs::NkEntityId id, const NkTransform2D &t, NkMaillage2D &m) {
				if (const NkCorps2D *c = scene.Monde().Get<NkCorps2D>(id)) {
					if (c->corpsId != physics::NK_INVALID_BODY) {
						return c->corpsId;
					}
				}
				if (m.ancre == 0u) {
					NkCollisionneur2D col;
					col.forme = NkForme2D::NK_CERCLE;
					col.rayon = 0.02f;
					col.couche = 0u; // ne touche rien : elle ne fait que tenir
					col.masque = 0u;
					NkCorps2D corps;
					corps.type = NkTypeCorps::NK_CINEMATIQUE;
					m.ancre = scene.CreerCorpsLibre(t, col, corps, id, 0u);
				}
				return m.ancre;
			}

			/// La couture des parties `a` et `b` (leurs sommets confondus), en repere
			/// du maillage ; rend les deux points les plus ECARTES (ou un seul).
			uint32 Couture(const NkMaillage2D &m, uint32 a, uint32 b, NkVec2f &p1, NkVec2f &p2) noexcept {
				NkVec2f pts[NK_MAILLAGE2D_SOMMETS_MAX];
				uint32 n = 0;
				for (uint32 i = 0; i < m.nbSommets; ++i) {
					if (m.partieSommet[i] != a) {
						continue;
					}
					for (uint32 j = 0; j < m.nbSommets; ++j) {
						if (m.partieSommet[j] == b && math::NkAbs(m.positions[i].x - m.positions[j].x) <= 1.0e-5f &&
							math::NkAbs(m.positions[i].y - m.positions[j].y) <= 1.0e-5f) {
							pts[n++] = m.positions[i];
							break;
						}
					}
				}
				if (n == 0u) {
					return 0u;
				}
				p1 = p2 = pts[0];
				float32 dMax = -1.f;
				for (uint32 i = 0; i < n; ++i) {
					for (uint32 j = i + 1u; j < n; ++j) {
						const float32 dx = pts[i].x - pts[j].x, dy = pts[i].y - pts[j].y;
						if (dx * dx + dy * dy > dMax) {
							dMax = dx * dx + dy * dy;
							p1 = pts[i];
							p2 = pts[j];
						}
					}
				}
				return n;
			}

			/// Le rang du sommet `s` parmi ceux de sa partie (l'ordre des particules).
			uint32 Rang(const NkMaillage2D &m, uint32 s) noexcept {
				uint32 r = 0;
				for (uint32 i = 0; i < s; ++i) {
					r += m.partieSommet[i] == m.partieSommet[s] ? 1u : 0u;
				}
				return r;
			}

			/// L'indice ABSOLU de la particule du sommet `s` (partie molle construite), ou -1.
			int32 Particule(const physics::NkParticules2D *p, const NkMaillage2D &m, uint32 s) noexcept {
				if (p == nullptr) {
					return -1;
				}
				const NkPartieMaillage2D &pt = m.parties[m.partieSommet[s]];
				const int32 ci = p->IndexCorps(pt.corps);
				if (ci < 0) {
					return -1;
				}
				const physics::NkCorpsP2D &c = p->corps[static_cast<uint32>(ci)];
				const uint32 k = pt.premier + Rang(m, s);
				return k < c.nombre ? static_cast<int32>(c.debut + k) : -1;
			}

			// =================================================================
			// LA CONSTRUCTION
			// =================================================================
			void Construire(NkScene &scene, ecs::NkEntityId id, const NkTransform2D &t, NkMaillage2D &m) {
				m.construit = true; // meme si rien ne se construit : on ne reessaie pas chaque pas
				if (!AUnePhysique(m) || m.nbTriangles == 0u) {
					return;
				}
				m.groupe = m.partiesSeTouchent ? 0u : scene.NouveauGroupe();
				// Les sommets en monde, au repos.
				NkVec2f monde[NK_MAILLAGE2D_SOMMETS_MAX];
				for (uint32 i = 0; i < m.nbSommets; ++i) {
					monde[i] = t.VersMonde(m.positions[i]);
				}
				// ── 1. Les parties RIGIDES : un corps libre chacune ──
				for (uint32 k = 0; k < m.nbParties; ++k) {
					NkPartieMaillage2D &p = m.parties[k];
					if (Physique(m, k) != NkPhysiquePartie2D::NK_RIGIDE) {
						continue;
					}
					p.centre = NkCentrePartie2D(m, k);
					NkVec2f contour[NK_MAILLAGE2D_SOMMETS_MAX];
					uint32 n = NkContourPartie2D(m, k, contour, NK_MAILLAGE2D_SOMMETS_MAX);
					if (n < 3u) {
						continue;
					}
					// Dans le repere du CORPS (centre de la partie), echelle de l'entite.
					for (uint32 i = 0; i < n; ++i) {
						contour[i] = NkVec2f((contour[i].x - p.centre.x) * t.echelle.x, (contour[i].y - p.centre.y) * t.echelle.y);
					}
					NkCollisionneur2D col;
					const bool dynamique = static_cast<NkTypeCorps>(p.typeCorps) == NkTypeCorps::NK_DYNAMIQUE;
					if (dynamique) {
						// Ce que NKCollision sait faire tomber : un convexe de 8 sommets.
						NkVec2f env[NK_MAILLAGE2D_SOMMETS_MAX];
						uint32 e = NkEnveloppeConvexe2D(contour, n, env, NK_MAILLAGE2D_SOMMETS_MAX);
						e = NkReduireConvexe2D(env, e, NK_POLYGONE_CONVEXE_MAX);
						col.forme = NkForme2D::NK_POLYGONE;
						col.nbSommets = static_cast<uint8>(e);
						for (uint32 i = 0; i < e; ++i) {
							col.sommets[i] = env[i];
						}
					} else {
						// Un decor : le contour EXACT, en chaine fermee.
						n = Reduire(contour, n, NK_COLLISION_SOMMETS_MAX - 1u);
						col.forme = NkForme2D::NK_CHAINE;
						col.boucle = true;
						col.nbSommets = static_cast<uint8>(n);
						for (uint32 i = 0; i < n; ++i) {
							col.sommets[i] = contour[i];
						}
					}
					if (!p.collision) {
						col.couche = 0u;
						col.masque = 0u;
					}
					NkCorps2D corps;
					corps.type = static_cast<NkTypeCorps>(p.typeCorps);
					corps.masse = p.masse > 0.f ? p.masse : 1.f;
					corps.friction = p.friction;
					corps.rebond = p.rebond;
					NkTransform2D tb;
					tb.position = t.VersMonde(p.centre);
					tb.rotation = t.rotation;
					p.corps = scene.CreerCorpsLibre(tb, col, corps, id, m.groupe);
				}
				// ── 2. Les parties MOLLES : un corps de particules par GROUPE relie ──
				physics::NkParticules2D *pw = scene.Particules();
				bool aMolle = false;
				for (uint32 k = 0; k < m.nbParties; ++k) {
					aMolle = aMolle || Physique(m, k) == NkPhysiquePartie2D::NK_MOLLE;
				}
				if (aMolle && pw == nullptr) {
					logger.Warn("[unkeny] maillage : partie MOLLE sans monde de particules (NkSceneConfig::particules) -- elle suit l'entite");
				}
				if (aMolle && pw != nullptr) {
					// Les groupes : les parties molles reliees entre elles (union-find).
					uint8 chef[NK_MAILLAGE2D_PARTIES_MAX];
					for (uint32 k = 0; k < NK_MAILLAGE2D_PARTIES_MAX; ++k) {
						chef[k] = static_cast<uint8>(k);
					}
					auto Chef = [&chef](uint32 k) {
						while (chef[k] != k) {
							k = chef[k];
						}
						return k;
					};
					for (uint32 l = 0; l < m.nbLiens; ++l) {
						const NkLienParties2D &x = m.liens[l];
						if (Physique(m, x.a) == NkPhysiquePartie2D::NK_MOLLE && Physique(m, x.b) == NkPhysiquePartie2D::NK_MOLLE) {
							chef[Chef(x.a)] = static_cast<uint8>(Chef(x.b));
						}
					}
					for (uint32 g = 0; g < m.nbParties; ++g) {
						if (Physique(m, g) != NkPhysiquePartie2D::NK_MOLLE || Chef(g) != g) {
							continue;
						}
						// Le rayon des particules : un tiers de l'arete moyenne du groupe.
						float32 somme = 0.f;
						uint32 aretes = 0;
						float32 masse = 0.f;
						uint32 nombre = 0;
						for (uint32 t2 = 0; t2 < m.nbTriangles; ++t2) {
							if (Physique(m, m.partieTriangle[t2]) != NkPhysiquePartie2D::NK_MOLLE || Chef(m.partieTriangle[t2]) != g) {
								continue;
							}
							for (uint32 e = 0; e < 3u; ++e) {
								const NkVec2f &a = monde[m.triangles[t2 * 3u + e]], &b = monde[m.triangles[t2 * 3u + (e + 1u) % 3u]];
								somme += math::NkSqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
								++aretes;
							}
						}
						const float32 rayon = math::NkClamp(aretes > 0u ? 0.3f * somme / static_cast<float32>(aretes) : 0.05f, 0.02f, 0.25f);
						const uint32 ci = pw->NouveauCorps(physics::NkMateriauP2D::NK_GELEE);
						physics::NkCorpsP2D &corps = pw->corps[ci];
						// ⚠️ Le preset REMET TOUT le corps a zero (id et plage compris) : on
						// garde ce que NouveauCorps a pose (mesure du banc, m9 : l'id valait 0).
						{
							const uint32 idCorps = corps.id, debutCorps = corps.debut;
							physics::NkAppliquerPresetP2D(corps, physics::NkPresetP2D::NK_GELEE);
							corps.id = idCorps;
							corps.debut = debutCorps;
							corps.nombre = 0;
						}
						corps.raideur = math::NkClamp(m.parties[g].raideur, 0.01f, 1.f);
						corps.formeRaideur = math::NkClamp(m.parties[g].forme, 0.f, 1.f);
						corps.friction = m.parties[g].friction;
						corps.rebond = m.parties[g].rebond;
						corps.espacement = rayon * 2.f;
						const uint32 debut = static_cast<uint32>(pw->particules.Size());
						for (uint32 k = 0; k < m.nbParties; ++k) {
							if (Physique(m, k) != NkPhysiquePartie2D::NK_MOLLE || Chef(k) != g) {
								continue;
							}
							m.parties[k].premier = static_cast<uint32>(pw->particules.Size()) - debut;
							for (uint32 i = 0; i < m.nbSommets; ++i) {
								if (m.partieSommet[i] == k) {
									pw->AjouterParticule(ci, monde[i], rayon);
									++nombre;
								}
							}
							masse += m.parties[k].masse > 0.f ? m.parties[k].masse : 1.f;
						}
						corps.masseParticule = nombre > 0u ? masse / static_cast<float32>(nombre) : 1.f;
						// Les LIENS : les aretes de chaque triangle (une fois), et la
						// FLEXION (les deux sommets opposes d'une arete interieure).
						NkVector<uint32> faits; // paires deja reliees (a * 256 + b)
						auto Relier = [&](uint32 sa, uint32 sb, physics::NkGenreLien2D genre) {
							const int32 pa = static_cast<int32>(debut) + static_cast<int32>(m.parties[m.partieSommet[sa]].premier + Rang(m, sa));
							const int32 pb = static_cast<int32>(debut) + static_cast<int32>(m.parties[m.partieSommet[sb]].premier + Rang(m, sb));
							const uint32 cle = sa < sb ? sa * 256u + sb : sb * 256u + sa;
							for (uint32 q = 0; q < faits.Size(); ++q) {
								if (faits[q] == cle) {
									return;
								}
							}
							faits.PushBack(cle);
							pw->AjouterLien(static_cast<uint32>(pa), static_cast<uint32>(pb), genre);
						};
						for (uint32 t2 = 0; t2 < m.nbTriangles; ++t2) {
							const uint32 k = m.partieTriangle[t2];
							if (Physique(m, k) != NkPhysiquePartie2D::NK_MOLLE || Chef(k) != g) {
								continue;
							}
							for (uint32 e = 0; e < 3u; ++e) {
								Relier(m.triangles[t2 * 3u + e], m.triangles[t2 * 3u + (e + 1u) % 3u], physics::NkGenreLien2D::NK_STRUCTURE);
							}
							// La flexion : le triangle voisin de chaque arete (meme partie).
							for (uint32 e = 0; e < 3u; ++e) {
								const uint8 a = m.triangles[t2 * 3u + e], b = m.triangles[t2 * 3u + (e + 1u) % 3u];
								const uint8 o = m.triangles[t2 * 3u + (e + 2u) % 3u];
								for (uint32 u = t2 + 1u; u < m.nbTriangles; ++u) {
									if (m.partieTriangle[u] != k) {
										continue;
									}
									for (uint32 f = 0; f < 3u; ++f) {
										if (m.triangles[u * 3u + f] == b && m.triangles[u * 3u + (f + 1u) % 3u] == a) {
											Relier(o, m.triangles[u * 3u + (f + 2u) % 3u], physics::NkGenreLien2D::NK_FLEXION);
										}
									}
								}
							}
						}
						// Les liens ENTRE parties molles du groupe : chaque sommet de couture
						// d'un cote est relie aux VOISINS de son double de l'autre cote
						// (deux particules confondues ne se relient pas : longueur nulle).
						for (uint32 l = 0; l < m.nbLiens; ++l) {
							const NkLienParties2D &x = m.liens[l];
							if (Physique(m, x.a) != NkPhysiquePartie2D::NK_MOLLE || Physique(m, x.b) != NkPhysiquePartie2D::NK_MOLLE ||
								Chef(x.a) != g) {
								continue;
							}
							const physics::NkGenreLien2D genre = x.genre == static_cast<uint8>(NkGenreLienParties2D::NK_ELASTIQUE)
																	 ? physics::NkGenreLien2D::NK_FLEXION
																	 : physics::NkGenreLien2D::NK_STRUCTURE;
							for (uint32 i = 0; i < m.nbSommets; ++i) {
								const uint32 pi = m.partieSommet[i];
								if (pi != x.a && pi != x.b) {
									continue;
								}
								const uint32 autre = pi == x.a ? x.b : x.a;
								for (uint32 j = 0; j < m.nbSommets; ++j) {
									if (m.partieSommet[j] != autre || math::NkAbs(m.positions[i].x - m.positions[j].x) > 1.0e-5f ||
										math::NkAbs(m.positions[i].y - m.positions[j].y) > 1.0e-5f) {
										continue;
									}
									// i et j sont confondus : i se relie aux voisins de j.
									for (uint32 t2 = 0; t2 < m.nbTriangles; ++t2) {
										if (m.partieTriangle[t2] != autre) {
											continue;
										}
										for (uint32 e = 0; e < 3u; ++e) {
											if (m.triangles[t2 * 3u + e] != j) {
												continue;
											}
											Relier(i, m.triangles[t2 * 3u + (e + 1u) % 3u], genre);
											Relier(i, m.triangles[t2 * 3u + (e + 2u) % 3u], genre);
										}
									}
								}
							}
						}
						// La masse APRES coup (les particules sont nees avec celle du preset).
						pw->AppliquerMasse(ci);
						pw->FinaliserCorps(ci);
						// La marque : ni dessinee par le rendu des corps mous, ni orpheline.
						pw->corps[ci].utilisateur = NK_CORPS_MOU_DE_MAILLAGE | id.Pack();
						for (uint32 k = 0; k < m.nbParties; ++k) {
							if (Physique(m, k) == NkPhysiquePartie2D::NK_MOLLE && Chef(k) == g) {
								m.parties[k].corps = pw->corps[ci].id;
								m.parties[k].centre = NkCentrePartie2D(m, k);
							}
						}
					}
				}
				// ── 3. Les LIENS qui ne sont pas DANS un corps mou ──
				for (uint32 l = 0; l < m.nbLiens; ++l) {
					NkLienParties2D &x = m.liens[l];
					x.casse = false;
					x.construit = false;
					const NkPhysiquePartie2D pa = Physique(m, x.a), pb = Physique(m, x.b);
					if (pa == NkPhysiquePartie2D::NK_MOLLE && pb == NkPhysiquePartie2D::NK_MOLLE) {
						x.construit = true; // fait au 2.
						continue;
					}
					if (pa == NkPhysiquePartie2D::NK_AUCUNE && pb == NkPhysiquePartie2D::NK_AUCUNE) {
						continue; // les deux suivent l'entite : rien a tenir
					}
					// Le point de couture (repere du maillage) : ses deux bouts, ou a
					// defaut le milieu des centres.
					NkVec2f c1, c2;
					if (Couture(m, x.a, x.b, c1, c2) == 0u) {
						c1 = c2 = (NkCentrePartie2D(m, x.a) + NkCentrePartie2D(m, x.b)) * 0.5f;
					}
					const NkVec2f w1 = t.VersMonde(c1), w2 = t.VersMonde(c2);
					// Le corps de chaque cote (une partie AUCUNE : celui qui la tient).
					auto Corps = [&](uint32 k, NkPhysiquePartie2D ph) -> NkBodyId {
						if (ph == NkPhysiquePartie2D::NK_RIGIDE) {
							return m.parties[k].corps;
						}
						if (ph == NkPhysiquePartie2D::NK_AUCUNE) {
							return Tenant(scene, id, t, m);
						}
						return physics::NK_INVALID_BODY;
					};
					if (pa != NkPhysiquePartie2D::NK_MOLLE && pb != NkPhysiquePartie2D::NK_MOLLE) {
						// ── RIGIDE <-> RIGIDE (ou tenu) ──
						const NkBodyId A = Corps(x.a, pa), B = Corps(x.b, pb);
						if (A == physics::NK_INVALID_BODY || B == physics::NK_INVALID_BODY) {
							continue;
						}
						x.corpsA = A;
						x.corpsB = B;
						const NkGenreLienParties2D genre = static_cast<NkGenreLienParties2D>(x.genre);
						if (genre == NkGenreLienParties2D::NK_ELASTIQUE) {
							// Deux ressorts aux bouts de la couture : ils tiennent ET plient.
							NkVec2f posA, posB;
							float32 angA = 0.f, angB = 0.f;
							if (!scene.PoseCorps(A, posA, angA) || !scene.PoseCorps(B, posB, angB)) {
								continue;
							}
							x.ancreA[0] = VersCorps(posA, angA, w1);
							x.ancreA[1] = VersCorps(posA, angA, w2);
							x.ancreB[0] = VersCorps(posB, angB, w1);
							x.ancreB[1] = VersCorps(posB, angB, w2);
							x.construit = true;
						} else {
							x.construit = scene.LierCorps(A, B, (w1 + w2) * 0.5f, genre == NkGenreLienParties2D::NK_RIGIDE);
						}
						continue;
					}
					// ── MOU <-> RIGIDE (ou tenu) : les particules de la couture s'attachent ──
					const uint32 molle = pa == NkPhysiquePartie2D::NK_MOLLE ? x.a : x.b;
					const uint32 dure = molle == x.a ? x.b : x.a;
					const NkBodyId R = Corps(dure, Physique(m, dure));
					if (pw == nullptr || R == physics::NK_INVALID_BODY || scene.MondePhysique() == nullptr) {
						continue;
					}
					const float32 raideur = x.genre == static_cast<uint8>(NkGenreLienParties2D::NK_ELASTIQUE) ? 0.08f : 1.f;
					uint32 attaches = 0;
					for (uint32 i = 0; i < m.nbSommets; ++i) {
						if (m.partieSommet[i] != molle) {
							continue;
						}
						bool couture = false;
						for (uint32 j = 0; j < m.nbSommets && !couture; ++j) {
							couture = m.partieSommet[j] == dure && math::NkAbs(m.positions[i].x - m.positions[j].x) <= 1.0e-5f &&
									  math::NkAbs(m.positions[i].y - m.positions[j].y) <= 1.0e-5f;
						}
						if (!couture) {
							continue;
						}
						const int32 q = Particule(pw, m, i);
						if (q >= 0 && pw->Attacher(static_cast<uint32>(q), *scene.MondePhysique(), R, raideur, x.rupture) >= 0) {
							++attaches;
						}
					}
					x.construit = attaches > 0u;
				}
			}
		} // namespace

		// =====================================================================
		void NkMaillagesConstruire(NkScene &scene) {
			NkVector<ecs::NkEntityId> ids;
			scene.Monde().Query<NkTransform2D, NkMaillage2D>().ForEach(
				[&ids](ecs::NkEntityId id, NkTransform2D &, NkMaillage2D &) { ids.PushBack(id); });
			for (uint32 i = 0; i < ids.Size(); ++i) {
				NkMaillage2D *m = scene.Monde().Get<NkMaillage2D>(ids[i]);
				const NkTransform2D *t = scene.Monde().Get<NkTransform2D>(ids[i]);
				if (m == nullptr || t == nullptr) {
					continue;
				}
				const bool actif = scene.EstActive(ids[i]);
				if (!actif && m->construit) {
					NkMaillageDetruirePhysique(scene, *m, true); // eteinte : ses morceaux partent
				} else if (actif && !m->construit) {
					const NkTransform2D tc = *t;
					Construire(scene, ids[i], tc, *m);
				}
			}
		}

		void NkMaillagesAvantPasFixe(NkScene &scene, float32 dt) {
			scene.Monde().Query<NkTransform2D, NkMaillage2D>().ForEach([&](ecs::NkEntityId, NkTransform2D &t, NkMaillage2D &m) {
				if (!m.construit) {
					return;
				}
				// L'ANCRE suit l'entite (un maillage deplace par un script emmene ce
				// qu'il tient).
				if (m.ancre != 0u) {
					scene.MenerCorps(m.ancre, t.position, t.rotation, dt);
				}
				// Les RESSORTS des liens elastiques entre corps rigides.
				for (uint32 l = 0; l < m.nbLiens; ++l) {
					NkLienParties2D &x = m.liens[l];
					if (!x.construit || x.casse || x.genre != static_cast<uint8>(NkGenreLienParties2D::NK_ELASTIQUE)) {
						continue;
					}
					const NkPhysiquePartie2D pa = Physique(m, x.a), pb = Physique(m, x.b);
					if (pa == NkPhysiquePartie2D::NK_MOLLE || pb == NkPhysiquePartie2D::NK_MOLLE) {
						continue; // tenus par la matiere elle-meme
					}
					const NkBodyId A = x.corpsA, B = x.corpsB;
					NkVec2f posA, posB;
					float32 angA = 0.f, angB = 0.f;
					if (!scene.PoseCorps(A, posA, angA) || !scene.PoseCorps(B, posB, angB)) {
						continue;
					}
					for (uint32 k = 0; k < 2u; ++k) {
						const NkVec2f wa = VersMondeCorps(posA, angA, x.ancreA[k]);
						const NkVec2f wb = VersMondeCorps(posB, angB, x.ancreB[k]);
						const NkVec2f d = wb - wa;
						const NkVec2f dv = scene.VitesseCorpsEnPoint(B, wb) - scene.VitesseCorpsEnPoint(A, wa);
						const NkVec2f f = d * x.raideur + dv * x.amortissement;
						scene.AppliquerForceCorps(A, f, wa);
						scene.AppliquerForceCorps(B, f * -1.f, wb);
					}
				}
			});
		}

		void NkMaillagesApresPasFixe(NkScene &scene, float32 dt) {
			(void)dt;
			scene.Monde().Query<NkTransform2D, NkMaillage2D>().ForEach([&](ecs::NkEntityId, NkTransform2D &, NkMaillage2D &m) {
				if (!m.construit) {
					return;
				}
				for (uint32 l = 0; l < m.nbLiens; ++l) {
					NkLienParties2D &x = m.liens[l];
					if (!x.construit || x.casse || x.rupture <= 0.f) {
						continue;
					}
					const NkPhysiquePartie2D pa = Physique(m, x.a), pb = Physique(m, x.b);
					if (pa == NkPhysiquePartie2D::NK_MOLLE || pb == NkPhysiquePartie2D::NK_MOLLE) {
						continue; // les attaches cassent d'elles-memes (NkAttacheP2D::rupture)
					}
					const NkBodyId A = x.corpsA, B = x.corpsB;
					if (x.genre == static_cast<uint8>(NkGenreLienParties2D::NK_ELASTIQUE)) {
						// Un ressort casse a l'ALLONGEMENT.
						NkVec2f posA, posB;
						float32 angA = 0.f, angB = 0.f;
						if (!scene.PoseCorps(A, posA, angA) || !scene.PoseCorps(B, posB, angB)) {
							continue;
						}
						for (uint32 k = 0; k < 2u; ++k) {
							const NkVec2f d = VersMondeCorps(posB, angB, x.ancreB[k]) - VersMondeCorps(posA, angA, x.ancreA[k]);
							if (d.x * d.x + d.y * d.y > x.rupture * x.rupture) {
								x.casse = true;
							}
						}
						continue;
					}
					// Une soudure, un pivot : a la FORCE transmise.
					const float32 f = scene.ForceLien(A, B);
					if (f > x.rupture) {
						scene.CouperLien(A, B);
						x.casse = true;
					}
				}
			});
		}

		void NkMaillageDetruirePhysique(NkScene &scene, NkMaillage2D &m, bool particules) {
			physics::NkParticules2D *pw = scene.Particules();
			for (uint32 k = 0; k < m.nbParties; ++k) {
				NkPartieMaillage2D &p = m.parties[k];
				if (p.corps == 0u) {
					continue;
				}
				if (Physique(m, k) == NkPhysiquePartie2D::NK_RIGIDE) {
					scene.DetruireCorpsLibre(p.corps);
				} else if (Physique(m, k) == NkPhysiquePartie2D::NK_MOLLE && particules && pw != nullptr) {
					const int32 ci = pw->IndexCorps(p.corps);
					if (ci >= 0) {
						pw->SupprimerCorps(static_cast<uint32>(ci)); // une fois : les autres parties du groupe ne le trouvent plus
					}
				}
			}
			if (m.ancre != 0u) {
				scene.DetruireCorpsLibre(m.ancre);
			}
			NkMaillageOublierPhysique(m);
		}

		void NkMaillageOublierPhysique(NkMaillage2D &m) noexcept {
			for (uint32 k = 0; k < NK_MAILLAGE2D_PARTIES_MAX; ++k) {
				m.parties[k].corps = 0u;
				m.parties[k].premier = 0u;
				m.parties[k].centre = NkVec2f(0.f, 0.f);
			}
			for (uint32 l = 0; l < NK_MAILLAGE2D_LIENS_MAX; ++l) {
				m.liens[l].corpsA = 0u;
				m.liens[l].corpsB = 0u;
				m.liens[l].casse = false;
				m.liens[l].construit = false;
			}
			m.construit = false;
			m.groupe = 0u;
			m.ancre = 0u;
		}

		// =====================================================================
		// LA PLACE DES SOMMETS
		// =====================================================================
		uint32 NkMaillagePositionsMonde(const NkScene &scene, ecs::NkEntityId e, const NkTransform2D &t, const NkMaillage2D &m,
										NkVec2f *sortie) noexcept {
			const physics::NkParticules2D *pw = scene.Particules();
			// (R30) La PEAU : les sommets ponderes suivent les os du squelette 2D de
			// l'entite (sa pose) ; les autres restent au repos.
			const NkSquelette2D *sq = e.IsValid() ? scene.Monde().Get<NkSquelette2D>(e) : nullptr;
			NkVec2f peau[NK_MAILLAGE2D_SOMMETS_MAX];
			NkMaillageDeformer2D(sq, m, peau);
			// Par partie : sa pose (rigide), une fois.
			bool rigide[NK_MAILLAGE2D_PARTIES_MAX] = {};
			NkVec2f pose[NK_MAILLAGE2D_PARTIES_MAX];
			float32 angle[NK_MAILLAGE2D_PARTIES_MAX] = {};
			if (m.construit) {
				for (uint32 k = 0; k < m.nbParties; ++k) {
					if (Physique(m, k) == NkPhysiquePartie2D::NK_RIGIDE && m.parties[k].corps != 0u) {
						rigide[k] = scene.PoseCorps(m.parties[k].corps, pose[k], angle[k]);
					}
				}
			}
			uint32 rang[NK_MAILLAGE2D_PARTIES_MAX] = {};
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				const uint32 k = m.partieSommet[i] < NK_MAILLAGE2D_PARTIES_MAX ? m.partieSommet[i] : 0u;
				const uint32 r = rang[k]++;
				if (m.construit && rigide[k]) {
					// Le sommet garde sa place DANS sa partie (son ecart au centre de repos).
					const NkVec2f l((m.positions[i].x - m.parties[k].centre.x) * t.echelle.x,
									(m.positions[i].y - m.parties[k].centre.y) * t.echelle.y);
					sortie[i] = VersMondeCorps(pose[k], angle[k], l);
					continue;
				}
				if (m.construit && pw != nullptr && Physique(m, k) == NkPhysiquePartie2D::NK_MOLLE && m.parties[k].corps != 0u) {
					const int32 ci = pw->IndexCorps(m.parties[k].corps);
					if (ci >= 0) {
						const physics::NkCorpsP2D &c = pw->corps[static_cast<uint32>(ci)];
						const uint32 q = m.parties[k].premier + r;
						if (q < c.nombre) {
							sortie[i] = pw->particules[c.debut + q].pos;
							continue;
						}
					}
				}
				sortie[i] = t.VersMonde(peau[i]);
			}
			return m.nbSommets;
		}

		float32 NkDistanceMaillage2D(const NkScene &scene, ecs::NkEntityId e, const NkTransform2D &t, const NkMaillage2D &m,
									 const NkVec2f &p) noexcept {
			NkVec2f w[NK_MAILLAGE2D_SOMMETS_MAX];
			NkMaillagePositionsMonde(scene, e, t, m, w);
			float32 d = 1.0e9f;
			bool dedans = false;
			for (uint32 tr = 0; tr < m.nbTriangles; ++tr) {
				const NkVec2f &a = w[m.triangles[tr * 3u]], &b = w[m.triangles[tr * 3u + 1u]], &c = w[m.triangles[tr * 3u + 2u]];
				d = math::NkMin(d, math::NkMin(NkDistanceSegment2D(p, a, b), math::NkMin(NkDistanceSegment2D(p, b, c), NkDistanceSegment2D(p, c, a))));
				// Dedans, dans un sens ou dans l'autre (un miroir retourne les triangles).
				const float32 c1 = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
				const float32 c2 = (c.x - b.x) * (p.y - b.y) - (c.y - b.y) * (p.x - b.x);
				const float32 c3 = (a.x - c.x) * (p.y - c.y) - (a.y - c.y) * (p.x - c.x);
				if ((c1 >= 0.f && c2 >= 0.f && c3 >= 0.f) || (c1 <= 0.f && c2 <= 0.f && c3 <= 0.f)) {
					dedans = true;
				}
			}
			return dedans ? -d : d;
		}

		bool NkPartieEnJeu2D(const NkScene &scene, const NkMaillage2D &m, uint32 k) noexcept {
			if (!m.construit || k >= m.nbParties || m.parties[k].corps == 0u) {
				return false;
			}
			if (Physique(m, k) == NkPhysiquePartie2D::NK_RIGIDE) {
				NkVec2f p;
				float32 a = 0.f;
				return scene.PoseCorps(m.parties[k].corps, p, a);
			}
			const physics::NkParticules2D *pw = scene.Particules();
			return pw != nullptr && pw->IndexCorps(m.parties[k].corps) >= 0;
		}

	} // namespace unkeny
} // namespace nkentseu
