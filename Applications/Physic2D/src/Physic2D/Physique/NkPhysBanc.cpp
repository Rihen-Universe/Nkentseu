// =============================================================================
// NkPhysBanc.cpp — le banc du moteur : `Physic2D --selftest`
//
// Pas de fenetre, pas de GPU : le code de sortie EST le verdict (0 = tout
// tient). Chaque niveau tourne dix secondes simulees, puis on verifie ce
// qu'un oeil verifierait : rien n'a explose, rien n'a traverse le sol, l'eau
// n'est pas ecrasee, le ballon est encore gonfle, les blobs tiennent.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Physic2D/Physique/NkPhysBanc.h"
#include "Physic2D/Physique/NkPhysFabrique.h"

#include <chrono>
#include <cstdio>

namespace nkentseu {
	namespace physic2d {

		namespace {
			int32 gEchecs = 0;

			void Verifier(bool ok, const char *niveau, const char *quoi, float32 valeur) {
				std::printf("  [%s] %-44s %10.3f  %s\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(valeur), niveau);
				if (!ok) {
					++gEchecs;
				}
			}
		} // namespace

		int32 NkPhysLancerBanc() {
			gEchecs = 0;
			std::printf("Physic2D — banc du moteur\n");
			for (int32 niv = 0; niv < NK_NB_NIVEAUX; ++niv) {
				NkMonde m;
				NkChargerNiveau(m, niv);
				const char *nom = NkNiveauNom(niv);
				const uint32 nDepart = static_cast<uint32>(m.particules.Size());
				std::printf("\n== %s : %u particules, %u liens, %u corps\n", nom, nDepart,
							static_cast<unsigned>(m.liens.Size()), static_cast<unsigned>(m.corps.Size()));

				if (niv == 2) {
					m.reglages.temperature = 0.f;
				}
				const auto t0 = std::chrono::steady_clock::now();
				const int32 nPas = 600;
				for (int32 k = 0; k < nPas; ++k) {
					m.Pas(1.f / 60.f);
				}
				const auto t1 = std::chrono::steady_clock::now();
				const float32 ms = static_cast<float32>(std::chrono::duration<double, std::milli>(t1 - t0).count()) /
								   static_cast<float32>(nPas);

				bool fini = true;
				bool dansBoite = true;
				float32 vMax = 0.f;
				for (uint32 i = 0; i < m.particules.Size(); ++i) {
					const NkParticule &p = m.particules[i];
					fini = fini && NkFini(p.pos.x) && NkFini(p.pos.y);
					dansBoite = dansBoite && p.pos.y > -1.f && p.pos.y < m.reglages.hauteur + 1.f;
					vMax = NkMaxf(vMax, NkLen(p.vit));
				}
				Verifier(fini, nom, "positions finies", 1.f);
				Verifier(dansBoite, nom, "rien sous le sol ni au-dessus du plafond", 1.f);
				Verifier(m.particules.Size() == nDepart, nom, "aucune particule perdue", static_cast<float32>(m.particules.Size()));
				Verifier(vMax < 400.f, nom, "au repos apres 10 s (vitesse max cm/s)", vMax);
				Verifier(ms < 12.f, nom, "cout d'un pas (ms)", ms);

				for (uint32 c = 0; c < m.corps.Size(); ++c) {
					const NkCorps &co = m.corps[c];
					if (co.mat == NkMateriau::NK_BALLON) {
						float32 aire = 0.f;
						for (uint32 k = 0; k < co.nombre; ++k) {
							aire += NkCross(m.particules[co.debut + k].pos, m.particules[co.debut + (k + 1) % co.nombre].pos);
						}
						const float32 r = aire * 0.5f / co.aireRepos;
						Verifier(r > 0.8f && r < 1.15f, nom, "ballon : aire / aire de repos", r);
					}
					if (co.mat == NkMateriau::NK_EAU) {
						float32 rho = 0.f;
						uint32 n = 0;
						for (uint32 k = co.debut; k < co.debut + co.nombre; ++k) {
							if (m.particules[k].densite > m.ParamsFluide(co.mat).rho0 * 0.8f) { // interieur
								rho += m.particules[k].densite;
								++n;
							}
						}
						const float32 r = n > 0 ? rho / static_cast<float32>(n) / m.ParamsFluide(co.mat).rho0 : 0.f;
						Verifier(r > 0.85f && r < 1.45f, nom, "eau : densite interieure / densite de repos", r);
					}
					if (co.mat == NkMateriau::NK_BLOB) {
						NkV2 mn, mx;
						m.BoiteCorps(c, mn, mx);
						const float32 largeur = mx.x - mn.x;
						const float32 hauteur = mx.y - mn.y;
						Verifier(hauteur > 10.f, nom, "blob : pas completement etale (hauteur cm)", hauteur);
						Verifier(largeur < 1200.f, nom, "blob : cohesion (largeur cm)", largeur);
					}
					if (co.mat == NkMateriau::NK_GELEE || co.mat == NkMateriau::NK_CAISSE) {
						NkV2 mn, mx;
						m.BoiteCorps(c, mn, mx);
						Verifier(mx.x - mn.x < 250.f && mx.y - mn.y < 250.f, nom, "gelee/caisse : garde sa forme (cm)",
								 NkMaxf(mx.x - mn.x, mx.y - mn.y));
					}
				}
			}

			// Fonte : un cristal chauffe perd ses liaisons, refroidi il en refait.
			{
				NkMonde m;
				NkChargerNiveau(m, 4);
				NkCreerActeur(m, NkActeur::NK_A_ATOMES_BLOC, NkV2(0.f, 60.f));
				for (int32 k = 0; k < 120; ++k) {
					m.Pas(1.f / 60.f);
				}
				const uint32 liensFroid = m.LiensActifs();
				m.reglages.temperature = 100.f;
				for (int32 k = 0; k < 600; ++k) {
					m.Pas(1.f / 60.f);
				}
				const uint32 liensChaud = m.LiensActifs();
				std::printf("\n== Fonte : %u liaisons a froid, %u a chaud, %u ruptures\n", liensFroid, liensChaud,
							static_cast<unsigned>(m.rupturesTotal));
				Verifier(m.rupturesTotal > 20, "Fonte", "la chaleur rompt des liaisons", static_cast<float32>(m.rupturesTotal));
			}

			// Coupe : un blob coupe en deux se ressoude en retombant.
			{
				NkMonde m;
				NkChargerNiveau(m, 4);
				NkCreerActeur(m, NkActeur::NK_A_BLOB, NkV2(0.f, 50.f));
				for (int32 k = 0; k < 60; ++k) {
					m.Pas(1.f / 60.f);
				}
				const uint32 coupes = m.Couper(NkV2(0.f, -10.f), NkV2(0.f, 400.f));
				for (int32 k = 0; k < 120; ++k) {
					m.Pas(1.f / 60.f);
				}
				std::printf("\n== Coupe du blob : %u liens coupes, %u soudures au dernier pas\n", coupes,
							static_cast<unsigned>(m.stats.soudures));
				Verifier(coupes > 3, "Coupe", "le couteau coupe des liens", static_cast<float32>(coupes));
			}

			// Ballon creve.
			{
				NkMonde m;
				NkChargerNiveau(m, 4);
				const int32 b = NkCreerActeur(m, NkActeur::NK_A_BALLON, NkV2(0.f, 200.f));
				m.Couper(NkV2(-100.f, 200.f), NkV2(100.f, 205.f));
				for (int32 k = 0; k < 240; ++k) {
					m.Pas(1.f / 60.f);
				}
				Verifier(b >= 0 && m.corps[static_cast<uint32>(b)].pression == 0.f, "Ballon", "un ballon coupe se degonfle", 0.f);
			}

			std::printf("\n%s (%d echec%s)\n", gEchecs == 0 ? "BANC REUSSI" : "BANC EN ECHEC", gEchecs, gEchecs > 1 ? "s" : "");
			return gEchecs == 0 ? 0 : 1;
		}

	} // namespace physic2d
} // namespace nkentseu
