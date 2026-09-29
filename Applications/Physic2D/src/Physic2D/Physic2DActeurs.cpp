// =============================================================================
// Physic2DActeurs.cpp
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Physic2D/Physic2DActeurs.h"
#include "NKPhysics/NkParticules2DFabrique.h"

#include <cstdio>

namespace nkentseu {
	namespace physic2d {

		using namespace unkeny;
		using physics::NkPresetP2D;

		namespace {
			// clang-format off
			const NkInfoActeur kActeurs[static_cast<int32>(NkActeur::NK_COUNT)] = {
				{"Ballon",          "Anneau gonflé par un gaz. Crève si on le coupe.",      0xE8483CFFu, false, false},
				{"Blob visqueux",   "Coule comme une gelée, se coupe et se ressoude.",      0x7CE64CFFu, false, false},
				{"Slime géant",     "Un gros blob, plus mou et plus plastique.",            0xB060F5FFu, false, false},
				{"Gelée",           "Tremble et revient toujours à sa forme.",              0xFF5CACFFu, false, false},
				{"Eau",             "Fluide peu visqueux : maintenir pour verser.",         0x3096FFFFu, true,  false},
				{"Miel",            "Fluide très visqueux : maintenir pour verser.",        0xF6AC22FFu, true,  false},
				{"Sable",           "Granulaire, forme des tas : maintenir pour verser.",   0xE4C282FFu, true,  false},
				{"Cristal",         "Réseau d'atomes : se fracture, fond à la chaleur.",    0x46D4FFFFu, false, false},
				{"Disque atomique", "Cristal rond. Chauffez le monde pour le faire fondre.", 0xBCCCE0FFu, false, false},
				{"Tissu",           "Grille de liens, déchirable.",                         0x4274E2FFu, false, false},
				{"Corde",           "Chaîne lestée, accrochée en haut.",                    0xD2B07CFFu, false, false},
				{"Pont suspendu",   "Planches entre deux points fixes : glisser pour tracer.", 0x9C6E44FFu, false, false},
				{"Caisse",          "Corps RIGIDE (NKPhysics) de 20 kg.",                   0xB67E48FFu, false, true},
				{"Balle",           "Corps RIGIDE rond de 3 kg, qui rebondit.",             0xDDDDE6FFu, false, true},
			};
			// clang-format on

			uint32 gCompteurs[static_cast<int32>(NkActeur::NK_COUNT)] = {};
			uint32 gTexCaisse = 0u; ///< 0 = pas de texture : aplat, dessine par le collisionneur
			uint32 gTexBalle = 0u;

			void Nommer(char *out, usize taille, NkActeur a) {
				const uint32 n = ++gCompteurs[static_cast<int32>(a)];
				std::snprintf(out, taille, "%s_%u", NkActeurInfo(a).nom, static_cast<unsigned>(n));
				for (usize i = 0; out[i] != '\0'; ++i) {
					if (out[i] == ' ') {
						out[i] = '_';
					}
				}
			}

			ecs::NkEntityId Mou(NkScene &s, NkActeur a, int32 indexCorps) {
				if (indexCorps < 0) {
					return ecs::NkEntityId::Invalid();
				}
				char nom[32];
				Nommer(nom, sizeof(nom), a);
				return s.CreerCorpsMou(nom, indexCorps, NkActeurInfo(a).couleur);
			}

			ecs::NkEntityId Statique(NkScene &s, const char *nom, const NkVec2f &centre, const NkVec2f &demi, bool sol) {
				const ecs::NkEntityId e = s.Creer(nom, centre);
				NkCollisionneur2D col;
				col.forme = NkForme2D::NK_BOITE;
				col.demiTaille = demi;
				s.Monde().Add<NkCollisionneur2D>(e, col);
				NkDecor2D d;
				d.sol = sol;
				s.Monde().Add<NkDecor2D>(e, d);
				NkCorps2D c;
				c.type = NkTypeCorps::NK_STATIQUE;
				c.friction = 0.6f;
				s.AjouterCorps(e, c);
				return e;
			}
		} // namespace

		const NkInfoActeur &NkActeurInfo(NkActeur a) noexcept {
			const int32 i = static_cast<int32>(a);
			return kActeurs[(i >= 0 && i < static_cast<int32>(NkActeur::NK_COUNT)) ? i : 0];
		}

		ecs::NkEntityId NkPoserActeur(NkScene &s, NkActeur a, const NkVec2f &posDemandee) {
			physics::NkParticules2D *p = s.Particules();
			if (p == nullptr) {
				return ecs::NkEntityId::Invalid();
			}
			// On pose DANS la boite : un acteur ne nait pas dans un mur.
			const NkVec2f pos(math::NkClamp(posDemandee.x, -NK_DEMI_LARGEUR + 0.7f, NK_DEMI_LARGEUR - 0.7f),
							  math::NkClamp(posDemandee.y, 0.7f, NK_HAUTEUR - 0.7f));
			switch (a) {
				case NkActeur::NK_BALLON:
					return Mou(s, a, physics::NkCreerBallonP2D(*p, pos));
				case NkActeur::NK_BLOB:
					return Mou(s, a, physics::NkCreerBlobP2D(*p, pos, 0.42f, NkPresetP2D::NK_BLOB));
				case NkActeur::NK_SLIME:
					return Mou(s, a, physics::NkCreerBlobP2D(*p, pos, 0.78f, NkPresetP2D::NK_SLIME));
				case NkActeur::NK_GELEE:
					return Mou(s, a, physics::NkCreerGeleeP2D(*p, pos));
				case NkActeur::NK_EAU:
					return Mou(s, a, physics::NkCreerFluideP2D(*p, pos, 0.22f, NkPresetP2D::NK_EAU));
				case NkActeur::NK_MIEL:
					return Mou(s, a, physics::NkCreerFluideP2D(*p, pos, 0.22f, NkPresetP2D::NK_MIEL));
				case NkActeur::NK_SABLE:
					return Mou(s, a, physics::NkCreerFluideP2D(*p, pos, 0.22f, NkPresetP2D::NK_SABLE));
				case NkActeur::NK_CRISTAL:
					return Mou(s, a, physics::NkCreerCristalP2D(*p, pos, false));
				case NkActeur::NK_DISQUE:
					return Mou(s, a, physics::NkCreerCristalP2D(*p, pos, true));
				case NkActeur::NK_TISSU:
					return Mou(s, a, physics::NkCreerTissuP2D(*p, NkVec2f(pos.x, math::NkMin(pos.y + 1.f, NK_HAUTEUR - 0.1f))));
				case NkActeur::NK_CORDE:
					return Mou(s, a, physics::NkCreerCordeP2D(*p, NkVec2f(pos.x, math::NkMin(pos.y + 1.5f, NK_HAUTEUR - 0.1f))));
				case NkActeur::NK_PONT:
					return NkPoserPont(s, pos - NkVec2f(1.7f, 0.f), pos + NkVec2f(1.7f, 0.f));
				case NkActeur::NK_CAISSE:
				case NkActeur::NK_BALLE: {
					char nom[32];
					Nommer(nom, sizeof(nom), a);
					const bool caisse = a == NkActeur::NK_CAISSE;
					const ecs::NkEntityId e = s.Creer(nom, pos);
					NkCollisionneur2D col;
					col.forme = caisse ? NkForme2D::NK_BOITE : NkForme2D::NK_CERCLE;
					col.demiTaille = NkVec2f(0.33f, 0.33f);
					col.rayon = 0.25f;
					s.Monde().Add<NkCollisionneur2D>(e, col);
					NkSprite2D sp;
					sp.taille = caisse ? NkVec2f(0.66f, 0.66f) : NkVec2f(0.5f, 0.5f);
					sp.texId = caisse ? gTexCaisse : gTexBalle;
					// Texture : l'image telle quelle (blanc). Sans texture, le sprite
					// est CACHE : la demo dessine alors la forme du collisionneur, et
					// un carre de couleur par-dessus une balle serait faux.
					sp.couleur = sp.texId != 0u ? 0xFFFFFFFFu : NkActeurInfo(a).couleur;
					sp.visible = sp.texId != 0u;
					s.Monde().Add<NkSprite2D>(e, sp);
					NkCorps2D c;
					c.type = NkTypeCorps::NK_DYNAMIQUE;
					c.masse = caisse ? 20.f : 3.f;
					c.friction = caisse ? 0.6f : 0.4f;
					c.rebond = caisse ? 0.1f : 0.55f;
					s.AjouterCorps(e, c);
					return e;
				}
				default:
					return ecs::NkEntityId::Invalid();
			}
		}

		void NkCreerTexturesActeurs(NkTextures2D &textures) {
			// --- La caisse : 32 x 32, planches horizontales, cadre, croisillon ---
			const int32 N = 32;
			uint8 px[N * N * 4];
			auto poser = [&](int32 x, int32 y, uint8 r, uint8 g, uint8 b, uint8 al) {
				uint8 *p = px + (y * N + x) * 4;
				p[0] = r;
				p[1] = g;
				p[2] = b;
				p[3] = al;
			};
			for (int32 y = 0; y < N; ++y) {
				for (int32 x = 0; x < N; ++x) {
					// Le bois : un veinage qui ondule, plus sombre entre les planches.
					const int32 planche = y / 8;
					const float32 veine = math::NkSin(static_cast<float32>(x) * 0.45f + static_cast<float32>(planche) * 1.7f +
													  static_cast<float32>(y % 8) * 0.3f);
					float32 k = 0.86f + 0.07f * veine;
					if (y % 8 == 0) {
						k = 0.55f; // le joint
					}
					const bool cadre = x < 3 || x >= N - 3 || y < 3 || y >= N - 3;
					const int32 d1 = x - y, d2 = x - (N - 1 - y);
					const bool croix = (d1 >= -2 && d1 <= 2) || (d2 >= -2 && d2 <= 2);
					if (cadre || croix) {
						k = (x == 0 || y == 0 || x == N - 1 || y == N - 1) ? 0.45f : 0.72f + 0.05f * veine;
					}
					poser(x, y, static_cast<uint8>(182.f * k), static_cast<uint8>(126.f * k), static_cast<uint8>(72.f * k), 255);
				}
			}
			// Les clous, aux quatre coins du cadre.
			const int32 clous[4][2] = {{1, 1}, {N - 3, 1}, {1, N - 3}, {N - 3, N - 3}};
			for (const auto &c : clous) {
				for (int32 dy = 0; dy < 2; ++dy) {
					for (int32 dx = 0; dx < 2; ++dx) {
						poser(c[0] + dx, c[1] + dy, 60, 58, 56, 255);
					}
				}
			}
			gTexCaisse = textures.Creer(px, N, N, "physic2d/caisse");

			// --- La balle : disque a quartiers (on VOIT qu'elle roule) ----------
			for (int32 y = 0; y < N; ++y) {
				for (int32 x = 0; x < N; ++x) {
					const float32 dx = (static_cast<float32>(x) + 0.5f) / static_cast<float32>(N) * 2.f - 1.f;
					const float32 dy = (static_cast<float32>(y) + 0.5f) / static_cast<float32>(N) * 2.f - 1.f;
					const float32 r = math::NkSqrt(dx * dx + dy * dy);
					// Bord adouci sur un pixel : sans lui, la balle crenele en tournant.
					const float32 a = math::NkClamp((1.f - r) * static_cast<float32>(N) * 0.5f, 0.f, 1.f);
					const bool quartier = (dx > 0.f) == (dy > 0.f);
					float32 k = quartier ? 0.95f : 0.30f;
					k *= 1.f - 0.35f * r * r;										// ombre vers le bord
					const float32 reflet = math::NkMax(0.f, 1.f - ((dx + 0.35f) * (dx + 0.35f) + (dy + 0.4f) * (dy + 0.4f)) * 9.f);
					k = math::NkMin(1.f, k + reflet * 0.5f);
					const uint8 v = static_cast<uint8>(255.f * k);
					poser(x, y, v, quartier ? v : static_cast<uint8>(static_cast<float32>(v) * 0.4f + 60.f),
						  quartier ? static_cast<uint8>(static_cast<float32>(v) * 0.9f) : static_cast<uint8>(180.f * k + 40.f),
						  static_cast<uint8>(255.f * a));
				}
			}
			gTexBalle = textures.Creer(px, N, N, "physic2d/balle");
		}

		void NkCreerSonsActeurs(NkSons2D &sons, uint32 &pose, uint32 &explosion, uint32 &coupe) {
			const int32 f = 44100;
			NkVector<float32> b;
			uint32 graine = 0x1234567u;
			auto bruit = [&]() {
				graine ^= graine << 13;
				graine ^= graine >> 17;
				graine ^= graine << 5;
				return static_cast<float32>(graine & 0xFFFFu) / 32768.f - 1.f;
			};
			// Pose : un « plop » — sinus qui descend de 520 a 180 Hz en 90 ms.
			b.Resize(static_cast<usize>(f * 0.09f));
			float32 phase = 0.f;
			for (uint32 i = 0; i < b.Size(); ++i) {
				const float32 t = static_cast<float32>(i) / static_cast<float32>(b.Size());
				phase += 6.2831853f * (520.f - 340.f * t) / static_cast<float32>(f);
				b[i] = 0.5f * math::NkSin(phase) * (1.f - t) * (1.f - t);
			}
			pose = sons.Creer(b.Data(), b.Size(), f, "physic2d/pose");
			// Explosion : bruit filtre qui s'assourdit, et un grondement a 55 Hz.
			b.Resize(static_cast<usize>(f * 0.8f));
			float32 bas = 0.f;
			phase = 0.f;
			for (uint32 i = 0; i < b.Size(); ++i) {
				const float32 t = static_cast<float32>(i) / static_cast<float32>(b.Size());
				const float32 k = 0.25f - 0.22f * t; // le filtre se ferme : le souffle devient sourd
				bas += (bruit() - bas) * k;
				phase += 6.2831853f * 55.f / static_cast<float32>(f);
				b[i] = (0.9f * bas + 0.5f * math::NkSin(phase)) * math::NkExp(-5.f * t);
			}
			explosion = sons.Creer(b.Data(), b.Size(), f, "physic2d/explosion");
			// Coupe : un souffle bref et aigu.
			b.Resize(static_cast<usize>(f * 0.07f));
			float32 prec = 0.f;
			for (uint32 i = 0; i < b.Size(); ++i) {
				const float32 t = static_cast<float32>(i) / static_cast<float32>(b.Size());
				const float32 n = bruit();
				b[i] = 0.35f * (n - prec) * math::NkSin(3.1415926f * t); // passe-haut grossier
				prec = n;
			}
			coupe = sons.Creer(b.Data(), b.Size(), f, "physic2d/coupe");
		}

		ecs::NkEntityId NkOuvrirPinceau(NkScene &s, NkActeur a) {
			physics::NkParticules2D *p = s.Particules();
			if (p == nullptr || !NkActeurInfo(a).pinceau) {
				return ecs::NkEntityId::Invalid();
			}
			const NkPresetP2D preset = a == NkActeur::NK_EAU	? NkPresetP2D::NK_EAU
									   : a == NkActeur::NK_MIEL ? NkPresetP2D::NK_MIEL
																: NkPresetP2D::NK_SABLE;
			return Mou(s, a, physics::NkOuvrirPinceauP2D(*p, preset));
		}

		bool NkVerser(NkScene &s, ecs::NkEntityId e, const NkVec2f &pos, int32 n, uint32 &graine) {
			physics::NkParticules2D *p = s.Particules();
			const NkCorpsMou2D *m = s.Monde().Get<NkCorpsMou2D>(e);
			if (p == nullptr || m == nullptr) {
				return false;
			}
			const int32 ci = p->IndexCorps(m->corpsId);
			if (ci < 0 || static_cast<uint32>(ci) + 1u != p->corps.Size()) {
				return false;
			}
			physics::NkVerserP2D(*p, static_cast<uint32>(ci), pos, 0.2f, n, graine);
			return true;
		}

		ecs::NkEntityId NkPoserPont(NkScene &s, const NkVec2f &a, const NkVec2f &b) {
			physics::NkParticules2D *p = s.Particules();
			if (p == nullptr) {
				return ecs::NkEntityId::Invalid();
			}
			return Mou(s, NkActeur::NK_PONT, physics::NkCreerPontP2D(*p, a, b));
		}

		ecs::NkEntityId NkPoserObstacle(NkScene &s, const NkVec2f &a, const NkVec2f &b, float32 rayon) {
			const NkVec2f ab = b - a;
			const float32 L = math::NkSqrt(ab.x * ab.x + ab.y * ab.y);
			const ecs::NkEntityId e = s.Creer("Obstacle", (a + b) * 0.5f);
			if (NkTransform2D *t = s.Monde().Get<NkTransform2D>(e)) {
				t->rotation = L > 1.0e-4f ? math::NkAtan2(ab.y, ab.x) : 0.f;
			}
			NkCollisionneur2D col;
			col.forme = L > 1.0e-4f ? NkForme2D::NK_CAPSULE : NkForme2D::NK_CERCLE;
			col.demiTaille = NkVec2f(L * 0.5f, 0.f);
			col.rayon = rayon;
			s.Monde().Add<NkCollisionneur2D>(e, col);
			s.Monde().Add<NkDecor2D>(e, NkDecor2D{});
			NkCorps2D c;
			c.type = NkTypeCorps::NK_STATIQUE;
			c.friction = 0.5f;
			s.AjouterCorps(e, c);
			return e;
		}

		const char *NkNiveauNom(int32 i) noexcept {
			switch (i) {
				case 0:
					return "Démo · Tout à la fois";
				case 1:
					return "Bac à slime";
				case 2:
					return "Cristaux et chaleur";
				case 3:
					return "Tissus, cordes et ponts";
				default:
					return "Niveau vide";
			}
		}

		void NkChargerNiveau(NkScene &s, int32 i) {
			// Le marqueur de decor doit survivre a Jouer / Arreter : sans lui,
			// le sol restaure devient effacable a la gomme.
			s.PhotographierAussi<NkDecor2D>("physic2d.NkDecor2D");
			NkVector<ecs::NkEntityId> ids;
			s.Entites(ids);
			for (uint32 k = 0; k < ids.Size(); ++k) {
				s.Detruire(ids[k]);
			}
			if (physics::NkParticules2D *p = s.Particules()) {
				p->Vider();
				p->reglages.temperature = 0.f;
				p->reglages.vent = 0.f;
			}
			for (int32 k = 0; k < static_cast<int32>(NkActeur::NK_COUNT); ++k) {
				gCompteurs[k] = 0;
			}

			// La boite : sol, murs, plafond. Des corps STATIQUES ordinaires : les
			// rigides et les particules les touchent par le meme chemin.
			const float32 L = NK_DEMI_LARGEUR;
			const float32 H = NK_HAUTEUR;
			Statique(s, "Sol", NkVec2f(0.f, -0.5f), NkVec2f(L + 1.f, 0.5f), true);
			Statique(s, "Mur_gauche", NkVec2f(-L - 0.5f, H * 0.5f), NkVec2f(0.5f, H * 0.5f + 1.f), true);
			Statique(s, "Mur_droit", NkVec2f(L + 0.5f, H * 0.5f), NkVec2f(0.5f, H * 0.5f + 1.f), true);
			Statique(s, "Plafond", NkVec2f(0.f, H + 0.5f), NkVec2f(L + 1.f, 0.5f), true);

			auto obstacle = [&](float32 ax, float32 ay, float32 bx, float32 by, float32 r) {
				NkPoserObstacle(s, NkVec2f(ax, ay), NkVec2f(bx, by), r);
			};
			auto poser = [&](NkActeur a, float32 x, float32 y) { NkPoserActeur(s, a, NkVec2f(x, y)); };
			physics::NkParticules2D *p = s.Particules();

			switch (i) {
				case 0: {
					obstacle(-11.5f, 7.6f, -6.4f, 5.6f, 0.10f); // rampe
					obstacle(1.5f, 0.f, 1.5f, 1.9f, 0.09f);	   // bac a eau
					obstacle(4.7f, 0.f, 4.7f, 1.9f, 0.09f);
					obstacle(-5.2f, 0.f, -5.2f, 3.3f, 0.12f); // piliers du pont
					obstacle(-1.2f, 0.f, -1.2f, 3.3f, 0.12f);
					obstacle(6.2f, 4.8f, 9.8f, 4.8f, 0.10f); // etagere
					obstacle(-0.4f, 7.6f, -0.4f, 7.6f, 0.22f); // chevilles
					obstacle(0.8f, 7.0f, 0.8f, 7.0f, 0.22f);
					NkPoserPont(s, NkVec2f(-5.2f, 3.45f), NkVec2f(-1.2f, 3.45f));
					if (p != nullptr) {
						Mou(s, NkActeur::NK_EAU, physics::NkCreerFluideP2D(*p, NkVec2f(3.1f, 1.1f), 0.95f, NkPresetP2D::NK_EAU));
					}
					poser(NkActeur::NK_BALLON, -10.5f, 9.f);
					poser(NkActeur::NK_BLOB, -3.3f, 8.f);
					if (p != nullptr) {
						Mou(s, NkActeur::NK_SLIME, physics::NkCreerBlobP2D(*p, NkVec2f(3.f, 9.f), 0.70f, NkPresetP2D::NK_SLIME));
					}
					poser(NkActeur::NK_GELEE, -3.f, 11.5f);
					poser(NkActeur::NK_CAISSE, 7.6f, 5.6f);
					poser(NkActeur::NK_BALLE, 9.f, 6.f);
					poser(NkActeur::NK_TISSU, 9.2f, 9.4f);
					poser(NkActeur::NK_CRISTAL, -8.6f, 0.6f);
					poser(NkActeur::NK_CORDE, -0.6f, 10.f);
					break;
				}
				case 1: {
					obstacle(-7.f, 4.2f, -3.f, 1.2f, 0.12f); // entonnoir
					obstacle(3.f, 1.2f, 7.f, 4.2f, 0.12f);
					obstacle(-3.f, 1.2f, -3.f, 0.f, 0.12f);
					obstacle(3.f, 1.2f, 3.f, 0.f, 0.12f);
					if (p != nullptr) {
						Mou(s, NkActeur::NK_SLIME, physics::NkCreerBlobP2D(*p, NkVec2f(-3.5f, 9.f), 0.80f, NkPresetP2D::NK_SLIME));
						Mou(s, NkActeur::NK_BLOB, physics::NkCreerBlobP2D(*p, NkVec2f(0.f, 11.5f), 0.45f, NkPresetP2D::NK_BLOB));
						Mou(s, NkActeur::NK_BLOB, physics::NkCreerBlobP2D(*p, NkVec2f(2.5f, 8.f), 0.40f, NkPresetP2D::NK_BLOB));
						Mou(s, NkActeur::NK_SLIME, physics::NkCreerBlobP2D(*p, NkVec2f(5.2f, 11.5f), 0.60f, NkPresetP2D::NK_SLIME));
						Mou(s, NkActeur::NK_MIEL, physics::NkCreerFluideP2D(*p, NkVec2f(-8.5f, 6.f), 0.70f, NkPresetP2D::NK_MIEL));
					}
					poser(NkActeur::NK_BALLON, 9.f, 7.f);
					poser(NkActeur::NK_GELEE, 0.f, 6.f);
					poser(NkActeur::NK_CAISSE, -9.f, 1.f);
					break;
				}
				case 2: {
					poser(NkActeur::NK_CRISTAL, -7.f, 0.6f);
					poser(NkActeur::NK_CRISTAL, -7.f, 4.f);
					poser(NkActeur::NK_DISQUE, 0.f, 0.8f);
					poser(NkActeur::NK_DISQUE, 0.f, 5.f);
					poser(NkActeur::NK_CRISTAL, 6.5f, 0.6f);
					obstacle(4.f, 7.f, 9.f, 7.f, 0.10f);
					poser(NkActeur::NK_CAISSE, 6.5f, 9.f);
					break;
				}
				case 3: {
					poser(NkActeur::NK_TISSU, -7.f, 10.f);
					poser(NkActeur::NK_TISSU, -2.5f, 10.f);
					poser(NkActeur::NK_CORDE, 2.5f, 10.5f);
					poser(NkActeur::NK_CORDE, 4.f, 10.5f);
					poser(NkActeur::NK_CORDE, 5.5f, 10.5f);
					obstacle(-9.f, 0.f, -9.f, 3.f, 0.12f);
					obstacle(-3.f, 0.f, -3.f, 3.f, 0.12f);
					NkPoserPont(s, NkVec2f(-9.f, 3.15f), NkVec2f(-3.f, 3.15f));
					poser(NkActeur::NK_BALLON, -6.f, 6.f);
					poser(NkActeur::NK_CAISSE, -7.5f, 7.f);
					poser(NkActeur::NK_GELEE, -4.5f, 7.5f);
					poser(NkActeur::NK_BALLE, 4.f, 6.f);
					break;
				}
				default:
					break;
			}
		}

	} // namespace physic2d
} // namespace nkentseu
