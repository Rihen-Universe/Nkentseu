// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_eau_sph.cpp — le témoin du PRODUCTEUR : le fluide publie-t-il vraiment
// ses contacts ? (2026-09-06, ROADMAP_PRODUITS.md §6.6 palier 1)
//
// Les témoins du CONTRAT (file bornée), de la LOI (combien de gouttes) et du
// MOUILLAGE vivent dans NKPhysics_Tests/test_eau.cpp : ils ne dépendent que de
// NKMath. Celui-ci est le seul qui ait besoin du solveur SPH, donc de NKRenderer.
//
// ⚠️ CE BANC NE TOUCHE AUCUN GPU. `NkParticleStoreCPU` a ses colonnes publiques
// (SoA) ; `Init` ne sert qu'à créer le tampon d'instances pour le DESSIN, et
// `NkSPHSolver::Apply` ne lit que `pos`, `vel`, `alive`, `capacity` et le champ
// de forces de l'émetteur. On dimensionne donc les colonnes à la main : aucun
// NkIDevice, aucune fenêtre, aucune charge sur la carte.
//
// SCÈNES, fixées avant la première exécution :
//   (f) UNE PARTICULE LÂCHÉE. Boîte [-0,5 ; 0,5] x [0 ; 2] x [-0,5 ; 0,5],
//       h = 0,1 m (donc bande de contact 0,05 m), une seule particule vivante
//       lâchée sans vitesse de y0 = 1,0 m. 240 images à 1/60 s.
//       Attendus, ÉCRITS D'AVANCE :
//         (f1) au moins UN événement ;
//         (f2) sa position est sur le plan du sol à moins d'une épaisseur
//              (bande = h/2 = 0,05 m) ;
//         (f3) son instant est celui de la chute libre jusqu'à l'entrée dans la
//              bande, t = sqrt(2 (y0 - (ymin + bande)) / g), à 3 pas de temps
//              près. ⚠️ La borne HAUTE est la seule qui puisse mordre : les
//              particules fantômes de paroi freinent la particule AVANT le plan,
//              elles ne peuvent pas l'accélérer. Si l'écart dépasse, je le dis
//              avec son chiffre au lieu de desserrer le critère.
//         (f4) sa vitesse d'approche vaut g t à 15 % près (même raison).
//         (f5) MUTATION `publishContacts = false` -> ZÉRO événement. Sans elle,
//              (f1) ne saurait pas rougir.
//   (g) LE FLUIDE AU REPOS NE REPUBLIE PAS. Un bloc de 8 x 8 x 8 posé sur le
//       fond, 120 images. Attendu : le débounce (front montant + hystérésis)
//       fait tomber le nombre d'événements par image à une poignée après les
//       premières images -- sinon la file déborderait en permanence et son
//       compteur de perte ne dirait plus rien.
//   (h) LA FILE PLEINE SE DIT. La même scène avec une file de capacité 4 :
//       Dropped() > 0 ET NkSPHStats::contactsDropped > 0. Le solveur ne peut pas
//       perdre en silence.
//   (i) L'ÉMETTEUR CONSOMME. NkSplashEmitter sur la file de (g) : des naissances,
//       toutes au-dessus du seuil de la loi, et la population dite.
// =============================================================================
#include "NKRenderer/Tools/VFX/NkSPHSolver.h"
#include "NKRenderer/Tools/VFX/NkSplashEmitter.h"
#include "NKRenderer/Tools/VFX/NkVFXSystem.h"
#include "NKMath/NkFunctions.h"
#include "NKMath/NkWetnessMap.h"
#include "NKImage/Core/NkImage.h"
#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {

	int gPass = 0;
	int gFail = 0;

	void SCHECK(bool ok, const char *what) {
		if (ok) {
			++gPass;
			std::fprintf(stderr, "  [ok]    %s\n", what);
		} else {
			++gFail;
			std::fprintf(stderr, "  [ROUGE] %s\n", what);
		}
	}

	// Dimensionne les colonnes SANS NkIDevice (en-tête).
	void PrepareStore(NkParticleStoreCPU &s, uint32 capacity) {
		s.capacity = capacity;
		s.pos.Resize((size_t)capacity, NkVec3f{0.f, 0.f, 0.f});
		s.vel.Resize((size_t)capacity, NkVec3f{0.f, 0.f, 0.f});
		s.life.Resize((size_t)capacity, 0.f);
		s.maxLife.Resize((size_t)capacity, 1.f);
		s.size.Resize((size_t)capacity, 0.05f);
		s.rotation.Resize((size_t)capacity, 0.f);
		s.rotSpeed.Resize((size_t)capacity, 0.f);
		s.color.Resize((size_t)capacity, NkVec4f{1.f, 1.f, 1.f, 1.f});
		s.alive.Resize((size_t)capacity, (uint8)0);
	}

	void ConfigureSolver(NkSPHSolver &sph) {
		sph.params.h = 0.1f;
		sph.params.boundsMin = {-0.5f, 0.f, -0.5f};
		sph.params.boundsMax = {0.5f, 2.f, 0.5f};
		sph.params.maxSubSteps = 8u; // le banc doit rester court : c'est un témoin, pas une demo
	}

	// ─────────────────────────────────────────────────────────────────────
	// (f) UNE PARTICULE LÂCHÉE SUR UN PLAN
	// ─────────────────────────────────────────────────────────────────────
	void TemoinGoutte() {
		std::fprintf(stderr, "-- (f) une particule lachee sur un plan --\n");
		const float32 y0 = 1.f, dt = 1.f / 60.f;

		NkSPHSolver sph;
		ConfigureSolver(sph);
		math::NkContactQueue file;
		file.Reserve(256);
		sph.contacts = &file;

		NkParticleStoreCPU store;
		PrepareStore(store, 4u);
		store.alive[0] = 1u;
		store.pos[0] = {0.f, y0, 0.f};
		store.vel[0] = {0.f, 0.f, 0.f};
		store.life[0] = 1.0e9f;

		NkEmitterDesc desc;
		const float32 bande = 0.5f * sph.params.h;
		const float32 g = -sph.params.gravity.y;
		// La chute libre jusqu'a l'ENTREE dans la bande : c'est l'instant attendu.
		const float32 tLibre = math::NkSqrt(2.f * (y0 - (sph.params.boundsMin.y + bande)) / g);

		bool premier = false;
		math::NkContactEvent e0;
		float32 tSim = 0.f;
		for (uint32 img = 0; img < 240u && !premier; ++img) {
			sph.Apply(store, desc, dt);
			tSim += dt;
			if (file.Count() > 0u) {
				e0 = file[0];
				premier = true;
			}
		}
		std::fprintf(stderr,
					 "     evenements=%u perdus=%u proposes=%u | chute libre attendue %.4f s, evenement a %.4f s "
					 "(horloge du solveur), image %.4f s\n",
					 file.Count(), file.Dropped(), file.Total(), (double)tLibre, (double)e0.time, (double)tSim);
		SCHECK(premier, "(f1) une particule lachee sur un plan produit AU MOINS UN evenement");
		if (premier) {
			std::fprintf(stderr, "     position y=%.5f (plan a %.3f, bande %.3f) ; normale (%.2f, %.2f, %.2f) ; vn=%.4f m/s\n",
						 (double)e0.position.y, (double)sph.params.boundsMin.y, (double)bande, (double)e0.normal.x,
						 (double)e0.normal.y, (double)e0.normal.z, (double)e0.normalSpeed);
			SCHECK(math::NkFabs(e0.position.y - sph.params.boundsMin.y) <= bande,
				   "(f2) a la bonne POSITION : sur le plan du sol, a moins d'une epaisseur");
			SCHECK(e0.normal.y > 0.9f, "(f3) avec la bonne NORMALE : du solide vers le fluide (vers le haut)");
			const float32 ecart = math::NkFabs(e0.time - tLibre);
			std::fprintf(stderr, "     ecart a la chute libre : %.4f s = %.2f pas de temps\n", (double)ecart,
						 (double)(ecart / dt));
			SCHECK(ecart <= 3.f * dt, "(f4) au bon INSTANT : la chute libre a 3 pas de temps pres");
			const float32 vAttendu = g * tLibre;
			std::fprintf(stderr, "     vitesse d'approche : %.4f m/s (chute libre %.4f, ecart %.1f %%)\n",
						 (double)e0.normalSpeed, (double)vAttendu,
						 (double)(100.f * math::NkFabs(e0.normalSpeed - vAttendu) / vAttendu));
			SCHECK(math::NkFabs(e0.normalSpeed - vAttendu) <= 0.15f * vAttendu,
				   "(f5) avec la bonne VITESSE d'impact (g t a 15 %)");
		}

		// (f6) MUTATION : la publication coupee. Sans elle (f1) ne sait pas rougir.
		NkSPHSolver mut;
		ConfigureSolver(mut);
		math::NkContactQueue fileMut;
		fileMut.Reserve(256);
		mut.contacts = &fileMut;
		mut.publishContacts = false;
		NkParticleStoreCPU store2;
		PrepareStore(store2, 4u);
		store2.alive[0] = 1u;
		store2.pos[0] = {0.f, y0, 0.f};
		store2.life[0] = 1.0e9f;
		for (uint32 img = 0; img < 240u; ++img)
			mut.Apply(store2, desc, dt);
		std::fprintf(stderr, "     MUTATION publishContacts = false : %u evenements (le sain en avait %u)\n",
					 fileMut.Count(), file.Count());
		SCHECK(fileMut.Count() == 0u && fileMut.Total() == 0u,
			   "(f6) MUTATION : publication coupee -> ZERO evenement, donc (f1) SAIT rougir");
	}

	// Remplit un bloc pose sur le fond et rend le nombre de particules.
	uint32 RemplitBloc(NkParticleStoreCPU &store, const NkSPHSolver &sph, uint32 cote) {
		const float32 d = 0.5f * sph.params.h;
		NkVector<NkParticleBirth> naissances;
		const NkVec3f min = {-0.5f * (float32)cote * d, sph.params.boundsMin.y + d, -0.5f * (float32)cote * d};
		const NkVec3f max = {min.x + (float32)cote * d, min.y + (float32)cote * d, min.z + (float32)cote * d};
		const uint32 n = NkSPHSolver::FillBlock(naissances, min, max, d);
		PrepareStore(store, n + 8u);
		for (uint32 i = 0; i < n; ++i) {
			store.alive[i] = 1u;
			store.pos[i] = naissances[i].pos;
			store.vel[i] = naissances[i].vel;
			store.life[i] = 1.0e9f;
		}
		return n;
	}

	// ─────────────────────────────────────────────────────────────────────
	// (g) (h) (i) LE FLUIDE POSÉ : débounce, file pleine, émetteur.
	// ─────────────────────────────────────────────────────────────────────
	void TemoinBloc() {
		std::fprintf(stderr, "-- (g)(h)(i) un bloc pose sur le fond --\n");
		const float32 dt = 1.f / 60.f;

		NkSPHSolver sph;
		ConfigureSolver(sph);
		math::NkContactQueue file;
		file.Reserve(8192);
		sph.contacts = &file;
		NkParticleStoreCPU store;
		const uint32 n = RemplitBloc(store, sph, 8u);
		NkEmitterDesc desc;

		// ⚠️ PREMIERE ECRITURE DE CE TEMOIN, ET ELLE NE POUVAIT PAS ROUGIR : le bloc
		// etait POSE sur le fond, donc au repos. Les particules du bas naissent DEJA
		// dans la bande de contact, avec une vitesse d approche nulle : le front
		// montant les marque sans rien publier (c est exactement ce que
		// PublishContact promet). Resultat : 0 evenement a l image 1, et un critere
		// « l image 120 en publie moins que l image 1 » qui compare 0 a 0 et passe
		// toujours. « Un critere qui ne peut pas departager est aussi vide qu un
		// compteur bloque a zero. » On LACHE donc le bloc de 0,3 m : il y a un pic
		// d impacts, puis le debounce doit le faire retomber.
		for (uint32 i = 0; i < store.capacity; ++i)
			if (store.alive[i])
				store.pos[i].y += 0.3f;
		uint32 pic = 0, derniere = 0, total = 0;
		for (uint32 img = 0; img < 120u; ++img) {
			file.Clear();
			sph.Apply(store, desc, dt);
			total += file.Count();
			if (file.Count() > pic)
				pic = file.Count();
			derniere = file.Count();
		}
		std::fprintf(stderr, "     %u particules lachees de 0,3 m ; evenements : pic = %u, image 120 = %u, total = %u\n",
					 n, pic, derniere, total);
		SCHECK(n > 0u && pic > 0u, "(g1) le bloc tombe et PUBLIE (population et pic dits)");
		SCHECK(derniere * 4u < pic, "(g2) puis le debounce le fait retomber : la derniere image publie < pic/4");
		SCHECK(pic <= n, "(g3) et jamais plus d evenements que de particules dans une image");

		// (h) LA FILE PLEINE SE DIT. Meme scene, capacite 4.
		NkSPHSolver sph2;
		ConfigureSolver(sph2);
		math::NkContactQueue petite;
		petite.Reserve(4);
		sph2.contacts = &petite;
		NkParticleStoreCPU store2;
		RemplitBloc(store2, sph2, 8u);
		for (uint32 i = 0; i < store2.capacity; ++i)
			if (store2.alive[i])
				store2.pos[i].y += 0.3f; // meme raison qu en (g) : au repos, rien n est publie
		// on avance jusqu a l image ou les impacts arrivent, puis on lit CETTE image
		for (uint32 img = 0; img < 60u; ++img) {
			petite.Clear();
			sph2.Apply(store2, desc, dt);
			if (petite.Total() > 4u)
				break;
		}
		std::fprintf(stderr, "     file de capacite 4 : prises=%u perdues=%u proposees=%u ; stats du solveur : "
							 "proposes=%u publies=%u perdus=%u\n",
					 petite.Count(), petite.Dropped(), petite.Total(), sph2.Stats().contactsProposed,
					 sph2.Stats().contactsPublished, sph2.Stats().contactsDropped);
		SCHECK(petite.Total() > 4u && petite.Dropped() > 0u, "(h1) la file pleine COMPTE ce qu'elle perd");
		SCHECK(sph2.Stats().contactsDropped == petite.Dropped() &&
				   sph2.Stats().contactsPublished == petite.Count(),
			   "(h2) et le SOLVEUR dit le meme chiffre qu'elle : aucune perte silencieuse");

		// (i) L'ÉMETTEUR CONSOMME.
		NkSPHSolver sph3;
		ConfigureSolver(sph3);
		math::NkContactQueue f3;
		f3.Reserve(8192);
		sph3.contacts = &f3;
		NkParticleStoreCPU store3;
		RemplitBloc(store3, sph3, 8u);
		// on laisse tomber le bloc de 0,3 m pour avoir de VRAIS impacts
		for (uint32 i = 0; i < store3.capacity; ++i)
			if (store3.alive[i])
				store3.pos[i].y += 0.3f;
		NkSplashEmitter emetteur;
		NkVector<NkParticleBirth> naissances;
		uint32 gouttes = 0, evts = 0, evtsAu = 0;
		for (uint32 img = 0; img < 60u; ++img) {
			f3.Clear();
			sph3.Apply(store3, desc, dt);
			gouttes += emetteur.Consume(f3, naissances);
			evts += emetteur.Last().events;
			evtsAu += emetteur.Last().eventsAbove;
		}
		std::fprintf(stderr, "     chute de 0,3 m, 60 images : %u evenements lus, %u au-dessus du seuil (%.1f m/s), "
							 "%u gouttes\n",
					 evts, evtsAu, (double)emetteur.law.speedThreshold, gouttes);
		SCHECK(evts > 0u, "(i1) l'emetteur LIT des evenements");
		SCHECK(gouttes > 0u, "(i2) et il en fait naitre des gouttes");
		SCHECK(evtsAu <= evts, "(i3) le seuil de la loi filtre : les gouttes ne viennent que des vrais impacts");
	}

	// =====================================================================
	// LES IMAGES. CE NE SONT PAS DES CAPTURES DU MOTEUR : ce banc ne touche
	// aucun GPU et n ouvre aucune fenetre. Ce sont des TRACES dessinees par le
	// banc a partir des CHIFFRES qu il vient de mesurer -- la meme difference
	// qu entre une photo d une piece et son plan cote. Une capture du RENDU
	// demande une scene de demonstration et la carte graphique ; elle est
	// nommee comme non faite.
	// =====================================================================
	void ImageEclaboussure() {
		const uint32 W = 900u, H = 420u;
		NkImage img = NkImage::Create(W, H, 4, 0x0E1A20FFu);
		if (!img.IsValid()) {
			std::fprintf(stderr, "     [image] creation impossible : aucune image ecrite\n");
			return;
		}
		const float32 echelle = 300.f; // 1 m = 300 px
		const float32 solY = (float32)H - 80.f;
		const float32 impactX = 260.f;
		const math::NkColor sol(0x2E4A52FFu), goutte(0xF79A28FFu), trace(0x0A555FFFu), incident(0xE05A3AFFu);
		for (uint32 x = 0; x < W; ++x)
			for (int32 e = 0; e < 3; ++e)
				img.SetPixel((int32)x, (int32)solY + e, sol);

		math::NkSplashParams loi;
		math::NkContactEvent ev;
		ev.position = {0.f, 0.f, 0.f};
		ev.normal = {0.f, 1.f, 0.f};
		ev.velocity = {2.f, -4.f, 0.f}; // impact oblique, dans le plan de l image
		ev.normalSpeed = 4.f;
		ev.index = 17u;
		math::NkSplashDrop drops[64];
		const uint32 k = math::NkSplashEmit(loi, ev, drops, 64u);

		// la vitesse incidente, en pointille
		for (int32 t = 0; t < 120; ++t) {
			if ((t / 6) % 2)
				continue;
			const float32 f = (float32)t / 120.f;
			const int32 px = (int32)(impactX - ev.velocity.x * echelle * 0.25f * (1.f - f));
			const int32 py = (int32)(solY + ev.velocity.y * echelle * 0.25f * (1.f - f));
			img.SetPixel(px, py, incident);
			img.SetPixel(px + 1, py, incident);
		}

		// chaque goutte : balistique, 0,45 s
		const float32 g = 9.81f;
		for (uint32 i = 0; i < k; ++i) {
			for (int32 n = 0; n < 90; ++n) {
				const float32 t = (float32)n * (0.45f / 90.f);
				const float32 x = drops[i].velocity.x * t;
				const float32 y = drops[i].velocity.y * t - 0.5f * g * t * t;
				if (y < 0.f)
					break;
				img.SetPixel((int32)(impactX + x * echelle), (int32)(solY - y * echelle), trace);
			}
			const float32 t = 0.15f;
			const float32 x = drops[i].velocity.x * t;
			const float32 y = drops[i].velocity.y * t - 0.5f * g * t * t;
			const int32 cx = (int32)(impactX + x * echelle), cy = (int32)(solY - y * echelle);
			for (int32 dy = -2; dy <= 2; ++dy)
				for (int32 dx = -2; dx <= 2; ++dx)
					if (dx * dx + dy * dy <= 4)
						img.SetPixel(cx + dx, cy + dy, goutte);
		}
		const bool ok = img.Save("Captures/noge_eau_eclaboussure_impact_2026-09-06.png");
		std::fprintf(stderr, "     [image] %u gouttes d un impact a %.1f m/s -> %s\n", k, (double)ev.normalSpeed,
					 ok ? "Captures/noge_eau_eclaboussure_impact_2026-09-06.png" : "ECHEC D ECRITURE");
	}

	void ImageMouillage() {
		const uint32 N = 220u, marge = 20u;
		const uint32 W = 3u * N + 4u * marge, H = N + 2u * marge;
		NkImage img = NkImage::Create(W, H, 4, 0x0E1A20FFu);
		if (!img.IsValid()) {
			std::fprintf(stderr, "     [image] creation impossible : aucune image ecrite\n");
			return;
		}
		math::NkWetnessMap carte;
		carte.Create(N, N);
		carte.material = math::NkWetSand();
		carte.material.dryingTime = 5.f;
		const NkVec3f albedoSec = {0.76f, 0.68f, 0.50f}; // un sable clair

		auto panneau = [&](uint32 col, bool mouille) {
			for (uint32 y = 0; y < N; ++y)
				for (uint32 x = 0; x < N; ++x) {
					const float32 w = mouille ? carte.At(x, y) : 0.f;
					const math::NkWetShading o = math::NkApplyWetness(albedoSec, 0.85f, 0.04f, w, carte.material);
					const math::NkColor c((uint8)(math::NkClamp(o.albedo.x, 0.f, 1.f) * 255.f),
										  (uint8)(math::NkClamp(o.albedo.y, 0.f, 1.f) * 255.f),
										  (uint8)(math::NkClamp(o.albedo.z, 0.f, 1.f) * 255.f), (uint8)255);
					img.SetPixel((int32)(marge + col * (N + marge) + x), (int32)(marge + y), c);
				}
		};

		panneau(0u, false); // SEC
		carte.Splat(0.35f, 0.45f, 0.30f, 1.f);
		carte.Splat(0.62f, 0.60f, 0.22f, 1.f);
		carte.Splat(0.50f, 0.28f, 0.16f, 0.8f);
		const float32 moyMouille = carte.Mean();
		panneau(1u, true); // MOUILLE
		for (uint32 i = 0; i < 60u * 25u; ++i)
			carte.Dry(1.f / 60.f); // 25 s = 5 tau
		const float32 moySec = carte.Mean();
		panneau(2u, true); // APRES SECHAGE
		const bool ok = img.Save("Captures/noge_eau_mouillage_avant_apres_2026-09-06.png");
		std::fprintf(stderr, "     [image] sec | mouille (canal moyen %.4f) | apres 5 tau (%.6f) -> %s\n",
					 (double)moyMouille, (double)moySec,
					 ok ? "Captures/noge_eau_mouillage_avant_apres_2026-09-06.png" : "ECHEC D ECRITURE");
	}

} // namespace

// Définie dans test_ubo_mouillage.cpp : la sonde du bloc uniforme partagé
// (ObjectUBO, sept copies C++ et six langages) et le témoin de la formule de
// mouillage. Elle rend son nombre d'échecs, et ce nombre entre dans le code de
// sortie de la suite — une sonde rouge ne doit jamais sortir 0.
int NkSondeUBOMouillage();
// Définie dans test_ocean_grille.cpp : les témoins de la grille projetée
// (Johanson 2004). Même contrat — elle rend ses échecs, ils entrent dans le
// code de sortie.
int NkSondeOceanGrille();

int main() {
	std::fprintf(stderr, "=== L'EAU : le fluide PUBLIE ses contacts (temoins §6.6 palier 1) ===\n");
	TemoinGoutte();
	TemoinBloc();
	std::fprintf(stderr, "-- les images (traces dessinees par le banc, PAS des captures du moteur) --\n");
	ImageEclaboussure();
	ImageMouillage();
	const int echecsUBO = NkSondeUBOMouillage();
	const int echecsGrille = NkSondeOceanGrille();
	std::fprintf(stderr,
				 "=== NKRenderer/eau : %d passes, %d echecs (+ %d sonde UBO, + %d grille projetee) ===\n",
				 gPass, gFail, echecsUBO, echecsGrille);
	return (gFail == 0 && echecsUBO == 0 && echecsGrille == 0) ? 0 : 1;
}
