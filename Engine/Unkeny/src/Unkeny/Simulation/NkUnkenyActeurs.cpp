// =============================================================================
// NkUnkenyActeurs.cpp
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Unkeny/Simulation/NkUnkenyActeurs.h"

#include "NKPhysics/NkParticules2DFabrique.h"
#include "Unkeny/Rendu/NkUnkenyTextures.h"
#include "Unkeny/Son/NkUnkenySon.h"

#include <cstdio>

namespace nkentseu {
	namespace unkeny {

		using physics::NkPresetP2D;

		namespace {
			using C = NkCategorieActeur;
			// clang-format off
			const NkInfoActeurSim kActeurs[static_cast<int32>(NkActeurSim::NK_COUNT)] = {
				{"Ballon",          "Anneau gonflé par un gaz. Crève si on le coupe.",         0xE8483CFFu, false, false, false, C::NK_CORPS_MOUS},
				{"Blob visqueux",   "Coule comme une gelée, se coupe et se ressoude.",         0x7CE64CFFu, false, false, false, C::NK_CORPS_MOUS},
				{"Slime géant",     "Un gros blob, plus mou et plus plastique.",               0xB060F5FFu, false, false, false, C::NK_CORPS_MOUS},
				{"Gelée",           "Tremble et revient toujours à sa forme.",                 0xFF5CACFFu, false, false, false, C::NK_CORPS_MOUS},
				{"Eau",             "Fluide peu visqueux : maintenir pour verser.",            0x3096FFFFu, true,  false, false, C::NK_FLUIDES},
				{"Miel",            "Fluide très visqueux : maintenir pour verser.",           0xF6AC22FFu, true,  false, false, C::NK_FLUIDES},
				{"Sable",           "Granulaire, forme des tas : maintenir pour verser.",      0xE4C282FFu, true,  false, false, C::NK_FLUIDES},
				{"Cristal",         "Réseau d'atomes : se fracture, fond à la chaleur.",       0x46D4FFFFu, false, false, false, C::NK_ATOMES},
				{"Disque atomique", "Cristal rond. Chauffez le monde pour le faire fondre.",   0xBCCCE0FFu, false, false, false, C::NK_ATOMES},
				{"Tissu",           "Grille de liens, déchirable.",                            0x4274E2FFu, false, false, false, C::NK_TISSUS},
				{"Corde",           "Chaîne lestée, accrochée en haut.",                       0xD2B07CFFu, false, false, false, C::NK_TISSUS},
				{"Pont suspendu",   "Planches entre deux points fixes : glisser pour tracer.", 0x9C6E44FFu, false, false, true,  C::NK_TISSUS},
				{"Caisse",          "Corps RIGIDE (NKPhysics) de 20 kg.",                      0xB67E48FFu, false, true,  false, C::NK_RIGIDES},
				{"Balle",           "Corps RIGIDE rond de 3 kg, qui rebondit.",                0xDDDDE6FFu, false, true,  false, C::NK_RIGIDES},
			};
			// clang-format on

			uint32 gCompteurs[static_cast<int32>(NkActeurSim::NK_COUNT)] = {};

			void Nommer(char *out, usize taille, NkActeurSim a) {
				const uint32 n = ++gCompteurs[static_cast<int32>(a)];
				std::snprintf(out, taille, "%s_%u", NkActeurSimInfo(a).nom, static_cast<unsigned>(n));
				for (usize i = 0; out[i] != '\0'; ++i) {
					if (out[i] == ' ') {
						out[i] = '_';
					}
				}
			}

			ecs::NkEntityId Mou(NkScene &s, NkActeurSim a, int32 indexCorps) {
				if (indexCorps < 0) {
					return ecs::NkEntityId::Invalid();
				}
				char nom[32];
				Nommer(nom, sizeof(nom), a);
				return s.CreerCorpsMou(nom, indexCorps, NkActeurSimInfo(a).couleur);
			}

			void Textures(NkRessourcesSim &res, NkTextures2D &textures) {
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
				res.texCaisse = textures.Creer(px, N, N, "unkeny/sim/caisse");

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
				res.texBalle = textures.Creer(px, N, N, "unkeny/sim/balle");
			}

			void Sons(NkRessourcesSim &res, NkSons2D &sons) {
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
				res.sonPose = sons.Creer(b.Data(), b.Size(), f, "unkeny/sim/pose");
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
				res.sonExplosion = sons.Creer(b.Data(), b.Size(), f, "unkeny/sim/explosion");
				// Coupe : un souffle bref et aigu.
				b.Resize(static_cast<usize>(f * 0.07f));
				float32 prec = 0.f;
				for (uint32 i = 0; i < b.Size(); ++i) {
					const float32 t = static_cast<float32>(i) / static_cast<float32>(b.Size());
					const float32 n = bruit();
					b[i] = 0.35f * (n - prec) * math::NkSin(3.1415926f * t); // passe-haut grossier
					prec = n;
				}
				res.sonCoupe = sons.Creer(b.Data(), b.Size(), f, "unkeny/sim/coupe");
			}

		} // namespace

		const char *NkCategorieActeurNom(NkCategorieActeur c) noexcept {
			switch (c) {
				case NkCategorieActeur::NK_CORPS_MOUS:
					return "Corps mous";
				case NkCategorieActeur::NK_FLUIDES:
					return "Fluides";
				case NkCategorieActeur::NK_ATOMES:
					return "Atomes";
				case NkCategorieActeur::NK_TISSUS:
					return "Tissus et cordes";
				case NkCategorieActeur::NK_RIGIDES:
					return "Corps rigides";
				default:
					return "";
			}
		}

		const NkInfoActeurSim &NkActeurSimInfo(NkActeurSim a) noexcept {
			const int32 i = static_cast<int32>(a);
			return kActeurs[(i >= 0 && i < static_cast<int32>(NkActeurSim::NK_COUNT)) ? i : 0];
		}

		void NkNommerActeurSim(char *sortie, usize taille, NkActeurSim a) noexcept {
			Nommer(sortie, taille, a);
		}

		void NkRemettreNomsSim() noexcept {
			for (int32 k = 0; k < static_cast<int32>(NkActeurSim::NK_COUNT); ++k) {
				gCompteurs[k] = 0;
			}
		}

		void NkCreerRessourcesSim(NkRessourcesSim &sortie, NkTextures2D *textures, NkSons2D *sons) {
			if (textures != nullptr) {
				Textures(sortie, *textures);
			}
			if (sons != nullptr) {
				Sons(sortie, *sons);
			}
		}

		ecs::NkEntityId NkPoserActeurSim(NkScene &s, NkActeurSim a, const NkVec2f &pos, const NkRessourcesSim *res,
										 float32 plafond) {
			physics::NkParticules2D *p = s.Particules();
			const bool rigide = NkActeurSimInfo(a).rigide;
			if (p == nullptr && !rigide) {
				return ecs::NkEntityId::Invalid();
			}
			if (!rigide) {
				return Mou(s, a, NkCreerMatiereSim(s, a, pos, plafond));
			}
			switch (a) {
				case NkActeurSim::NK_CAISSE:
				case NkActeurSim::NK_BALLE: {
					if (!s.PhysiqueActive()) {
						return ecs::NkEntityId::Invalid();
					}
					char nom[32];
					Nommer(nom, sizeof(nom), a);
					const bool caisse = a == NkActeurSim::NK_CAISSE;
					const ecs::NkEntityId e = s.Creer(nom, pos);
					NkCollisionneur2D col;
					col.forme = caisse ? NkForme2D::NK_BOITE : NkForme2D::NK_CERCLE;
					col.demiTaille = NkVec2f(0.33f, 0.33f);
					col.rayon = 0.25f;
					s.Monde().Add<NkCollisionneur2D>(e, col);
					NkSprite2D sp;
					sp.taille = caisse ? NkVec2f(0.66f, 0.66f) : NkVec2f(0.5f, 0.5f);
					sp.texId = res != nullptr ? (caisse ? res->texCaisse : res->texBalle) : 0u;
					// Texture : l'image telle quelle (blanc). Sans texture, le sprite
					// est CACHE : NkDessinerFormes dessine alors le collisionneur, et
					// un carre de couleur par-dessus une balle serait faux.
					sp.couleur = sp.texId != 0u ? 0xFFFFFFFFu : NkActeurSimInfo(a).couleur;
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

		int32 NkCreerMatiereSim(NkScene &s, NkActeurSim a, const NkVec2f &pos, float32 plafond) {
			physics::NkParticules2D *p = s.Particules();
			if (p == nullptr) {
				return -1;
			}
			switch (a) {
				case NkActeurSim::NK_BALLON:
					return physics::NkCreerBallonP2D(*p, pos);
				case NkActeurSim::NK_BLOB:
					return physics::NkCreerBlobP2D(*p, pos, 0.42f, NkPresetP2D::NK_BLOB);
				case NkActeurSim::NK_SLIME:
					return physics::NkCreerBlobP2D(*p, pos, 0.78f, NkPresetP2D::NK_SLIME);
				case NkActeurSim::NK_GELEE:
					return physics::NkCreerGeleeP2D(*p, pos);
				case NkActeurSim::NK_EAU:
					return physics::NkCreerFluideP2D(*p, pos, 0.22f, NkPresetP2D::NK_EAU);
				case NkActeurSim::NK_MIEL:
					return physics::NkCreerFluideP2D(*p, pos, 0.22f, NkPresetP2D::NK_MIEL);
				case NkActeurSim::NK_SABLE:
					return physics::NkCreerFluideP2D(*p, pos, 0.22f, NkPresetP2D::NK_SABLE);
				case NkActeurSim::NK_CRISTAL:
					return physics::NkCreerCristalP2D(*p, pos, false);
				case NkActeurSim::NK_DISQUE:
					return physics::NkCreerCristalP2D(*p, pos, true);
				case NkActeurSim::NK_TISSU:
					return physics::NkCreerTissuP2D(*p, NkVec2f(pos.x, math::NkMin(pos.y + 1.f, plafond)));
				case NkActeurSim::NK_CORDE:
					return physics::NkCreerCordeP2D(*p, NkVec2f(pos.x, math::NkMin(pos.y + 1.5f, plafond)));
				case NkActeurSim::NK_PONT:
					return physics::NkCreerPontP2D(*p, pos - NkVec2f(1.7f, 0.f), pos + NkVec2f(1.7f, 0.f));
				default:
					return -1; // les rigides n'ont pas de matiere
			}
		}

		ecs::NkEntityId NkOuvrirPinceauSim(NkScene &s, NkActeurSim a) {
			physics::NkParticules2D *p = s.Particules();
			if (p == nullptr || !NkActeurSimInfo(a).pinceau) {
				return ecs::NkEntityId::Invalid();
			}
			const NkPresetP2D preset = a == NkActeurSim::NK_EAU	  ? NkPresetP2D::NK_EAU
									   : a == NkActeurSim::NK_MIEL ? NkPresetP2D::NK_MIEL
																   : NkPresetP2D::NK_SABLE;
			return Mou(s, a, physics::NkOuvrirPinceauP2D(*p, preset));
		}

		bool NkVerserSim(NkScene &s, ecs::NkEntityId e, const NkVec2f &pos, int32 n, uint32 &graine) {
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

		ecs::NkEntityId NkPoserPontSim(NkScene &s, const NkVec2f &a, const NkVec2f &b) {
			physics::NkParticules2D *p = s.Particules();
			if (p == nullptr) {
				return ecs::NkEntityId::Invalid();
			}
			return Mou(s, NkActeurSim::NK_PONT, physics::NkCreerPontP2D(*p, a, b));
		}

		ecs::NkEntityId NkPoserObstacleSim(NkScene &s, const NkVec2f &a, const NkVec2f &b, float32 rayon) {
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
			NkCorps2D c;
			c.type = NkTypeCorps::NK_STATIQUE;
			c.friction = 0.5f;
			s.AjouterCorps(e, c);
			return e;
		}

	} // namespace unkeny
} // namespace nkentseu
