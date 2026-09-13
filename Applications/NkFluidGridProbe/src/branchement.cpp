// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// branchement.cpp — BRANCHER la grille (palier ⑤, 2026-09-06).
//
// LE MANQUE, dit tel quel le 05/09 dans mon propre rapport : « la grille n'est
// branchée sur RIEN : ni ECS, ni NkVFXSystem, ni le vent. C'est un solveur et son
// banc, pas une fonctionnalité du moteur. » Ce fichier éprouve DEUX des trois
// branchements sans device ; le troisième (ECS) est éprouvé par `NkFluidEcsProbe`,
// qui a besoin de NKECS.
//
// PRÉ-ENREGISTREMENT — écrit AVANT d'avoir lu le moindre chiffre.
//
// ⑤A LE VENT. Contrat `math::NkIForceField` (NKMath) : Force(position, temps)
//    rend des NEWTONS sur une particule ponctuelle, et CHAQUE CONSOMMATEUR
//    DIVISE PAR LA MASSE DE SA PARTICULE. On suit la convention, on ne la
//    réinvente pas. Ce qu'on lira :
//      (w1) une force uniforme F sur une masse m, partant du repos, déplace la
//           fumée de a t^2 / 2 avec a = F/m — écart < 1 cellule ;
//      (w2) DOUBLER LA MASSE DIVISE L'ACCÉLÉRATION PAR DEUX : rapport des
//           déplacements = 2,00 ± 2 %. C'est CE témoin qui prouve que la masse
//           est HONORÉE — un paramètre déclaré et non honoré est pire qu'un
//           paramètre absent, et il ne se voit nulle part ailleurs ;
//      (w3) le vrai objet vent du dépôt (`renderer::NkForceField`, UNIFORM) passe
//           par le MÊME contrat et donne le MÊME déplacement à 1 % près ;
//      (w4) contrôle NÉGATIF : aucun champ -> déplacement nul ;
//      (w5) MUTATION : le champ est là mais `fieldEnabled = false` -> (w1) rougit.
//
// ⑤B LE REGISTRE (`renderer::NkFluidVolumeStore`), que `NkVFXSystem` possède et
//    fait avancer dans son Update. Le banc appelle `StepAll`, EXACTEMENT la
//    fonction que l'Update appelle. Ce qu'on lira :
//      (r1) créer rend une poignée valide et recale les bornes en monde ;
//      (r2) `StepAll` fait AVANCER : le compteur `SteppedLastFrame` et la masse ;
//      (r3) la source est un DÉBIT : doubler dt double la masse injectée ;
//      (r4) déplacer le centre translate la boîte ET son contenu (sémantique
//           documentée : le volume est solidaire de son entité) ;
//      (r5) le vent passe par le registre (`SetField`) et fait dériver la fumée
//           — projection COUPÉE, voir (r5b) pour pourquoi ;
//      (r5b) PHYSIQUE, ajouté le 07/09 avant la course : avec la PROJECTION, le
//           MÊME vent uniforme dans la MÊME boîte close ne doit RIEN déplacer —
//           le gradient de pression équilibre exactement une force de volume
//           uniforme. Les deux ensemble ne peuvent pas être satisfaits par un
//           solveur cassé : une projection morte rendrait (r5) plus vert et
//           (r5b) rouge ;
//      (r6) contrôle NÉGATIF : volume éteint -> rien n'avance ;
//      (r7) MUTATION : ne jamais appeler `StepAll` -> rien n'avance ;
//      (r8) un refus se DIT : une grille impossible rend une poignée INVALIDE ;
//      (r9) détruire invalide la poignée de l'appelant et la grille disparaît.
//
// CE QUE CE FICHIER NE PROUVE PAS : que `NkVFXSystem::Update` tourne (il exige un
// device et une fenêtre — non éprouvé cette nuit, et c'est dit), ni que quoi que
// ce soit s'AFFICHE.
// =============================================================================
#include "NKRenderer/Tools/VFX/NkFluidVolumeStore.h"
#include "NKRenderer/Tools/VFX/NkForceField.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::math;

void ProbeCheck(bool ok, const char *nom, const char *detail);
float32 ProbeAbs(float32 v);

// =============================================================================
// ⑤A — LE VENT
// =============================================================================
// Une course commune : boîte longue, pas de flottabilité, pas de projection (une
// force uniforme est déjà à divergence nulle ; la projection la ferait combattre
// la condition de paroi, et on mesurerait la paroi, pas le vent).
static float32 DeplacementSousVent(const NkIForceField *champ, float32 masse, bool champActif, uint32 pas,
								   float32 dt, float32 &accelMoyOut) {
	NkFluidGridParams p;
	p.boundsMin = {0.f, 0.f, 0.f};
	p.boundsMax = {1.5f, 0.5f, 0.5f};
	p.cellSize = 0.025f; // 60 x 20 x 20 = 24 000 cellules intérieures
	p.projectionEnabled = false;
	p.buoyancyEnabled = false;
	p.field = champ;
	p.fieldParticleMass = masse;
	p.fieldEnabled = champActif;
	NkFluidGrid g;
	accelMoyOut = 0.f;
	if (!g.Init(p))
		return 0.f;
	g.EmitSphere({0.25f, 0.25f, 0.25f}, 0.06f, 1.f, 0.f, 0.f);
	NkVec3f c0;
	if (!g.DensityCentroid(c0))
		return 0.f;
	float64 accSum = 0.0;
	for (uint32 s = 0; s < pas; ++s) {
		g.Step(dt);
		accSum += (float64)g.Stats().windAccelMean;
	}
	accelMoyOut = (float32)(accSum / (float64)pas);
	NkVec3f c1;
	if (!g.DensityCentroid(c1))
		return 0.f;
	return c1.x - c0.x;
}

static void PalierVent() {
	printf("\n=== ⑤A LE VENT — contrat math::NkIForceField : des NEWTONS, divisés par la masse ===\n");
	char buf[420];

	const uint32 pas = 120;
	const float32 dt = 1.f / 120.f;
	const float32 t = dt * (float32)pas;
	const float32 F = 0.5f; // newtons

	// Le déplacement DISCRET attendu : sum_{n=1..N} (n a dt) dt = a dt^2 N(N+1)/2.
	// Ce n'est pas a t^2 / 2 : l'écrire serait comparer un schéma à sa limite
	// continue et appeler « erreur » ce qui est la définition du schéma.
	auto attenduDiscret = [&](float32 masse) {
		const float32 a = F / masse;
		return a * dt * dt * (float32)pas * (float32)(pas + 1) * 0.5f;
	};

	NkUniformForceField champ({F, 0.f, 0.f});

	// (w1) masse = 1 kg
	float32 acc1 = 0.f;
	const float32 d1 = DeplacementSousVent(&champ, 1.f, true, pas, dt, acc1);
	const float32 a1 = attenduDiscret(1.f);
	snprintf(buf, sizeof(buf),
			 "F = %.2f N, m = 1,00 kg, t = %.3f s : déplacement %.5f m, attendu %.5f m (a = F/m = %.3f m/s^2, somme "
			 "discrète) — écart %.3f cellule ; accélération moyenne réellement ajoutée %.4f m/s^2",
			 (double)F, (double)t, (double)d1, (double)a1, (double)F, (double)(ProbeAbs(d1 - a1) / 0.025f),
			 (double)acc1);
	ProbeCheck(ProbeAbs(d1 - a1) < 0.025f, "(w1) la fumée accélère à F/m (± 1 cellule)", buf);

	// (w2) LA MASSE EST-ELLE HONORÉE ? c'est le seul témoin qui le dit.
	float32 acc2 = 0.f;
	const float32 d2 = DeplacementSousVent(&champ, 2.f, true, pas, dt, acc2);
	const float32 rapport = (d2 > 0.f) ? (d1 / d2) : 0.f;
	snprintf(buf, sizeof(buf),
			 "m = 1 kg -> %.5f m ; m = 2 kg -> %.5f m ; rapport %.4f (attendu 2,0000). Accélérations moyennes %.4f "
			 "contre %.4f m/s^2",
			 (double)d1, (double)d2, (double)rapport, (double)acc1, (double)acc2);
	ProbeCheck(ProbeAbs(rapport - 2.f) < 0.02f, "(w2) DOUBLER LA MASSE divise l'accélération par 2", buf);

	// (w3) le VRAI objet vent du dépôt passe par le même contrat
	NkForceField vent;
	vent.type = NkForceFieldType::UNIFORM;
	vent.direction = {1.f, 0.f, 0.f};
	vent.strength = F;
	float32 acc3 = 0.f;
	const float32 d3 = DeplacementSousVent(&vent, 1.f, true, pas, dt, acc3);
	snprintf(buf, sizeof(buf), "renderer::NkForceField (UNIFORM, %.2f N) : %.5f m contre %.5f m pour le champ de test "
							   "(écart %.4f %%) — même contrat, même résultat",
			 (double)F, (double)d3, (double)d1, (double)(ProbeAbs(d3 - d1) / d1 * 100.f));
	ProbeCheck(ProbeAbs(d3 - d1) < 0.01f * d1, "(w3) le vrai NkForceField du dépôt est lu pareil", buf);

	// (w4) contrôle NÉGATIF : aucun champ
	float32 acc4 = 0.f;
	const float32 d4 = DeplacementSousVent(nullptr, 1.f, true, pas, dt, acc4);
	snprintf(buf, sizeof(buf), "aucun champ : déplacement %.6f m (%.4f cellule), accélération moyenne %.6f m/s^2",
			 (double)d4, (double)(ProbeAbs(d4) / 0.025f), (double)acc4);
	ProbeCheck(ProbeAbs(d4) < 0.1f * 0.025f && acc4 == 0.f, "contrôle NÉGATIF du vent (aucun champ)", buf);

	// (w5) MUTATION : le champ est là, l'interrupteur est coupé
	float32 acc5 = 0.f;
	const float32 d5 = DeplacementSousVent(&champ, 1.f, false, pas, dt, acc5);
	snprintf(buf, sizeof(buf), "champ présent, fieldEnabled = false : déplacement %.6f m au lieu de %.5f m — le témoin "
							   "(w1) rougirait bien",
			 (double)d5, (double)a1);
	ProbeCheck(ProbeAbs(d5 - a1) >= 0.025f && ProbeAbs(d5) < 0.1f * 0.025f,
			   "MUTATION (w5) : vent coupé -> (w1) rougit", buf);
}

// =============================================================================
// ⑤B — LE REGISTRE
// =============================================================================
static NkFluidVolumeDesc DescriptionDeTest() {
	NkFluidVolumeDesc d;
	d.grid.boundsMin = {-0.2f, -0.1f, -0.2f}; // RELATIVES au centre
	d.grid.boundsMax = {0.2f, 0.5f, 0.2f};
	d.grid.cellSize = 0.04f; // 10 x 15 x 10
	d.grid.pressureTolerance = 1.0e-3f;
	d.grid.pressureIterations = 120;
	d.source.offset = {0.f, 0.f, 0.f};
	d.source.radius = 0.06f;
	d.source.densityRate = 4.f;
	d.source.temperatureRate = 600.f;
	d.source.fuelRate = 0.f;
	return d;
}

static void PalierRegistre() {
	printf("\n=== ⑤B LE REGISTRE — NkFluidVolumeStore, celui que NkVFXSystem possède ===\n");
	char buf[420];
	const float32 dt = 1.f / 120.f;
	const NkVec3f centre = {2.f, 0.5f, -1.f};

	// (r1) création et recalage en monde
	{
		NkFluidVolumeStore store;
		const NkFluidVolumeDesc d = DescriptionDeTest();
		const NkFluidVolumeId id = store.Create(d, centre);
		const NkFluidGrid *g = store.Grid(id);
		const bool bornesOk =
			(g != nullptr) && ProbeAbs(g->Params().boundsMin.x - (d.grid.boundsMin.x + centre.x)) < 1.0e-6f &&
			ProbeAbs(g->Params().boundsMax.y - (d.grid.boundsMax.y + centre.y)) < 1.0e-6f;
		snprintf(buf, sizeof(buf), "poignée %llu, %u volume(s) ; bornes relatives (%.2f..%.2f en y) recalées en monde "
								   "(%.2f..%.2f) pour un centre y = %.2f ; grille %u x %u x %u",
				 (unsigned long long)id.id, store.Count(), (double)d.grid.boundsMin.y, (double)d.grid.boundsMax.y,
				 g ? (double)g->Params().boundsMin.y : 0.0, g ? (double)g->Params().boundsMax.y : 0.0,
				 (double)centre.y, g ? g->Nx() : 0u, g ? g->Ny() : 0u, g ? g->Nz() : 0u);
		ProbeCheck(id.IsValid() && store.Count() == 1 && g != nullptr && bornesOk,
				   "(r1) créer rend une poignée et recale les bornes en MONDE", buf);
	}

	// (r2) StepAll fait AVANCER — c'est LA question « la grille est-elle branchée »
	{
		NkFluidVolumeStore store;
		const NkFluidVolumeId id = store.Create(DescriptionDeTest(), centre);
		const NkFluidGrid *g = store.Grid(id);
		const float32 m0 = g ? g->TotalMass() : 0.f;
		for (uint32 s = 0; s < 30; ++s)
			store.StepAll(dt);
		const float32 m1 = g ? g->TotalMass() : 0.f;
		NkVec3f c;
		const bool aUnBarycentre = g && g->DensityCentroid(c);
		snprintf(buf, sizeof(buf), "30 appels : SteppedLastFrame = %u, StepsTotal = %llu, masse %.9f -> %.9f, "
								   "barycentre (%.3f, %.3f, %.3f) près du centre (%.2f, %.2f, %.2f), %.2f ms/pas",
				 store.SteppedLastFrame(), (unsigned long long)store.StepsTotal(), (double)m0, (double)m1,
				 aUnBarycentre ? (double)c.x : 0.0, aUnBarycentre ? (double)c.y : 0.0,
				 aUnBarycentre ? (double)c.z : 0.0, (double)centre.x, (double)centre.y, (double)centre.z,
				 (double)store.LastStepMs());
		ProbeCheck(store.SteppedLastFrame() == 1 && store.StepsTotal() == 30 && m1 > m0 && aUnBarycentre &&
					   ProbeAbs(c.x - centre.x) < 0.1f && ProbeAbs(c.z - centre.z) < 0.1f,
				   "(r2) StepAll FAIT AVANCER, et la source sort au bon endroit", buf);
	}

	// (r3) la source est un DÉBIT : doubler dt double la masse injectée
	{
		NkFluidVolumeStore a, b;
		const NkFluidVolumeId ia = a.Create(DescriptionDeTest(), centre);
		const NkFluidVolumeId ib = b.Create(DescriptionDeTest(), centre);
		a.StepAll(dt);
		b.StepAll(2.f * dt);
		const float32 ma = a.Grid(ia)->TotalMass();
		const float32 mb = b.Grid(ib)->TotalMass();
		const float32 r = (ma > 0.f) ? (mb / ma) : 0.f;
		snprintf(buf, sizeof(buf), "un pas à dt = %.5f s -> %.9f ; un pas à 2 dt -> %.9f ; rapport %.4f (attendu "
								   "2,0000 : l'entité déclare un débit, le registre multiplie par dt)",
				 (double)dt, (double)ma, (double)mb, (double)r);
		ProbeCheck(ProbeAbs(r - 2.f) < 0.02f, "(r3) la source est un DÉBIT par seconde, pas une quantité", buf);
	}

	// (r4) déplacer le centre translate la boîte ET son contenu
	{
		NkFluidVolumeStore store;
		const NkFluidVolumeId id = store.Create(DescriptionDeTest(), centre);
		for (uint32 s = 0; s < 20; ++s)
			store.StepAll(dt);
		NkVec3f avant;
		store.Grid(id)->DensityCentroid(avant);
		const NkVec3f delta = {1.f, 0.f, 0.5f};
		const NkVec3f nouveau = {centre.x + delta.x, centre.y + delta.y, centre.z + delta.z};
		store.SetCenter(id, nouveau);
		NkVec3f apres;
		store.Grid(id)->DensityCentroid(apres);
		const float32 ex = ProbeAbs((apres.x - avant.x) - delta.x);
		const float32 ez = ProbeAbs((apres.z - avant.z) - delta.z);
		snprintf(buf, sizeof(buf), "centre + (%.2f, %.2f, %.2f) : barycentre (%.3f, %.3f) -> (%.3f, %.3f) ; écart au "
								   "déplacement demandé %.2e et %.2e m (le contenu est SOLIDAIRE de l'entité)",
				 (double)delta.x, (double)delta.y, (double)delta.z, (double)avant.x, (double)avant.z, (double)apres.x,
				 (double)apres.z, (double)ex, (double)ez);
		ProbeCheck(ex < 1.0e-4f && ez < 1.0e-4f, "(r4) déplacer le centre emporte la boîte ET la fumée", buf);
	}

	// (r5) le VENT passe par le registre
	//
	// ⚠️ LA PROJECTION EST COUPÉE ICI, ET C'EST DE LA PHYSIQUE, PAS UNE FACILITÉ.
	// Dans une boîte CLOSE, une force de volume UNIFORME est exactement équilibrée
	// par le gradient de pression : la projection l'annule, et RIEN NE BOUGE. C'est
	// le comportement JUSTE — un ventilateur uniforme ne déplace pas le contenu
	// d'une boîte fermée. Ce témoin-ci éprouve la PLOMBERIE (le registre passe-t-il
	// le champ à la grille ?), donc il coupe la projection comme (w1) ; le témoin
	// (r5b) juste après exige l'autre moitié, avec la projection.
	{
		NkFluidVolumeStore sans, avec;
		NkFluidVolumeDesc d = DescriptionDeTest();
		d.grid.boundsMin = {-0.2f, -0.1f, -0.6f};
		d.grid.boundsMax = {0.2f, 0.5f, 0.6f};
		d.grid.projectionEnabled = false;
		d.grid.buoyancyEnabled = false;
		d.source.enabled = false; // une bulle posée une fois, pas un débit : on suit un barycentre
		const NkFluidVolumeId is = sans.Create(d, centre);
		const NkFluidVolumeId ia = avec.Create(d, centre);
		sans.Grid(is)->EmitSphere(centre, 0.08f, 1.f, 0.f, 0.f);
		avec.Grid(ia)->EmitSphere(centre, 0.08f, 1.f, 0.f, 0.f);
		NkUniformForceField brise({0.f, 0.f, 1.2f}); // 1,2 N sur +z
		avec.SetField(ia, &brise, 1.f);
		for (uint32 s = 0; s < 60; ++s) {
			sans.StepAll(dt);
			avec.StepAll(dt);
		}
		NkVec3f cs, ca;
		const bool ok = sans.Grid(is)->DensityCentroid(cs) && avec.Grid(ia)->DensityCentroid(ca);
		const float32 derive = ca.z - cs.z;
		snprintf(buf, sizeof(buf), "60 pas : z du barycentre %.4f sans vent, %.4f avec 1,2 N sur +z — dérive %+.4f m "
								   "(%.2f cellules) ; accélération moyenne du vent %.4f m/s^2",
				 (double)cs.z, (double)ca.z, (double)derive, (double)(derive / d.grid.cellSize),
				 (double)avec.Grid(ia)->Stats().windAccelMean);
		ProbeCheck(ok && derive > d.grid.cellSize, "(r5) le VENT passe par SetField et fait dériver la fumée", buf);
	}

	// (r5b) LA PHYSIQUE, ET ELLE EST LA MOITIÉ QUI MANQUAIT : avec la PROJECTION,
	// le même vent uniforme dans la même boîte close ne doit RIEN déplacer — le
	// gradient de pression l'équilibre exactement. Un témoin qui ne vérifierait que
	// (r5) laisserait passer une projection morte : elle rendrait (r5) encore plus
	// vert. Les deux ensemble ne peuvent pas être satisfaits par un solveur cassé.
	{
		NkFluidVolumeStore store;
		NkFluidVolumeDesc d = DescriptionDeTest();
		d.grid.boundsMin = {-0.2f, -0.1f, -0.6f};
		d.grid.boundsMax = {0.2f, 0.5f, 0.6f};
		d.grid.buoyancyEnabled = false;
		d.source.enabled = false;
		const NkFluidVolumeId id = store.Create(d, centre);
		store.Grid(id)->EmitSphere(centre, 0.08f, 1.f, 0.f, 0.f);
		NkVec3f c0;
		store.Grid(id)->DensityCentroid(c0);
		NkUniformForceField brise({0.f, 0.f, 1.2f});
		store.SetField(id, &brise, 1.f);
		for (uint32 s = 0; s < 60; ++s)
			store.StepAll(dt);
		NkVec3f c1;
		store.Grid(id)->DensityCentroid(c1);
		const float32 derive = c1.z - c0.z;
		snprintf(buf, sizeof(buf), "60 pas, projection ACTIVE, 1,2 N uniformes sur +z : le barycentre bouge de %+.5f "
								   "m (%.3f cellule) — le gradient de pression équilibre la force de volume",
				 (double)derive, (double)(derive / d.grid.cellSize));
		ProbeCheck(ProbeAbs(derive) < 0.25f * d.grid.cellSize,
				   "(r5b) PHYSIQUE : un vent UNIFORME ne souffle pas une boîte CLOSE", buf);
	}

	// (r6) contrôle NÉGATIF : volume éteint
	{
		NkFluidVolumeStore store;
		NkFluidVolumeDesc d = DescriptionDeTest();
		d.enabled = false;
		const NkFluidVolumeId id = store.Create(d, centre);
		const float32 m0 = store.Grid(id)->TotalMass();
		for (uint32 s = 0; s < 30; ++s)
			store.StepAll(dt);
		const float32 m1 = store.Grid(id)->TotalMass();
		snprintf(buf, sizeof(buf), "30 appels sur un volume éteint : SteppedLastFrame = %u, StepsTotal = %llu, masse "
								   "%.9f -> %.9f",
				 store.SteppedLastFrame(), (unsigned long long)store.StepsTotal(), (double)m0, (double)m1);
		ProbeCheck(store.SteppedLastFrame() == 0 && store.StepsTotal() == 0 && m1 == m0,
				   "contrôle NÉGATIF (r6) : un volume éteint n'avance pas", buf);
	}

	// (r7) MUTATION : personne n'appelle StepAll
	{
		NkFluidVolumeStore store;
		const NkFluidVolumeId id = store.Create(DescriptionDeTest(), centre);
		const float32 m0 = store.Grid(id)->TotalMass();
		// (rien)
		const float32 m1 = store.Grid(id)->TotalMass();
		snprintf(buf, sizeof(buf), "aucun appel : SteppedLastFrame = %u, StepsTotal = %llu, masse %.9f (inchangée) — "
								   "le témoin (r2) rougirait bien",
				 store.SteppedLastFrame(), (unsigned long long)store.StepsTotal(), (double)m1);
		ProbeCheck(store.SteppedLastFrame() == 0 && store.StepsTotal() == 0 && m1 == m0,
				   "MUTATION (r7) : sans StepAll, rien n'avance", buf);
	}

	// (r8) un refus se DIT
	{
		NkFluidVolumeStore store;
		NkFluidVolumeDesc d = DescriptionDeTest();
		d.grid.cellSize = 0.f; // grille impossible
		const NkFluidVolumeId id = store.Create(d, centre);
		snprintf(buf, sizeof(buf), "cellSize = 0 : poignée valide ? %s ; %u volume(s) dans le registre",
				 id.IsValid() ? "OUI" : "non", store.Count());
		ProbeCheck(!id.IsValid() && store.Count() == 0,
				   "(r8) une grille impossible rend une poignée INVALIDE, pas une poignée morte", buf);
	}

	// (r9) détruire invalide la poignée de l'appelant
	{
		NkFluidVolumeStore store;
		NkFluidVolumeId id = store.Create(DescriptionDeTest(), centre);
		const uint64 ancienne = id.id;
		store.Destroy(id);
		NkFluidVolumeId revenante;
		revenante.id = ancienne;
		snprintf(buf, sizeof(buf), "après Destroy : la poignée de l'appelant vaut %llu (0 attendu), %u volume(s), "
								   "Grid(ancienne poignée) = %s",
				 (unsigned long long)id.id, store.Count(), store.Grid(revenante) == nullptr ? "nullptr" : "NON NUL");
		ProbeCheck(id.id == 0 && store.Count() == 0 && store.Grid(revenante) == nullptr,
				   "(r9) détruire invalide la poignée et la grille disparaît", buf);
	}
}

void PalierBranchement() {
	PalierVent();
	PalierRegistre();
}
