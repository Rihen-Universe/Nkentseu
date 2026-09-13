// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkEauEcsProbe — le PONT ECS -> producteur d'eau, éprouvé SANS DEVICE, et son
// CONTRÔLE DE VIE.
//
// POURQUOI CE BANC EXISTE. `NkWaterSystem` prend un `renderer::NkMeshSystem*`
// — la dépendance la plus étroite, idiome de `NkFluidVolumeSystem` — et son
// étage CPU tourne SANS lui. On éprouve donc le chemin COMPLET sans fenêtre : un
// monde ECS réel, le VRAI `NkTransformSystem` pour la position monde (le banc ne
// l'écrit PAS lui-même), le VRAI ordonnanceur de NKECS qui appelle `Execute`, et
// le VRAI producteur de NKVFX.
//
// PRÉ-ENREGISTREMENT — écrit AVANT d'avoir lu le moindre chiffre :
//   (e1) une entité (NkWaterComponent + NkMeshComponent + NkTransform) devant une
//        caméra produit (cols+1)(rows+1) sommets — compte calculé À LA MAIN —,
//        aucun manquant, aucune poignée INVENTÉE sans registre ;
//   (e2) le plan SUIT l'entité : à houle nulle, les sommets locaux ont y = 0 AU
//        BIT, et l'empreinte au sol change quand l'entité monte ;
//   (e3) éteindre le composant éteint la surface (invisible, rien produit) ;
//   (e4) sans caméra active, rien n'est produit et le système le COMPTE ;
//   (e5) contrôle NÉGATIF : une entité sans NkMeshComponent est ignorée ;
//   (p3) CONTRÔLE DE VIE : le temps d'une image de l'ORDONNANCEUR bouge quand on
//        double la résolution — rapport ≥ 2 pour ×4 de sommets ;
//   (p3b) VOLET NÉGATIF : le système enregistré mais DÉBRANCHÉ (SetEnabled false)
//        rend un rapport plat — (p3) rougirait.
//
// CE QUE CE BANC NE PROUVE PAS : qu'un maillage est créé ni téléversé (il faut un
// device), ni que quoi que ce soit s'affiche. Et il n'y a aujourd'hui AUCUN hôte
// fenêtré de Noge qui dessine la scène ECS — Nogee nettoie son FBO et s'arrête
// (ViewportLayer::RenderScene est un TODO). C'est mesuré, pas supposé.
// =============================================================================
#include "NKECS/World/NkWorld.h"
#include "NKECS/System/NkScheduler.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Core/NkTag.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "Noge/ECS/Components/Rendering/NkWaterComponent.h"
#include "Noge/ECS/Systems/NkTransformSystem.h"
#include "Noge/ECS/Systems/NkWaterSystem.h"
#include "NKVFX/NkVfxLiveness.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::ecs;
using namespace nkentseu::math;

static int gChecks = 0, gFailures = 0;

static void Check(bool ok, const char *nom, const char *detail) {
	++gChecks;
	if (!ok)
		++gFailures;
	printf("  [%s] %-58s %s\n", ok ? "VERT" : "ROUGE", nom, detail);
	fflush(stdout);
}

static float32 Absf(float32 v) {
	return v < 0.f ? -v : v;
}

// ⚠️ Les mondes ECS et les ordonnanceurs sont des objets LOURDS : en variables
// locales, le cadre de pile de `main` dépasse la pile par défaut de Windows et le
// programme meurt en 0xC00000FD AVANT le premier printf. Ils vivent en portée
// FICHIER — leçon payée par NkFluidEcsProbe.
static NkWorld gWorld;
static NkScheduler gScheduler(1);
static NkWorld gWorldDebranche;
static NkScheduler gSchedulerDebranche(1);

static NkWaterParams UneHoule(float32 amplitude, float32 raideur) {
	NkWaterParams w;
	w.waveCount = 1;
	w.waves[0].wavelength = 20.f;
	w.waves[0].amplitude = amplitude;
	w.waves[0].steepness = raideur;
	w.waves[0].direction = {1.f, 0.f};
	w.waves[0].phase = 0.f;
	return w;
}

static NkEntityId PoserCamera(NkWorld &world, const NkVec3f &oeil, const NkVec3f &cible) {
	const NkEntityId cam = world.CreateEntity();
	NkTransform tf;
	tf.SetLocalPosition(oeil);
	tf.SetLocalRotation(NkQuatf::LookAt(oeil, cible, {0.f, 1.f, 0.f}));
	world.Add<NkTransform>(cam, tf);
	NkCameraComponent cc;
	cc.projection = NkCameraProjection::Perspective;
	cc.fovDeg = 60.f;
	cc.aspect = 16.f / 9.f;
	cc.nearClip = 0.1f;
	cc.farClip = 1000.f;
	cc.priority = 10;
	world.Add<NkCameraComponent>(cam, cc);
	return cam;
}

static NkEntityId PoserEau(NkWorld &world, float32 y, uint32 res, float32 amplitude, float32 raideur) {
	const NkEntityId e = world.CreateEntity();
	NkTransform tf;
	tf.SetLocalPosition({0.f, y, 0.f});
	world.Add<NkTransform>(e, tf);
	NkWaterComponent w;
	w.grid.cols = res;
	w.grid.rows = res;
	w.grid.displacementMax = 2.f;
	w.waves = UneHoule(amplitude, raideur);
	world.Add<NkWaterComponent>(e, w);
	world.Add<NkMeshComponent>(e, NkMeshComponent{});
	return e;
}

// Le tick du contrôle de vie : UNE image de l'ordonnanceur, à la résolution demandée.
struct ContexteVie {
		NkWorld *world;
		NkScheduler *scheduler;
		NkEntityId eau;
		float32 dt;
		uint32 derniereResolution;
};

static void TickImage(void *vctx, uint32 res) {
	ContexteVie &c = *static_cast<ContexteVie *>(vctx);
	c.derniereResolution = res; // ENREGISTRÉ à chaque image, branché ou non
	NkWaterComponent *w = c.world->Get<NkWaterComponent>(c.eau);
	if (w != nullptr) {
		w->grid.cols = res;
		w->grid.rows = res;
	}
	c.scheduler->Run(*c.world, c.dt); // c'est l'ORDONNANCEUR qui appelle Execute, pas nous
}

int main() {
	printf("=============================================================\n");
	printf("NkEauEcsProbe — le pont ECS -> producteur d'eau, sans device.\n");
	printf("Monde ECS reel + le VRAI NkTransformSystem + le VRAI ordonnanceur.\n");
	printf("=============================================================\n");

	char buf[420];
	const float32 dt = 1.f / 60.f;
	const NkVec3f oeil = {0.f, 11.f, 0.f};
	const NkVec3f cible = {0.f, 5.f, -60.f};

	NkWorld &world = gWorld;
	NkScheduler &sched = gScheduler;
	sched.AddSystem<NkTransformSystem>();
	NkWaterSystem &eaux = sched.AddSystem<NkWaterSystem>();
	eaux.Init(nullptr); // AUCUN registre de maillages : l'étage CPU seul
	sched.Init(world);

	const NkEntityId cam = PoserCamera(world, oeil, cible);
	const uint32 res = 64u;
	const NkEntityId eau = PoserEau(world, 3.f, res, 0.6f, 0.5f);

	// (e1) UNE IMAGE
	{
		sched.Run(world, dt);
		const uint32 attendu = (res + 1u) * (res + 1u); // à la main : 65 x 65
		const NkWaterComponent *w = world.Get<NkWaterComponent>(eau);
		const NkMeshComponent *m = world.Get<NkMeshComponent>(eau);
		snprintf(buf, sizeof(buf),
				 "%u sommets produits (attendu %u), manquants %u | systeme : construits %u, televerses %u, "
				 "crees %u, caches %u, images sans camera %u | poignee %llu (0 attendu sans registre)",
				 w ? w->lastVertexCount : 0u, attendu, w ? w->lastMissing : 0u, eaux.BuiltLastFrame(),
				 eaux.UploadedLastFrame(), eaux.CreatedTotal(), eaux.HiddenLastFrame(), eaux.FramesWithoutCamera(),
				 (unsigned long long)(m ? m->meshHandle : 0ull));
		Check(w != nullptr && m != nullptr && w->lastVertexCount == attendu && w->lastMissing == 0u &&
				  eaux.BuiltLastFrame() == attendu && eaux.UploadedLastFrame() == 0u && eaux.CreatedTotal() == 0u &&
				  eaux.FramesWithoutCamera() == 0u && m->meshHandle == 0ull,
			  "(e1) une entite devant une camera produit la grille PLEINE, sans poignee inventee", buf);
	}

	// (e2) LE PLAN SUIT L'ENTITÉ — houle nulle : y local = 0 au bit, et l'empreinte bouge.
	{
		NkWaterComponent *w = world.Get<NkWaterComponent>(eau);
		w->waves = UneHoule(0.f, 0.f);
		sched.Run(world, dt);
		const uint32 n3 = eaux.BuiltLastFrame();
		float64 zMoy3 = 0.0;
		uint32 horsPlan3 = 0u;
		for (uint32 k = 0; k < n3; ++k) {
			zMoy3 += (float64)eaux.LastVertices()[k].pos.z;
			if (eaux.LastVertices()[k].pos.y != 0.f)
				++horsPlan3;
		}
		zMoy3 /= (n3 ? (float64)n3 : 1.0);

		NkTransform *tf = world.Get<NkTransform>(eau);
		tf->SetLocalPosition({0.f, 8.f, 0.f}); // l'eau MONTE de 5 m ; la caméra ne bouge pas
		sched.Run(world, dt);
		const uint32 n8 = eaux.BuiltLastFrame();
		float64 zMoy8 = 0.0;
		uint32 horsPlan8 = 0u;
		for (uint32 k = 0; k < n8; ++k) {
			zMoy8 += (float64)eaux.LastVertices()[k].pos.z;
			if (eaux.LastVertices()[k].pos.y != 0.f)
				++horsPlan8;
		}
		zMoy8 /= (n8 ? (float64)n8 : 1.0);
		const float64 ecart = zMoy3 - zMoy8;
		snprintf(buf, sizeof(buf),
				 "plan a 3 m : %u sommets, %u hors du plan, z moyen %.2f m | plan a 8 m : %u sommets, %u hors "
				 "du plan, z moyen %.2f m | l'empreinte a bouge de %.2f m",
				 n3, horsPlan3, zMoy3, n8, horsPlan8, zMoy8, ecart);
		Check(n3 > 0u && n8 > 0u && horsPlan3 == 0u && horsPlan8 == 0u && (ecart > 1.0 || ecart < -1.0),
			  "(e2) le plan SUIT l'entite : y local nul AU BIT, empreinte deplacee", buf);
		tf->SetLocalPosition({0.f, 3.f, 0.f});
		w->waves = UneHoule(0.6f, 0.5f);
	}

	// (e3) EXTINCTION par le composant
	{
		NkWaterComponent *w = world.Get<NkWaterComponent>(eau);
		w->enabled = false;
		sched.Run(world, dt);
		const NkMeshComponent *m = world.Get<NkMeshComponent>(eau);
		snprintf(buf, sizeof(buf), "enabled = false : construits %u, caches %u, visible = %d",
				 eaux.BuiltLastFrame(), eaux.HiddenLastFrame(), m->visible ? 1 : 0);
		Check(eaux.BuiltLastFrame() == 0u && eaux.HiddenLastFrame() == 1u && !m->visible,
			  "(e3) eteindre le composant eteint la surface", buf);
		w->enabled = true;
	}

	// (e4) SANS CAMÉRA ACTIVE — par le VRAI prédicat du moteur, `NkInactive`, celui
	// que NkRenderSystem et NkWaterSystem consultent.
	//
	// ⚠️ DÉFAUT NKECS TROUVÉ ICI, NOMMÉ ET NON RÉPARÉ (hors de ce lot) : ajouter une
	// étiquette VIDE fait parler `NKECS_ASSERT(ptr)` dans NkWorld::AddImpl
	// (NkWorld.h:550) puis ÉCRIT À TRAVERS UN POINTEUR NUL (`*ptr = value`, l.551)
	// — l'archétype ne réserve aucun stockage à une étiquette, et AddImpl le
	// suppose. L'assertion ne fait que journaliser (std::abort() est en
	// commentaire, NkECSDefines.h:179), donc le programme continue et le masque,
	// lui, porte bien l'étiquette : `Has<NkInactive>` répond vrai et ce témoin
	// reste VERT par-dessus le défaut. Il n'est PAS latent : NkGameObject::SetActive
	// (NkGameObject.cpp:91, AddDeferred<NkInactive>) emprunte ce même chemin.
	{
		world.Add<NkInactive>(cam, NkInactive{});
		const uint32 avant = eaux.FramesWithoutCamera();
		sched.Run(world, dt);
		snprintf(buf, sizeof(buf), "camera inactive : construits %u, images sans camera %u -> %u",
				 eaux.BuiltLastFrame(), avant, eaux.FramesWithoutCamera());
		Check(eaux.BuiltLastFrame() == 0u && eaux.FramesWithoutCamera() == avant + 1u,
			  "(e4) sans camera active, rien n'est produit et le systeme le COMPTE", buf);
		world.Remove<NkInactive>(cam);
		sched.Run(world, dt);
	}

	// (e5) CONTRÔLE NÉGATIF : une entité d'eau SANS NkMeshComponent
	{
		const NkEntityId orpheline = world.CreateEntity();
		NkTransform tf;
		tf.SetLocalPosition({0.f, 3.f, 0.f});
		world.Add<NkTransform>(orpheline, tf);
		NkWaterComponent w;
		w.grid.cols = 8u;
		w.grid.rows = 8u;
		w.waves = UneHoule(0.6f, 0.5f);
		world.Add<NkWaterComponent>(orpheline, w);
		sched.Run(world, dt);
		const uint32 attendu = (res + 1u) * (res + 1u); // seule l'entité complète compte
		const NkWaterComponent *wo = world.Get<NkWaterComponent>(orpheline);
		snprintf(buf, sizeof(buf), "orpheline (sans NkMeshComponent) : ses sommets %u (0 attendu) ; total construit %u (%u attendu)",
				 wo ? wo->lastVertexCount : 0u, eaux.BuiltLastFrame(), attendu);
		Check(wo != nullptr && wo->lastVertexCount == 0u && eaux.BuiltLastFrame() == attendu,
			  "controle NEGATIF (e5) : sans NkMeshComponent, l'entite est ignoree", buf);
		world.Destroy(orpheline);
		sched.Run(world, dt);
	}

	// (p3) LE CONTRÔLE DE VIE — au niveau de l'ORDONNANCEUR.
	// « Si le temps par image ne bouge pas quand on double la résolution, le
	// système ne tourne pas. » Le tick est une image complète : transformations,
	// eau, drains ; le rapport porte sur tout, et l'eau doit y peser.
	{
		ContexteVie cx{&world, &sched, eau, dt, 0u};
		const vfx::NkLivenessReport a = vfx::NkMeasureLiveness(&TickImage, &cx, 32u, 64u, 9u, 3u);
		const vfx::NkLivenessReport b = vfx::NkMeasureLiveness(&TickImage, &cx, 64u, 128u, 9u, 3u);
		snprintf(buf, sizeof(buf),
				 "32 -> 64 : %.1f -> %.1f us/image, rapport %.2f | 64 -> 128 : %.1f -> %.1f us/image, rapport "
				 "%.2f | derniere resolution %u, sommets a la derniere image %u",
				 a.usLow, a.usHigh, a.ratio, b.usLow, b.usHigh, b.ratio, cx.derniereResolution, eaux.BuiltLastFrame());
		Check(b.ratio >= 2.0 && eaux.BuiltLastFrame() == 129u * 129u,
			  "(p3) VIVANT : doubler la resolution fait BOUGER le temps par image de l'ordonnanceur", buf);
	}

	// (p3b) VOLET NÉGATIF : le système est ENREGISTRÉ, décrit, ordonnancé — et
	// DÉBRANCHÉ (SetEnabled false). C'est la forme exacte du défaut maison : tout
	// est déclaré, rien n'est honoré. Le rapport doit rester plat.
	{
		NkWorld &w2 = gWorldDebranche;
		NkScheduler &s2 = gSchedulerDebranche;
		s2.AddSystem<NkTransformSystem>();
		NkWaterSystem &e2 = s2.AddSystem<NkWaterSystem>();
		e2.Init(nullptr);
		s2.Init(w2);
		PoserCamera(w2, oeil, cible);
		const NkEntityId eau2 = PoserEau(w2, 3.f, 64u, 0.6f, 0.5f);
		s2.SetEnabled<NkWaterSystem>(false); // ← DÉBRANCHÉ, délibérément
		ContexteVie cx{&w2, &s2, eau2, dt, 0u};
		const vfx::NkLivenessReport d = vfx::NkMeasureLiveness(&TickImage, &cx, 64u, 128u, 9u, 20u);
		const NkWaterComponent *wc = w2.Get<NkWaterComponent>(eau2);
		snprintf(buf, sizeof(buf),
				 "64 -> 128 : %.1f -> %.1f us/image, rapport %.2f | resolution enregistree %u, sommets produits %u, "
				 "construits par le systeme %u",
				 d.usLow, d.usHigh, d.ratio, cx.derniereResolution, wc ? wc->lastVertexCount : 0u, e2.BuiltLastFrame());
		Check(cx.derniereResolution == 128u && (wc == nullptr || wc->lastVertexCount == 0u) && e2.BuiltLastFrame() == 0u &&
				  d.ratio < 2.0,
			  "(p3b) VOLET NEGATIF : enregistre mais DEBRANCHE, le rapport est PLAT -- (p3) rougirait", buf);
	}

	printf("\n=============================================================\n");
	printf("BILAN : %d controles, %d ROUGES\n", gChecks, gFailures);
	printf("=============================================================\n");
	return gFailures == 0 ? 0 : 1;
}
