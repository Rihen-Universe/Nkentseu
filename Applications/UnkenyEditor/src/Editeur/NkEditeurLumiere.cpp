//
// NkEditeurLumiere.cpp
// =============================================================================
// Description :
//   L'eclairage 2D et les effets dans l'editeur : actions, dessin du viseur,
//   poignee de portee, sections des Details et de l'onglet Monde, scene de
//   nuit d'exemple. Voir NkEditeurLumiere.h.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurLumiere.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurInterface.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKGui/Widgets/NkGuiWidgets.h"

#include <cstdio>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkGuiContext;
		using nkgui::NkRect;

		namespace {
			constexpr float32 PI = 3.14159265f;
			constexpr float32 DEG = 57.2957795f;
			constexpr float32 ICONE_PX = 9.f;	 ///< rayon d'une icone de lumiere, en pixels
			constexpr float32 POIGNEE_PX = 6.f; ///< demi-cote de la poignee de portee

			NkEditeurChiffresLumiere gChiffres;

			/// La poignee de portee tenue. Un seul editeur par processus : un etat
			/// de fichier suffit, et il ne touche pas NkEditeurInterface (qu'un
			/// autre chantier refait).
			struct NkPrisePortee {
					bool tenue = false;
					bool survol = false;
			};
			NkPrisePortee gPrise;

			NkVec2f Position(NkEditeurModele &m, ecs::NkEntityId id) {
				const NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(id);
				if (t == nullptr) {
					return NkVec2f(0.f, 0.f);
				}
				if (const NkLumiere2D *l = m.scene.Monde().Get<NkLumiere2D>(id)) {
					return t->VersMonde(l->decalage);
				}
				if (const NkEmetteur2D *e = m.scene.Monde().Get<NkEmetteur2D>(id)) {
					return t->VersMonde(e->decalage);
				}
				return t->position;
			}

			NkColor Couleur(uint32 rgba, uint8 a) noexcept {
				return NkColor(static_cast<uint8>((rgba >> 24) & 0xFFu), static_cast<uint8>((rgba >> 16) & 0xFFu),
							   static_cast<uint8>((rgba >> 8) & 0xFFu), a);
			}

			/// Une section repliable OUVERTE la premiere fois (meme regle que les
			/// autres sections des Details : NkEditeurDetails.cpp, Section).
			bool Section(NkGuiContext &ctx, const char *libelle) {
				static NkVector<nkgui::NkGuiId> vues;
				const nkgui::NkGuiId id = ctx.GetId(libelle);
				bool nouvelle = true;
				for (uint32 i = 0; i < vues.Size() && nouvelle; ++i) {
					nouvelle = !(vues[i] == id);
				}
				if (nouvelle) {
					vues.PushBack(id);
					ctx.SetNodeOpen(id, true);
				}
				return nkgui::CollapsingHeader(ctx, libelle);
			}

			/// Un choix segmente : des boutons sur une ligne, la valeur grisee.
			bool Choix(NkGuiContext &ctx, const char *libelle, const char *const *noms, int32 n, int32 &valeur) {
				nkgui::Text(ctx, NkString::Format("%s : %s", libelle, noms[valeur]).CStr());
				bool change = false;
				ctx.PushId(libelle);
				for (int32 i = 0; i < n; ++i) {
					if (i > 0) {
						ctx.SameLine();
					}
					ctx.BeginDisabled(i == valeur);
					if (nkgui::Button(ctx, noms[i])) {
						valeur = i;
						change = true;
					}
					ctx.EndDisabled();
				}
				ctx.PopId();
				return change;
			}

			bool CouleurRgba(NkGuiContext &ctx, const char *libelle, uint32 &rgba) {
				float32 c[4] = {static_cast<float32>((rgba >> 24) & 0xFFu) / 255.f,
								static_cast<float32>((rgba >> 16) & 0xFFu) / 255.f,
								static_cast<float32>((rgba >> 8) & 0xFFu) / 255.f,
								static_cast<float32>(rgba & 0xFFu) / 255.f};
				if (!nkgui::ColorEdit4(ctx, libelle, c)) {
					return false;
				}
				auto o = [](float32 v) {
					return static_cast<uint32>((v < 0.f ? 0.f : (v > 1.f ? 1.f : v)) * 255.f + 0.5f);
				};
				rgba = (o(c[0]) << 24) | (o(c[1]) << 16) | (o(c[2]) << 8) | o(c[3]);
				return true;
			}

			/// Un angle stocke en radians, regle en degres.
			bool Angle(NkGuiContext &ctx, const char *libelle, float32 &radians, float32 mini, float32 maxi) {
				float32 d = radians * DEG;
				if (!nkgui::SliderFloat(ctx, libelle, d, mini, maxi)) {
					return false;
				}
				radians = d / DEG;
				return true;
			}

			/// L'en-tete d'une section : son nom, et « Retirer ce composant ». Rend
			/// false si le composant vient d'etre retire (ne plus rien lire).
			bool EnTete(NkEditeurCadre &c, ecs::NkEntityId id, NkComposantEditeur comp, bool &ouvert) {
				NkGuiContext &ctx = c.ctx;
				nkgui::Separator(ctx);
				ouvert = Section(ctx, NkComposantEditeurNom(comp));
				if (ouvert) {
					ctx.PushId(NkComposantEditeurNom(comp));
					const bool retirer = nkgui::Button(ctx, "Retirer ce composant");
					ctx.PopId();
					if (retirer) {
						NkEditeurRetirerComposant(c.m, id, comp);
						return false;
					}
				}
				return true;
			}

			/// Rappel qui ne se tait pas : une lumiere sans eclairage de scene ne fait
			/// rien, et un reglage sans effet visible passe pour une panne.
			void RappelEteint(NkEditeurCadre &c) {
				if (c.m.scene.Eclairage().actif) {
					return;
				}
				nkgui::TextWrapped(c.ctx, "L'éclairage de la scène est éteint : cette lumière n'a pas d'effet (réglages : onglet Monde).");
				if (nkgui::Button(c.ctx, "Activer l'éclairage de la scène")) {
					c.m.scene.Eclairage().actif = true;
					NkEditeurAnnoncer(c.m, "Eclairage 2D de la scene active");
				}
			}

			ecs::NkEntityId Decor(NkScene &s, const char *nom, const NkVec2f &c, const NkVec2f &demi, uint32 couleur,
								  float32 rotation = 0.f) {
				const ecs::NkEntityId e = s.Creer(nom, c);
				s.Monde().Get<NkTransform2D>(e)->rotation = rotation;
				NkSprite2D sp;
				sp.taille = NkVec2f(demi.x * 2.f, demi.y * 2.f);
				sp.couleur = couleur;
				sp.couche = -10;
				sp.visible = false; // dessine par sa FORME ; le sprite garde la couleur (comme la scene neuve)
				s.Monde().Add<NkSprite2D>(e, sp);
				NkCollisionneur2D col;
				col.demiTaille = demi;
				s.Monde().Add<NkCollisionneur2D>(e, col);
				NkCorps2D b;
				b.type = NkTypeCorps::NK_STATIQUE;
				s.AjouterCorps(e, b);
				return e;
			}

			/// Un sprite sans collisionneur : du decor qui ne porte pas d'ombre.
			void Plat(NkScene &s, const char *nom, const NkVec2f &c, const NkVec2f &taille, uint32 couleur, int32 couche,
					  float32 rotation) {
				const ecs::NkEntityId e = s.Creer(nom, c);
				s.Monde().Get<NkTransform2D>(e)->rotation = rotation;
				NkSprite2D sp;
				sp.taille = taille;
				sp.couleur = couleur;
				sp.couche = couche;
				s.Monde().Add<NkSprite2D>(e, sp);
			}
		} // namespace

		// =====================================================================
		// Actions
		// =====================================================================
		bool NkEditeurAjouterLumiere(NkEditeurModele &m, ecs::NkEntityId id, NkTypeLumiere2D type) {
			ecs::NkWorld &w = m.scene.Monde();
			if (!w.IsAlive(id) || w.Has<NkLumiere2D>(id)) {
				return false;
			}
			NkLumiere2D l;
			l.type = type;
			if (type == NkTypeLumiere2D::NK_SPOT) {
				l.portee = 7.f;
				l.couleur = 0xFFF4D8FFu;
			} else if (type == NkTypeLumiere2D::NK_DIRECTIONNELLE) {
				// La lune : froide, faible, des ombres de quelques metres.
				l.couleur = 0x8C9CD0FFu;
				l.intensite = 0.4f;
				l.portee = 6.f;
				l.direction = -1.5707963f + 0.35f;
			}
			w.Add<NkLumiere2D>(id, l);
			if (!m.scene.Eclairage().actif) {
				NkEditeurAnnoncer(m, "Lumiere ajoutee : l'eclairage de la scene est eteint (onglet Monde)");
			}
			return true;
		}

		bool NkEditeurAjouterEffet(NkEditeurModele &m, ecs::NkEntityId id, NkPresetEffet2D preset) {
			ecs::NkWorld &w = m.scene.Monde();
			if (!w.IsAlive(id) || w.Has<NkEmetteur2D>(id)) {
				return false;
			}
			NkEmetteur2D e = NkPresetEmetteur2D(preset);
			// Une graine PAR emetteur : deux feux poses cote a cote ne doivent pas
			// danser a l'unisson.
			m.graine = m.graine * 1664525u + 1013904223u;
			e.graine = m.graine;
			w.Add<NkEmetteur2D>(id, e);
			return true;
		}

		ecs::NkEntityId NkEditeurPoserLumiere(NkEditeurModele &m, const NkVec2f &monde, NkTypeLumiere2D type) {
			static const char *kNoms[3] = {"Lumiere", "Projecteur", "Lune"};
			const ecs::NkEntityId e = NkEditeurCreerEntite(m, kNoms[static_cast<int32>(type)], monde);
			NkEditeurAjouterLumiere(m, e, type);
			return e;
		}

		ecs::NkEntityId NkEditeurPoserEffet(NkEditeurModele &m, const NkVec2f &monde, NkPresetEffet2D preset) {
			// Le nom de l'entite est celui du preset, sans accent : il sert aussi
			// de cle a --selection= (une ligne de commande Windows et l'UTF-8...).
			static const char *kNoms[7] = {"Effet", "Feu", "Fumee", "Etincelles", "Pluie", "Neige", "Explosion"};
			const int32 k = static_cast<int32>(preset);
			const ecs::NkEntityId e = NkEditeurCreerEntite(m, kNoms[k >= 0 && k < 7 ? k : 0], monde);
			NkEditeurAjouterEffet(m, e, preset);
			return e;
		}

		bool NkEditeurAppliquerPreset(NkEditeurModele &m, ecs::NkEntityId id, NkPresetEffet2D preset) {
			NkEmetteur2D *e = m.scene.Monde().Get<NkEmetteur2D>(id);
			if (e == nullptr) {
				return false;
			}
			NkEmetteur2D n = NkPresetEmetteur2D(preset);
			n.graine = e->graine;
			n.actif = e->actif;
			n.decalage = e->decalage;
			*e = n;
			// L'effet repart de zero : garder les particules de l'ancienne recette
			// melangerait deux effets a l'ecran.
			m.scene.Effets().Rejouer(id.Pack());
			return true;
		}

		bool NkEditeurPoserPortee(NkEditeurModele &m, ecs::NkEntityId id, float32 portee) {
			portee = portee < 0.1f ? 0.1f : (portee > 100.f ? 100.f : portee);
			if (NkLumiere2D *l = m.scene.Monde().Get<NkLumiere2D>(id)) {
				l->portee = portee;
				return true;
			}
			if (NkEmetteur2D *e = m.scene.Monde().Get<NkEmetteur2D>(id)) {
				e->porteeLumiere = portee;
				return true;
			}
			return false;
		}

		float32 NkEditeurPortee(NkEditeurModele &m, ecs::NkEntityId id) {
			if (const NkLumiere2D *l = m.scene.Monde().Get<NkLumiere2D>(id)) {
				return l->portee;
			}
			if (const NkEmetteur2D *e = m.scene.Monde().Get<NkEmetteur2D>(id)) {
				return e->eclaire ? e->porteeLumiere : 0.f;
			}
			return 0.f;
		}

		bool NkEditeurIconeSous(NkEditeurModele &m, const NkVec2f &monde, float32 rayonPx, ecs::NkEntityId &sortie) {
			if (m.etat == NkEtatJeu::NK_JEU) {
				return false;
			}
			const float32 r = rayonPx / m.scene.Camera().Zoom();
			float32 meilleur = r * r;
			bool trouve = false;
			auto Tester = [&](ecs::NkEntityId id) {
				const NkVec2f p = Position(m, id);
				const float32 dx = p.x - monde.x;
				const float32 dy = p.y - monde.y;
				const float32 d2 = dx * dx + dy * dy;
				if (d2 <= meilleur) {
					meilleur = d2;
					sortie = id;
					trouve = true;
				}
			};
			NkVector<ecs::NkEntityId> ids;
			m.scene.Monde().Query<NkTransform2D, NkLumiere2D>().ForEach(
				[&ids](ecs::NkEntityId id, NkTransform2D &, NkLumiere2D &) { ids.PushBack(id); });
			m.scene.Monde().Query<NkTransform2D, NkEmetteur2D>().ForEach(
				[&ids](ecs::NkEntityId id, NkTransform2D &, NkEmetteur2D &) { ids.PushBack(id); });
			for (uint32 i = 0; i < ids.Size(); ++i) {
				Tester(ids[i]);
			}
			return trouve;
		}

		void NkEditeurSceneNuit(NkEditeurModele &m) {
			NkSceneConfig cfg;
			cfg.physique = true;
			cfg.particules = true;
			cfg.gravite = NkVec2f(0.f, -9.81f);
			m.scene.Init(cfg);
			NkRemettreNomsSim();
			m.etat = NkEtatJeu::NK_EDITION;
			m.photo.valide = false;
			m.aSelection = false;
			m.pinceau = ecs::NkEntityId::Invalid();
			NkScene &s = m.scene;
			// L'eclairage D'ABORD : les lumieres posees ensuite n'ont pas a
			// annoncer qu'il est eteint.
			NkEclairage2D &ec = s.Eclairage();
			ec.actif = true;
			ec.ambiante = 0x1A2036FFu;
			ec.ombres = true;
			// Le sol et ce qui porte ombre : un rocher, une souche, un mur, deux caisses.
			Decor(s, "Sol", NkVec2f(0.f, -4.5f), NkVec2f(12.f, 0.5f), 0x4A5A48FFu);
			Decor(s, "Rocher", NkVec2f(3.2f, -3.45f), NkVec2f(0.7f, 0.55f), 0x7A7C84FFu, 0.2f);
			Decor(s, "Souche", NkVec2f(-2.6f, -3.6f), NkVec2f(0.35f, 0.4f), 0x6E4A30FFu);
			Decor(s, "Mur", NkVec2f(6.5f, -2.5f), NkVec2f(0.3f, 1.5f), 0x8A8070FFu);
			Decor(s, "Tronc", NkVec2f(-7.f, -2.2f), NkVec2f(0.3f, 1.8f), 0x5A3C28FFu);
			// « Houppier » et non « Feuillage » : --selection= choisit la PREMIERE
			// entite dont le nom commence par ce qu'on donne, et « Feu » tombait
			// sur le feuillage (vu sur la capture du 30/09).
			Decor(s, "Houppier", NkVec2f(-7.f, 0.2f), NkVec2f(1.4f, 0.9f), 0x2E5A34FFu);
			NkPoserActeurSim(s, NkActeurSim::NK_CAISSE, NkVec2f(-4.6f, -3.7f), &m.ressources);
			NkPoserActeurSim(s, NkActeurSim::NK_CAISSE, NkVec2f(-4.6f, -3.1f), &m.ressources);
			// Le feu de camp : des buches (sans collisionneur : elles n'ombrent pas
			// le feu qu'elles portent), les flammes qui eclairent, la fumee, les
			// etincelles.
			Plat(s, "Buche", NkVec2f(-0.18f, -3.9f), NkVec2f(0.8f, 0.14f), 0x5C3A22FFu, 1, 0.35f);
			Plat(s, "Buche 2", NkVec2f(0.18f, -3.9f), NkVec2f(0.8f, 0.14f), 0x4E3220FFu, 1, -0.35f);
			NkEditeurPoserEffet(m, NkVec2f(0.f, -3.75f), NkPresetEffet2D::NK_FEU);
			NkEditeurPoserEffet(m, NkVec2f(0.05f, -3.1f), NkPresetEffet2D::NK_FUMEE);
			NkEditeurPoserEffet(m, NkVec2f(0.f, -3.7f), NkPresetEffet2D::NK_ETINCELLES);
			// Une lanterne pendue au mur, un reverbere en cone, la lune.
			const ecs::NkEntityId lanterne = NkEditeurPoserLumiere(m, NkVec2f(6.f, -1.6f), NkTypeLumiere2D::NK_PONCTUELLE);
			if (NkLumiere2D *l = s.Monde().Get<NkLumiere2D>(lanterne)) {
				l->couleur = 0xFFD890FFu;
				l->portee = 3.5f;
				l->halo = 0.35f;
			}
			const ecs::NkEntityId reverbere = NkEditeurPoserLumiere(m, NkVec2f(9.5f, 0.5f), NkTypeLumiere2D::NK_SPOT);
			if (NkLumiere2D *l = s.Monde().Get<NkLumiere2D>(reverbere)) {
				// Le sol est a 4,5 m sous la lampe : a 6 m de portee et en
				// decroissance douce, il n'en recevait que 6 % (capture du 30/09).
				l->portee = 6.5f;
				l->attenuation = 1.f;
				l->intensite = 1.4f;
				l->ouverture = 0.45f;
				l->halo = 0.2f;
			}
			Plat(s, "Poteau", NkVec2f(9.5f, -1.75f), NkVec2f(0.12f, 4.5f), 0x303238FFu, 0, 0.f);
			NkEditeurPoserLumiere(m, NkVec2f(-10.f, 5.f), NkTypeLumiere2D::NK_DIRECTIONNELLE);
			m.aSelection = false;
		}

		// =====================================================================
		// Dessin
		// =====================================================================
		const NkEditeurChiffresLumiere &NkEditeurDerniersChiffresLumiere() noexcept {
			return gChiffres;
		}

		void NkEditeurDessinerEffetsEtLumiere(nkgui::NkGuiDrawList &dl, NkEditeurModele &m) {
			// ⚠️ L'ORDRE PORTE LE SENS : la fumee (alpha) AVANT la carte, que la
			//    nuit l'assombrisse ; le feu (additif) APRES, qu'il brille.
			//    Eclairage eteint et aucune particule : aucun de ces appels n'ecrit
			//    dans la liste (temoin L1 du banc d'Unkeny).
			int32 n = NkDessinerEffets(dl, m.scene, NkPasseEffets2D::NK_EFFETS_ECLAIRES);
			const NkStatsEclairage2D st = NkDessinerEclairage(dl, m.scene);
			n += NkDessinerEffets(dl, m.scene, NkPasseEffets2D::NK_EFFETS_EMISSIFS);
			gChiffres.lumieres = st.lumieres;
			gChiffres.ignorees = st.ignorees;
			gChiffres.occulteurs = st.occulteurs;
			gChiffres.mailles = st.mailles;
			gChiffres.points = st.points;
			gChiffres.particulesDessinees = n;
		}

		void NkEditeurDessinerIconesLumiere(nkgui::NkGuiDrawList &dl, NkEditeurModele &m) {
			if (m.etat == NkEtatJeu::NK_JEU) {
				return; // en jeu, on regarde le jeu
			}
			const NkVue2D &cam = m.scene.Camera();
			const NkColor contour(20, 22, 30, 220);
			m.scene.Monde().Query<NkTransform2D, NkLumiere2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &, NkLumiere2D &l) {
				const NkVec2f e = cam.MondeVersEcran(Position(m, id));
				const NkColor c = Couleur(l.couleur, l.actif ? 255 : 110);
				// Un soleil : un disque et huit rayons.
				for (int32 k = 0; k < 8; ++k) {
					const float32 a = PI * 0.25f * static_cast<float32>(k);
					dl.AddLine(NkVec2f(e.x + math::NkCos(a) * 6.f, e.y + math::NkSin(a) * 6.f),
							   NkVec2f(e.x + math::NkCos(a) * ICONE_PX, e.y + math::NkSin(a) * ICONE_PX), c, 1.5f);
				}
				dl.AddCircleFilled(e, 4.5f, c, 12);
				dl.AddCircle(e, 4.5f, contour, 1.f, 12);
			});
			m.scene.Monde().Query<NkTransform2D, NkEmetteur2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &, NkEmetteur2D &em) {
				const NkVec2f e = cam.MondeVersEcran(Position(m, id));
				const NkColor c = Couleur(em.additif ? 0xFF9A3CFFu : 0xB8C0D0FFu, em.actif ? 255 : 110);
				// Une goutte / une flamme : un losange pointe en haut.
				const NkVec2f pts[4] = {NkVec2f(e.x, e.y - ICONE_PX), NkVec2f(e.x + 5.f, e.y + 1.f), NkVec2f(e.x, e.y + 6.f),
										NkVec2f(e.x - 5.f, e.y + 1.f)};
				dl.AddConvexPolyFilled(pts, 4, c);
				dl.AddPolyline(pts, 4, contour, 1.f, true);
			});

			// La portee de la SELECTION : le cercle, le cone ou la direction, et la
			// poignee a droite du cercle (glisser pour regler la portee).
			if (!m.aSelection || !m.scene.Monde().IsAlive(m.selection)) {
				return;
			}
			const float32 portee = NkEditeurPortee(m, m.selection);
			if (portee <= 0.f) {
				return;
			}
			const NkVec2f o = cam.MondeVersEcran(Position(m, m.selection));
			const float32 r = cam.LongueurVersEcran(portee);
			const NkColor trait(255, 214, 120, 200);
			const NkLumiere2D *l = m.scene.Monde().Get<NkLumiere2D>(m.selection);
			const NkTransform2D *t = m.scene.Monde().Get<NkTransform2D>(m.selection);
			const float32 rot = t != nullptr ? t->rotation : 0.f;
			if (l != nullptr && l->type == NkTypeLumiere2D::NK_DIRECTIONNELLE) {
				// Une fleche dans le sens de la lumiere ; le cercle reste celui de la
				// longueur des ombres.
				const float32 a = l->direction + rot;
				const NkVec2f bout(o.x + math::NkCos(a) * 48.f, o.y - math::NkSin(a) * 48.f);
				dl.AddLine(o, bout, trait, 2.f);
				dl.AddCircleFilled(bout, 4.f, trait, 10);
			}
			dl.AddCircle(o, r, trait, 1.5f, 96);
			if (l != nullptr && l->type == NkTypeLumiere2D::NK_SPOT) {
				const float32 a = l->direction + rot;
				for (int32 k = -1; k <= 1; k += 2) {
					const float32 b = a + static_cast<float32>(k) * l->ouverture;
					dl.AddLine(o, NkVec2f(o.x + math::NkCos(b) * r, o.y - math::NkSin(b) * r), trait, 1.5f);
				}
			}
			if (m.etat == NkEtatJeu::NK_EDITION) {
				const NkRect prise{o.x + r - POIGNEE_PX, o.y - POIGNEE_PX, POIGNEE_PX * 2.f, POIGNEE_PX * 2.f};
				const bool vive = gPrise.tenue || gPrise.survol;
				dl.AddRectFilled(prise, vive ? NkColor(255, 190, 60, 255) : NkColor(255, 214, 120, 220));
				dl.AddRect(prise, contour, 1.f);
			}
		}

		bool NkEditeurGizmoPorteeSouris(NkEditeurCadre &c, const nkgui::NkRect &aire) {
			NkEditeurModele &m = c.m;
			const nkgui::NkGuiInput &in = c.ctx.input;
			const NkVec2f p(in.mousePos.x, in.mousePos.y);
			gPrise.survol = false;
			const bool possible = m.etat == NkEtatJeu::NK_EDITION && m.aSelection && m.scene.Monde().IsAlive(m.selection) &&
								  NkEditeurPortee(m, m.selection) > 0.f;
			if (!possible) {
				gPrise.tenue = false;
				return false;
			}
			const NkVue2D &cam = m.scene.Camera();
			const NkVec2f centre = Position(m, m.selection);
			if (gPrise.tenue) {
				if (!in.mouseDown[0]) {
					gPrise.tenue = false;
					return true;
				}
				const NkVec2f monde = cam.EcranVersMonde(p);
				const float32 dx = monde.x - centre.x;
				const float32 dy = monde.y - centre.y;
				float32 d = math::NkSqrt(dx * dx + dy * dy);
				// Ctrl inverse l'accrochage, comme pour les autres gizmos.
				if (c.ui.accrochage != in.ctrlDown) {
					d = NkEditeurAccrocher(d, c.ui.pasGrille);
				}
				NkEditeurPoserPortee(m, m.selection, d);
				return true;
			}
			const NkVec2f o = cam.MondeVersEcran(centre);
			const float32 r = cam.LongueurVersEcran(NkEditeurPortee(m, m.selection));
			const bool dessus = math::NkAbs(p.x - (o.x + r)) <= POIGNEE_PX + 2.f && math::NkAbs(p.y - o.y) <= POIGNEE_PX + 2.f;
			gPrise.survol = dessus && NkEditeurDans(aire, in.mousePos);
			if (gPrise.survol && in.mouseClicked[0]) {
				gPrise.tenue = true;
				return true;
			}
			return false;
		}

		// =====================================================================
		// Les sections
		// =====================================================================
		void NkEditeurSectionLumiere(NkEditeurCadre &c, ecs::NkEntityId id) {
			bool ouvert = false;
			if (!EnTete(c, id, NkComposantEditeur::NK_LUMIERE, ouvert) || !ouvert) {
				return;
			}
			NkGuiContext &ctx = c.ctx;
			NkLumiere2D *l = c.m.scene.Monde().Get<NkLumiere2D>(id);
			if (l == nullptr) {
				return;
			}
			ctx.PushId("lumiere2d");
			RappelEteint(c);
			static const char *kTypes[3] = {"ponctuelle", "cône", "directionnelle"};
			int32 type = static_cast<int32>(l->type);
			if (Choix(ctx, "type", kTypes, 3, type)) {
				l->type = static_cast<NkTypeLumiere2D>(type);
			}
			CouleurRgba(ctx, "couleur", l->couleur);
			nkgui::SliderFloat(ctx, "intensité", l->intensite, 0.f, 4.f);
			if (l->type == NkTypeLumiere2D::NK_DIRECTIONNELLE) {
				nkgui::SliderFloat(ctx, "longueur des ombres (m)", l->portee, 0.5f, 50.f);
			} else {
				nkgui::SliderFloat(ctx, "portée (m)", l->portee, 0.1f, 30.f);
				nkgui::SliderFloat(ctx, "atténuation (exposant)", l->attenuation, 0.2f, 4.f);
			}
			if (l->type != NkTypeLumiere2D::NK_PONCTUELLE) {
				Angle(ctx, "direction (°)", l->direction, -180.f, 180.f);
			}
			if (l->type == NkTypeLumiere2D::NK_SPOT) {
				Angle(ctx, "ouverture, demi-angle (°)", l->ouverture, 1.f, 180.f);
				nkgui::SliderFloat(ctx, "douceur du bord", l->douceur, 0.f, 1.f);
			}
			nkgui::SliderFloat(ctx, "halo additif", l->halo, 0.f, 1.f);
			nkgui::Checkbox(ctx, "porte des ombres", l->ombres);
			nkgui::Checkbox(ctx, "allumée", l->actif);
			ctx.PopId();
		}

		void NkEditeurSectionEmetteur(NkEditeurCadre &c, ecs::NkEntityId id) {
			bool ouvert = false;
			if (!EnTete(c, id, NkComposantEditeur::NK_EMETTEUR, ouvert) || !ouvert) {
				return;
			}
			NkGuiContext &ctx = c.ctx;
			NkEmetteur2D *e = c.m.scene.Monde().Get<NkEmetteur2D>(id);
			if (e == nullptr) {
				return;
			}
			ctx.PushId("emetteur2d");
			nkgui::Text(ctx, NkString::Format("préréglage : %s", NkNomPresetEffet2D(e->preset)).CStr());
			for (int32 p = 1; p < static_cast<int32>(NkPresetEffet2D::NK_COUNT); ++p) {
				if (p != 1 && p != 4) {
					ctx.SameLine();
				}
				if (nkgui::Button(ctx, NkNomPresetEffet2D(static_cast<NkPresetEffet2D>(p)))) {
					NkEditeurAppliquerPreset(c.m, id, static_cast<NkPresetEffet2D>(p));
					e = c.m.scene.Monde().Get<NkEmetteur2D>(id);
				}
			}
			uint32 vivantes = 0u;
			const NkVector<NkParticuleEffet2D> &ps = c.m.scene.Effets().Particules();
			for (uint32 i = 0; i < ps.Size(); ++i) {
				vivantes += ps[i].emetteur == id.Pack() ? 1u : 0u;
			}
			nkgui::Text(ctx, NkString::Format("particules vivantes : %u / %u", vivantes, e->maxParticules).CStr());
			nkgui::Checkbox(ctx, "actif", e->actif);
			ctx.SameLine();
			nkgui::Checkbox(ctx, "en boucle", e->boucle);
			ctx.SameLine();
			if (nkgui::Button(ctx, "Rejouer")) {
				c.m.scene.Effets().Rejouer(id.Pack());
			}
			if (!e->boucle) {
				nkgui::SliderFloat(ctx, "durée d'émission (s)", e->duree, 0.f, 10.f);
			}
			int32 rafale = static_cast<int32>(e->rafale);
			if (nkgui::InputInt(ctx, "rafale au départ", rafale)) {
				e->rafale = static_cast<uint32>(rafale < 0 ? 0 : (rafale > 4096 ? 4096 : rafale));
			}
			nkgui::SliderFloat(ctx, "débit (par s)", e->debit, 0.f, 500.f);
			nkgui::DragFloat(ctx, "vie min (s)", e->vieMin, 0.01f);
			nkgui::DragFloat(ctx, "vie max (s)", e->vieMax, 0.01f);
			e->vieMin = e->vieMin < 0.02f ? 0.02f : e->vieMin;
			e->vieMax = e->vieMax < e->vieMin ? e->vieMin : e->vieMax;
			nkgui::DragFloat(ctx, "vitesse min (m/s)", e->vitesseMin, 0.02f);
			nkgui::DragFloat(ctx, "vitesse max (m/s)", e->vitesseMax, 0.02f);
			Angle(ctx, "direction (°)", e->direction, -180.f, 180.f);
			Angle(ctx, "dispersion (°)", e->dispersion, 0.f, 180.f);
			nkgui::DragFloat(ctx, "gravité x (m/s²)", e->gravite.x, 0.05f);
			nkgui::DragFloat(ctx, "gravité y (m/s²)", e->gravite.y, 0.05f);
			nkgui::SliderFloat(ctx, "frein de l'air (1/s)", e->frein, 0.f, 5.f);
			nkgui::SliderFloat(ctx, "taille au début (m)", e->tailleDebut, 0.01f, 3.f);
			nkgui::SliderFloat(ctx, "taille à la fin (m)", e->tailleFin, 0.01f, 3.f);
			CouleurRgba(ctx, "couleur au début", e->couleurDebut);
			CouleurRgba(ctx, "couleur à mi-vie", e->couleurMilieu);
			CouleurRgba(ctx, "couleur à la fin", e->couleurFin);
			nkgui::Checkbox(ctx, "additif (émet de la lumière)", e->additif);
			static const char *kFormes[3] = {"douce", "trait", "pleine"};
			int32 forme = static_cast<int32>(e->forme);
			if (Choix(ctx, "forme", kFormes, 3, forme)) {
				e->forme = static_cast<NkFormeParticule2D>(forme);
			}
			static const char *kZones[3] = {"point", "disque", "ligne"};
			int32 zone = static_cast<int32>(e->zone);
			if (Choix(ctx, "zone d'émission", kZones, 3, zone)) {
				e->zone = static_cast<NkZoneEmission2D>(zone);
			}
			if (e->zone == NkZoneEmission2D::NK_DISQUE) {
				nkgui::SliderFloat(ctx, "rayon de la zone (m)", e->rayonZone, 0.f, 10.f);
			} else if (e->zone == NkZoneEmission2D::NK_LIGNE) {
				nkgui::SliderFloat(ctx, "largeur de la zone (m)", e->largeurZone, 0.f, 60.f);
			}
			int32 graine = static_cast<int32>(e->graine);
			if (nkgui::InputInt(ctx, "graine", graine)) {
				e->graine = static_cast<uint32>(graine);
				c.m.scene.Effets().Rejouer(id.Pack()); // une autre graine : un autre effet, depuis le debut
			}
			int32 maxi = static_cast<int32>(e->maxParticules);
			if (nkgui::InputInt(ctx, "plafond de particules", maxi, 16)) {
				e->maxParticules = static_cast<uint32>(maxi < 1 ? 1 : (maxi > 8192 ? 8192 : maxi));
			}
			nkgui::Separator(ctx);
			nkgui::Checkbox(ctx, "éclaire (lumière liée)", e->eclaire);
			if (e->eclaire) {
				if (!c.m.scene.Eclairage().actif) {
					nkgui::TextWrapped(ctx, "(sans effet tant que l'éclairage de la scène est éteint)");
				}
				CouleurRgba(ctx, "couleur de la lumière", e->couleurLumiere);
				nkgui::SliderFloat(ctx, "intensité de la lumière", e->intensiteLumiere, 0.f, 4.f);
				nkgui::SliderFloat(ctx, "portée de la lumière (m)", e->porteeLumiere, 0.1f, 30.f);
				nkgui::SliderFloat(ctx, "vacillement", e->scintillement, 0.f, 1.f);
				nkgui::Checkbox(ctx, "la lumière porte des ombres", e->ombresLumiere);
			}
			ctx.PopId();
		}

		void NkEditeurSectionEclairageMonde(NkEditeurCadre &c) {
			NkGuiContext &ctx = c.ctx;
			NkEclairage2D &ec = c.m.scene.Eclairage();
			nkgui::Separator(ctx);
			nkgui::Text(ctx, "Éclairage 2D (facultatif)");
			ctx.PushId("eclairage2d");
			nkgui::Checkbox(ctx, "éclairage actif", ec.actif);
			if (ec.actif) {
				struct NkAmbiance {
						const char *nom;
						uint32 couleur;
				};
				static const NkAmbiance kAmbiances[4] = {
					{"Jour", 0xFFFFFFFFu}, {"Crépuscule", 0xB08470FFu}, {"Nuit", 0x2A3148FFu}, {"Grotte", 0x0C0C12FFu}};
				for (int32 k = 0; k < 4; ++k) {
					if (k > 0) {
						ctx.SameLine();
					}
					if (nkgui::Button(ctx, kAmbiances[k].nom)) {
						ec.ambiante = kAmbiances[k].couleur;
					}
				}
				CouleurRgba(ctx, "lumière ambiante", ec.ambiante);
				nkgui::Checkbox(ctx, "ombres (collisionneurs)", ec.ombres);
				static const char *kModes[2] = {"multiplié", "voile (repli)"};
				int32 mode = static_cast<int32>(ec.mode);
				if (Choix(ctx, "composition", kModes, 2, mode)) {
					ec.mode = static_cast<NkModeEclairage2D>(mode);
				}
				static const char *kMailles[3] = {"4 px", "8 px", "16 px"};
				int32 maille = ec.maille <= 5.f ? 0 : (ec.maille <= 11.f ? 1 : 2);
				if (Choix(ctx, "maille", kMailles, 3, maille)) {
					ec.maille = maille == 0 ? 4.f : (maille == 1 ? 8.f : 16.f);
				}
				const NkEditeurChiffresLumiere &ch = gChiffres;
				nkgui::Text(ctx, NkString::Format("%d lumières (%d hors budget), %d occulteurs", ch.lumieres, ch.ignorees,
												  ch.occulteurs)
									 .CStr());
				nkgui::Text(ctx, NkString::Format("%d mailles, %d points évalués", ch.mailles, ch.points).CStr());
			} else {
				nkgui::TextWrapped(ctx, "Éteint : la scène se dessine sans lumière ni ombre, exactement comme sans ce réglage.");
			}
			const NkEffets2D &fx = c.m.scene.Effets();
			nkgui::Text(ctx, NkString::Format("particules d'effet : %u / %u (refusées : %u)", fx.NbParticules(), fx.plafond,
											  fx.Refusees())
								 .CStr());
			ctx.PopId();
		}

	} // namespace editeur
} // namespace nkentseu
