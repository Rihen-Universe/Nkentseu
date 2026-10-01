//
// NkUnkenyEclairage.cpp
// =============================================================================
// Description :
//   La carte de lumiere 2D : lumieres, occulteurs, seaux d'aretes, maille
//   adaptative, composition MULTIPLY ou VOILE. Voir NkUnkenyEclairage.h.
//
// L'ORDRE D'UNE IMAGE
//   1. Recueillir les lumieres qui touchent la vue (composants, emetteurs).
//   2. Recueillir les occulteurs (collisionneurs) a portee, en polygones
//      convexes tournes dans le sens trigonometrique.
//   3. Pour chaque lumiere qui porte ombre, ranger ses aretes en seaux.
//   4. Evaluer la carte aux sommets de la maille de base ; affiner les
//      cellules ou le masque des lumieres visibles change ; fusionner les
//      cellules uniformes d'une meme ligne.
//   5. Poser le tout en MULTIPLY (ou le voile), puis les halos additifs.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Rendu/NkUnkenyEclairage.h"

#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace unkeny {

		namespace {
			using nkgui::NkColor;
			using nkgui::NkGuiBlend;
			using nkgui::NkRect;

			constexpr float32 PI = 3.14159265f;
			constexpr int32 SEAUX = 128;		///< seaux angulaires par lumiere
			constexpr int32 SEGMENTS_CERCLE = 16; ///< un cercle occulteur est un 16-gone
			constexpr float32 MAILLE_MIN = 2.f;	///< px : l'affinage s'arrete la
			constexpr int32 POINTS_MAX = 90000;	///< budget d'evaluations de la maille de base

			struct NkRgb {
					float32 r = 0.f;
					float32 g = 0.f;
					float32 b = 0.f;
			};

			NkRgb DepuisRgba(uint32 c, float32 k) noexcept {
				NkRgb o;
				o.r = static_cast<float32>((c >> 24) & 0xFFu) / 255.f * k;
				o.g = static_cast<float32>((c >> 16) & 0xFFu) / 255.f * k;
				o.b = static_cast<float32>((c >> 8) & 0xFFu) / 255.f * k;
				return o;
			}

			float32 Croix(const NkVec2f &a, const NkVec2f &b) noexcept {
				return a.x * b.y - a.y * b.x;
			}

			float32 Borne01(float32 v) noexcept {
				return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
			}

			uint8 Octet(float32 v) noexcept {
				return static_cast<uint8>(Borne01(v) * 255.f + 0.5f);
			}

			/// Une lumiere prete au calcul : tout ce qui ne change pas d'un point a
			/// l'autre est fait UNE fois ici.
			struct NkLumiereCalc {
					NkTypeLumiere2D type = NkTypeLumiere2D::NK_PONCTUELLE;
					NkVec2f pos{0.f, 0.f};
					NkVec2f dir{0.f, -1.f};
					float32 portee = 1.f;
					float32 portee2 = 1.f;
					float32 attenuation = 2.f;
					float32 cosExt = -1.f; ///< cone : cos(ouverture)
					float32 cosInt = -1.f; ///< cone : cos(ouverture * (1 - douceur))
					NkRgb c;
					float32 halo = 0.f;
					bool ombres = false;
					int32 exclu = -1;		  ///< l'occulteur qui CONTIENT la lumiere
					uint32 seauDebut = 0u;	  ///< dans NkContexte::seaux (SEAUX + 1 entrees)
					float32 uMin = 0.f;		  ///< directionnelle : origine des seaux
					float32 uPas = 1.f;		  ///< directionnelle : largeur d'un seau
			};

			struct NkArete {
					NkVec2f a{0.f, 0.f};
					NkVec2f b{0.f, 0.f};
					int32 occ = 0;
			};

			struct NkOcculteur {
					uint32 debut = 0u; ///< premiere arete
					uint32 nombre = 0u;
					NkVec2f mn{0.f, 0.f};
					NkVec2f mx{0.f, 0.f};
			};

			/// Tout ce qu'une image d'eclairage calcule avant le premier sommet.
			class NkContexte {
				public:
					NkVector<NkLumiereCalc> lumieres;
					NkVector<NkArete> aretes;
					NkVector<NkOcculteur> occs;
					NkVector<uint32> seaux;		  ///< debuts (CSR), SEAUX + 1 par lumiere a ombre
					NkVector<uint32> seauAretes; ///< les aretes de chaque seau, bout a bout
					NkRgb ambiante;
					bool ombresScene = false;
					int32 ignorees = 0;

					/// `centre` et `rayon` : le disque du monde a eclairer (la vue).
					void Construire(NkScene &scene, const NkVec2f &centre, float32 rayon) {
						const NkEclairage2D &reglage = scene.Eclairage();
						ambiante = DepuisRgba(reglage.ambiante, 1.f);
						ombresScene = reglage.ombres;
						RecueillirLumieres(scene, centre, rayon);
						bool ombre = false;
						float32 porteeMax = 0.f;
						for (uint32 i = 0; i < lumieres.Size(); ++i) {
							ombre = ombre || lumieres[i].ombres;
							porteeMax = lumieres[i].portee > porteeMax ? lumieres[i].portee : porteeMax;
						}
						if (!ombre) {
							return;
						}
						// Un occulteur ne compte que s'il peut s'interposer entre une
						// lumiere qui touche la vue et un point de la vue.
						RecueillirOcculteurs(scene, reglage.masqueOcculteurs, centre, rayon + 2.f * porteeMax);
						for (uint32 i = 0; i < lumieres.Size(); ++i) {
							if (lumieres[i].ombres) {
								RangerSeaux(lumieres[i], centre, rayon);
							}
						}
					}

					/// La lumiere en `p` ; `masque` recoit le bit de chaque lumiere qui
					/// ATTEINT le point (les 32 premieres) : c'est lui qui decide
					/// d'affiner la maille.
					NkRgb Evaluer(const NkVec2f &p, uint32 &masque) const {
						NkRgb o = ambiante;
						masque = 0u;
						for (uint32 i = 0; i < lumieres.Size(); ++i) {
							const NkLumiereCalc &l = lumieres[i];
							float32 f = 1.f;
							if (l.type != NkTypeLumiere2D::NK_DIRECTIONNELLE) {
								const float32 dx = p.x - l.pos.x;
								const float32 dy = p.y - l.pos.y;
								const float32 d2 = dx * dx + dy * dy;
								if (d2 >= l.portee2) {
									continue; // AU-DELA DE LA PORTEE : rien, exactement
								}
								const float32 d = math::NkSqrt(d2);
								const float32 base = 1.f - d / l.portee;
								f = l.attenuation == 1.f ? base : math::NkPow(base, l.attenuation);
								if (l.type == NkTypeLumiere2D::NK_SPOT && d > 1.0e-5f) {
									const float32 c = (dx * l.dir.x + dy * l.dir.y) / d;
									if (c <= l.cosExt) {
										continue;
									}
									if (c < l.cosInt) {
										const float32 t = (c - l.cosExt) / (l.cosInt - l.cosExt);
										f *= t * t * (3.f - 2.f * t);
									}
								}
							}
							if (f <= 0.f) {
								continue;
							}
							if (l.ombres && Occulte(l, p)) {
								continue;
							}
							if (i < 32u) {
								masque |= 1u << i;
							}
							o.r += l.c.r * f;
							o.g += l.c.g * f;
							o.b += l.c.b * f;
						}
						return o;
					}

					/// Les bornes ecran des occulteurs (pour affiner les cellules qui les
					/// touchent : une petite caisse peut tenir entre quatre sommets).
					void BoitesEcran(const NkVue2D &cam, NkVector<NkRect> &out) const {
						out.Clear();
						for (uint32 i = 0; i < occs.Size(); ++i) {
							const NkVec2f a = cam.MondeVersEcran(occs[i].mn);
							const NkVec2f b = cam.MondeVersEcran(occs[i].mx);
							const NkVec2f c = cam.MondeVersEcran(NkVec2f(occs[i].mn.x, occs[i].mx.y));
							const NkVec2f d = cam.MondeVersEcran(NkVec2f(occs[i].mx.x, occs[i].mn.y));
							float32 x0 = a.x;
							float32 x1 = a.x;
							float32 y0 = a.y;
							float32 y1 = a.y;
							const NkVec2f pts[3] = {b, c, d};
							for (int32 k = 0; k < 3; ++k) {
								x0 = pts[k].x < x0 ? pts[k].x : x0;
								x1 = pts[k].x > x1 ? pts[k].x : x1;
								y0 = pts[k].y < y0 ? pts[k].y : y0;
								y1 = pts[k].y > y1 ? pts[k].y : y1;
							}
							out.PushBack(NkRect{x0, y0, x1 - x0, y1 - y0});
						}
					}

				private:
					void Ajouter(const NkLumiereCalc &l, const NkVec2f &centre) {
						if (lumieres.Size() < static_cast<uint32>(NK_ECLAIRAGE_MAX_LUMIERES)) {
							lumieres.PushBack(l);
							return;
						}
						// Le budget est plein : la plus LOINTAINE du centre de la vue
						// cede sa place. Une lumiere au milieu de l'ecran ne disparait
						// jamais au profit d'une lumiere hors champ.
						auto distance = [&centre](const NkLumiereCalc &x) {
							if (x.type == NkTypeLumiere2D::NK_DIRECTIONNELLE) {
								return -1.f;
							}
							const float32 dx = x.pos.x - centre.x;
							const float32 dy = x.pos.y - centre.y;
							return dx * dx + dy * dy;
						};
						uint32 pire = 0u;
						for (uint32 i = 1; i < lumieres.Size(); ++i) {
							if (distance(lumieres[i]) > distance(lumieres[pire])) {
								pire = i;
							}
						}
						++ignorees;
						if (distance(l) < distance(lumieres[pire])) {
							lumieres[pire] = l;
						}
					}

					void RecueillirLumieres(NkScene &scene, const NkVec2f &centre, float32 rayon) {
						const bool ombresScene = scene.Eclairage().ombres;
						scene.Monde().Query<NkTransform2D, NkLumiere2D>().ForEach(
							[&](ecs::NkEntityId id, NkTransform2D &t, NkLumiere2D &src) {
								// Une entite ETEINTE (NkUnkenyActif.h) n'eclaire pas.
								if (!src.actif || src.intensite <= 0.f || !scene.EstActive(id)) {
									return;
								}
								NkLumiereCalc l;
								l.type = src.type;
								l.pos = t.VersMonde(src.decalage);
								const float32 a = src.direction + t.rotation;
								l.dir = NkVec2f(math::NkCos(a), math::NkSin(a));
								l.portee = src.portee > 0.01f ? src.portee : 0.01f;
								l.portee2 = l.portee * l.portee;
								l.attenuation = src.attenuation > 0.05f ? src.attenuation : 0.05f;
								const float32 ouv = math::NkClamp(src.ouverture, 0.01f, PI);
								l.cosExt = math::NkCos(ouv);
								l.cosInt = math::NkCos(ouv * (1.f - math::NkClamp(src.douceur, 0.f, 0.99f)));
								l.c = DepuisRgba(src.couleur, src.intensite);
								l.halo = src.halo;
								l.ombres = ombresScene && src.ombres;
								if (l.type != NkTypeLumiere2D::NK_DIRECTIONNELLE) {
									const float32 dx = l.pos.x - centre.x;
									const float32 dy = l.pos.y - centre.y;
									const float32 r = l.portee + rayon;
									if (dx * dx + dy * dy >= r * r) {
										return; // son disque ne touche pas la vue
									}
								}
								Ajouter(l, centre);
							});
						// Les lumieres LIEES aux emetteurs : un feu eclaire, et vacille.
						const NkEffets2D &fx = scene.Effets();
						scene.Monde().Query<NkTransform2D, NkEmetteur2D>().ForEach(
							[&](ecs::NkEntityId id, NkTransform2D &t, NkEmetteur2D &e) {
								if (!e.eclaire || !scene.EstActive(id)) {
									return;
								}
								const float32 k = fx.FacteurLumiere(id.Pack(), e) * e.intensiteLumiere;
								if (k <= 0.f) {
									return;
								}
								NkLumiereCalc l;
								l.pos = t.VersMonde(e.decalage);
								l.portee = e.porteeLumiere > 0.01f ? e.porteeLumiere : 0.01f;
								l.portee2 = l.portee * l.portee;
								l.c = DepuisRgba(e.couleurLumiere, k);
								l.ombres = ombresScene && e.ombresLumiere;
								const float32 dx = l.pos.x - centre.x;
								const float32 dy = l.pos.y - centre.y;
								const float32 r = l.portee + rayon;
								if (dx * dx + dy * dy >= r * r) {
									return;
								}
								Ajouter(l, centre);
							});
					}

					void Polygone(const NkVec2f *pts, int32 n, int32 occ) {
						NkOcculteur o;
						o.debut = static_cast<uint32>(aretes.Size());
						o.nombre = static_cast<uint32>(n);
						o.mn = pts[0];
						o.mx = pts[0];
						for (int32 k = 0; k < n; ++k) {
							NkArete a;
							a.a = pts[k];
							a.b = pts[(k + 1) % n];
							a.occ = occ;
							aretes.PushBack(a);
							o.mn.x = pts[k].x < o.mn.x ? pts[k].x : o.mn.x;
							o.mn.y = pts[k].y < o.mn.y ? pts[k].y : o.mn.y;
							o.mx.x = pts[k].x > o.mx.x ? pts[k].x : o.mx.x;
							o.mx.y = pts[k].y > o.mx.y ? pts[k].y : o.mx.y;
						}
						occs.PushBack(o);
					}

					void RecueillirOcculteurs(NkScene &scene, uint32 masque, const NkVec2f &centre, float32 rayon) {
						scene.Monde().Query<NkTransform2D, NkCollisionneur2D>().ForEach(
							[&](ecs::NkEntityId id, NkTransform2D &t, NkCollisionneur2D &c) {
								// Une entite ETEINTE ne porte pas d'ombre.
								if (c.declencheur || (c.couche & masque) == 0u || !scene.EstActive(id)) {
									return;
								}
								// Le meme repere que NkDistanceForme2D et NkDessinerFormes :
								// rotation du transform, echelle 1, decalage local.
								float32 etendue = c.forme == NkForme2D::NK_BOITE
													  ? math::NkSqrt(c.demiTaille.x * c.demiTaille.x + c.demiTaille.y * c.demiTaille.y)
													  : c.rayon + (c.forme == NkForme2D::NK_CAPSULE ? c.demiTaille.x : 0.f);
								const bool sommets = c.forme == NkForme2D::NK_POLYGONE || c.forme == NkForme2D::NK_CHAINE;
								if (sommets) {
									// (2026-10-01) Polygone, chaine : le sommet le plus loin.
									etendue = 0.f;
									for (uint32 k = 0; k < NkNbSommetsCollision2D(c); ++k) {
										const NkVec2f &q = c.sommets[k];
										etendue = math::NkMax(etendue, math::NkSqrt(q.x * q.x + q.y * q.y));
									}
								}
								const NkVec2f o = t.VersMonde(c.decalage);
								const float32 dx = o.x - centre.x;
								const float32 dy = o.y - centre.y;
								const float32 r = rayon + etendue;
								if (dx * dx + dy * dy >= r * r) {
									return;
								}
								// (2026-10-01) La rotation PROPRE du collisionneur y est aussi.
								const float32 cr = math::NkCos(c.rotation), sr = math::NkSin(c.rotation);
								auto P = [&](float32 lx0, float32 ly0) {
									const float32 lx = lx0 * cr - ly0 * sr;
									const float32 ly = lx0 * sr + ly0 * cr;
									const float32 co = math::NkCos(t.rotation);
									const float32 si = math::NkSin(t.rotation);
									const float32 x = lx + c.decalage.x;
									const float32 y = ly + c.decalage.y;
									return NkVec2f(t.position.x + x * co - y * si, t.position.y + x * si + y * co);
								};
								const int32 occ = static_cast<int32>(occs.Size());
								NkVec2f pts[SEGMENTS_CERCLE + 2];
								if (sommets) {
									// Ferme : son contour. Ouvert (une pente, un trait) : un
									// polygone PLAT, aller puis retour, qui porte ombre des deux
									// cotes sans fermer le dessous.
									NkVec2f q[2u * NK_COLLISION_SOMMETS_MAX];
									const uint32 n = NkNbSommetsCollision2D(c);
									int32 m = 0;
									for (uint32 k = 0; k < n; ++k) {
										q[m++] = P(c.sommets[k].x, c.sommets[k].y);
									}
									const bool ferme = c.forme == NkForme2D::NK_POLYGONE || c.boucle;
									for (uint32 k = n; !ferme && k-- > 1u;) {
										q[m++] = P(c.sommets[k - 1u].x, c.sommets[k - 1u].y);
									}
									if (m >= 2) {
										Polygone(q, m, occ);
									}
								} else if (c.forme == NkForme2D::NK_BOITE) {
									const float32 hx = c.demiTaille.x;
									const float32 hy = c.demiTaille.y;
									pts[0] = P(-hx, -hy);
									pts[1] = P(hx, -hy);
									pts[2] = P(hx, hy);
									pts[3] = P(-hx, hy);
									Polygone(pts, 4, occ);
								} else if (c.forme == NkForme2D::NK_CERCLE) {
									for (int32 k = 0; k < SEGMENTS_CERCLE; ++k) {
										const float32 a = 2.f * PI * static_cast<float32>(k) / static_cast<float32>(SEGMENTS_CERCLE);
										pts[k] = P(c.rayon * math::NkCos(a), c.rayon * math::NkSin(a));
									}
									Polygone(pts, SEGMENTS_CERCLE, occ);
								} else {
									// La capsule : demi-cercle droit de -90 a +90 degres, puis
									// demi-cercle gauche de +90 a +270 — sens trigonometrique,
									// donc un polygone convexe.
									const int32 demi = SEGMENTS_CERCLE / 2;
									int32 n = 0;
									for (int32 k = 0; k <= demi; ++k) {
										const float32 a = -PI * 0.5f + PI * static_cast<float32>(k) / static_cast<float32>(demi);
										pts[n++] = P(c.demiTaille.x + c.rayon * math::NkCos(a), c.rayon * math::NkSin(a));
									}
									for (int32 k = 0; k < demi; ++k) {
										const float32 a = PI * 0.5f + PI * static_cast<float32>(k) / static_cast<float32>(demi);
										if (n < SEGMENTS_CERCLE + 2) {
											pts[n++] = P(-c.demiTaille.x + c.rayon * math::NkCos(a), c.rayon * math::NkSin(a));
										}
									}
									Polygone(pts, n, occ);
								}
							});
					}

					bool Dedans(const NkVec2f &p, int32 occ) const noexcept {
						const NkOcculteur &o = occs[static_cast<uint32>(occ)];
						for (uint32 k = 0; k < o.nombre; ++k) {
							const NkArete &a = aretes[o.debut + k];
							if (Croix(a.b - a.a, p - a.a) < -1.0e-6f) {
								return false;
							}
						}
						return true;
					}

					int32 SeauAngle(float32 a) const noexcept {
						int32 k = static_cast<int32>((a + PI) * (static_cast<float32>(SEAUX) / (2.f * PI)));
						k = k < 0 ? 0 : k;
						return k >= SEAUX ? SEAUX - 1 : k;
					}

					/// Range les aretes a portee de `l` dans ses seaux. Deux passes (compter
					/// puis remplir) : aucun tableau par seau, une seule allocation qui
					/// grandit d'une image a l'autre puis ne bouge plus.
					void RangerSeaux(NkLumiereCalc &l, const NkVec2f &centre, float32 rayon) {
						const bool dirle = l.type == NkTypeLumiere2D::NK_DIRECTIONNELLE;
						l.exclu = -1;
						if (!dirle) {
							for (uint32 o = 0; o < occs.Size(); ++o) {
								if (l.pos.x >= occs[o].mn.x && l.pos.x <= occs[o].mx.x && l.pos.y >= occs[o].mn.y &&
									l.pos.y <= occs[o].mx.y && Dedans(l.pos, static_cast<int32>(o))) {
									l.exclu = static_cast<int32>(o);
									break;
								}
							}
						}
						const NkVec2f n(-l.dir.y, l.dir.x); // la perpendiculaire (directionnelle)
						if (dirle) {
							const float32 uc = centre.x * n.x + centre.y * n.y;
							l.uMin = uc - rayon;
							l.uPas = (2.f * rayon) / static_cast<float32>(SEAUX);
							l.uPas = l.uPas > 1.0e-4f ? l.uPas : 1.0e-4f;
						}
						// La plage de seaux d'une arete, ou -1 si elle ne compte pas.
						auto Plage = [&](const NkArete &a, int32 &k0, int32 &k1) {
							if (a.occ == l.exclu) {
								return false;
							}
							if (dirle) {
								const float32 ua = a.a.x * n.x + a.a.y * n.y;
								const float32 ub = a.b.x * n.x + a.b.y * n.y;
								const float32 lo = ua < ub ? ua : ub;
								const float32 hi = ua < ub ? ub : ua;
								k0 = static_cast<int32>((lo - l.uMin) / l.uPas);
								k1 = static_cast<int32>((hi - l.uMin) / l.uPas);
								if (k1 < 0 || k0 >= SEAUX) {
									return false;
								}
								k0 = k0 < 0 ? 0 : k0;
								k1 = k1 >= SEAUX ? SEAUX - 1 : k1;
								return true;
							}
							// Distance de l'arete a la lumiere : au-dela de la portee, elle
							// ne peut rien masquer de ce que la lumiere touche.
							const NkVec2f ab = a.b - a.a;
							const NkVec2f al = l.pos - a.a;
							const float32 l2 = ab.x * ab.x + ab.y * ab.y;
							float32 t = l2 > 0.f ? (al.x * ab.x + al.y * ab.y) / l2 : 0.f;
							t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
							const NkVec2f q(a.a.x + ab.x * t - l.pos.x, a.a.y + ab.y * t - l.pos.y);
							if (q.x * q.x + q.y * q.y >= l.portee2) {
								return false;
							}
							const float32 aa = math::NkAtan2(a.a.y - l.pos.y, a.a.x - l.pos.x);
							const float32 ab2 = math::NkAtan2(a.b.y - l.pos.y, a.b.x - l.pos.x);
							float32 d = ab2 - aa;
							while (d > PI) {
								d -= 2.f * PI;
							}
							while (d < -PI) {
								d += 2.f * PI;
							}
							const float32 depart = d >= 0.f ? aa : ab2;
							k0 = SeauAngle(depart);
							// Le bout peut depasser +pi : on deborde au-dela de SEAUX, et
							// l'appelant replie (modulo).
							k1 = k0 + static_cast<int32>(math::NkAbs(d) * (static_cast<float32>(SEAUX) / (2.f * PI))) + 1;
							k1 = k1 > k0 + SEAUX - 1 ? k0 + SEAUX - 1 : k1;
							return true;
						};
						l.seauDebut = static_cast<uint32>(seaux.Size());
						const uint32 base = l.seauDebut;
						for (int32 k = 0; k <= SEAUX; ++k) {
							seaux.PushBack(0u);
						}
						for (uint32 i = 0; i < aretes.Size(); ++i) {
							int32 k0 = 0;
							int32 k1 = 0;
							if (!Plage(aretes[i], k0, k1)) {
								continue;
							}
							for (int32 k = k0; k <= k1; ++k) {
								++seaux[base + 1u + static_cast<uint32>(k % SEAUX)];
							}
						}
						// Les comptes deviennent des debuts (somme prefixe), decales de
						// la position deja atteinte dans seauAretes.
						seaux[base] = static_cast<uint32>(seauAretes.Size());
						for (int32 k = 1; k <= SEAUX; ++k) {
							seaux[base + static_cast<uint32>(k)] += seaux[base + static_cast<uint32>(k) - 1u];
						}
						const uint32 total = seaux[base + static_cast<uint32>(SEAUX)] - seaux[base];
						const uint32 origine = static_cast<uint32>(seauAretes.Size());
						seauAretes.Resize(origine + total);
						NkVector<uint32> curseur;
						curseur.Resize(static_cast<usize>(SEAUX));
						for (int32 k = 0; k < SEAUX; ++k) {
							curseur[static_cast<uint32>(k)] = seaux[base + static_cast<uint32>(k)];
						}
						for (uint32 i = 0; i < aretes.Size(); ++i) {
							int32 k0 = 0;
							int32 k1 = 0;
							if (!Plage(aretes[i], k0, k1)) {
								continue;
							}
							for (int32 k = k0; k <= k1; ++k) {
								const uint32 s = static_cast<uint32>(k % SEAUX);
								seauAretes[curseur[s]++] = i;
							}
						}
					}

					/// Le segment [p0, p1] coupe-t-il l'arete, strictement entre ses
					/// bouts (ni la lumiere ni le point lui-meme ne comptent) ?
					static bool Coupe(const NkVec2f &p0, const NkVec2f &p1, const NkArete &e) noexcept {
						const NkVec2f d = p1 - p0;
						const NkVec2f f = e.b - e.a;
						const float32 den = Croix(d, f);
						if (math::NkAbs(den) < 1.0e-12f) {
							return false;
						}
						const NkVec2f w = e.a - p0;
						const float32 t = Croix(w, f) / den;
						const float32 u = Croix(w, d) / den;
						return t > 1.0e-5f && t < 1.f - 1.0e-5f && u >= 0.f && u <= 1.f;
					}

					bool Occulte(const NkLumiereCalc &l, const NkVec2f &p) const {
						int32 k = 0;
						NkVec2f source = l.pos;
						if (l.type == NkTypeLumiere2D::NK_DIRECTIONNELLE) {
							const NkVec2f n(-l.dir.y, l.dir.x);
							k = static_cast<int32>((p.x * n.x + p.y * n.y - l.uMin) / l.uPas);
							if (k < 0 || k >= SEAUX) {
								return false;
							}
							// Vers la lumiere, sur la longueur maximale des ombres.
							source = NkVec2f(p.x - l.dir.x * l.portee, p.y - l.dir.y * l.portee);
						} else {
							k = SeauAngle(math::NkAtan2(p.y - l.pos.y, p.x - l.pos.x));
						}
						const uint32 d0 = seaux[l.seauDebut + static_cast<uint32>(k)];
						const uint32 d1 = seaux[l.seauDebut + static_cast<uint32>(k) + 1u];
						int32 dedansDe = -1; ///< le dernier occulteur reconnu comme CONTENANT p
						for (uint32 s = d0; s < d1; ++s) {
							const NkArete &e = aretes[seauAretes[s]];
							if (e.occ == dedansDe || !Coupe(source, p, e)) {
								continue;
							}
							// ⚠️ LA FACE D'UN MUR RESTE ECLAIREE. Un point DANS l'occulteur
							// dont il coupe le bord n'est pas a l'ombre de celui-ci : sans ce
							// test, tout collisionneur serait noir de l'interieur.
							if (Dedans(p, e.occ)) {
								dedansDe = e.occ;
								continue;
							}
							return true;
						}
						return false;
					}
			};

			// =================================================================
			// La maille
			// =================================================================
			struct NkSommet {
					uint32 couleur = 0u; ///< deja emballee pour NkGuiDrawList (NkGuiPackColor)
					uint32 masque = 0u;
			};

			class NkMaille {
				public:
					NkMaille(nkgui::NkGuiDrawList &dl, const NkContexte &cx, const NkVue2D &cam, bool voile)
						: mDl(dl), mCx(cx), mCam(cam), mVoile(voile) {
					}

					NkVector<NkRect> boitesOcc;
					NkVector<NkVec2f> sources; ///< les lumieres, a l'ecran
					int32 points = 0;
					int32 mailles = 0;

					NkSommet Sommet(float32 x, float32 y) {
						++points;
						uint32 masque = 0u;
						const NkRgb l = mCx.Evaluer(mCam.EcranVersMonde(NkVec2f(x, y)), masque);
						NkSommet s;
						s.masque = masque;
						if (mVoile) {
							// Le voile : noir, d'autant plus opaque que la lumiere est faible.
							// Pour une lumiere blanche, image x (1 - a) = image x lumiere :
							// exactement le multiply, en melange alpha.
							const float32 lum = 0.2126f * Borne01(l.r) + 0.7152f * Borne01(l.g) + 0.0722f * Borne01(l.b);
							s.couleur = nkgui::NkGuiPackColor(NkColor(0, 0, 0, Octet(1.f - lum)));
						} else {
							s.couleur = nkgui::NkGuiPackColor(NkColor(Octet(l.r), Octet(l.g), Octet(l.b), 255));
						}
						return s;
					}

					bool Affiner(float32 x, float32 y, float32 s, const NkSommet &a, const NkSommet &b, const NkSommet &c,
								 const NkSommet &d) const {
						if (s <= MAILLE_MIN) {
							return false;
						}
						if (a.masque != b.masque || a.masque != c.masque || a.masque != d.masque) {
							return true;
						}
						// Une source DANS la cellule : la pente y est la plus forte.
						for (uint32 i = 0; i < sources.Size(); ++i) {
							if (sources[i].x >= x - 1.f && sources[i].x <= x + s + 1.f && sources[i].y >= y - 1.f &&
								sources[i].y <= y + s + 1.f) {
								return true;
							}
						}
						// Le BORD d'un occulteur qui passe dans la cellule, sous une
						// lumiere : une petite caisse peut tenir entre les quatre sommets
						// et y jeter une ombre qu'aucun d'eux ne voit. Une cellule
						// entierement DANS la boite d'un occulteur n'est pas affinee : un
						// grand mur couvrirait sinon l'ecran de mailles de 2 px.
						if (a.masque != 0u) {
							for (uint32 i = 0; i < boitesOcc.Size(); ++i) {
								const NkRect &r = boitesOcc[i];
								const bool touche = !(r.x > x + s || r.x + r.w < x || r.y > y + s || r.y + r.h < y);
								const bool dedans = x >= r.x && x + s <= r.x + r.w && y >= r.y && y + s <= r.y + r.h;
								if (touche && !dedans) {
									return true;
								}
							}
						}
						return false;
					}

					/// Une cellule [x, x+s] x [y, y+s] dont les coins sont connus :
					/// a haut-gauche, b haut-droit, c bas-droit, d bas-gauche.
					void Cellule(float32 x, float32 y, float32 s, const NkSommet &a, const NkSommet &b, const NkSommet &c,
								 const NkSommet &d) {
						if (Affiner(x, y, s, a, b, c, d)) {
							const float32 h = s * 0.5f;
							const NkSommet haut = Sommet(x + h, y);
							const NkSommet droite = Sommet(x + s, y + h);
							const NkSommet bas = Sommet(x + h, y + s);
							const NkSommet gauche = Sommet(x, y + h);
							const NkSommet milieu = Sommet(x + h, y + h);
							Cellule(x, y, h, a, haut, milieu, gauche);
							Cellule(x + h, y, h, haut, b, droite, milieu);
							Cellule(x + h, y + h, h, milieu, droite, c, bas);
							Cellule(x, y + h, h, gauche, milieu, bas, d);
							return;
						}
						Quad(NkRect{x, y, s, s}, a.couleur, b.couleur, c.couleur, d.couleur);
					}

					void Quad(const NkRect &r, uint32 a, uint32 b, uint32 c, uint32 d) {
						++mailles;
						mDl.AddRectFilledMultiColor(r, Deballer(a), Deballer(b), Deballer(c), Deballer(d));
					}

				private:
					static NkColor Deballer(uint32 p) noexcept {
						return NkColor(static_cast<uint8>(p & 0xFFu), static_cast<uint8>((p >> 8) & 0xFFu),
									   static_cast<uint8>((p >> 16) & 0xFFu), static_cast<uint8>((p >> 24) & 0xFFu));
					}

					nkgui::NkGuiDrawList &mDl;
					const NkContexte &mCx;
					const NkVue2D &mCam;
					bool mVoile;
			};

			/// Un disque au bord fondu en eventail : le centre `c`, le bord transparent.
			void Halo(nkgui::NkGuiDrawList &dl, const NkVec2f &centre, float32 rayon, const NkColor &c) {
				if (rayon < 1.f) {
					return;
				}
				const NkColor bord(0, 0, 0, 0);
				const int32 n = 24;
				for (int32 k = 0; k < n; ++k) {
					const float32 a0 = 2.f * PI * static_cast<float32>(k) / static_cast<float32>(n);
					const float32 a1 = 2.f * PI * static_cast<float32>(k + 1) / static_cast<float32>(n);
					dl.AddTriangleMultiColor(centre, NkVec2f(centre.x + rayon * math::NkCos(a0), centre.y + rayon * math::NkSin(a0)),
											 NkVec2f(centre.x + rayon * math::NkCos(a1), centre.y + rayon * math::NkSin(a1)), c, bord,
											 bord);
				}
			}
		} // namespace

		// =====================================================================
		NkStatsEclairage2D NkDessinerEclairage(nkgui::NkGuiDrawList &dl, NkScene &scene) {
			NkStatsEclairage2D stats;
			const NkEclairage2D &reglage = scene.Eclairage();
			// ⚠️ LE CHEMIN ETEINT NE TOUCHE RIEN : pas un sommet, pas une commande,
			//    pas un changement de melange. C'est la promesse du chantier.
			if (!reglage.actif) {
				return stats;
			}
			const NkVue2D &cam = scene.Camera();
			const NkRect v = cam.Viseur();
			if (v.w < 1.f || v.h < 1.f) {
				return stats;
			}
			const NkVec2f centre = cam.EcranVersMonde(NkVec2f(v.x + v.w * 0.5f, v.y + v.h * 0.5f));
			const float32 rayon = math::NkSqrt(v.w * v.w + v.h * v.h) * 0.5f / cam.Zoom();

			NkContexte cx;
			cx.Construire(scene, centre, rayon);
			stats.lumieres = static_cast<int32>(cx.lumieres.Size());
			stats.ignorees = cx.ignorees;
			stats.occulteurs = static_cast<int32>(cx.occs.Size());

			const bool voile = reglage.mode == NkModeEclairage2D::NK_VOILE;
			NkMaille maille(dl, cx, cam, voile);
			cx.BoitesEcran(cam, maille.boitesOcc);
			for (uint32 i = 0; i < cx.lumieres.Size(); ++i) {
				if (cx.lumieres[i].type != NkTypeLumiere2D::NK_DIRECTIONNELLE) {
					maille.sources.PushBack(cam.MondeVersEcran(cx.lumieres[i].pos));
				}
			}

			// La maille de base : 4, 8 ou 16 px, alignee sur un pixel entier. Les
			// coordonnees restent ENTIERES ou demi-entieres jusqu'a 2 px : les
			// jonctions en T de l'affinage tombent exactement sur l'arete voisine,
			// sans fente ni recouvrement (un recouvrement se verrait : multiplie
			// deux fois).
			float32 s0 = reglage.maille <= 5.f ? 4.f : (reglage.maille <= 11.f ? 8.f : 16.f);
			const float32 x0 = math::NkFloor(v.x);
			const float32 y0 = math::NkFloor(v.y);
			int32 nx = static_cast<int32>(math::NkCeil((v.x + v.w - x0) / s0));
			int32 ny = static_cast<int32>(math::NkCeil((v.y + v.h - y0) / s0));
			// Le budget : sur un ecran immense, la maille grossit plutot que la
			// trame ne s'effondre.
			while ((nx + 1) * (ny + 1) > POINTS_MAX && s0 < 64.f) {
				s0 *= 2.f;
				nx = static_cast<int32>(math::NkCeil((v.x + v.w - x0) / s0));
				ny = static_cast<int32>(math::NkCeil((v.y + v.h - y0) / s0));
			}
			NkVector<NkSommet> grille;
			grille.Resize(static_cast<usize>((nx + 1) * (ny + 1)));
			for (int32 j = 0; j <= ny; ++j) {
				for (int32 i = 0; i <= nx; ++i) {
					grille[static_cast<uint32>(j * (nx + 1) + i)] =
						maille.Sommet(x0 + static_cast<float32>(i) * s0, y0 + static_cast<float32>(j) * s0);
				}
			}

			dl.PushClipRect(v, true);
			dl.PushBlend(voile ? NkGuiBlend::Alpha : NkGuiBlend::Multiply);
			for (int32 j = 0; j < ny; ++j) {
				const float32 y = y0 + static_cast<float32>(j) * s0;
				int32 i = 0;
				while (i < nx) {
					const NkSommet &a = grille[static_cast<uint32>(j * (nx + 1) + i)];
					const NkSommet &b = grille[static_cast<uint32>(j * (nx + 1) + i + 1)];
					const NkSommet &c = grille[static_cast<uint32>((j + 1) * (nx + 1) + i + 1)];
					const NkSommet &d = grille[static_cast<uint32>((j + 1) * (nx + 1) + i)];
					const float32 x = x0 + static_cast<float32>(i) * s0;
					const bool uniforme = a.couleur == b.couleur && a.couleur == c.couleur && a.couleur == d.couleur;
					if (!uniforme || maille.Affiner(x, y, s0, a, b, c, d)) {
						maille.Cellule(x, y, s0, a, b, c, d);
						++i;
						continue;
					}
					// Une BANDE : les cellules uniformes suivantes, de meme couleur.
					int32 fin = i + 1;
					while (fin < nx) {
						const NkSommet &a2 = grille[static_cast<uint32>(j * (nx + 1) + fin)];
						const NkSommet &b2 = grille[static_cast<uint32>(j * (nx + 1) + fin + 1)];
						const NkSommet &c2 = grille[static_cast<uint32>((j + 1) * (nx + 1) + fin + 1)];
						const NkSommet &d2 = grille[static_cast<uint32>((j + 1) * (nx + 1) + fin)];
						const float32 x2 = x0 + static_cast<float32>(fin) * s0;
						if (b2.couleur != a.couleur || c2.couleur != a.couleur ||
							maille.Affiner(x2, y, s0, a2, b2, c2, d2)) {
							break;
						}
						++fin;
					}
					maille.Quad(NkRect{x, y, static_cast<float32>(fin - i) * s0, s0}, a.couleur, a.couleur, a.couleur,
								a.couleur);
					i = fin;
				}
			}
			dl.PopBlend();

			// Les halos : ADDITIFS (ONE / ONE), apres la carte, pour qu'ils ne
			// soient pas multiplies par elle. En voile, chaque lumiere en porte un
			// faible : c'est la seule couleur que le voile ne sait pas donner.
			dl.PushBlend(NkGuiBlend::PlusLighter);
			for (uint32 i = 0; i < cx.lumieres.Size(); ++i) {
				const NkLumiereCalc &l = cx.lumieres[i];
				if (l.type == NkTypeLumiere2D::NK_DIRECTIONNELLE) {
					continue;
				}
				const float32 k = voile ? (l.halo > 0.3f ? l.halo : 0.3f) : l.halo;
				if (k <= 0.f) {
					continue;
				}
				const NkColor c(Octet(l.c.r * k), Octet(l.c.g * k), Octet(l.c.b * k), 255);
				Halo(dl, cam.MondeVersEcran(l.pos), cam.LongueurVersEcran(l.portee * (voile ? 0.8f : 0.45f)), c);
			}
			dl.PopBlend();
			dl.PopClipRect();
			stats.points = maille.points;
			stats.mailles = maille.mailles;
			return stats;
		}

		void NkLumiereAuPoint(NkScene &scene, const NkVec2f &monde, float32 rgb[3]) {
			if (!scene.Eclairage().actif) {
				// Eteint : rien n'est multiplie, tout se voit comme en plein jour.
				rgb[0] = 1.f;
				rgb[1] = 1.f;
				rgb[2] = 1.f;
				return;
			}
			NkContexte cx;
			cx.Construire(scene, monde, 0.01f);
			uint32 masque = 0u;
			const NkRgb l = cx.Evaluer(monde, masque);
			rgb[0] = l.r;
			rgb[1] = l.g;
			rgb[2] = l.b;
		}

	} // namespace unkeny
} // namespace nkentseu
