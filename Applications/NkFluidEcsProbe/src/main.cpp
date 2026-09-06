// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkFluidEcsProbe — le PONT ECS -> volumes de fluide, éprouvé SANS DEVICE.
//
// POURQUOI CE BANC EXISTE. `NkParticleSystem`, le pont ECS des particules, prend
// un `renderer::NkRenderer*` : aucun banc ne peut donc l'exercer sans device, et
// personne ne l'a jamais fait tourner sans fenêtre. Le pont des fluides prend le
// REGISTRE, qui est CPU pur — c'est ce qui rend ce fichier possible.
//
// CE QU'ON ÉPROUVE ICI, ET C'EST LE CHEMIN COMPLET : un monde ECS réel, le VRAI
// `NkTransformSystem` de Noge pour calculer la position monde (le banc ne l'écrit
// PAS lui-même : une position monde posée à la main éprouverait le banc, pas le
// moteur), puis `NkFluidVolumeSystem::Execute`.
//
// PRÉ-ENREGISTREMENT — écrit AVANT d'avoir lu le moindre chiffre :
//   (e1) une entité (NkFluidVolume + NkTransform) fait NAÎTRE un volume dans le
//        registre, à la position MONDE calculée par NkTransformSystem ;
//   (e2) déplacer l'entité déplace le volume, du même vecteur ;
//   (e3) le registre AVANCE (`StepAll`) et la fumée apparaît — c'est la fonction
//        que `NkVFXSystem::Update` appelle, la même ;
//   (e4) éteindre le composant éteint le volume ;
//   (e5) DÉTRUIRE l'entité RAMASSE le volume : sans ça, un volume survivrait à
//        son entité et serait simulé pour personne ;
//   (e6) le pont ne ramasse QUE ce qu'il a créé : un volume posé directement dans
//        le registre par un autre producteur SURVIT ;
//   (e7) contrôle NÉGATIF : sans registre (Init jamais appelé), rien n'est créé
//        et rien ne plante ;
//   (e8) MUTATION : le système n'est pas exécuté -> (e1) rougirait.
//
// CE QUE CE BANC NE PROUVE PAS : que `NkVFXSystem::Update` tourne (il exige un
// device), ni que quoi que ce soit s'affiche.
// =============================================================================
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Rendering/NkFluidVolume.h"
#include "Noge/ECS/Systems/NkFluidVolumeSystem.h"
#include "Noge/ECS/Systems/NkTransformSystem.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::ecs;
using namespace nkentseu::renderer;
using namespace nkentseu::math;

static int gChecks = 0, gFailures = 0;

static void Check(bool ok, const char *nom, const char *detail) {
	++gChecks;
	if (!ok)
		++gFailures;
	printf("  [%s] %-54s %s\n", ok ? "VERT" : "ROUGE", nom, detail);
	fflush(stdout);
}

static float32 Absf(float32 v) {
	return v < 0.f ? -v : v;
}

// ⚠️ Les mondes ECS et le registre sont des objets LOURDS : posés en variables
// locales, le cadre de pile de `main` dépasse la pile par défaut de Windows et le
// programme meurt en 0xC00000FD (STACK_OVERFLOW) AVANT le premier `printf` —
// mesuré ici même, et le symptôme trompe : rien ne s'affiche, on croit à un
// binaire mort ou à une DLL manquante. Ils vivent donc en portée FICHIER.
static NkWorld gWorld;
static NkTransformSystem gTransforms;
static NkFluidVolumeSystem gFluides;
static NkFluidVolumeStore gRegistre;
static NkWorld gWorld2;
static NkTransformSystem gTransforms2;
static NkFluidVolumeSystem gFluides2;
static NkWorld gWorld3;
static NkTransformSystem gTransforms3;
static NkFluidVolumeSystem gFluides3;
static NkFluidVolumeStore gRegistre3;

// Un volume court, pour que le banc reste rapide : ce qu'on éprouve est le PONT,
// pas le solveur (il a son propre banc).
static NkFluidVolume VolumeCourt() {
	NkFluidVolume v;
	v.desc.grid.boundsMin = {-0.15f, -0.05f, -0.15f};
	v.desc.grid.boundsMax = {0.15f, 0.35f, 0.15f};
	v.desc.grid.cellSize = 0.05f; // 6 x 8 x 6
	v.desc.grid.pressureIterations = 60;
	v.desc.grid.pressureTolerance = 1.0e-3f;
	v.desc.source.radius = 0.06f;
	v.desc.source.densityRate = 5.f;
	v.desc.source.temperatureRate = 400.f;
	return v;
}

int main() {
	printf("=============================================================\n");
	printf("NkFluidEcsProbe — le pont ECS -> NkFluidVolumeStore, sans device.\n");
	printf("Monde ECS reel + le VRAI NkTransformSystem de Noge.\n");
	printf("=============================================================\n");

	char buf[420];
	const float32 dt = 1.f / 60.f;

	NkWorld &world = gWorld;
	NkTransformSystem &transforms = gTransforms;
	NkFluidVolumeSystem &fluides = gFluides;
	NkFluidVolumeStore &registre = gRegistre;
	fluides.Init(&registre);

	// (e1) NAISSANCE à la position MONDE
	const NkVec3f pos0 = {3.f, 1.f, -2.f};
	NkEntityId e = world.CreateEntity();
	{
		NkTransform tf;
		tf.SetLocalPosition(pos0);
		world.Add<NkTransform>(e, tf);
		world.Add<NkFluidVolume>(e, VolumeCourt());
	}
	transforms.Execute(world, dt); // le VRAI système calcule worldPosition
	fluides.Execute(world, dt);
	{
		const NkFluidVolume *fv = world.Get<NkFluidVolume>(e);
		const NkFluidVolumeId id{fv ? fv->volumeId : 0ull};
		const NkVec3f c = registre.Center(id);
		snprintf(buf, sizeof(buf), "%u volume(s), poignee %llu, centre (%.2f, %.2f, %.2f) pour une entite posee en "
								   "(%.2f, %.2f, %.2f) ; crees au total : %u",
				 registre.Count(), (unsigned long long)(fv ? fv->volumeId : 0ull), (double)c.x, (double)c.y,
				 (double)c.z, (double)pos0.x, (double)pos0.y, (double)pos0.z, fluides.CreatedTotal());
		Check(registre.Count() == 1 && id.IsValid() && Absf(c.x - pos0.x) < 1.0e-5f &&
				  Absf(c.y - pos0.y) < 1.0e-5f && Absf(c.z - pos0.z) < 1.0e-5f && fluides.CreatedTotal() == 1,
			  "(e1) une entite fait NAITRE un volume a sa position MONDE", buf);
	}

	// (e2) DÉPLACEMENT
	const NkVec3f delta = {0.5f, 0.25f, -0.75f};
	{
		NkTransform *tf = world.Get<NkTransform>(e);
		tf->SetLocalPosition({pos0.x + delta.x, pos0.y + delta.y, pos0.z + delta.z});
		transforms.Execute(world, dt);
		fluides.Execute(world, dt);
		const NkFluidVolume *fv = world.Get<NkFluidVolume>(e);
		const NkVec3f c = registre.Center(NkFluidVolumeId{fv->volumeId});
		const float32 err = Absf(c.x - (pos0.x + delta.x)) + Absf(c.y - (pos0.y + delta.y)) +
							Absf(c.z - (pos0.z + delta.z));
		snprintf(buf, sizeof(buf), "entite deplacee de (%.2f, %.2f, %.2f) : centre du volume (%.3f, %.3f, %.3f), "
								   "erreur cumulee %.2e m ; suivis cette image : %u",
				 (double)delta.x, (double)delta.y, (double)delta.z, (double)c.x, (double)c.y, (double)c.z,
				 (double)err, fluides.FollowedLastFrame());
		Check(err < 1.0e-5f && fluides.FollowedLastFrame() == 1, "(e2) le volume SUIT le transform", buf);
	}

	// (e3) le registre AVANCE — la meme fonction que NkVFXSystem::Update appelle
	{
		const NkFluidVolume *fv = world.Get<NkFluidVolume>(e);
		const NkFluidVolumeId id{fv->volumeId};
		const float32 m0 = registre.Grid(id)->TotalMass();
		for (uint32 s = 0; s < 20; ++s) {
			transforms.Execute(world, dt);
			fluides.Execute(world, dt);
			registre.StepAll(dt); // <- ce que fait NkVFXSystem::Update, une fois par image
		}
		const float32 m1 = registre.Grid(id)->TotalMass();
		snprintf(buf, sizeof(buf), "20 images : SteppedLastFrame = %u, StepsTotal = %llu, masse %.9f -> %.9f, %.2f "
								   "ms/pas",
				 registre.SteppedLastFrame(), (unsigned long long)registre.StepsTotal(), (double)m0, (double)m1,
				 (double)registre.LastStepMs());
		Check(registre.SteppedLastFrame() == 1 && registre.StepsTotal() == 20 && m1 > m0,
			  "(e3) StepAll fait AVANCER le volume ne de l'ECS", buf);
	}

	// (e4) EXTINCTION par le composant
	{
		NkFluidVolume *fv = world.Get<NkFluidVolume>(e);
		fv->enabled = false;
		transforms.Execute(world, dt);
		fluides.Execute(world, dt);
		const NkFluidVolumeId id{fv->volumeId};
		const float32 m0 = registre.Grid(id)->TotalMass();
		for (uint32 s = 0; s < 10; ++s)
			registre.StepAll(dt);
		const float32 m1 = registre.Grid(id)->TotalMass();
		snprintf(buf, sizeof(buf), "enabled = false : SteppedLastFrame = %u, masse %.9f -> %.9f (inchangee)",
				 registre.SteppedLastFrame(), (double)m0, (double)m1);
		Check(registre.SteppedLastFrame() == 0 && m1 == m0, "(e4) eteindre le composant eteint le volume", buf);
		fv->enabled = true;
		fluides.Execute(world, dt);
	}

	// (e6) un volume d'UN AUTRE producteur doit SURVIVRE au ramassage
	NkFluidVolumeDesc autre;
	autre.grid.boundsMin = {-0.1f, 0.f, -0.1f};
	autre.grid.boundsMax = {0.1f, 0.2f, 0.1f};
	autre.grid.cellSize = 0.05f;
	const NkFluidVolumeId etranger = registre.Create(autre, {0.f, 0.f, 0.f});

	// (e5) DESTRUCTION de l'entite -> ramassage du volume
	{
		const NkFluidVolume *fv = world.Get<NkFluidVolume>(e);
		const uint64 ancienne = fv->volumeId;
		const uint32 avant = registre.Count();
		world.Destroy(e);
		transforms.Execute(world, dt);
		fluides.Execute(world, dt);
		NkFluidVolumeId revenante;
		revenante.id = ancienne;
		snprintf(buf, sizeof(buf), "entite detruite : %u -> %u volume(s), Grid(ancienne poignee) = %s, ramasses au "
								   "total : %u",
				 avant, registre.Count(), registre.Grid(revenante) == nullptr ? "nullptr" : "NON NUL",
				 fluides.ReapedTotal());
		Check(registre.Grid(revenante) == nullptr && fluides.ReapedTotal() == 1,
			  "(e5) detruire l'entite RAMASSE son volume", buf);
	}

	// (e6) suite : l'etranger est toujours la
	{
		snprintf(buf, sizeof(buf), "le volume cree DIRECTEMENT dans le registre est %s ; %u volume(s) restant(s)",
				 registre.Grid(etranger) != nullptr ? "toujours la" : "DETRUIT A TORT", registre.Count());
		Check(registre.Grid(etranger) != nullptr && registre.Count() == 1,
			  "(e6) le pont ne ramasse QUE ce qu'il a cree", buf);
	}

	// (e7) controle NEGATIF : aucun registre
	{
		NkWorld &w2 = gWorld2;
		NkTransformSystem &t2 = gTransforms2;
		NkFluidVolumeSystem &f2 = gFluides2; // Init JAMAIS appele
		NkEntityId e2 = w2.CreateEntity();
		NkTransform tf;
		tf.SetLocalPosition({1.f, 2.f, 3.f});
		w2.Add<NkTransform>(e2, tf);
		w2.Add<NkFluidVolume>(e2, VolumeCourt());
		t2.Execute(w2, dt);
		f2.Execute(w2, dt);
		const NkFluidVolume *fv = w2.Get<NkFluidVolume>(e2);
		snprintf(buf, sizeof(buf), "sans registre : poignee %llu (0 attendu), crees %u, suivis %u — rien ne plante",
				 (unsigned long long)(fv ? fv->volumeId : 0ull), f2.CreatedTotal(), f2.FollowedLastFrame());
		Check(fv != nullptr && fv->volumeId == 0 && f2.CreatedTotal() == 0,
			  "controle NEGATIF (e7) : sans registre, rien n'est cree", buf);
	}

	// (e8) MUTATION : le systeme n'est PAS execute
	{
		NkWorld &w3 = gWorld3;
		NkTransformSystem &t3 = gTransforms3;
		NkFluidVolumeSystem &f3 = gFluides3;
		NkFluidVolumeStore &r3 = gRegistre3;
		f3.Init(&r3);
		NkEntityId e3 = w3.CreateEntity();
		NkTransform tf;
		tf.SetLocalPosition({1.f, 2.f, 3.f});
		w3.Add<NkTransform>(e3, tf);
		w3.Add<NkFluidVolume>(e3, VolumeCourt());
		t3.Execute(w3, dt);
		// f3.Execute VOLONTAIREMENT non appele
		const NkFluidVolume *fv = w3.Get<NkFluidVolume>(e3);
		snprintf(buf, sizeof(buf), "systeme non execute : %u volume(s), poignee %llu — le temoin (e1) rougirait bien",
				 r3.Count(), (unsigned long long)(fv ? fv->volumeId : 0ull));
		Check(r3.Count() == 0 && fv->volumeId == 0, "MUTATION (e8) : sans Execute, aucun volume", buf);
	}

	printf("\n=============================================================\n");
	printf("BILAN : %d controles, %d ROUGES\n", gChecks, gFailures);
	printf("=============================================================\n");
	return gFailures == 0 ? 0 : 1;
}
