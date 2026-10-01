// =============================================================================
// NkEditeurActions.cpp
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurLumiere.h"
#include "Editeur/NkEditeurPlacer.h"
#include "Editeur/NkEditeurViseur.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"
#include "Unkeny/Partie/NkUnkenyPartie.h"

#include <cstdio>
#include <cstring>

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
				s.PhotographierAussi<NkEchelleEditeur>("UnkenyEditor.Echelle");
				s.PhotographierAussi<NkEteintsEditeur>("UnkenyEditor.Eteints");
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
			NkEditeurOublierHistorique(m);
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
				// Les effets repartent de leur graine : ce qu'on voit en jeu ne
				// depend pas de la duree de l'apercu en edition.
				m.scene.Effets().Vider();
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
			// (2026-09-29) Son IDENTITE, elle, traverse Restaurer
			// (m.scene.EntiteParUid) : la garder serait possible. Ce n'est PAS
			// fait — le banc (e5) tient « Arreter oublie la selection », et changer
			// ce comportement est une decision de Rihen, pas un ajout.
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
			// (2026-10-01, R34) Les effets ne TOURNENT qu'en jeu ; en edition, seuls
			// ceux qui ont « Aperçu en édition » (NkEffets2D::edition).
			m.scene.Effets().edition = m.etat != NkEtatJeu::NK_JEU;
			if (m.etat == NkEtatJeu::NK_JEU) {
				// LA trame du jeu, celle que joue aussi le joueur autonome
				// (Unkeny/Partie) : jouer dans l'editeur, c'est le jeu. Elle garde
				// dt > 0 et le plafond de trame, et Pas propage la hierarchie.
				NkAvancerPartie(m.scene, dt);
			} else {
				// En EDITION rien ne fait Pas : deplacer un parent au gizmo doit
				// pourtant emporter ses enfants a l'ecran, a cette trame.
				m.scene.PropagerHierarchie();
				if (m.etat == NkEtatJeu::NK_EDITION && dt > 0.f) {
					// L'APERCU des effets en edition (2026-09-30) : un feu pose brule
					// deja. Les particules visuelles ne touchent a rien de la scene
					// (temoin f6) ; ni corps, ni matiere, ni transform ne bougent.
					// APRES la hierarchie : un feu porte par un parent deplace nait
					// la ou il est.
					m.scene.Effets().Avancer(m.scene, dt < 0.05f ? dt : 0.05f);
				}
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
					int32 niveau = 0; ///< l'ordre du viseur : 0 formes, 1 sprites, 2 matiere, 3 marqueurs, 4 icones
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
					case NkForme2D::NK_POLYGONE:
					case NkForme2D::NK_CHAINE: {
						// (2026-10-01) L'aire du contour (une chaine ouverte : sa boite).
						const uint32 n = NkNbSommetsCollision2D(c);
						const float32 a = math::NkAbs(NkAireSignee2D(c.sommets, n));
						if (a > 1.0e-6f) {
							return a;
						}
						NkVec2f mn(0.f, 0.f), mx(0.f, 0.f);
						for (uint32 i = 0; i < n; ++i) {
							mn = NkVec2f(math::NkMin(mn.x, c.sommets[i].x), math::NkMin(mn.y, c.sommets[i].y));
							mx = NkVec2f(math::NkMax(mx.x, c.sommets[i].x), math::NkMax(mx.y, c.sommets[i].y));
						}
						return (mx.x - mn.x) * (mx.y - mn.y);
					}
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

		namespace {
			/// Un ANCETRE porte-t-il ce drapeau ? Borne : une hierarchie mal
			/// formee (une boucle que Rattacher aurait laissee passer) ne fige pas
			/// l'editeur.
			bool Herite(NkEditeurModele &m, ecs::NkEntityId id, bool NkDrapeauxEditeur::*champ) {
				ecs::NkEntityId p = m.scene.Parent(id);
				for (int32 k = 0; k < 64 && p.IsValid(); ++k) {
					const NkDrapeauxEditeur *d = m.scene.Monde().Get<NkDrapeauxEditeur>(p);
					if (d != nullptr && d->*champ) {
						return true;
					}
					p = m.scene.Parent(p);
				}
				return false;
			}
		} // namespace

		bool NkEditeurCacheHerite(NkEditeurModele &m, ecs::NkEntityId id) {
			return Herite(m, id, &NkDrapeauxEditeur::cache);
		}

		bool NkEditeurVerrouHerite(NkEditeurModele &m, ecs::NkEntityId id) {
			return Herite(m, id, &NkDrapeauxEditeur::verrou);
		}

		bool NkEditeurCacheDansLaVue(NkEditeurModele &m, ecs::NkEntityId id) {
			// En jeu et en pause, le jeu montre TOUT : l'oeil est celui de
			// l'editeur, pas un « cache en jeu » (NkDrapeauxEditeur). Un parent
			// cache cache sa descendance, comme dans UE5.
			return m.etat == NkEtatJeu::NK_EDITION && (NkEditeurEstCache(m, id) || NkEditeurCacheHerite(m, id));
		}

		bool NkEditeurVerrouilleDansLaVue(NkEditeurModele &m, ecs::NkEntityId id) {
			// Un parent fige fige ses enfants : les deplacer a la main ferait
			// mentir le cadenas du parent, qui les emporte avec lui.
			return NkEditeurEstVerrouille(m, id) || NkEditeurVerrouHerite(m, id);
		}

		void NkEditeurCacher(NkEditeurModele &m, ecs::NkEntityId id, bool cache) {
			PoserDrapeau(m, id, &NkDrapeauxEditeur::cache, cache);
		}

		void NkEditeurVerrouiller(NkEditeurModele &m, ecs::NkEntityId id, bool verrou) {
			PoserDrapeau(m, id, &NkDrapeauxEditeur::verrou, verrou);
		}

		bool NkEditeurSansVisuel(NkEditeurModele &m, ecs::NkEntityId id) {
			ecs::NkWorld &w = m.scene.Monde();
			// Une lumiere, un emetteur ont LEUR icone (NkEditeurLumiere.h) : pas de
			// losange par-dessus.
			if (w.Has<NkCorpsMou2D>(id) || w.Has<NkCollisionneur2D>(id) || w.Has<NkLumiere2D>(id) || w.Has<NkEmetteur2D>(id)) {
				return false;
			}
			// (2026-10-01) Une FORME 2D visible est un visuel, comme un sprite.
			if (const NkRenduForme2D *f = w.Get<NkRenduForme2D>(id)) {
				if (f->visible) {
					return false;
				}
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
			// (2026-10-01) Une entite ETEINTE n'est pas rendue : elle ne se prend pas
			// non plus dans la vue (l'Outliner la choisit toujours).
			auto exclue = [&](ecs::NkEntityId id) {
				return NkEditeurVerrouilleDansLaVue(m, id) || NkEditeurCacheDansLaVue(m, id) || !s.EstActive(id);
			};

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
			// (2026-10-01) Les FORMES 2D visibles (niveau 1, par couche, comme les
			// sprites : NkDessinerScene les trie ensemble). Prises sur leur CONTOUR
			// dessine : le creux d'une etoile n'est pas l'etoile.
			w.Query<NkTransform2D, NkRenduForme2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &t, NkRenduForme2D &f) {
				if (!f.visible || exclue(id)) {
					return;
				}
				CandidatPrise c;
				c.id = id;
				c.niveau = 1;
				c.couche = f.couche;
				c.distance = NkDistanceRenduForme2D(t, f, monde);
				const NkVec2f d = NkDemiBoiteForme2D(f);
				c.aire = math::NkAbs(4.f * d.x * t.echelle.x * d.y * t.echelle.y);
				c.centre = t.position;
				prise.Proposer(c);
			});
			// Les formes (niveau 0) : ce que NkDessinerFormes peint, soit toute
			// entite a collisionneur SAUF celle qu'un sprite texture remplace (et,
			// depuis le 2026-10-01, celle qui a sa FORME 2D : prise ci-dessus).
			w.Query<NkTransform2D, NkCollisionneur2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &t, NkCollisionneur2D &col) {
				const NkSprite2D *sp = w.Get<NkSprite2D>(id);
				if ((sp != nullptr && sp->visible && sp->texId != 0u) || exclue(id) || w.Has<NkRenduForme2D>(id)) {
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

			// Les ICONES des lumieres et des emetteurs (niveau 4, 2026-09-30) : peintes
			// par-dessus tout, et une lumiere n'a souvent rien d'autre a cliquer. Le
			// rayon est celui de NkEditeurIconeSous, qui ne les montre qu'hors jeu.
			{
				ecs::NkEntityId icone;
				if (NkEditeurIconeSous(m, monde, 10.f, icone) && !exclue(icone)) {
					CandidatPrise c;
					c.id = icone;
					c.niveau = 4;
					c.distance = 0.f;
					if (const NkTransform2D *t = w.Get<NkTransform2D>(icone)) {
						c.centre = t->position;
					}
					prise.Proposer(c);
				}
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
			const NkCollisionneur2D *col = s.Monde().Get<NkCollisionneur2D>(m.selection);
			if (const NkRenduForme2D *f = s.Monde().Get<NkRenduForme2D>(m.selection)) {
				// (2026-10-01) La FORME 2D : la boite de son contour dessine.
				const NkVec2f d = NkDemiBoiteForme2D(*f);
				hw = d.x * math::NkAbs(t->echelle.x);
				hh = d.y * math::NkAbs(t->echelle.y);
				if (t->rotation != 0.f) {
					const float32 r = math::NkSqrt(hw * hw + hh * hh);
					hw = hh = r;
				}
			} else if (col != nullptr && (col->forme == NkForme2D::NK_POLYGONE || col->forme == NkForme2D::NK_CHAINE)) {
				// Polygone, chaine : la boite de leur contour, rotations comprises.
				NkVec2f pts[NK_COLLISION_SOMMETS_MAX + 72u];
				bool ferme = true;
				const uint32 n = NkContourCollisionneur2D(*t, *col, pts, NK_COLLISION_SOMMETS_MAX + 72u, ferme);
				if (n > 0u) {
					mn = mx = pts[0];
					for (uint32 i = 1; i < n; ++i) {
						mn = NkVec2f(math::NkMin(mn.x, pts[i].x), math::NkMin(mn.y, pts[i].y));
						mx = NkVec2f(math::NkMax(mx.x, pts[i].x), math::NkMax(mx.y, pts[i].y));
					}
					return true;
				}
			} else if (const NkCollisionneur2D *c = col) {
				hw = c->forme == NkForme2D::NK_CERCLE ? c->rayon : c->demiTaille.x + (c->forme == NkForme2D::NK_CAPSULE ? c->rayon : 0.f);
				hh = c->forme == NkForme2D::NK_BOITE ? c->demiTaille.y : c->rayon;
				// Une forme TOURNEE : la boite englobante couvre sa diagonale.
				if (t->rotation != 0.f || c->rotation != 0.f) {
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

		namespace {
			bool CuireEchelle(NkEditeurModele &m, const NkVec2f &f);
		} // namespace

		NkVec2f NkEditeurEchelle(NkEditeurModele &m, ecs::NkEntityId id) {
			const NkEchelleEditeur *e = m.scene.Monde().Get<NkEchelleEditeur>(id);
			return e != nullptr ? e->facteur : NkVec2f(1.f, 1.f);
		}

		bool NkEditeurMettreAEchelle(NkEditeurModele &m, const NkVec2f &f) {
			if (!CuireEchelle(m, f)) {
				return false;
			}
			// Le CUMUL que lit le Transform. Revenu a 1 x 1, le composant s'en va :
			// une scene qu'on n'a pas mise a l'echelle s'enregistre comme avant.
			ecs::NkWorld &w = m.scene.Monde();
			const NkVec2f e = NkEditeurEchelle(m, m.selection);
			NkEchelleEditeur n;
			n.facteur = NkVec2f(e.x * f.x, e.y * f.y);
			const bool unite = math::NkAbs(n.facteur.x - 1.f) < 1.0e-4f && math::NkAbs(n.facteur.y - 1.f) < 1.0e-4f;
			if (unite) {
				if (w.Has<NkEchelleEditeur>(m.selection)) {
					w.Remove<NkEchelleEditeur>(m.selection);
				}
			} else {
				w.Add<NkEchelleEditeur>(m.selection, n);
			}
			return true;
		}

		bool NkEditeurPoserEchelle(NkEditeurModele &m, ecs::NkEntityId id, const NkVec2f &facteur) {
			const NkVec2f cible(facteur.x < 0.05f ? 0.05f : facteur.x, facteur.y < 0.05f ? 0.05f : facteur.y);
			const NkVec2f e = NkEditeurEchelle(m, id);
			const ecs::NkEntityId avant = m.selection;
			const bool avait = m.aSelection;
			m.selection = id;
			m.aSelection = true;
			const bool ok = NkEditeurMettreAEchelle(m, NkVec2f(cible.x / e.x, cible.y / e.y));
			m.selection = avant;
			m.aSelection = avait;
			return ok;
		}

		namespace {
		bool CuireEchelle(NkEditeurModele &m, const NkVec2f &f) {
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
			// (2026-10-01) La FORME 2D : sa boite, ses points ; les epaisseurs et
			// l'arrondi prennent la moyenne geometrique (un trait reste un trait).
			if (NkRenduForme2D *fo = w.Get<NkRenduForme2D>(id)) {
				const float32 g = math::NkSqrt(f.x * f.y);
				fo->taille = NkVec2f(fo->taille.x * f.x, fo->taille.y * f.y);
				for (uint32 i = 0; i < NK_FORME_POINTS_MAX; ++i) {
					fo->points[i] = NkVec2f(fo->points[i].x * f.x, fo->points[i].y * f.y);
				}
				fo->epaisseur *= g;
				fo->epaisseurContour *= g;
				fo->arrondi *= g;
			}
			if (NkCollisionneur2D *col = w.Get<NkCollisionneur2D>(id)) {
				col->decalage = NkVec2f(col->decalage.x * f.x, col->decalage.y * f.y);
				// Polygone, chaine : chaque sommet (repere du collisionneur ; sa
				// rotation propre est le plus souvent nulle).
				for (uint32 i = 0; i < NK_COLLISION_SOMMETS_MAX; ++i) {
					col->sommets[i] = NkVec2f(col->sommets[i].x * f.x, col->sommets[i].y * f.y);
				}
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

		} // namespace

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
			if (const NkLumiere2D *s = w.Get<NkLumiere2D>(src)) {
				NkLumiere2D c = *s;
				w.Add<NkLumiere2D>(e, c);
			}
			if (const NkEmetteur2D *s = w.Get<NkEmetteur2D>(src)) {
				NkEmetteur2D c = *s;
				// Une autre graine : une copie qui brulerait a l'unisson de
				// l'original se verrait comme un defaut.
				m.graine = m.graine * 1664525u + 1013904223u;
				c.graine = m.graine;
				w.Add<NkEmetteur2D>(e, c);
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
					return "Son";
				case NkComposantEditeur::NK_ANIMATION:
					return "Animation";
				case NkComposantEditeur::NK_LUMIERE:
					return "Lumière";
				case NkComposantEditeur::NK_EMETTEUR:
					return "Émetteur";
				case NkComposantEditeur::NK_FORME:
					return "Forme 2D";
				case NkComposantEditeur::NK_ANIMATEUR:
					return "Animateur";
				case NkComposantEditeur::NK_ANCRAGE:
					return "Ancrage à l'écran";
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
				case NkComposantEditeur::NK_LUMIERE:
					return w.Has<NkLumiere2D>(id);
				case NkComposantEditeur::NK_EMETTEUR:
					return w.Has<NkEmetteur2D>(id);
				case NkComposantEditeur::NK_FORME:
					return w.Has<NkRenduForme2D>(id);
				case NkComposantEditeur::NK_ANIMATEUR:
					return w.Has<NkAnimateur2D>(id);
				case NkComposantEditeur::NK_ANCRAGE:
					return w.Has<NkAncrageEcran2D>(id);
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
				case NkComposantEditeur::NK_LUMIERE:
					return NkEditeurAjouterLumiere(m, id, NkTypeLumiere2D::NK_PONCTUELLE);
				case NkComposantEditeur::NK_EMETTEUR:
					return NkEditeurAjouterEffet(m, id, NkPresetEffet2D::NK_FEU);
				case NkComposantEditeur::NK_FORME:
					// Un rectangle : le genre se change dans sa carte (Rendu).
					return NkEditeurAjouterForme(m, id, NkGenreForme2D::NK_RECTANGLE);
				case NkComposantEditeur::NK_ANIMATEUR:
					// Le modele toujours present ; le menu en propose d'autres
					// (NkEditeurAjouterAnimateur, les .nkanimctl du Contenu).
					return NkEditeurAjouterAnimateur(m, id, "plateforme", nullptr);
				case NkComposantEditeur::NK_ANCRAGE: {
					NkAncrageEcran2D a;
					w.Add<NkAncrageEcran2D>(id, a);
					return true;
				}
				default:
					return false;
			}
		}

		bool NkEditeurAjouterAnimateur(NkEditeurModele &m, ecs::NkEntityId id, const char *modele, const char *fichier) {
			ecs::NkWorld &w = m.scene.Monde();
			if (!w.IsAlive(id) || modele == nullptr || modele[0] == '\0') {
				return false;
			}
			// Un .nkanimctl du Contenu : lu et enregistre sous son nom avant tout.
			if (fichier != nullptr && fichier[0] != '\0' && !NkChargerModeleAnimateur(modele, fichier)) {
				NkEditeurAnnoncer(m, NkString::Format("Contrôleur illisible : %s", fichier).CStr());
				return false;
			}
			// L'animateur choisit le CLIP d'une animation de sprites : sans elle, il
			// tournerait a vide. On l'ajoute avec lui.
			if (!w.Has<NkAnimSprite2D>(id)) {
				NkAnimSprite2D a;
				a.nbClips = 1;
				w.Add<NkAnimSprite2D>(id, a);
			}
			const NkAnimateur2D a = NkCreerAnimateur2D(modele);
			if (w.Has<NkAnimateur2D>(id)) {
				w.Set<NkAnimateur2D>(id, a);
			} else {
				w.Add<NkAnimateur2D>(id, a);
			}
			NkEditeurAnnoncer(m, NkString::Format("Animateur ajouté : %s", modele).CStr());
			return true;
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
				case NkComposantEditeur::NK_LUMIERE:
					w.Remove<NkLumiere2D>(id);
					return true;
				case NkComposantEditeur::NK_EMETTEUR:
					// Ses particules finissent leur vie : retirer un feu ne fait pas
					// disparaitre d'un coup les flammeches deja en l'air.
					w.Remove<NkEmetteur2D>(id);
					return true;
				case NkComposantEditeur::NK_FORME:
					// Le collisionneur reste : il ne suit plus rien, il se regle a la main.
					w.Remove<NkRenduForme2D>(id);
					return true;
				case NkComposantEditeur::NK_ANIMATEUR:
					w.Remove<NkAnimateur2D>(id);
					return true;
				case NkComposantEditeur::NK_ANCRAGE:
					w.Remove<NkAncrageEcran2D>(id);
					return true;
				default:
					return false;
			}
		}

		// =====================================================================
		// Les cartes de l'inspecteur
		// =====================================================================
		bool NkComposantDeCarte(NkCarteEditeur c, NkComposantEditeur &sortie) noexcept {
			if (c > NkCarteEditeur::NK_TRANSFORM && c < NkCarteEditeur::NK_ANIMATEUR) {
				sortie = static_cast<NkComposantEditeur>(static_cast<uint8>(c) - 1u);
				return true;
			}
			if (c == NkCarteEditeur::NK_LUMIERE) {
				sortie = NkComposantEditeur::NK_LUMIERE;
				return true;
			}
			if (c == NkCarteEditeur::NK_EMETTEUR) {
				sortie = NkComposantEditeur::NK_EMETTEUR;
				return true;
			}
			if (c == NkCarteEditeur::NK_ANIMATEUR) {
				sortie = NkComposantEditeur::NK_ANIMATEUR;
				return true;
			}
			if (c == NkCarteEditeur::NK_FORME) {
				sortie = NkComposantEditeur::NK_FORME;
				return true;
			}
			if (c == NkCarteEditeur::NK_ANCRAGE) {
				sortie = NkComposantEditeur::NK_ANCRAGE;
				return true;
			}
			return false;
		}

		const char *NkCarteEditeurNom(NkCarteEditeur c) noexcept {
			switch (c) {
				case NkCarteEditeur::NK_TRANSFORM:
					return "Transform";
				case NkCarteEditeur::NK_ANIMATEUR:
					return "Animateur";
				case NkCarteEditeur::NK_HIERARCHIE:
					return "Hiérarchie";
				default: {
					NkComposantEditeur comp;
					return NkComposantDeCarte(c, comp) ? NkComposantEditeurNom(comp) : "";
				}
			}
		}

		bool NkEditeurAUneCarte(NkEditeurModele &m, ecs::NkEntityId id, NkCarteEditeur c) {
			ecs::NkWorld &w = m.scene.Monde();
			if (!w.IsAlive(id)) {
				return false;
			}
			switch (c) {
				case NkCarteEditeur::NK_TRANSFORM:
					return w.Has<NkTransform2D>(id);
				case NkCarteEditeur::NK_ANIMATEUR:
					return w.Has<NkAnimateur2D>(id);
				case NkCarteEditeur::NK_HIERARCHIE:
					return true;
				default: {
					NkComposantEditeur comp;
					return NkComposantDeCarte(c, comp) && NkEditeurAUnComposant(m, id, comp);
				}
			}
		}

		bool NkEditeurCarteSeCopie(NkCarteEditeur c) noexcept {
			return c == NkCarteEditeur::NK_TRANSFORM || c == NkCarteEditeur::NK_SPRITE || c == NkCarteEditeur::NK_COLLISIONNEUR ||
				   c == NkCarteEditeur::NK_CORPS || c == NkCarteEditeur::NK_SOURCE || c == NkCarteEditeur::NK_ANIMATION ||
				   c == NkCarteEditeur::NK_LUMIERE || c == NkCarteEditeur::NK_EMETTEUR || c == NkCarteEditeur::NK_FORME ||
				   c == NkCarteEditeur::NK_ANCRAGE;
		}

		namespace {
			/// Deplace `id` comme le fait le gizmo (NkEditeurDeplacer vise la
			/// selection) : un rigide TELEPORTE, une matiere TRANSLATEE.
			void DeplacerEntite(NkEditeurModele &m, ecs::NkEntityId id, const NkVec2f &cible) {
				const ecs::NkEntityId avant = m.selection;
				const bool avait = m.aSelection;
				m.selection = id;
				m.aSelection = true;
				NkEditeurDeplacer(m, cible);
				m.selection = avant;
				m.aSelection = avait;
			}

			template <typename T> bool CopierOctets(ecs::NkWorld &w, ecs::NkEntityId id, NkCarteEditeur c, NkPressePapierComposant &pp) {
				static_assert(sizeof(T) <= sizeof(NkPressePapierComposant::octets), "composant trop gros pour le presse-papiers");
				const T *x = w.Get<T>(id);
				if (x == nullptr) {
					return false;
				}
				std::memcpy(pp.octets, x, sizeof(T));
				pp.taille = static_cast<uint32>(sizeof(T));
				pp.carte = static_cast<int32>(c);
				return true;
			}

			template <typename T> bool LireOctets(const NkPressePapierComposant &pp, T &sortie) {
				if (pp.taille != sizeof(T)) {
					return false;
				}
				std::memcpy(&sortie, pp.octets, sizeof(T));
				return true;
			}
		} // namespace

		bool NkEditeurReinitialiserCarte(NkEditeurModele &m, ecs::NkEntityId id, NkCarteEditeur c) {
			ecs::NkWorld &w = m.scene.Monde();
			if (!NkEditeurAUneCarte(m, id, c) || !NkEditeurCarteSeCopie(c)) {
				return false;
			}
			switch (c) {
				case NkCarteEditeur::NK_TRANSFORM: {
					DeplacerEntite(m, id, NkVec2f(0.f, 0.f));
					NkTransform2D *t = w.Get<NkTransform2D>(id);
					if (t != nullptr && !w.Has<NkCorpsMou2D>(id)) {
						t->rotation = 0.f;
						t->echelle = NkVec2f(1.f, 1.f);
						if (w.Has<NkCorps2D>(id)) {
							m.scene.ActualiserCorps(id);
						}
					}
					// L'echelle AFFICHEE (cuite) revient a 1 x 1, comme dans Unreal.
					NkEditeurPoserEchelle(m, id, NkVec2f(1.f, 1.f));
					return true;
				}
				case NkCarteEditeur::NK_SPRITE: {
					NkSprite2D *s = w.Get<NkSprite2D>(id);
					NkSprite2D d;
					d.texId = s->texId;
					d.uv0 = s->uv0;
					d.uv1 = s->uv1;
					d.visible = s->visible;
					*s = d;
					return true;
				}
				case NkCarteEditeur::NK_COLLISIONNEUR: {
					NkCollisionneur2D d;
					// A la taille du sprite, comme a l'ajout : ce qu'on voit touche.
					if (const NkSprite2D *s = w.Get<NkSprite2D>(id)) {
						d.demiTaille = NkVec2f(s->taille.x * 0.5f, s->taille.y * 0.5f);
					}
					*w.Get<NkCollisionneur2D>(id) = d;
					if (w.Has<NkCorps2D>(id)) {
						m.scene.ActualiserCorps(id);
					}
					return true;
				}
				case NkCarteEditeur::NK_CORPS: {
					NkCorps2D *b = w.Get<NkCorps2D>(id);
					NkCorps2D d;
					d.corpsId = b->corpsId;
					*b = d;
					m.scene.ActualiserCorps(id);
					return true;
				}
				case NkCarteEditeur::NK_SOURCE: {
					NkSource2D *s = w.Get<NkSource2D>(id);
					NkSource2D d;
					d.son = s->son;
					d.voix = s->voix;
					d.lance = s->lance;
					*s = d;
					return true;
				}
				case NkCarteEditeur::NK_ANIMATION: {
					NkAnimSprite2D *a = w.Get<NkAnimSprite2D>(id);
					NkAnimSprite2D d;
					d.nbClips = 1;
					d.enPause = a->enPause;
					*a = d;
					return true;
				}
				case NkCarteEditeur::NK_LUMIERE: {
					// Les valeurs par defaut de SON type : une lumiere cone reste un cone.
					NkLumiere2D *l = w.Get<NkLumiere2D>(id);
					NkLumiere2D d;
					d.type = l->type;
					d.actif = l->actif;
					*l = d;
					return true;
				}
				case NkCarteEditeur::NK_EMETTEUR:
					// Un emetteur se remet a SA recette (graine et etat gardes).
					return NkEditeurAppliquerPreset(m, id, w.Get<NkEmetteur2D>(id)->preset);
				case NkCarteEditeur::NK_FORME: {
					// La forme par defaut de SON genre, a SA taille et visible comme avant.
					NkRenduForme2D *f = w.Get<NkRenduForme2D>(id);
					NkRenduForme2D d = NkFormeParDefaut(f->genre);
					d.taille = f->taille;
					d.visible = f->visible;
					*f = d;
					NkEditeurFormeChangee(m, id);
					return true;
				}
				case NkCarteEditeur::NK_ANCRAGE:
					*w.Get<NkAncrageEcran2D>(id) = NkAncrageEcran2D();
					return true;
				default:
					return false;
			}
		}

		bool NkEditeurCopierCarte(NkEditeurModele &m, ecs::NkEntityId id, NkCarteEditeur c, NkPressePapierComposant &pp) {
			ecs::NkWorld &w = m.scene.Monde();
			if (!NkEditeurAUneCarte(m, id, c) || !NkEditeurCarteSeCopie(c)) {
				return false;
			}
			switch (c) {
				case NkCarteEditeur::NK_TRANSFORM: {
					// La position COURANTE, matiere comprise (son centre, pas un
					// transform qui la suit en retard).
					NkTransform2D t = *w.Get<NkTransform2D>(id);
					const ecs::NkEntityId avant = m.selection;
					const bool avait = m.aSelection;
					m.selection = id;
					m.aSelection = true;
					NkEditeurCentreSelection(m, t.position);
					m.selection = avant;
					m.aSelection = avait;
					std::memcpy(pp.octets, &t, sizeof(t));
					pp.taille = static_cast<uint32>(sizeof(t));
					pp.carte = static_cast<int32>(c);
					return true;
				}
				case NkCarteEditeur::NK_SPRITE:
					return CopierOctets<NkSprite2D>(w, id, c, pp);
				case NkCarteEditeur::NK_COLLISIONNEUR:
					return CopierOctets<NkCollisionneur2D>(w, id, c, pp);
				case NkCarteEditeur::NK_CORPS:
					return CopierOctets<NkCorps2D>(w, id, c, pp);
				case NkCarteEditeur::NK_SOURCE:
					return CopierOctets<NkSource2D>(w, id, c, pp);
				case NkCarteEditeur::NK_ANIMATION:
					return CopierOctets<NkAnimSprite2D>(w, id, c, pp);
				case NkCarteEditeur::NK_LUMIERE:
					return CopierOctets<NkLumiere2D>(w, id, c, pp);
				case NkCarteEditeur::NK_EMETTEUR:
					return CopierOctets<NkEmetteur2D>(w, id, c, pp);
				case NkCarteEditeur::NK_FORME:
					return CopierOctets<NkRenduForme2D>(w, id, c, pp);
				case NkCarteEditeur::NK_ANCRAGE:
					return CopierOctets<NkAncrageEcran2D>(w, id, c, pp);
				default:
					return false;
			}
		}

		bool NkEditeurCollerCarte(NkEditeurModele &m, ecs::NkEntityId id, const NkPressePapierComposant &pp) {
			if (pp.carte < 0 || pp.carte >= static_cast<int32>(NkCarteEditeur::NK_COUNT)) {
				return false;
			}
			const NkCarteEditeur c = static_cast<NkCarteEditeur>(pp.carte);
			ecs::NkWorld &w = m.scene.Monde();
			if (!NkEditeurAUneCarte(m, id, c) || !NkEditeurCarteSeCopie(c)) {
				return false;
			}
			switch (c) {
				case NkCarteEditeur::NK_TRANSFORM: {
					NkTransform2D t;
					if (!LireOctets(pp, t)) {
						return false;
					}
					DeplacerEntite(m, id, t.position);
					NkTransform2D *cur = w.Get<NkTransform2D>(id);
					if (cur != nullptr && !w.Has<NkCorpsMou2D>(id)) {
						cur->rotation = t.rotation;
						cur->echelle = t.echelle;
						if (w.Has<NkCorps2D>(id)) {
							m.scene.ActualiserCorps(id);
						}
					}
					return true;
				}
				case NkCarteEditeur::NK_SPRITE: {
					NkSprite2D v;
					if (!LireOctets(pp, v)) {
						return false;
					}
					*w.Get<NkSprite2D>(id) = v;
					return true;
				}
				case NkCarteEditeur::NK_COLLISIONNEUR: {
					NkCollisionneur2D v;
					if (!LireOctets(pp, v)) {
						return false;
					}
					*w.Get<NkCollisionneur2D>(id) = v;
					if (w.Has<NkCorps2D>(id)) {
						m.scene.ActualiserCorps(id);
					}
					return true;
				}
				case NkCarteEditeur::NK_CORPS: {
					NkCorps2D v;
					if (!LireOctets(pp, v)) {
						return false;
					}
					NkCorps2D *cur = w.Get<NkCorps2D>(id);
					v.corpsId = cur->corpsId; // SON corps, pas celui de la source
					*cur = v;
					m.scene.ActualiserCorps(id);
					return true;
				}
				case NkCarteEditeur::NK_SOURCE: {
					NkSource2D v;
					if (!LireOctets(pp, v)) {
						return false;
					}
					NkSource2D *cur = w.Get<NkSource2D>(id);
					v.voix = cur->voix;
					v.lance = cur->lance;
					v.demande = false;
					v.arret = false;
					*cur = v;
					return true;
				}
				case NkCarteEditeur::NK_ANIMATION: {
					NkAnimSprite2D v;
					if (!LireOctets(pp, v)) {
						return false;
					}
					*w.Get<NkAnimSprite2D>(id) = v;
					return true;
				}
				case NkCarteEditeur::NK_LUMIERE: {
					NkLumiere2D v;
					if (!LireOctets(pp, v)) {
						return false;
					}
					*w.Get<NkLumiere2D>(id) = v;
					return true;
				}
				case NkCarteEditeur::NK_EMETTEUR: {
					NkEmetteur2D v;
					if (!LireOctets(pp, v)) {
						return false;
					}
					// SA graine : deux feux colles l'un sur l'autre ne bruleraient pas
					// a l'unisson (la meme regle que Dupliquer).
					NkEmetteur2D *cur = w.Get<NkEmetteur2D>(id);
					v.graine = cur->graine;
					*cur = v;
					m.scene.Effets().Rejouer(id.Pack());
					return true;
				}
				case NkCarteEditeur::NK_FORME: {
					NkRenduForme2D v;
					if (!LireOctets(pp, v)) {
						return false;
					}
					*w.Get<NkRenduForme2D>(id) = v;
					NkEditeurFormeChangee(m, id);
					return true;
				}
				case NkCarteEditeur::NK_ANCRAGE: {
					NkAncrageEcran2D v;
					if (!LireOctets(pp, v)) {
						return false;
					}
					*w.Get<NkAncrageEcran2D>(id) = v;
					return true;
				}
				default:
					return false;
			}
		}

		bool NkEditeurCarteAUneCase(NkCarteEditeur c) noexcept {
			// L'ancrage n'a rien a eteindre : il se retire (menu « ⋮ »).
			return c != NkCarteEditeur::NK_TRANSFORM && c != NkCarteEditeur::NK_HIERARCHIE && c != NkCarteEditeur::NK_ANCRAGE &&
				   c < NkCarteEditeur::NK_COUNT;
		}

		bool NkEditeurCarteActive(NkEditeurModele &m, ecs::NkEntityId id, NkCarteEditeur c) {
			ecs::NkWorld &w = m.scene.Monde();
			switch (c) {
				case NkCarteEditeur::NK_SPRITE: {
					const NkSprite2D *s = w.Get<NkSprite2D>(id);
					return s != nullptr && s->visible;
				}
				case NkCarteEditeur::NK_CORPS_MOU: {
					const NkCorpsMou2D *s = w.Get<NkCorpsMou2D>(id);
					return s != nullptr && s->visible;
				}
				case NkCarteEditeur::NK_ANIMATION: {
					const NkAnimSprite2D *s = w.Get<NkAnimSprite2D>(id);
					return s != nullptr && !s->enPause;
				}
				case NkCarteEditeur::NK_ANIMATEUR: {
					const NkAnimateur2D *s = w.Get<NkAnimateur2D>(id);
					return s != nullptr && !s->enPause;
				}
				case NkCarteEditeur::NK_LUMIERE: {
					const NkLumiere2D *s = w.Get<NkLumiere2D>(id);
					return s != nullptr && s->actif;
				}
				case NkCarteEditeur::NK_EMETTEUR: {
					const NkEmetteur2D *s = w.Get<NkEmetteur2D>(id);
					return s != nullptr && s->actif;
				}
				case NkCarteEditeur::NK_FORME: {
					const NkRenduForme2D *s = w.Get<NkRenduForme2D>(id);
					return s != nullptr && s->visible;
				}
				case NkCarteEditeur::NK_COLLISIONNEUR:
				case NkCarteEditeur::NK_CORPS:
				case NkCarteEditeur::NK_SOURCE: {
					const NkEteintsEditeur *e = w.Get<NkEteintsEditeur>(id);
					return e == nullptr || (e->bits & (1u << static_cast<uint32>(c))) == 0u;
				}
				default:
					return true;
			}
		}

		bool NkEditeurActiverCarte(NkEditeurModele &m, ecs::NkEntityId id, NkCarteEditeur c, bool actif) {
			ecs::NkWorld &w = m.scene.Monde();
			if (!NkEditeurCarteAUneCase(c) || !NkEditeurAUneCarte(m, id, c) || NkEditeurCarteActive(m, id, c) == actif) {
				return false;
			}
			switch (c) {
				case NkCarteEditeur::NK_SPRITE:
					w.Get<NkSprite2D>(id)->visible = actif;
					return true;
				case NkCarteEditeur::NK_CORPS_MOU:
					w.Get<NkCorpsMou2D>(id)->visible = actif;
					return true;
				case NkCarteEditeur::NK_ANIMATION:
					w.Get<NkAnimSprite2D>(id)->enPause = !actif;
					return true;
				case NkCarteEditeur::NK_ANIMATEUR:
					w.Get<NkAnimateur2D>(id)->enPause = !actif;
					return true;
				case NkCarteEditeur::NK_LUMIERE:
					w.Get<NkLumiere2D>(id)->actif = actif;
					return true;
				case NkCarteEditeur::NK_EMETTEUR:
					w.Get<NkEmetteur2D>(id)->actif = actif;
					return true;
				case NkCarteEditeur::NK_FORME:
					w.Get<NkRenduForme2D>(id)->visible = actif;
					return true;
				default:
					break;
			}
			// Ceux qui n'ont pas de drapeau : on change ce qui les rend effectifs,
			// et on garde ce qu'ils avaient.
			NkEteintsEditeur e;
			if (const NkEteintsEditeur *x = w.Get<NkEteintsEditeur>(id)) {
				e = *x;
			}
			const uint32 bit = 1u << static_cast<uint32>(c);
			if (c == NkCarteEditeur::NK_COLLISIONNEUR) {
				NkCollisionneur2D *col = w.Get<NkCollisionneur2D>(id);
				if (!actif) {
					e.couche = col->couche;
					e.masque = col->masque;
					col->couche = 0u;
					col->masque = 0u;
				} else {
					col->couche = e.couche;
					col->masque = e.masque;
				}
				if (w.Has<NkCorps2D>(id)) {
					m.scene.ActualiserCorps(id);
				}
			} else if (c == NkCarteEditeur::NK_CORPS) {
				NkCorps2D *b = w.Get<NkCorps2D>(id);
				if (!actif) {
					e.typeCorps = static_cast<uint8>(b->type);
					e.echelleGravite = b->echelleGravite;
					b->type = NkTypeCorps::NK_CINEMATIQUE;
					b->echelleGravite = 0.f;
				} else {
					b->type = static_cast<NkTypeCorps>(e.typeCorps);
					b->echelleGravite = e.echelleGravite;
				}
				m.scene.ActualiserCorps(id);
				if (!actif) {
					m.scene.PoserVitesse(id, NkVec2f(0.f, 0.f));
				}
			} else if (c == NkCarteEditeur::NK_SOURCE) {
				NkSource2D *so = w.Get<NkSource2D>(id);
				if (!actif) {
					e.volume = so->volume;
					so->volume = 0.f;
					so->arret = true;
				} else {
					so->volume = e.volume;
				}
			} else {
				return false;
			}
			e.bits = actif ? (e.bits & ~bit) : (e.bits | bit);
			if (e.bits == 0u) {
				if (w.Has<NkEteintsEditeur>(id)) {
					w.Remove<NkEteintsEditeur>(id);
				}
			} else {
				w.Add<NkEteintsEditeur>(id, e);
			}
			return true;
		}

		const char *NkEditeurTypeDe(NkScene &scene, ecs::NkEntityId id) {
			if (scene.Monde().Has<NkCorpsMou2D>(id)) {
				return "mou";
			}
			if (const NkCorps2D *c = scene.Monde().Get<NkCorps2D>(id)) {
				return c->type == NkTypeCorps::NK_DYNAMIQUE ? "rigide" : "decor";
			}
			if (scene.Monde().Has<NkRenduForme2D>(id)) {
				return "forme"; // (2026-10-01) une forme 2D sans corps
			}
			return "entite";
		}

		// =====================================================================
		// Hierarchie et prefabs
		// =====================================================================
		bool NkEditeurRattacher(NkEditeurModele &m, ecs::NkEntityId enfant, ecs::NkEntityId parent) {
			if (!m.scene.Rattacher(enfant, parent, true)) {
				NkEditeurAnnoncer(m, "Rattachement refuse : une entite ne descend pas d'elle-meme");
				return false;
			}
			return true;
		}

		bool NkEditeurDetacher(NkEditeurModele &m, ecs::NkEntityId enfant) {
			return m.scene.Detacher(enfant, true);
		}

		NkString NkEditeurCheminPrefab(NkEditeurModele &m, const char *nom) {
			// A cote de la scene. ⚠️ Un chemin ABSOLU tant que l'editeur n'a pas de
			// PROJET (palier U1) : CONVENTIONS_FICHIERS.md § 5 veut des chemins
			// relatifs au dossier du projet, et il n'y a pas encore de dossier.
			const NkString scene(NkEditeurChemin(m));
			usize fin = 0;
			for (usize i = 0; i < static_cast<usize>(scene.Length()); ++i) {
				if (scene.CStr()[i] == '/' || scene.CStr()[i] == '\\') {
					fin = i + 1u;
				}
			}
			NkString chemin(scene.CStr(), fin);
			chemin.Append(nom != nullptr && nom[0] != '\0' ? nom : "Prefab");
			chemin.Append(".nkprefab");
			return chemin;
		}

		uint32 NkEditeurCreerPrefab(NkEditeurModele &m) {
			if (!m.aSelection || !m.scene.Monde().IsAlive(m.selection) || m.etat != NkEtatJeu::NK_EDITION) {
				return 0u;
			}
			const NkEtiquette *e = m.scene.Monde().Get<NkEtiquette>(m.selection);
			const NkString chemin = NkEditeurCheminPrefab(m, e != nullptr && e->nom[0] != '\0' ? e->nom : "Prefab");
			const uint32 id = m.prefabs.Creer(m.scene, m.selection, chemin.CStr());
			const bool ecrit = id != 0u && m.prefabs.Enregistrer(m.scene, id, chemin.CStr(), m.RessourcesScene());
			NkEditeurAnnoncer(m, ecrit ? "Prefab cree" : "Prefab impossible a ecrire");
			return ecrit ? id : 0u;
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

		// =====================================================================
		// L'HISTORIQUE (2026-10-01)
		// =====================================================================
		void NkEditeurOublierHistorique(NkEditeurModele &m) {
			m.historique.annuler.Clear();
			m.historique.refaire.Clear();
		}

		void NkEditeurRetenir(NkEditeurModele &m) {
			if (m.etat != NkEtatJeu::NK_EDITION) {
				return;
			}
			NkHistoriqueEditeur &h = m.historique;
			NkScene::NkPhoto photo;
			m.scene.Photographier(photo);
			if (h.annuler.Size() >= h.maximum && h.annuler.Size() > 0u) {
				h.annuler.RemoveAt(0);
			}
			h.annuler.PushBack(photo);
			h.refaire.Clear();
		}

		namespace {
			/// Rend `photo` a la scene ; la selection suit par IDENTITE.
			void RendrePhoto(NkEditeurModele &m, const NkScene::NkPhoto &photo) {
				const uint64 uid = m.aSelection ? m.scene.Uid(m.selection) : 0u;
				m.scene.Restaurer(photo);
				const ecs::NkEntityId e = uid != 0u ? m.scene.EntiteParUid(uid) : ecs::NkEntityId::Invalid();
				m.aSelection = e.IsValid();
				m.selection = e;
			}

			bool Basculer(NkEditeurModele &m, NkVector<NkScene::NkPhoto> &depuis, NkVector<NkScene::NkPhoto> &vers,
						  const char *rien, const char *fait) {
				if (m.etat != NkEtatJeu::NK_EDITION) {
					NkEditeurAnnoncer(m, "En jeu, Arreter rend la scene d'avant : l'historique est celui de l'edition");
					return false;
				}
				if (depuis.Empty()) {
					NkEditeurAnnoncer(m, rien);
					return false;
				}
				NkScene::NkPhoto maintenant;
				m.scene.Photographier(maintenant);
				vers.PushBack(maintenant);
				const NkScene::NkPhoto photo = depuis[depuis.Size() - 1u];
				depuis.PopBack();
				RendrePhoto(m, photo);
				NkEditeurAnnoncer(m, fait);
				return true;
			}
		} // namespace

		bool NkEditeurAnnuler(NkEditeurModele &m) {
			return Basculer(m, m.historique.annuler, m.historique.refaire, "Rien a annuler", "Annule");
		}

		bool NkEditeurRefaire(NkEditeurModele &m) {
			return Basculer(m, m.historique.refaire, m.historique.annuler, "Rien a retablir", "Retabli");
		}

		bool NkEditeurActiverEntite(NkEditeurModele &m, ecs::NkEntityId id, bool actif) {
			if (!m.scene.Monde().IsAlive(id) || m.scene.EstActiveSoi(id) == actif) {
				return false;
			}
			NkEditeurRetenir(m);
			const bool ok = m.scene.Activer(id, actif);
			NkEditeurAnnoncer(m, actif ? "Entite activee" : "Entite desactivee : ni rendue, ni simulee, ni animee (Ctrl+Z annule)");
			return ok;
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
			const bool ok = NkSauverSceneFichier(m.scene, NkEditeurChemin(m), m.RessourcesScene());
			// L'appareil simule part AVEC la scene (document 03, §2.2) : un
			// fichier a cote, comme les entrees (.nkentrees).
			if (ok) {
				NkEditeurAppareilEnregistrer(m, NkEditeurChemin(m));
			}
			NkEditeurAnnoncer(m, ok ? "Scene enregistree" : "Enregistrement impossible");
			return ok;
		}

		bool NkEditeurAppareilEnregistrer(const NkEditeurModele &m, const char *cheminScene) {
			const NkString chemin = NkFichierAppareil(cheminScene);
			return NkFile::WriteAllText(chemin.CStr(), NkEcrireAppareil(m.profil, m.orientation, m.appareil).CStr());
		}

		bool NkEditeurAppareilCharger(NkEditeurModele &m, const char *cheminScene) {
			// ⚠️ ABSENT = RIEN NE CHANGE : une scene d'avant le 01/10 n'a pas ce
			// fichier, et l'ouvrir ne doit pas jeter l'appareil qu'on regardait.
			const NkString chemin = NkFichierAppareil(cheminScene);
			if (!NkFile::Exists(chemin.CStr())) {
				return false;
			}
			return NkLireAppareil(NkFile::ReadAllText(chemin.CStr()), m.profil, m.orientation, m.appareil);
		}

		bool NkEditeurOuvrir(NkEditeurModele &m) {
			NkString erreur;
			// AVANT la lecture : un composant non declare serait saute en silence.
			DeclarerDrapeaux(m.scene);
			const bool ok = NkChargerSceneFichier(m.scene, NkEditeurChemin(m), m.RessourcesScene(), &erreur);
			if (ok) {
				NkEditeurAppareilCharger(m, NkEditeurChemin(m));
				NkEditeurOublierHistorique(m);
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
