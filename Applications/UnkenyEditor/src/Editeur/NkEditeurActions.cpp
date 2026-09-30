// =============================================================================
// NkEditeurActions.cpp
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurViseur.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"

#include <cstdio>

namespace nkentseu {
	namespace editeur {

		namespace {
			void Aligner(NkEditeurModele &m) {
				// La photo et le fichier rendent les reglages des PARTICULES ; la
				// gravite des rigides les suit (elle doit rester UNE).
				if (m.scene.MondePhysique() != nullptr && m.scene.Particules() != nullptr) {
					const NkVec2f g = m.scene.Particules()->reglages.gravite;
					m.scene.MondePhysique()->SetGravity(math::NkVec3f(g.x, g.y, 0.f));
				}
			}

			/// L'oeil et le cadenas voyagent avec la scene : dans la photo de
			/// « Jouer » (Arreter les rend) et dans le fichier, objet « jeu ».
			/// ⚠️ LE NOM EST LA CLE DU FICHIER : le changer rend les scenes deja
			///    enregistrees muettes sur leurs drapeaux.
			void DeclarerDrapeaux(NkScene &s) {
				s.PhotographierAussi<NkDrapeauxEditeur>("UnkenyEditor.Drapeaux");
			}

			ecs::NkEntityId Statique(NkScene &s, const char *nom, const NkVec2f &c, const NkVec2f &demi, uint32 couleur) {
				const ecs::NkEntityId e = s.Creer(nom, c);
				NkSprite2D sp;
				sp.taille = NkVec2f(demi.x * 2.f, demi.y * 2.f);
				sp.couleur = couleur;
				sp.couche = -10;
				sp.visible = false; // dessine par sa FORME (NkDessinerFormes) ; le sprite garde la couleur
				s.Monde().Add<NkSprite2D>(e, sp);
				NkCollisionneur2D col;
				col.demiTaille = demi;
				s.Monde().Add<NkCollisionneur2D>(e, col);
				NkCorps2D b;
				b.type = NkTypeCorps::NK_STATIQUE;
				b.friction = 0.6f;
				s.AjouterCorps(e, b);
				return e;
			}
		} // namespace

		// =====================================================================
		void NkEditeurNouvelleScene(NkEditeurModele &m) {
			NkSceneConfig cfg;
			cfg.physique = true;   // l'editeur exerce la physique : c'est son role
			cfg.particules = true; // et la matiere : corps mous, fluides, atomes
			cfg.gravite = NkVec2f(0.f, -9.81f);
			m.scene.Init(cfg);
			DeclarerDrapeaux(m.scene);
			NkRemettreNomsSim();
			m.etat = NkEtatJeu::NK_EDITION;
			m.photo.valide = false;
			m.aSelection = false;
			m.pinceau = ecs::NkEntityId::Invalid();

			NkScene &s = m.scene;
			Statique(s, "Sol", NkVec2f(0.f, -4.5f), NkVec2f(11.f, 0.5f), 0x3E4756FFu);
			Statique(s, "Mur gauche", NkVec2f(-11.5f, 0.f), NkVec2f(0.5f, 5.f), 0x3E4756FFu);
			Statique(s, "Mur droit", NkVec2f(11.5f, 0.f), NkVec2f(0.5f, 5.f), 0x3E4756FFu);
			// Quelques caisses : elles rendent la physique VISIBLE des l'ouverture.
			for (int32 i = 0; i < 3; ++i) {
				NkPoserActeurSim(s, NkActeurSim::NK_CAISSE, NkVec2f(-6.f + static_cast<float32>(i) * 0.8f, -3.6f + static_cast<float32>(i) * 0.7f),
								 &m.ressources);
			}
			NkPoserActeurSim(s, NkActeurSim::NK_BLOB, NkVec2f(-1.5f, -1.f), &m.ressources);
			NkPoserActeurSim(s, NkActeurSim::NK_GELEE, NkVec2f(1.5f, -2.5f), &m.ressources);
			NkPoserActeurSim(s, NkActeurSim::NK_BALLON, NkVec2f(4.5f, -2.f), &m.ressources);
			NkPoserActeurSim(s, NkActeurSim::NK_EAU, NkVec2f(7.5f, -3.f), &m.ressources);
			NkPoserActeurSim(s, NkActeurSim::NK_TISSU, NkVec2f(-8.5f, 2.f), &m.ressources, 4.f);
		}

		// =====================================================================
		void NkEditeurJouer(NkEditeurModele &m) {
			if (!m.photo.valide) {
				m.scene.Photographier(m.photo); // ce que « Arreter » rendra
			}
			m.etat = NkEtatJeu::NK_JEU;
		}

		void NkEditeurPause(NkEditeurModele &m) {
			if (m.etat == NkEtatJeu::NK_JEU) {
				m.etat = NkEtatJeu::NK_PAUSE;
			}
		}

		void NkEditeurArreter(NkEditeurModele &m) {
			if (m.photo.valide) {
				m.scene.Restaurer(m.photo);
				Aligner(m);
			}
			m.photo.valide = false;
			m.etat = NkEtatJeu::NK_EDITION;
			// Les identifiants d'entite ont change (Restaurer) : une selection
			// retenue designerait une entite morte.
			m.aSelection = false;
			m.deplace = false;
			m.pinceau = ecs::NkEntityId::Invalid();
		}

		void NkEditeurUnPas(NkEditeurModele &m) {
			if (!m.photo.valide) {
				m.scene.Photographier(m.photo);
			}
			m.etat = NkEtatJeu::NK_PAUSE;
			m.scene.Pas(m.scene.Config().pasFixe);
		}

		void NkEditeurAvancer(NkEditeurModele &m, float32 dt) {
			m.messageAge += dt;
			if (m.etat == NkEtatJeu::NK_JEU && dt > 0.f) {
				m.scene.Pas(dt < 0.05f ? dt : 0.05f);
			}
			// Une selection dont l'entite a disparu (matiere gommee, tombee) : oubliee.
			if (m.aSelection && !m.scene.Monde().IsAlive(m.selection)) {
				m.aSelection = false;
				m.deplace = false;
			}
		}

		// =====================================================================
		ecs::NkEntityId NkEditeurPoser(NkEditeurModele &m, const NkVec2f &monde) {
			ecs::NkEntityId e;
			if (m.acteurSimple) {
				// L'entite elementaire d'origine de l'editeur : sprite + boite.
				m.graine = m.graine * 1664525u + 1013904223u;
				const uint32 teinte = 0x40404000u | ((m.graine >> 8) & 0x00BFBFBFu) | 0xFFu;
				e = m.scene.Creer("Entite", monde);
				NkSprite2D s;
				s.taille = NkVec2f(0.8f, 0.8f);
				s.couleur = teinte;
				m.scene.Monde().Add<NkSprite2D>(e, s);
				NkCollisionneur2D c;
				c.demiTaille = NkVec2f(0.4f, 0.4f);
				m.scene.Monde().Add<NkCollisionneur2D>(e, c);
				NkCorps2D b;
				m.scene.AjouterCorps(e, b);
			} else {
				e = NkPoserActeurSim(m.scene, m.acteur, monde, &m.ressources);
			}
			if (e.IsValid()) {
				m.selection = e;
				m.aSelection = true;
			}
			return e;
		}

		bool NkEditeurCentreSelection(const NkEditeurModele &m, NkVec2f &centre) {
			if (!m.aSelection) {
				return false;
			}
			NkScene &s = const_cast<NkScene &>(m.scene);
			if (const NkCorpsMou2D *mou = s.Monde().Get<NkCorpsMou2D>(m.selection)) {
				const physics::NkParticules2D *p = s.Particules();
				const int32 ci = p != nullptr ? p->IndexCorps(mou->corpsId) : -1;
				if (ci >= 0) {
					centre = p->CentreCorps(static_cast<uint32>(ci));
					return true;
				}
			}
			if (const NkTransform2D *t = s.Monde().Get<NkTransform2D>(m.selection)) {
				centre = t->position;
				return true;
			}
			return false;
		}

		// =====================================================================
		// La prise au clic
		// =====================================================================
		namespace {
			/// Un candidat a la prise : l'entite, OU elle se dessine (niveau, puis
			/// couche), et sa distance au point -- negative ou nulle : DEDANS.
			struct CandidatPrise {
					ecs::NkEntityId id;
					int32 niveau = 0; ///< l'ordre du viseur : 0 formes, 1 sprites, 2 matiere, 3 marqueurs
					int32 couche = 0; ///< sprites : NkSprite2D::couche
					float32 distance = 0.f;
					float32 aire = 0.f;
					NkVec2f centre{0.f, 0.f};
			};

			/// `a` est-il dessine PAR-DESSUS `b` ?
			bool Devant(const CandidatPrise &a, const CandidatPrise &b) noexcept {
				if (a.niveau != b.niveau) {
					return a.niveau > b.niveau;
				}
				if (a.couche != b.couche) {
					return a.couche > b.couche;
				}
				// A egalite, le plus PETIT : pose sur le grand, il ne se verrait
				// pas s'il etait dessous.
				return a.aire < b.aire;
			}

			/// Le meilleur coup au but, et le plus proche des autres.
			struct Prise {
					float32 tolerance = 0.f;
					CandidatPrise dedans;
					CandidatPrise proche;
					bool aDedans = false;
					bool aProche = false;

					void Proposer(const CandidatPrise &c) noexcept {
						if (c.distance <= 0.f) {
							if (!aDedans || Devant(c, dedans)) {
								dedans = c;
								aDedans = true;
							}
							return;
						}
						if (c.distance > tolerance) {
							return;
						}
						if (!aProche || c.distance < proche.distance || (c.distance == proche.distance && Devant(c, proche))) {
							proche = c;
							aProche = true;
						}
					}
			};

			float32 Longueur(float32 x, float32 y) noexcept {
				return math::NkSqrt(x * x + y * y);
			}

			float32 DistanceSegment(const NkVec2f &p, const NkVec2f &a, const NkVec2f &b) noexcept {
				const float32 vx = b.x - a.x;
				const float32 vy = b.y - a.y;
				const float32 l2 = vx * vx + vy * vy;
				float32 t = l2 > 0.f ? ((p.x - a.x) * vx + (p.y - a.y) * vy) / l2 : 0.f;
				t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
				return Longueur(p.x - (a.x + vx * t), p.y - (a.y + vy * t));
			}

			/// Le point est-il dans le triangle, quel que soit son sens ?
			/// ⚠️ Un triangle PLAT (maille ecrasee) ne contient rien : sans ce
			///    garde, ses trois produits sont nuls et tout point y « tombe ».
			bool DansTriangle(const NkVec2f &p, const NkVec2f &a, const NkVec2f &b, const NkVec2f &c) noexcept {
				const float32 aire = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
				if (aire > -1e-9f && aire < 1e-9f) {
					return false;
				}
				const float32 d1 = (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
				const float32 d2 = (p.x - c.x) * (b.y - c.y) - (b.x - c.x) * (p.y - c.y);
				const float32 d3 = (p.x - a.x) * (c.y - a.y) - (c.x - a.x) * (p.y - a.y);
				const bool negatif = d1 < 0.f || d2 < 0.f || d3 < 0.f;
				const bool positif = d1 > 0.f || d2 > 0.f || d3 > 0.f;
				return !(negatif && positif);
			}

			/// La regle du rendu (NkUnkenyRenduParticules.cpp, CelluleDechiree) :
			/// une maille de tissu dont un lien a casse n'est plus dessinee, donc
			/// plus prise.
			bool MailleDechiree(const physics::NkParticules2D &p, const physics::NkCorpsP2D &c, int32 i, int32 j) noexcept {
				const int32 nx = c.nx;
				const int32 ny = c.ny;
				const uint32 attendus = static_cast<uint32>(ny * (nx - 1) + (ny - 1) * nx);
				if (c.lienNombre < attendus) {
					return false;
				}
				const uint32 h0 = c.lienDebut + static_cast<uint32>(j * (nx - 1) + i);
				const uint32 h1 = c.lienDebut + static_cast<uint32>((j + 1) * (nx - 1) + i);
				const uint32 base = c.lienDebut + static_cast<uint32>(ny * (nx - 1));
				const uint32 v0 = base + static_cast<uint32>(j * nx + i);
				const uint32 v1 = base + static_cast<uint32>(j * nx + i + 1);
				return p.liens[h0].casse || p.liens[h1].casse || p.liens[v0].casse || p.liens[v1].casse;
			}

			/// La distance du point a la MATIERE du corps `ci` TELLE QUE LE VISEUR
			/// LA PEINT (NkDessinerCorpsMous) : l'interieur d'un ballon et les
			/// mailles d'une grille sont pleins, pas seulement leurs particules.
			/// `pixel` : la taille d'un pixel en metres (les traits ont une
			/// epaisseur minimale a l'ecran).
			float32 DistanceMatiere(const physics::NkParticules2D &p, uint32 ci, const NkVec2f &q, float32 pixel) noexcept {
				using physics::NkMateriauP2D;
				const physics::NkCorpsP2D &c = p.corps[ci];
				float32 d = 1e30f;
				if (c.nombre == 0u) {
					return d;
				}
				// Les particules, au rayon DESSINE : un fluide se peint 1,45 fois
				// plus large que sa particule (son halo translucide ne compte pas).
				// ⚠️ UN CORPS MAILLE (ballon, gelee, tissu) NE PEINT PAS SES
				//    PARTICULES : son bord passe par leurs CENTRES. Compter leur
				//    disque ferait « toucher » 4 cm hors du tissu, et le milieu d'une
				//    maille dechiree (7,8 cm des coins, 4 cm une fois les disques
				//    retires : 4,6 px a 122 px/m) serait pris comme s'il etait plein.
				const bool maille = c.mat == NkMateriauP2D::NK_BALLON || physics::NkEstGrilleP2D(c.mat);
				float32 facteur = maille ? 0.f : 1.f;
				if (physics::NkEstFluideP2D(c.mat)) {
					facteur = 1.45f;
				} else if (c.mat == NkMateriauP2D::NK_SABLE) {
					facteur = 1.05f;
				}
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					const physics::NkParticule2D &a = p.particules[i];
					const float32 di = Longueur(q.x - a.pos.x, q.y - a.pos.y) - a.rayon * facteur;
					d = di < d ? di : d;
				}
				switch (c.mat) {
					case NkMateriauP2D::NK_BALLON: {
						// PLEIN : le rendu le peint en eventail depuis son centre.
						if (c.nombre < 3u) {
							break;
						}
						const NkVec2f centre = p.CentreCorps(ci);
						for (uint32 k = 0; k < c.nombre; ++k) {
							const NkVec2f &a = p.particules[c.debut + k].pos;
							const NkVec2f &b = p.particules[c.debut + (k + 1u) % c.nombre].pos;
							if (DansTriangle(q, centre, a, b)) {
								return d < 0.f ? d : 0.f;
							}
						}
						break;
					}
					case NkMateriauP2D::NK_GELEE:
					case NkMateriauP2D::NK_TISSU: {
						// Les MAILLES : un clic entre quatre particules de tissu est
						// sur le tissu, pas dans le vide.
						const int32 nx = c.nx;
						const int32 ny = c.ny;
						if (nx < 2 || ny < 2 || c.nombre != static_cast<uint32>(nx * ny)) {
							break;
						}
						const bool tissu = c.mat == NkMateriauP2D::NK_TISSU;
						for (int32 j = 0; j + 1 < ny; ++j) {
							for (int32 i = 0; i + 1 < nx; ++i) {
								if (tissu && MailleDechiree(p, c, i, j)) {
									continue;
								}
								const NkVec2f &pa = p.particules[c.debut + static_cast<uint32>(j * nx + i)].pos;
								const NkVec2f &pb = p.particules[c.debut + static_cast<uint32>(j * nx + i + 1)].pos;
								const NkVec2f &pc = p.particules[c.debut + static_cast<uint32>((j + 1) * nx + i + 1)].pos;
								const NkVec2f &pd = p.particules[c.debut + static_cast<uint32>((j + 1) * nx + i)].pos;
								if (DansTriangle(q, pa, pb, pc) || DansTriangle(q, pa, pc, pd)) {
									return d < 0.f ? d : 0.f;
								}
							}
						}
						break;
					}
					case NkMateriauP2D::NK_CORDE: {
						// Le TRAIT : une planche de pont a l'epaisseur de ses
						// particules, une corde au moins 3,5 px (ou 8 cm).
						const bool pont = p.particules[c.debut].rayon > 0.06f;
						const float32 demiCorde = 1.75f * pixel > 0.04f ? 1.75f * pixel : 0.04f;
						for (uint32 i = c.debut; i + 1u < c.debut + c.nombre; ++i) {
							const physics::NkParticule2D &a = p.particules[i];
							const physics::NkParticule2D &b = p.particules[i + 1u];
							const float32 demi = pont ? a.rayon + pixel : demiCorde;
							const float32 ds = DistanceSegment(q, a.pos, b.pos) - demi;
							d = ds < d ? ds : d;
						}
						break;
					}
					default:
						break;
				}
				return d;
			}

			/// La distance au sprite tel que NkDessinerScene le peint : pivot,
			/// rotation ET echelle du transform. (L'ancienne prise ignorait pivot
			/// et rotation : une planche tournee de 45 degres se prenait dans sa
			/// boite droite, a cote de ce qu'on voyait.)
			float32 DistanceSprite(const NkTransform2D &t, const NkSprite2D &s, const NkVec2f &q) noexcept {
				const float32 co = math::NkCos(-t.rotation);
				const float32 si = math::NkSin(-t.rotation);
				const float32 dx = q.x - t.position.x;
				const float32 dy = q.y - t.position.y;
				const float32 lx = dx * co - dy * si;
				const float32 ly = dx * si + dy * co;
				// Le rectangle du rendu, [-pivot, 1 - pivot] * taille, a l'echelle.
				float32 x0 = -s.pivot.x * s.taille.x * t.echelle.x;
				float32 x1 = (1.f - s.pivot.x) * s.taille.x * t.echelle.x;
				float32 y0 = -s.pivot.y * s.taille.y * t.echelle.y;
				float32 y1 = (1.f - s.pivot.y) * s.taille.y * t.echelle.y;
				if (x0 > x1) {
					const float32 x = x0;
					x0 = x1;
					x1 = x;
				}
				if (y0 > y1) {
					const float32 y = y0;
					y0 = y1;
					y1 = y;
				}
				const float32 qx = math::NkAbs(lx - (x0 + x1) * 0.5f) - (x1 - x0) * 0.5f;
				const float32 qy = math::NkAbs(ly - (y0 + y1) * 0.5f) - (y1 - y0) * 0.5f;
				const float32 ex = qx > 0.f ? qx : 0.f;
				const float32 ey = qy > 0.f ? qy : 0.f;
				const float32 dedans = qx > qy ? qx : qy;
				return Longueur(ex, ey) + (dedans < 0.f ? dedans : 0.f);
			}

			float32 AireForme(const NkCollisionneur2D &c) noexcept {
				switch (c.forme) {
					case NkForme2D::NK_CERCLE:
						return 3.14159265f * c.rayon * c.rayon;
					case NkForme2D::NK_CAPSULE:
						return 4.f * c.demiTaille.x * c.rayon + 3.14159265f * c.rayon * c.rayon;
					default:
						return 4.f * c.demiTaille.x * c.demiTaille.y;
				}
			}

			void PoserDrapeau(NkEditeurModele &m, ecs::NkEntityId id, bool NkDrapeauxEditeur::*champ, bool valeur) {
				ecs::NkWorld &w = m.scene.Monde();
				if (!w.IsAlive(id)) {
					return;
				}
				NkDrapeauxEditeur d;
				if (const NkDrapeauxEditeur *x = w.Get<NkDrapeauxEditeur>(id)) {
					d = *x;
				}
				d.*champ = valeur;
				// Les deux a faux : le composant s'en va. Une scene dont personne n'a
				// touche l'oeil ni le cadenas s'enregistre comme avant.
				if (!d.cache && !d.verrou) {
					if (w.Has<NkDrapeauxEditeur>(id)) {
						w.Remove<NkDrapeauxEditeur>(id);
					}
					return;
				}
				w.Add<NkDrapeauxEditeur>(id, d);
			}
		} // namespace

		bool NkEditeurEstCache(NkEditeurModele &m, ecs::NkEntityId id) {
			const NkDrapeauxEditeur *d = m.scene.Monde().Get<NkDrapeauxEditeur>(id);
			return d != nullptr && d->cache;
		}

		bool NkEditeurEstVerrouille(NkEditeurModele &m, ecs::NkEntityId id) {
			const NkDrapeauxEditeur *d = m.scene.Monde().Get<NkDrapeauxEditeur>(id);
			return d != nullptr && d->verrou;
		}

		bool NkEditeurCacheDansLaVue(NkEditeurModele &m, ecs::NkEntityId id) {
			// En jeu et en pause, le jeu montre TOUT : l'oeil est celui de
			// l'editeur, pas un « cache en jeu » (NkDrapeauxEditeur).
			return m.etat == NkEtatJeu::NK_EDITION && NkEditeurEstCache(m, id);
		}

		void NkEditeurCacher(NkEditeurModele &m, ecs::NkEntityId id, bool cache) {
			PoserDrapeau(m, id, &NkDrapeauxEditeur::cache, cache);
		}

		void NkEditeurVerrouiller(NkEditeurModele &m, ecs::NkEntityId id, bool verrou) {
			PoserDrapeau(m, id, &NkDrapeauxEditeur::verrou, verrou);
		}

		bool NkEditeurSansVisuel(NkEditeurModele &m, ecs::NkEntityId id) {
			ecs::NkWorld &w = m.scene.Monde();
			if (w.Has<NkCorpsMou2D>(id) || w.Has<NkCollisionneur2D>(id)) {
				return false;
			}
			const NkSprite2D *s = w.Get<NkSprite2D>(id);
			return s == nullptr || !s->visible;
		}

		bool NkEditeurPrendreSous(NkEditeurModele &m, const NkVec2f &monde, ecs::NkEntityId &sortie, NkVec2f *centre) {
			NkScene &s = m.scene;
			ecs::NkWorld &w = s.Monde();
			// Un pixel en metres : la camera de la scene EST celle du viseur, et
			// son zoom n'est jamais nul (NkVue2D::PoserZoom).
			const float32 pixel = 1.f / s.Camera().Zoom();
			Prise prise;
			prise.tolerance = NK_PRISE_TOLERANCE_PX * pixel;
			auto exclue = [&](ecs::NkEntityId id) { return NkEditeurEstVerrouille(m, id) || NkEditeurCacheDansLaVue(m, id); };

			// La matiere (niveau 2).
			if (const physics::NkParticules2D *p = s.Particules()) {
				w.Query<NkCorpsMou2D>().ForEach([&](ecs::NkEntityId id, NkCorpsMou2D &mou) {
					if (!mou.visible || exclue(id)) {
						return;
					}
					const int32 ci = p->IndexCorps(mou.corpsId);
					if (ci < 0) {
						return;
					}
					const uint32 k = static_cast<uint32>(ci);
					CandidatPrise c;
					c.id = id;
					c.niveau = 2;
					c.distance = DistanceMatiere(*p, k, monde, pixel);
					NkVec2f mn, mx;
					p->BoiteCorps(k, mn, mx);
					c.aire = (mx.x - mn.x) * (mx.y - mn.y);
					c.centre = p->CentreCorps(k);
					prise.Proposer(c);
				});
			}
			// Les sprites visibles (niveau 1, par couche).
			w.Query<NkTransform2D, NkSprite2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &t, NkSprite2D &sp) {
				if (!sp.visible || exclue(id)) {
					return;
				}
				CandidatPrise c;
				c.id = id;
				c.niveau = 1;
				c.couche = sp.couche;
				c.distance = DistanceSprite(t, sp, monde);
				c.aire = math::NkAbs(sp.taille.x * t.echelle.x * sp.taille.y * t.echelle.y);
				c.centre = t.position;
				prise.Proposer(c);
			});
			// Les formes (niveau 0) : ce que NkDessinerFormes peint, soit toute
			// entite a collisionneur SAUF celle qu'un sprite texture remplace.
			w.Query<NkTransform2D, NkCollisionneur2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &t, NkCollisionneur2D &col) {
				const NkSprite2D *sp = w.Get<NkSprite2D>(id);
				if ((sp != nullptr && sp->visible && sp->texId != 0u) || exclue(id)) {
					return;
				}
				CandidatPrise c;
				c.id = id;
				c.niveau = 0;
				c.distance = NkDistanceForme2D(t, col, monde);
				c.aire = AireForme(col);
				c.centre = t.position;
				prise.Proposer(c);
			});
			// Les marqueurs des entites sans visuel (niveau 3) : le viseur ne les
			// dessine qu'en EDITION.
			if (m.etat == NkEtatJeu::NK_EDITION) {
				const float32 rayon = NK_MARQUEUR_RAYON_PX * pixel;
				w.Query<NkTransform2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &t) {
					if (!NkEditeurSansVisuel(m, id) || exclue(id)) {
						return;
					}
					CandidatPrise c;
					c.id = id;
					c.niveau = 3;
					c.distance = Longueur(monde.x - t.position.x, monde.y - t.position.y) - rayon;
					c.aire = rayon * rayon;
					c.centre = t.position;
					prise.Proposer(c);
				});
			}

			const CandidatPrise *choix = prise.aDedans ? &prise.dedans : (prise.aProche ? &prise.proche : nullptr);
			if (choix == nullptr) {
				return false;
			}
			sortie = choix->id;
			if (centre != nullptr) {
				*centre = choix->centre;
			}
			return true;
		}

		bool NkEditeurChoisirSous(NkEditeurModele &m, const NkVec2f &monde, NkVec2f *centre) {
			ecs::NkEntityId trouve;
			if (!NkEditeurPrendreSous(m, monde, trouve, centre)) {
				m.aSelection = false;
				return false;
			}
			m.selection = trouve;
			m.aSelection = true;
			return true;
		}

		void NkEditeurDeplacer(NkEditeurModele &m, const NkVec2f &cible) {
			if (!m.aSelection) {
				return;
			}
			NkScene &s = m.scene;
			if (const NkCorpsMou2D *mou = s.Monde().Get<NkCorpsMou2D>(m.selection)) {
				physics::NkParticules2D *p = s.Particules();
				const int32 ci = p != nullptr ? p->IndexCorps(mou->corpsId) : -1;
				if (ci >= 0) {
					const NkVec2f c = p->CentreCorps(static_cast<uint32>(ci));
					p->Translater(static_cast<uint32>(ci), cible - c);
					// Le transform suit tout de suite (sans attendre un Pas).
					if (NkTransform2D *t = s.Monde().Get<NkTransform2D>(m.selection)) {
						t->position = cible;
					}
				}
				return;
			}
			// ⚠️ TELEPORTER, pas ecrire le transform : le solveur garderait
			// l'ancienne position et l'objet reviendrait d'un coup au pas suivant.
			s.TeleporterEntite(m.selection, cible);
		}

		bool NkEditeurBoiteSelection(NkEditeurModele &m, NkVec2f &mn, NkVec2f &mx) {
			if (!m.aSelection) {
				return false;
			}
			NkScene &s = m.scene;
			if (const NkCorpsMou2D *mou = s.Monde().Get<NkCorpsMou2D>(m.selection)) {
				const physics::NkParticules2D *p = s.Particules();
				const int32 ci = p != nullptr ? p->IndexCorps(mou->corpsId) : -1;
				if (ci >= 0) {
					p->BoiteCorps(static_cast<uint32>(ci), mn, mx);
					return true;
				}
				return false;
			}
			const NkTransform2D *t = s.Monde().Get<NkTransform2D>(m.selection);
			if (t == nullptr) {
				return false;
			}
			float32 hw = 0.5f, hh = 0.5f;
			if (const NkCollisionneur2D *c = s.Monde().Get<NkCollisionneur2D>(m.selection)) {
				hw = c->forme == NkForme2D::NK_CERCLE ? c->rayon : c->demiTaille.x + (c->forme == NkForme2D::NK_CAPSULE ? c->rayon : 0.f);
				hh = c->forme == NkForme2D::NK_BOITE ? c->demiTaille.y : c->rayon;
				// Une forme TOURNEE : la boite englobante couvre sa diagonale.
				if (t->rotation != 0.f) {
					const float32 r = math::NkSqrt(hw * hw + hh * hh);
					hw = hh = r;
				}
			} else if (const NkSprite2D *sp = s.Monde().Get<NkSprite2D>(m.selection)) {
				hw = sp->taille.x * 0.5f;
				hh = sp->taille.y * 0.5f;
			}
			mn = NkVec2f(t->position.x - hw, t->position.y - hh);
			mx = NkVec2f(t->position.x + hw, t->position.y + hh);
			return true;
		}

		bool NkEditeurCliquerSelection(NkEditeurModele &m, const NkVec2f &monde, NkVec2f *centre) {
			if (NkEditeurChoisirSous(m, monde, centre)) {
				return true;
			}
			// Le vide veut dire « plus rien » : ni selection, ni glisser en cours.
			m.aSelection = false;
			m.deplace = false;
			return false;
		}

		bool NkEditeurPeutDeplacer(const NkEditeurModele &m) noexcept {
			return m.etat == NkEtatJeu::NK_EDITION;
		}

		bool NkEditeurCadrerSelection(NkEditeurModele &m) {
			if (!m.aSelection || !m.scene.Monde().IsAlive(m.selection)) {
				return false;
			}
			NkVec2f centre;
			NkVec2f taille;
			if (!NkEditeurZoneACadrer(m, centre, taille)) {
				return false;
			}
			m.scene.Camera().Cadrer(centre, taille);
			return true;
		}

		bool NkEditeurZoneACadrer(NkEditeurModele &m, NkVec2f &centre, NkVec2f &taille) {
			NkVec2f mn(0.f, 0.f);
			NkVec2f mx(0.f, 0.f);
			bool ok = false;
			if (m.aSelection && m.scene.Monde().IsAlive(m.selection)) {
				ok = NkEditeurBoiteSelection(m, mn, mx);
			} else {
				// TOUTE LA SCENE : l'union des boites de chaque entite, par la meme
				// fonction que le cadre de selection -- une seule idee de « la
				// boite d'une entite », matiere comprise.
				const ecs::NkEntityId garde = m.selection;
				const bool avait = m.aSelection;
				NkVector<ecs::NkEntityId> ids;
				m.scene.Entites(ids);
				for (uint32 i = 0; i < ids.Size(); ++i) {
					m.selection = ids[i];
					m.aSelection = true;
					NkVec2f a(0.f, 0.f);
					NkVec2f b(0.f, 0.f);
					if (!NkEditeurBoiteSelection(m, a, b)) {
						continue;
					}
					if (!ok) {
						mn = a;
						mx = b;
						ok = true;
						continue;
					}
					mn.x = a.x < mn.x ? a.x : mn.x;
					mn.y = a.y < mn.y ? a.y : mn.y;
					mx.x = b.x > mx.x ? b.x : mx.x;
					mx.y = b.y > mx.y ? b.y : mx.y;
				}
				m.selection = garde;
				m.aSelection = avait;
			}
			if (!ok) {
				return false;
			}
			centre = NkVec2f((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f);
			// La marge : 30 % autour, et jamais moins de 3 m -- une entite d'un
			// centimetre cadree « bord a bord » remplirait l'ecran d'un pixel.
			const float32 w = (mx.x - mn.x) * 1.3f;
			const float32 h = (mx.y - mn.y) * 1.3f;
			taille = NkVec2f(w > 3.f ? w : 3.f, h > 3.f ? h : 3.f);
			return true;
		}

		bool NkEditeurTourner(NkEditeurModele &m, float32 delta) {
			if (!m.aSelection || !m.scene.Monde().IsAlive(m.selection)) {
				return false;
			}
			ecs::NkWorld &w = m.scene.Monde();
			const ecs::NkEntityId id = m.selection;
			const float32 co = math::NkCos(delta);
			const float32 si = math::NkSin(delta);
			if (const NkCorpsMou2D *mou = w.Get<NkCorpsMou2D>(id)) {
				physics::NkParticules2D *p = m.scene.Particules();
				const int32 ci = p != nullptr ? p->IndexCorps(mou->corpsId) : -1;
				if (ci < 0) {
					return false;
				}
				NkVec2f c(0.f, 0.f);
				NkEditeurCentreSelection(m, c);
				const physics::NkCorpsP2D &corps = p->corps[static_cast<uint32>(ci)];
				auto tourne = [&](const NkVec2f &v) {
					return NkVec2f(v.x * co - v.y * si, v.x * si + v.y * co);
				};
				for (uint32 i = corps.debut; i < corps.debut + corps.nombre; ++i) {
					physics::NkParticule2D &q = p->particules[i];
					const NkVec2f r = tourne(q.pos - c);
					const NkVec2f rp = tourne(q.prec - c);
					q.pos = c + r;
					q.prec = c + rp;
					q.vit = tourne(q.vit);
				}
				return true;
			}
			NkTransform2D *t = w.Get<NkTransform2D>(id);
			if (t == nullptr) {
				return false;
			}
			t->rotation += delta;
			if (w.Has<NkCorps2D>(id)) {
				m.scene.ActualiserCorps(id); // le corps prend la nouvelle orientation
			}
			return true;
		}

		bool NkEditeurMettreAEchelle(NkEditeurModele &m, const NkVec2f &f) {
			if (!m.aSelection || !m.scene.Monde().IsAlive(m.selection) || f.x <= 0.f || f.y <= 0.f) {
				return false;
			}
			ecs::NkWorld &w = m.scene.Monde();
			const ecs::NkEntityId id = m.selection;
			if (const NkCorpsMou2D *mou = w.Get<NkCorpsMou2D>(id)) {
				physics::NkParticules2D *p = m.scene.Particules();
				const int32 ci = p != nullptr ? p->IndexCorps(mou->corpsId) : -1;
				if (ci < 0) {
					return false;
				}
				NkVec2f c(0.f, 0.f);
				NkEditeurCentreSelection(m, c);
				physics::NkCorpsP2D &corps = p->corps[static_cast<uint32>(ci)];
				for (uint32 i = corps.debut; i < corps.debut + corps.nombre; ++i) {
					physics::NkParticule2D &q = p->particules[i];
					q.pos = NkVec2f(c.x + (q.pos.x - c.x) * f.x, c.y + (q.pos.y - c.y) * f.y);
					q.prec = NkVec2f(c.x + (q.prec.x - c.x) * f.x, c.y + (q.prec.y - c.y) * f.y);
					q.repos = NkVec2f(q.repos.x * f.x, q.repos.y * f.y);
				}
				// Les LONGUEURS DE REPOS suivent, lien par lien : un lien horizontal
				// s'allonge de f.x, un vertical de f.y, un oblique entre les deux.
				for (uint32 k = 0; k < p->liens.Size(); ++k) {
					physics::NkLien2D &l = p->liens[k];
					if (l.corps != static_cast<uint32>(ci)) {
						continue;
					}
					const NkVec2f d = p->particules[l.b].pos - p->particules[l.a].pos;
					// `d` est deja a la nouvelle echelle : on retrouve la direction d'avant.
					const float32 ax = d.x / f.x;
					const float32 ay = d.y / f.y;
					const float32 avant = math::NkSqrt(ax * ax + ay * ay);
					const float32 apres = math::NkSqrt(d.x * d.x + d.y * d.y);
					if (avant > 1.0e-6f) {
						const float32 k2 = apres / avant;
						l.repos *= k2;
						l.reposInitial *= k2;
					}
				}
				corps.aireRepos *= f.x * f.y; // la pression d'un ballon vise sa nouvelle aire
				return true;
			}
			if (NkSprite2D *s = w.Get<NkSprite2D>(id)) {
				s->taille = NkVec2f(s->taille.x * f.x, s->taille.y * f.y);
			}
			if (NkCollisionneur2D *col = w.Get<NkCollisionneur2D>(id)) {
				col->decalage = NkVec2f(col->decalage.x * f.x, col->decalage.y * f.y);
				switch (col->forme) {
					case NkForme2D::NK_BOITE:
						col->demiTaille = NkVec2f(col->demiTaille.x * f.x, col->demiTaille.y * f.y);
						break;
					case NkForme2D::NK_CAPSULE:
						col->demiTaille.x *= f.x;
						col->rayon *= f.y;
						break;
					default:
						// Un cercle reste un cercle : il prend la moyenne GEOMETRIQUE,
						// qui vaut le facteur exact quand il est uniforme.
						col->rayon *= math::NkSqrt(f.x * f.y);
						break;
				}
				if (w.Has<NkCorps2D>(id)) {
					m.scene.ActualiserCorps(id); // la forme vit aussi dans le solveur
				}
			}
			return true;
		}

		float32 NkEditeurAccrocher(float32 v, float32 pas) noexcept {
			if (pas <= 0.f) {
				return v;
			}
			const float32 q = v / pas;
			const float32 arrondi = static_cast<float32>(static_cast<int64>(q < 0.f ? q - 0.5f : q + 0.5f));
			return arrondi * pas;
		}

		void NkEditeurSupprimerSelection(NkEditeurModele &m) {
			if (!m.aSelection) {
				return;
			}
			m.scene.Detruire(m.selection);
			m.aSelection = false;
			m.deplace = false;
		}

		bool NkEditeurEffacerSous(NkEditeurModele &m, const NkVec2f &monde) {
			const bool avait = m.aSelection;
			const ecs::NkEntityId avant = m.selection;
			if (!NkEditeurChoisirSous(m, monde)) {
				m.aSelection = avait;
				m.selection = avant;
				return false;
			}
			NkEditeurSupprimerSelection(m);
			return true;
		}

		// =====================================================================
		// Entites et composants
		// =====================================================================
		ecs::NkEntityId NkEditeurCreerEntite(NkEditeurModele &m, const char *nom, const NkVec2f &position) {
			const ecs::NkEntityId e = m.scene.Creer(nom != nullptr && nom[0] != '\0' ? nom : "Entite", position);
			m.selection = e;
			m.aSelection = true;
			return e;
		}

		bool NkEditeurRenommer(NkEditeurModele &m, ecs::NkEntityId id, const char *nom) {
			if (!m.scene.Monde().IsAlive(id) || nom == nullptr) {
				return false;
			}
			NkEtiquette e;
			std::snprintf(e.nom, sizeof(e.nom), "%s", nom);
			m.scene.Monde().Set<NkEtiquette>(id, e);
			return true;
		}

		ecs::NkEntityId NkEditeurDupliquer(NkEditeurModele &m) {
			if (!m.aSelection) {
				return ecs::NkEntityId::Invalid();
			}
			ecs::NkWorld &w = m.scene.Monde();
			const ecs::NkEntityId src = m.selection;
			if (w.Has<NkCorpsMou2D>(src)) {
				NkEditeurAnnoncer(m, "La matiere d'un corps mou ne se duplique pas : posez-en un autre");
				return ecs::NkEntityId::Invalid();
			}
			const NkTransform2D *t = w.Get<NkTransform2D>(src);
			if (t == nullptr) {
				return ecs::NkEntityId::Invalid();
			}
			char nom[32] = "Copie";
			if (const NkEtiquette *et = w.Get<NkEtiquette>(src)) {
				std::snprintf(nom, sizeof(nom), "%.26s copie", et->nom);
			}
			NkTransform2D tt = *t;
			tt.position = tt.position + NkVec2f(0.5f, 0.5f);
			const ecs::NkEntityId e = m.scene.Creer(nom, tt.position);
			w.Set<NkTransform2D>(e, tt);
			if (const NkSprite2D *s = w.Get<NkSprite2D>(src)) {
				NkSprite2D c = *s;
				w.Add<NkSprite2D>(e, c);
			}
			if (const NkCollisionneur2D *s = w.Get<NkCollisionneur2D>(src)) {
				NkCollisionneur2D c = *s;
				w.Add<NkCollisionneur2D>(e, c);
			}
			if (const NkAnimSprite2D *s = w.Get<NkAnimSprite2D>(src)) {
				NkAnimSprite2D c = *s;
				w.Add<NkAnimSprite2D>(e, c);
			}
			if (const NkSource2D *s = w.Get<NkSource2D>(src)) {
				NkSource2D c = *s;
				c.voix = 0u;
				c.lance = false;
				w.Add<NkSource2D>(e, c);
			}
			if (const NkCorps2D *s = w.Get<NkCorps2D>(src)) {
				NkCorps2D c = *s;
				c.corpsId = 0;
				m.scene.AjouterCorps(e, c);
			}
			m.selection = e;
			m.aSelection = true;
			return e;
		}

		const char *NkComposantEditeurNom(NkComposantEditeur c) noexcept {
			switch (c) {
				case NkComposantEditeur::NK_SPRITE:
					return "Sprite";
				case NkComposantEditeur::NK_COLLISIONNEUR:
					return "Collisionneur";
				case NkComposantEditeur::NK_CORPS:
					return "Corps rigide";
				case NkComposantEditeur::NK_CORPS_MOU:
					return "Corps mou";
				case NkComposantEditeur::NK_SOURCE:
					return "Source sonore";
				case NkComposantEditeur::NK_ANIMATION:
					return "Animation";
				default:
					return "";
			}
		}

		bool NkEditeurAUnComposant(NkEditeurModele &m, ecs::NkEntityId id, NkComposantEditeur c) {
			ecs::NkWorld &w = m.scene.Monde();
			switch (c) {
				case NkComposantEditeur::NK_SPRITE:
					return w.Has<NkSprite2D>(id);
				case NkComposantEditeur::NK_COLLISIONNEUR:
					return w.Has<NkCollisionneur2D>(id);
				case NkComposantEditeur::NK_CORPS:
					return w.Has<NkCorps2D>(id);
				case NkComposantEditeur::NK_CORPS_MOU:
					return w.Has<NkCorpsMou2D>(id);
				case NkComposantEditeur::NK_SOURCE:
					return w.Has<NkSource2D>(id);
				case NkComposantEditeur::NK_ANIMATION:
					return w.Has<NkAnimSprite2D>(id);
				default:
					return false;
			}
		}

		bool NkEditeurPeutAjouter(NkEditeurModele &m, ecs::NkEntityId id, NkComposantEditeur c) {
			if (!m.scene.Monde().IsAlive(id) || NkEditeurAUnComposant(m, id, c)) {
				return false;
			}
			ecs::NkWorld &w = m.scene.Monde();
			switch (c) {
				case NkComposantEditeur::NK_CORPS:
					// Un corps rigide ET une matiere : deux solveurs qui se
					// disputeraient la meme position.
					return m.scene.PhysiqueActive() && !w.Has<NkCorpsMou2D>(id);
				case NkComposantEditeur::NK_CORPS_MOU:
					return m.scene.Particules() != nullptr && !w.Has<NkCorps2D>(id);
				default:
					return true;
			}
		}

		bool NkEditeurAjouterComposant(NkEditeurModele &m, ecs::NkEntityId id, NkComposantEditeur c, NkActeurSim matiere) {
			if (!NkEditeurPeutAjouter(m, id, c)) {
				return false;
			}
			ecs::NkWorld &w = m.scene.Monde();
			switch (c) {
				case NkComposantEditeur::NK_SPRITE: {
					NkSprite2D s;
					s.couleur = 0xC8CCD8FFu;
					w.Add<NkSprite2D>(id, s);
					return true;
				}
				case NkComposantEditeur::NK_COLLISIONNEUR: {
					NkCollisionneur2D col;
					// A la taille du sprite, s'il y en a un : ce qu'on voit touche.
					if (const NkSprite2D *s = w.Get<NkSprite2D>(id)) {
						col.demiTaille = NkVec2f(s->taille.x * 0.5f, s->taille.y * 0.5f);
					}
					w.Add<NkCollisionneur2D>(id, col);
					return true;
				}
				case NkComposantEditeur::NK_CORPS: {
					if (!w.Has<NkCollisionneur2D>(id)) {
						NkEditeurAjouterComposant(m, id, NkComposantEditeur::NK_COLLISIONNEUR);
					}
					NkCorps2D b;
					return m.scene.AjouterCorps(id, b);
				}
				case NkComposantEditeur::NK_CORPS_MOU: {
					if (NkActeurSimInfo(matiere).rigide) {
						return false;
					}
					const NkTransform2D *t = w.Get<NkTransform2D>(id);
					const int32 ci = NkCreerMatiereSim(m.scene, matiere, t != nullptr ? t->position : NkVec2f(0.f, 0.f));
					return m.scene.AttacherCorpsMou(id, ci, NkActeurSimInfo(matiere).couleur);
				}
				case NkComposantEditeur::NK_SOURCE: {
					NkSource2D s;
					w.Add<NkSource2D>(id, s);
					return true;
				}
				case NkComposantEditeur::NK_ANIMATION: {
					NkAnimSprite2D a;
					a.nbClips = 1;
					w.Add<NkAnimSprite2D>(id, a);
					return true;
				}
				default:
					return false;
			}
		}

		bool NkEditeurRetirerComposant(NkEditeurModele &m, ecs::NkEntityId id, NkComposantEditeur c) {
			if (!NkEditeurAUnComposant(m, id, c)) {
				return false;
			}
			ecs::NkWorld &w = m.scene.Monde();
			switch (c) {
				case NkComposantEditeur::NK_SPRITE:
					w.Remove<NkSprite2D>(id);
					return true;
				case NkComposantEditeur::NK_COLLISIONNEUR:
					// Le corps rigide s'appuie sur sa forme : il part avec elle.
					m.scene.RetirerCorps(id);
					w.Remove<NkCollisionneur2D>(id);
					return true;
				case NkComposantEditeur::NK_CORPS:
					return m.scene.RetirerCorps(id);
				case NkComposantEditeur::NK_CORPS_MOU:
					return m.scene.RetirerCorpsMou(id);
				case NkComposantEditeur::NK_SOURCE:
					w.Remove<NkSource2D>(id);
					return true;
				case NkComposantEditeur::NK_ANIMATION:
					w.Remove<NkAnimSprite2D>(id);
					return true;
				default:
					return false;
			}
		}

		const char *NkEditeurTypeDe(NkScene &scene, ecs::NkEntityId id) {
			if (scene.Monde().Has<NkCorpsMou2D>(id)) {
				return "mou";
			}
			if (const NkCorps2D *c = scene.Monde().Get<NkCorps2D>(id)) {
				return c->type == NkTypeCorps::NK_DYNAMIQUE ? "rigide" : "decor";
			}
			return "entite";
		}

		// =====================================================================
		const char *NkEditeurChemin(NkEditeurModele &m) {
			if (m.chemin.Empty()) {
				const NkPath base = NkDirectory::GetAppDataDirectory();
				if (!base.ToString().Empty()) {
					const NkPath dossier = base / "UnkenyEditor";
					NkDirectory::CreateRecursive(dossier);
					m.chemin = (dossier / "scene.nkscene").ToString();
				} else {
					m.chemin = "scene.nkscene";
				}
			}
			return m.chemin.CStr();
		}

		void NkEditeurAnnoncer(NkEditeurModele &m, const char *texte) {
			m.message = texte;
			m.messageAge = 0.f;
		}

		bool NkEditeurSauver(NkEditeurModele &m) {
			// On enregistre ce qu'on EDITE : en jeu, la photo d'avant « Jouer »
			// serait perdue au profit d'un instant de simulation. On arrete d'abord.
			if (m.etat != NkEtatJeu::NK_EDITION) {
				NkEditeurArreter(m);
			}
			const bool ok = NkSauverSceneFichier(m.scene, NkEditeurChemin(m), &m.textures);
			NkEditeurAnnoncer(m, ok ? "Scene enregistree" : "Enregistrement impossible");
			return ok;
		}

		bool NkEditeurOuvrir(NkEditeurModele &m) {
			NkString erreur;
			// AVANT la lecture : un composant non declare serait saute en silence.
			DeclarerDrapeaux(m.scene);
			const bool ok = NkChargerSceneFichier(m.scene, NkEditeurChemin(m), &m.textures, &erreur);
			if (ok) {
				Aligner(m);
				m.etat = NkEtatJeu::NK_EDITION;
				m.photo.valide = false;
				m.aSelection = false;
				NkEditeurAnnoncer(m, "Scene ouverte");
			} else {
				NkEditeurAnnoncer(m, erreur.Empty() ? "Aucune scene enregistree" : erreur.CStr());
			}
			return ok;
		}

	} // namespace editeur
} // namespace nkentseu
