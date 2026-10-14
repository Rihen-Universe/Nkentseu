// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkRenduLivresProbe -- les correctifs de ETUDE_LIVRES/RENDU_2026-10-10.md,
// prouves A L'IMAGE, sans fenetre.
//
// Chaine : peripherique SANS SURFACE, NkRenderer complet, sortie redirigee vers
// une cible hors ecran (SetFinalColorTarget), relue par ReadbackPixels. Sur
// OpenGL seulement, NKRHI cree lui-meme une fenetre CACHEE de 1x1 pour son
// contexte ; ce banc n'en ouvre aucune.
//
// Variables :
//   NK_RL_API     = dx11 | dx12 | gl | vk     (defaut dx11)
//   NK_RL_SCENE   = reflet | source | peau | tissu | rampe | planche | tout    (defaut tout)
//                   | ssao  (a part : exige NK_RL_SSAO=1, qui allume la SSAO)
//   NK_RL_CAPTURE = <prefixe>  -> <prefixe>_<scene>.png pour chaque image mesuree
//
// Post-traitement : bloom, SSAO, FXAA, TAA et exposition automatique ETEINTS.
// Chacun deplacerait les nombres pour une raison etrangere au constat mesure
// (le bloom etale un reflet, la SSAO multiplie tout -- constat T4).
//
// ── SCENE « reflet » (T3) ────────────────────────────────────────────────────
// Deux spheres grises (albedo 0,5, dielectriques), rugosite 0,1 a gauche et 0,5
// a droite, sous la meme directionnelle venue de l'axe de la camera. Le pic du
// reflet de la sphere LISSE doit depasser celui de la sphere RUGUEUSE : la NDF
// GGX a un pic 1/(PI alpha^2), 595 fois plus haut a r = 0,1 qu'a r = 0,5. Avec
// `a2/(PI*d*d + 1e-4)` (code du 10/10), le pic lisse valait D = 1,0 contre 5,05
// pour le rugueux : la relation etait INVERSEE (calcul sur la ligne lue).
//   critere : PIC_LISSE - PIC_RUGUEUX > 20 niveaux de luminance (ce qui se voit,
//   pas « different » : different-ne-veut-pas-dire-visible).
//   temoin  : la luminance MOYENNE de la sphere rugueuse est imprimee ; le lobe
//   exact ne doit presque pas la bouger (a comparer d'une course a l'autre).
//
// ── SCENE « source » (T31) ───────────────────────────────────────────────────
// Une sphere miroir (metal, rugosite 0 -> plancher), une ponctuelle sur l'axe de
// la camera. DEUX rendus, ampoule de rayon 0,02 m puis 0,25 m, tout le reste
// identique. Le reflet d'une lampe a la taille de la lampe : la tache saturee
// doit GRANDIR avec le rayon. Attendu derive (miroir convexe de rayon 0,9,
// lampe a 2,1 m de la surface, grandissement f/(f-d) = 0,176) : image de
// l'ampoule de 0,044 m de rayon a 4,1 m de la camera, ~4,7 px -> ~70 px de
// tache pour 0,25 m, moins de 4 px pour 0,02 m.
//   critere : TACHE(0,25) >= 4 x max(TACHE(0,02), 1) ET les deux images different.
//   Sans lecture de la taille de la source, les deux rendus sont IDENTIQUES.
// =============================================================================
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkDeviceInitInfo.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Materials/NkRenderStyle.h"
#include "NKRenderer/Tools/PostProcess/NkPostProcessStack.h"
#include "NKImage/Core/NkImage.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKMemory/NKMemory.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {

	const uint32 kW = 480, kH = 320;
	// Quelques images avant de relire : la premiere compile les pipelines et
	// remplit l'atlas d'ombres ; la scene est fixe, la derniere image fait foi.
	const uint32 kImages = 6;

	struct Banc {
			NkIDevice *device = nullptr;
			NkRenderer *r = nullptr;
			NkOffscreenTarget *cible = nullptr;
			NkMeshSystem *meshes = nullptr;
			NkRender3D *r3d = nullptr;
	};

	double Lum(const uint8 *p) {
		return 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
	}

	NkMat4f Placement(float32 x, float32 y, float32 z, float32 echelle) {
		NkMat4f m = NkMat4f::Identity();
		m[0][0] = echelle;
		m[1][1] = echelle;
		m[2][2] = echelle;
		m[3][0] = x;
		m[3][1] = y;
		m[3][2] = z;
		return m;
	}

	NkDrawCall3D Sphere(NkMeshHandle mesh, float32 x, float32 rayon, NkVec3f teinte, float32 metal, float32 rug) {
		NkDrawCall3D dc;
		dc.mesh = mesh;
		// La primitive a un rayon de 0,5.
		dc.transform = Placement(x, 0.f, 0.f, 2.f * rayon);
		dc.tint = teinte;
		dc.metallic = metal;
		dc.roughness = rug;
		// L'AABB est obligatoire : NkAABB nait INVERSEE et Submit ecarte
		// silencieusement tout appel qui n'en pose pas.
		dc.aabb = {{x - rayon, -rayon, -rayon}, {x + rayon, rayon, rayon}};
		return dc;
	}

	void CameraFace(NkSceneContext &ctx) {
		ctx.camera.SetPosition({0.f, 0.f, 5.f});
		ctx.camera.SetTarget({0.f, 0.f, 0.f});
		ctx.camera.SetUp({0.f, 1.f, 0.f});
		ctx.camera.SetFOV(40.f);
		ctx.camera.SetAspect(kW, kH);
		ctx.camera.SetNearFar(0.1f, 100.f);
	}

	bool Rendre(Banc &b, const NkSceneContext &ctx, const NkVector<NkDrawCall3D> &dcs, NkVector<uint8> &out,
				const NkVector<NkDrawCallSkinned> *skin = nullptr) {
		for (uint32 i = 0; i < kImages; ++i) {
			if (!b.r->BeginFrame())
				continue;
			b.r3d->BeginScene(ctx);
			for (uint32 k = 0; k < dcs.Size(); ++k)
				b.r3d->Submit(dcs[k]);
			if (skin)
				for (uint32 k = 0; k < skin->Size(); ++k)
					b.r3d->SubmitSkinned((*skin)[k]);
			b.r->Present();
			b.r->EndFrame();
		}
		if (!b.cible->ReadbackPixels(out.Data()))
			return false;
		// EMPREINTE de l'image relue (FNV-1a sur RVB) et nombre de pixels non-fond :
		// deux dorsaux qui rendent les memes mesures doivent pouvoir etre departages
		// -- un accord parfait est suspect tant qu'on ne sait pas s'il vient de deux
		// images identiques ou de deux images differentes aux memes moyennes.
		uint32 h = 2166136261u, nonFond = 0;
		for (uint32 i = 0; i < kW * kH; ++i) {
			const uint8 *p = out.Data() + i * 4;
			for (int c = 0; c < 3; ++c) {
				h ^= p[c];
				h *= 16777619u;
			}
			if (p[0] != out[0] || p[1] != out[1] || p[2] != out[2])
				nonFond++;
		}
		printf("empreinte image : %08x (%u px non-fond)\n", h, nonFond);
		return true;
	}

	void Capturer(Banc &b, const char *scene) {
		const char *pre = getenv("NK_RL_CAPTURE");
		if (!(pre && pre[0]))
			return;
		char chemin[1024];
		snprintf(chemin, sizeof(chemin), "%s_%s.png", pre, scene);
		if (b.cible->Capture(chemin))
			printf("capture ecrite : %s\n", chemin);
		else
			printf("[ECHEC] capture %s\n", chemin);
	}

	// Pixels non-fond d'une moitie d'image (gauche ou droite) : pic et moyenne de
	// luminance. Le fond est lu au coin (0,0), jamais suppose.
	struct Moitie {
			double pic = 0.0;
			double moyenne = 0.0;
			uint32 pixels = 0;
	};

	Moitie MesurerMoitie(const NkVector<uint8> &img, bool gauche) {
		Moitie m;
		const uint8 *fond = img.Data();
		double somme = 0.0;
		const uint32 x0 = gauche ? 0 : kW / 2, x1 = gauche ? kW / 2 : kW;
		for (uint32 y = 0; y < kH; ++y)
			for (uint32 x = x0; x < x1; ++x) {
				const uint8 *p = img.Data() + (y * kW + x) * 4;
				const int d = abs((int)p[0] - fond[0]) + abs((int)p[1] - fond[1]) + abs((int)p[2] - fond[2]);
				if (d <= 3)
					continue;
				const double l = Lum(p);
				if (l > m.pic)
					m.pic = l;
				somme += l;
				m.pixels++;
			}
		m.moyenne = m.pixels ? somme / (double)m.pixels : 0.0;
		return m;
	}

	// ── T3 ───────────────────────────────────────────────────────────────────
	int SceneReflet(Banc &b) {
		printf("\n--- scene reflet (T3) : rugosite 0,1 a gauche, 0,5 a droite ---\n");
		NkSceneContext ctx;
		CameraFace(ctx);
		NkLightDesc soleil;
		soleil.type = NkLightType::NK_DIRECTIONAL;
		// Venue de l'axe de la camera, legerement d'en haut : le reflet tombe
		// sur la face visible des deux spheres.
		soleil.direction = {0.f, -0.35f, -1.f};
		soleil.color = {1.f, 1.f, 1.f};
		soleil.intensity = 3.f;
		ctx.lights.PushBack(soleil);

		NkMeshHandle sph = b.meshes->GetSphere();
		NkVector<NkDrawCall3D> dcs;
		dcs.PushBack(Sphere(sph, -1.1f, 0.9f, {0.5f, 0.5f, 0.5f}, 0.f, 0.1f));
		dcs.PushBack(Sphere(sph, 1.1f, 0.9f, {0.5f, 0.5f, 0.5f}, 0.f, 0.5f));

		NkVector<uint8> img;
		img.Resize(kW * kH * 4);
		if (!Rendre(b, ctx, dcs, img)) {
			printf("[ECHEC] relecture\n");
			return 1;
		}
		Capturer(b, "reflet");
		const Moitie g = MesurerMoitie(img, true), d = MesurerMoitie(img, false);
		printf("fond (coin)        : (%u,%u,%u)\n", img[0], img[1], img[2]);
		printf("sphere lisse  r0,1 : %u px, pic %.1f, moyenne %.2f\n", g.pixels, g.pic, g.moyenne);
		printf("sphere rugueuse 0,5: %u px, pic %.1f, moyenne %.2f\n", d.pixels, d.pic, d.moyenne);
		// La scene doit etre la : deux disques de ~95 px de rayon (0,9 m a 5 m,
		// fov 40 -> 0,9 x 320 / (2 x 5 x tan 20) = 79 px ; aire ~19 600 px).
		if (g.pixels < 10000 || d.pixels < 10000) {
			printf("[ECHEC] spheres absentes ou tronquees : le banc ne conclut pas\n");
			return 1;
		}
		const double ecart = g.pic - d.pic;
		printf("PIC_LISSE - PIC_RUGUEUX = %.1f (critere > 20)\n", ecart);
		if (ecart > 20.0) {
			printf("[ ok ] T3 : le reflet de la surface lisse est plus fort que celui de la rugueuse\n");
			return 0;
		}
		printf("[ROUGE] T3 : la surface lisse n'a pas un reflet plus fort que la rugueuse\n");
		return 2;
	}

	// ── T31 ──────────────────────────────────────────────────────────────────
	int SceneSource(Banc &b) {
		printf("\n--- scene source (T31) : miroir, ampoule de 0,02 m puis 0,25 m ---\n");
		NkMeshHandle sph = b.meshes->GetSphere();
		NkVector<NkDrawCall3D> dcs;
		dcs.PushBack(Sphere(sph, 0.f, 0.9f, {0.9f, 0.9f, 0.9f}, 1.f, 0.f));

		const float32 rayons[2] = {0.02f, 0.25f};
		uint32 tache[2] = {0, 0};
		NkVector<uint8> img[2];
		for (int k = 0; k < 2; ++k) {
			NkSceneContext ctx;
			CameraFace(ctx);
			NkLightDesc lampe;
			lampe.type = NkLightType::NK_POINT;
			lampe.position = {0.f, 0.f, 3.f};
			lampe.color = {1.f, 1.f, 1.f};
			lampe.intensity = 200.f; // W (loi physique, defaut) : I = P / 4PI
			lampe.range = 50.f;
			lampe.castShadow = false;
			lampe.sourceRadius = rayons[k];
			ctx.lights.PushBack(lampe);
			img[k].Resize(kW * kH * 4);
			if (!Rendre(b, ctx, dcs, img[k])) {
				printf("[ECHEC] relecture\n");
				return 1;
			}
			Capturer(b, k == 0 ? "source_002" : "source_025");
			for (uint32 i = 0; i < kW * kH; ++i)
				if (Lum(img[k].Data() + i * 4) >= 250.0)
					tache[k]++;
		}
		uint32 diff = 0;
		for (uint32 i = 0; i < kW * kH; ++i) {
			const uint8 *a = img[0].Data() + i * 4, *c = img[1].Data() + i * 4;
			if (a[0] != c[0] || a[1] != c[1] || a[2] != c[2])
				diff++;
		}
		printf("tache saturee (lum >= 250) : %u px pour 0,02 m, %u px pour 0,25 m\n", tache[0], tache[1]);
		printf("pixels differents entre les deux rendus : %u\n", diff);
		const uint32 base = tache[0] > 0 ? tache[0] : 1;
		if (diff > 0 && tache[1] >= 4 * base) {
			printf("[ ok ] T31 : le reflet grandit avec la lampe\n");
			return 0;
		}
		printf("[ROUGE] T31 : la taille de la source ne change pas le reflet\n");
		return 2;
	}

	// Disque de mesure autour du centre projete d'une sphere : moyenne et pic de
	// luminance. Un disque INTERIEUR a la silhouette ne contient que la sphere.
	Moitie MesurerDisque(const NkVector<uint8> &img, int cx, int cy, int rayon) {
		Moitie m;
		double somme = 0.0;
		for (int y = cy - rayon; y <= cy + rayon; ++y)
			for (int x = cx - rayon; x <= cx + rayon; ++x) {
				if (x < 0 || y < 0 || x >= (int)kW || y >= (int)kH)
					continue;
				if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > rayon * rayon)
					continue;
				const double l = Lum(img.Data() + ((uint32)y * kW + (uint32)x) * 4);
				if (l > m.pic)
					m.pic = l;
				somme += l;
				m.pixels++;
			}
		m.moyenne = m.pixels ? somme / (double)m.pixels : 0.0;
		return m;
	}

	// Sphere UV de rayon 0,5 aux sommets SKINNES (un os, poids 1) : le nuanceur
	// de peau ne sert qu'au chemin skinne (un Submit statique avec un materiau
	// Skin n'est pas dessine -- mesure : disque a la couleur du fond). C'est le
	// chemin du patient de PV3DE (SubmitSkinned + materialSlots).
	NkMeshHandle SphereSkinnee(NkMeshSystem *meshes) {
		const uint32 st = 32, sl = 32;
		NkVector<NkVertexSkinned> v;
		NkVector<uint32> idx;
		const float32 kPi = 3.14159265f;
		for (uint32 i = 0; i <= st; ++i) {
			const float32 phi = kPi * (float32)i / (float32)st;
			const float32 sp = (i == 0 || i == st) ? 0.f : (float32)::sin((double)phi);
			const float32 cp = (i == 0) ? 1.f : (i == st ? -1.f : (float32)::cos((double)phi));
			for (uint32 j = 0; j <= sl; ++j) {
				const float32 th = 2.f * kPi * (float32)j / (float32)sl;
				const float32 x = sp * (float32)::cos((double)th), z = sp * (float32)::sin((double)th);
				NkVertexSkinned s;
				s.pos = {0.5f * x, 0.5f * cp, 0.5f * z};
				s.normal = {x, cp, z};
				s.tangent = {-(float32)::sin((double)th), 0.f, (float32)::cos((double)th)};
				s.uv = {(float32)j / (float32)sl, (float32)i / (float32)st};
				s.uv2 = s.uv;
				s.color = 0xFFFFFFFFu; // vColor multiplie l'albedo : blanc
				v.PushBack(s);
			}
		}
		for (uint32 i = 0; i < st; ++i)
			for (uint32 j = 0; j < sl; ++j) {
				const uint32 a = i * (sl + 1) + j, b = a + sl + 1;
				// Meme enroulement que la primitive (NkMeshSystem::BuildSphereData).
				idx.PushBack(a);
				idx.PushBack(b);
				idx.PushBack(a + 1);
				idx.PushBack(a + 1);
				idx.PushBack(b);
				idx.PushBack(b + 1);
			}
		renderer::NkMeshDesc d;
		d.layout = renderer::NkVertexLayout::Skinned();
		d.vertices = v.Data();
		d.vertexCount = (uint32)v.Size();
		d.indices = idx.Data();
		d.indexCount = (uint32)idx.Size();
		renderer::NkSubMesh sm;
		sm.name = NkString("peau");
		sm.firstIndex = 0;
		sm.indexCount = (uint32)idx.Size();
		d.subMeshes.PushBack(sm);
		return meshes->Create(d);
	}

	// ── T5 ───────────────────────────────────────────────────────────────────
	// Une sphere de PEAU (materiau Skin, subsurface 0,5, couleur 1 ; 0,5 ; 0,3) a
	// gauche, une sphere PBR de meme albedo a droite, sous une directionnelle
	// venue d'en haut. Ambiante a zero (iblStrength 0) : dans une ombre portee,
	// la seule lumiere est celle que le nuanceur invente.
	//   course A, sans dalle : temoin -- les deux spheres sont eclairees.
	//   course B, une dalle au-dessus des deux : toutes deux dans l'ombre portee.
	//   critere : en B, moyenne(peau) - moyenne(PBR) < 3 niveaux et pic(peau) < 10.
	// Avant le correctif, le wrap de la peau ignorait l'ombre : la peau restait
	// eclairee sous la dalle.
	// Centres projetes : x = 240 -/+ 1,1 x f / 5, f = 160 / tan 20 = 439,6 px ;
	// rayon a l'ecran ~79 px : disque de mesure de 55 px, interieur.
	int ScenePeau(Banc &b, NkMaterialSystem *mats) {
		printf("\n--- scene peau (T5) : peau a gauche, PBR a droite, dalle au-dessus ---\n");
		NkMaterialInstance *inst = mats ? mats->CreateInstance(mats->DefaultSkin()) : nullptr;
		if (!inst) {
			printf("[ECHEC] instance du materiau Skin\n");
			return 1;
		}
		inst->SetAlbedo({0.5f, 0.5f, 0.5f});
		inst->SetRoughness(0.5f);
		inst->SetSubsurface(0.5f, {1.f, 0.5f, 0.3f});
		NkMeshHandle sph = b.meshes->GetSphere();
		NkMeshHandle cube = b.meshes->GetCube();
		NkMeshHandle sphSk = SphereSkinnee(b.meshes);
		if (!sphSk.IsValid()) {
			printf("[ECHEC] sphere skinnee\n");
			return 1;
		}
		NkDrawCallSkinned peau;
		peau.mesh = sphSk;
		peau.transform = Placement(-1.1f, 0.f, 0.f, 1.8f);
		peau.boneMatrices.PushBack(NkMat4f::Identity());
		peau.materialSlots.PushBack(inst->GetHandle());
		peau.aabb = {{-2.f, -0.9f, -0.9f}, {-0.2f, 0.9f, 0.9f}};
		NkVector<NkDrawCallSkinned> skins;
		skins.PushBack(peau);
		NkDrawCall3D pbr = Sphere(sph, 1.1f, 0.9f, {0.5f, 0.5f, 0.5f}, 0.f, 0.5f);
		NkDrawCall3D dalle;
		dalle.mesh = cube;
		dalle.transform = NkMat4f::Identity();
		dalle.transform[0][0] = 8.f;
		dalle.transform[1][1] = 0.05f;
		dalle.transform[2][2] = 4.f;
		dalle.transform[3][1] = 1.6f;
		dalle.tint = {0.5f, 0.5f, 0.5f};
		dalle.aabb = {{-4.f, 1.5f, -2.f}, {4.f, 1.7f, 2.f}};

		const int cxPeau = 240 - 97, cxPbr = 240 + 97, cy = 160, rMes = 55;
		Moitie mesP[2], mesR[2];
		double lumFond = 0.0;
		for (int k = 0; k < 2; ++k) {
			NkSceneContext ctx;
			CameraFace(ctx);
			NkLightDesc soleil;
			soleil.type = NkLightType::NK_DIRECTIONAL;
			// Pas exactement verticale : une direction parallele au « haut » du
			// repere de la lumiere degenere la matrice d'ombre.
			soleil.direction = {0.05f, -1.f, 0.02f};
			soleil.color = {1.f, 1.f, 1.f};
			soleil.intensity = 3.f;
			ctx.lights.PushBack(soleil);
			NkVector<NkDrawCall3D> dcs;
			dcs.PushBack(pbr);
			if (k == 1)
				dcs.PushBack(dalle);
			NkVector<uint8> img;
			img.Resize(kW * kH * 4);
			if (!Rendre(b, ctx, dcs, img, &skins)) {
				printf("[ECHEC] relecture\n");
				return 1;
			}
			Capturer(b, k == 0 ? "peau_A_eclairee" : "peau_B_ombre");
			mesP[k] = MesurerDisque(img, cxPeau, cy, rMes);
			mesR[k] = MesurerDisque(img, cxPbr, cy, rMes);
			if (k == 0)
				lumFond = Lum(img.Data()); // fond de la course A, celle du temoin
			printf("course %c : peau moyenne %.2f pic %.1f | PBR moyenne %.2f pic %.1f | fond %.2f\n",
				   k == 0 ? 'A' : 'B', mesP[k].moyenne, mesP[k].pic, mesR[k].moyenne, mesR[k].pic, lumFond);
		}
		// Temoin : sans dalle, chaque sphere est DESSINEE et eclairee -- son pic
		// depasse le fond de plus de 40 niveaux. (Le premier jet comparait la
		// moyenne a 20 : le fond vaut 42, et une sphere de peau jamais dessinee
		// passait le temoin.)
		if (mesP[0].pic < lumFond + 40.0 || mesR[0].pic < lumFond + 40.0) {
			printf("[ECHEC] temoin : sans dalle, une sphere n'est pas eclairee (pic < fond + 40)\n");
			return 1;
		}
		const double ecart = mesP[1].moyenne - mesR[1].moyenne;
		printf("ombre portee : peau - PBR = %.2f (critere < 3), pic peau %.1f (critere < 10)\n", ecart, mesP[1].pic);
		if (ecart < 3.0 && mesP[1].pic < 10.0) {
			printf("[ ok ] T5 : la peau ne s'eclaire plus dans une ombre portee\n");
			return 0;
		}
		printf("[ROUGE] T5 : la peau recoit de la lumiere sous la dalle\n");
		return 2;
	}

	// ── T44 ──────────────────────────────────────────────────────────────────
	// Une sphere de TISSU (materiau Cloth, albedo 0,5) sous une directionnelle
	// venue d'en haut, ambiante 0. Le diffus « wrap » du tissu vaut, juste au-
	// dessus du terminateur, (0 + 0,3) / 1,3 / PI = 0,0735 ; le `continue` a
	// N.L <= 0 le coupait a ZERO juste en dessous : une marche, une ligne dure
	// sur les draps. Attendu derive : apres tonemap, une marche de 0,11 en HDR
	// (0,0735 x 0,5 x 3) donne ~100 niveaux d'un pixel au suivant ; une
	// luminance continue ne bouge que de quelques niveaux par pixel.
	//   mesure  : le plus grand saut de luminance entre deux pixels voisins sur
	//   la verticale du centre de la sphere, a l'INTERIEUR de la silhouette
	//   (8 px de marge aux bords).
	//   critere : saut max < 20 niveaux.
	int SceneTissu(Banc &b, NkMaterialSystem *mats) {
		printf("\n--- scene tissu (T44) : sphere de tissu, lumiere d'en haut ---\n");
		NkMaterialInstance *inst = mats ? mats->CreateInstance(mats->DefaultCloth()) : nullptr;
		if (!inst) {
			printf("[ECHEC] instance du materiau Cloth\n");
			return 1;
		}
		inst->SetAlbedo({0.5f, 0.5f, 0.5f});
		inst->SetRoughness(0.6f);
		NkMeshHandle sph = b.meshes->GetSphere();
		NkDrawCall3D tissu = Sphere(sph, 0.f, 0.9f, {1.f, 1.f, 1.f}, 0.f, 0.6f);
		tissu.material = inst->GetHandle();
		NkSceneContext ctx;
		CameraFace(ctx);
		NkLightDesc soleil;
		soleil.type = NkLightType::NK_DIRECTIONAL;
		soleil.direction = {0.05f, -1.f, 0.02f};
		soleil.color = {1.f, 1.f, 1.f};
		soleil.intensity = 3.f;
		ctx.lights.PushBack(soleil);
		NkVector<NkDrawCall3D> dcs;
		dcs.PushBack(tissu);
		NkVector<uint8> img;
		img.Resize(kW * kH * 4);
		if (!Rendre(b, ctx, dcs, img)) {
			printf("[ECHEC] relecture\n");
			return 1;
		}
		Capturer(b, "tissu");
		// Verticale du centre (x = 240) : bornes de la silhouette relevees sur
		// l'image (premier et dernier pixel non-fond), jamais supposees.
		const uint8 *fond = img.Data();
		const uint32 x = kW / 2;
		int y0 = -1, y1 = -1;
		for (uint32 y = 0; y < kH; ++y) {
			const uint8 *p = img.Data() + (y * kW + x) * 4;
			const int d = abs((int)p[0] - fond[0]) + abs((int)p[1] - fond[1]) + abs((int)p[2] - fond[2]);
			if (d > 3) {
				if (y0 < 0)
					y0 = (int)y;
				y1 = (int)y;
			}
		}
		if (y0 < 0 || y1 - y0 < 100) {
			printf("[ECHEC] sphere de tissu absente sur la verticale du centre (%d..%d)\n", y0, y1);
			return 1;
		}
		double sautMax = 0.0, lumHaut = 0.0;
		int ySaut = -1;
		for (int y = y0 + 8; y < y1 - 8; ++y) {
			const double a = Lum(img.Data() + ((uint32)y * kW + x) * 4);
			const double c = Lum(img.Data() + ((uint32)(y + 1) * kW + x) * 4);
			if (a > lumHaut)
				lumHaut = a;
			if (fabs(a - c) > sautMax) {
				sautMax = fabs(a - c);
				ySaut = y;
			}
		}
		printf("silhouette y %d..%d, luminance max %.1f, saut max %.1f a y = %d (critere < 20)\n", y0, y1, lumHaut,
			   sautMax, ySaut);
		if (lumHaut < 60.0) {
			printf("[ECHEC] temoin : le tissu n'est pas eclaire (max < 60)\n");
			return 1;
		}
		if (sautMax < 20.0) {
			printf("[ ok ] T44 : la luminance du tissu est continue au terminateur\n");
			return 0;
		}
		printf("[ROUGE] T44 : marche de luminance au terminateur du tissu\n");
		return 2;
	}

	// ── T39 ──────────────────────────────────────────────────────────────────
	// Une sphere a 3 m DEVANT un mur, SSAO allumee (NK_RL_SSAO=1, sinon la scene
	// refuse de conclure). Le rayon de la SSAO est de 0,5 m : la sphere est six
	// rayons trop loin pour occulter le mur. Sans borne de distance, les prises
	// qui tombent sur la sphere comptent quand meme (poids v.n / v.v ~ 1/3 m) :
	// un HALO sombre entoure la silhouette sur le mur.
	//   mesure  : luminance moyenne du mur dans une couronne de 4 a 14 px autour
	//   de la silhouette, contre le mur loin de la sphere (bande |x - 240| > 150).
	//   critere : couronne / loin > 0,97.
	int SceneSsao(Banc &b) {
		printf("\n--- scene ssao (T39) : sphere a 3 m devant un mur ---\n");
		const char *v = getenv("NK_RL_SSAO");
		if (!(v && v[0] == '1')) {
			printf("[ECHEC] cette scene exige NK_RL_SSAO=1 (SSAO allumee a l'initialisation)\n");
			return 1;
		}
		NkMeshHandle sph = b.meshes->GetSphere();
		NkMeshHandle cube = b.meshes->GetCube();
		// Couleurs DISTINCTES : la sphere rouge, le mur bleu. La silhouette se
		// lit a la teinte (R > B), pas a un saut de luminance -- le premier jet
		// prenait l'assombrissement du bord de la sphere pour son contour et
		// mesurait sa « couronne » SUR la sphere.
		NkDrawCall3D boule = Sphere(sph, 0.f, 0.6f, {0.7f, 0.15f, 0.12f}, 0.f, 0.6f);
		NkDrawCall3D mur;
		mur.mesh = cube;
		mur.transform = NkMat4f::Identity();
		mur.transform[0][0] = 20.f;
		mur.transform[1][1] = 20.f;
		mur.transform[2][2] = 0.1f;
		mur.transform[3][2] = -3.f;
		mur.tint = {0.30f, 0.40f, 0.70f};
		mur.roughness = 0.8f;
		mur.castShadow = false;
		mur.aabb = {{-10.f, -10.f, -3.1f}, {10.f, 10.f, -2.9f}};
		boule.castShadow = false;
		NkSceneContext ctx;
		CameraFace(ctx);
		NkLightDesc soleil;
		soleil.type = NkLightType::NK_DIRECTIONAL;
		soleil.direction = {0.f, 0.f, -1.f};
		soleil.color = {1.f, 1.f, 1.f};
		soleil.intensity = 1.5f;
		soleil.castShadow = false;
		ctx.lights.PushBack(soleil);
		NkVector<NkDrawCall3D> dcs;
		dcs.PushBack(mur);
		dcs.PushBack(boule);
		NkVector<uint8> img;
		img.Resize(kW * kH * 4);
		if (!Rendre(b, ctx, dcs, img)) {
			printf("[ECHEC] relecture\n");
			return 1;
		}
		Capturer(b, "ssao");
		// Silhouette : plus grande distance au centre d'un pixel ROUGE (R > B + 20).
		const int cx = 240, cy = 160;
		int rS = -1;
		for (int y = 0; y < (int)kH; ++y)
			for (int x = 0; x < (int)kW; ++x) {
				const uint8 *p = img.Data() + ((uint32)y * kW + (uint32)x) * 4;
				if ((int)p[0] > (int)p[2] + 20) {
					const int d = (int)sqrt((double)((x - cx) * (x - cx) + (y - cy) * (y - cy)) + 0.5);
					if (d > rS)
						rS = d;
				}
			}
		if (rS < 20 || rS > 140) {
			printf("[ECHEC] silhouette introuvable (rayon %d px)\n", rS);
			return 1;
		}
		double sC = 0.0, sL = 0.0;
		uint32 nC = 0, nL = 0;
		for (int y = 0; y < (int)kH; ++y)
			for (int x = 0; x < (int)kW; ++x) {
				const double d = sqrt((double)((x - cx) * (x - cx) + (y - cy) * (y - cy)));
				const double l = Lum(img.Data() + ((uint32)y * kW + (uint32)x) * 4);
				const uint8 *pp = img.Data() + ((uint32)y * kW + (uint32)x) * 4;
				if ((int)pp[0] > (int)pp[2])
					continue; // la sphere, pas le mur
				if (d > rS + 3 && d < rS + 13) {
					sC += l;
					nC++;
				} else if (abs(x - cx) > 150 && y > 20 && y < (int)kH - 20) {
					sL += l;
					nL++;
				}
			}
		const double mC = nC ? sC / nC : 0.0, mL = nL ? sL / nL : 0.0;
		const double rapport = mL > 0.0 ? mC / mL : 0.0;
		printf("rayon silhouette %d px ; mur : couronne %.2f (%u px), loin %.2f (%u px), rapport %.3f (critere > 0,97)\n",
			   rS, mC, nC, mL, nL, rapport);
		if (mL < 30.0) {
			printf("[ECHEC] temoin : le mur n'est pas eclaire\n");
			return 1;
		}
		if (rapport > 0.97) {
			printf("[ ok ] T39 : pas de halo d'occlusion autour d'un objet lointain\n");
			return 0;
		}
		printf("[ROUGE] T39 : halo sombre sur le mur autour d'une sphere a 3 m\n");
		return 2;
	}

	// ── STYLISE : la rampe cel a n tons ──────────────────────────────────────
	// Trois spheres Toon cote a cote, rampe a 2, 3 et 4 tons (SetToonBands), sous
	// une directionnelle venue d'en haut et de face. Reflet reduit a un point,
	// ni liseré ni contour : seule la rampe dessine.
	//   mesure  : sur la verticale du centre de chaque sphere (8 px de marge), les
	//   PALIERS : suites d'au moins 4 pixels dont la luminance varie de 1 au plus ;
	//   deux paliers a moins de 6 niveaux l'un de l'autre comptent pour un.
	//   critere : nombre de paliers = nombre de tons demande (2, 3, 4).
	// Avant la rampe, `bands` n'existait pas : trois spheres a 2 tons.
	int CompterPaliers(const NkVector<uint8> &img, uint32 x, int &y0, int &y1) {
		const uint8 *fond = img.Data();
		y0 = -1;
		y1 = -1;
		for (uint32 y = 0; y < kH; ++y) {
			const uint8 *p = img.Data() + (y * kW + x) * 4;
			const int d = abs((int)p[0] - fond[0]) + abs((int)p[1] - fond[1]) + abs((int)p[2] - fond[2]);
			if (d > 3) {
				if (y0 < 0)
					y0 = (int)y;
				y1 = (int)y;
			}
		}
		if (y0 < 0 || y1 - y0 < 60)
			return -1;
		double niveaux[16];
		int nNiv = 0;
		int debut = y0 + 8;
		for (int y = y0 + 9; y <= y1 - 8; ++y) {
			const double a = Lum(img.Data() + ((uint32)(y - 1) * kW + x) * 4);
			const double c = Lum(img.Data() + ((uint32)y * kW + x) * 4);
			const bool fin = (fabs(a - c) > 1.0) || (y == y1 - 8);
			if (fin) {
				if (y - debut >= 4) {
					const double niv = Lum(img.Data() + ((uint32)debut * kW + x) * 4);
					bool connu = false;
					for (int k = 0; k < nNiv; ++k)
						if (fabs(niveaux[k] - niv) < 6.0)
							connu = true;
					if (!connu && nNiv < 16)
						niveaux[nNiv++] = niv;
				}
				debut = y;
			}
		}
		return nNiv;
	}

	int SceneRampe(Banc &b, NkMaterialSystem *mats) {
		printf("\n--- scene rampe (stylise) : Toon a 2, 3 et 4 tons ---\n");
		NkMeshHandle sph = b.meshes->GetSphere();
		const float32 xs[3] = {-1.6f, 0.f, 1.6f};
		const int tons[3] = {2, 3, 4};
		NkVector<NkDrawCall3D> dcs;
		for (int k = 0; k < 3; ++k) {
			NkMaterialInstance *t = mats ? mats->CreateInstance(mats->DefaultToon()) : nullptr;
			if (!t) {
				printf("[ECHEC] instance Toon\n");
				return 1;
			}
			t->SetAlbedo({0.85f, 0.35f, 0.2f});
			t->SetToonThreshold(0.2f);
			t->SetToonSmooth(0.01f);
			t->SetToonShadowColor({0.4f, 0.4f, 0.55f});
			t->SetToonBands((float32)tons[k]);
			t->SetOutline(0.f, {0.f, 0.f, 0.f});
			t->SetRim(0.f, {1.f, 1.f, 1.f});
			t->SetSpecHardness(4096.f);
			NkDrawCall3D dc = Sphere(sph, xs[k], 0.7f, {1.f, 1.f, 1.f}, 0.f, 0.5f);
			dc.material = t->GetHandle();
			dcs.PushBack(dc);
		}
		NkSceneContext ctx;
		CameraFace(ctx);
		NkLightDesc soleil;
		soleil.type = NkLightType::NK_DIRECTIONAL;
		soleil.direction = {0.f, -0.6f, -0.8f};
		soleil.color = {1.f, 1.f, 1.f};
		soleil.intensity = 3.f;
		soleil.castShadow = false;
		ctx.lights.PushBack(soleil);
		NkVector<uint8> img;
		img.Resize(kW * kH * 4);
		if (!Rendre(b, ctx, dcs, img)) {
			printf("[ECHEC] relecture\n");
			return 1;
		}
		Capturer(b, "rampe");
		int rouge = 0;
		for (int k = 0; k < 3; ++k) {
			const uint32 x = (uint32)(240.f + xs[k] * 439.6f / 5.f + 0.5f);
			int y0, y1;
			const int n = CompterPaliers(img, x, y0, y1);
			printf("rampe %d tons : %d paliers sur x = %u (y %d..%d)\n", tons[k], n, x, y0, y1);
			if (n < 0) {
				printf("[ECHEC] sphere absente en x = %u\n", x);
				return 1;
			}
			if (n != tons[k])
				rouge = 2;
		}
		if (rouge == 0)
			printf("[ ok ] rampe : chaque sphere a le nombre de tons demande\n");
		else
			printf("[ROUGE] rampe : le nombre de paliers ne suit pas SetToonBands\n");
		return rouge;
	}

	// ── STYLISE : la planche des trois styles ────────────────────────────────
	// Meme scene (plastique rouge, metal or, peau, tissu bleu, sur un sol), une
	// colonne par style (NkRenderStyle), soleil + lumiere de remplissage. Les
	// materiaux viennent de NkCreateStyledMaterial ; la saturation du style est
	// posee a chaud (bloom et SSAO restent ceux de l'initialisation du banc,
	// eteints : la planche montre les MATIERES).
	// Temoins, sur la sphere de plastique :
	//   cartoon  : part des pixels de la sphere dans ses 2 classes de luminance
	//              (pas de 4) les plus peuplees >= 60 % (aplats) ; realiste < 40 %.
	//   rihen    : couronne de 1 a 2 px HORS de la silhouette realiste, sur fond
	//              clair (luminance realiste >= 100) : part des
	//              pixels plus sombres que l'image realiste de plus de 25 niveaux
	//              >= 50 % (le contour). Attendu derive : coque de 1,2 x 0,0035 x
	//              4,3 m = 1,8 cm, soit ~1,8 px a 4,3 m (f = 439,6 px) ; la couronne
	//              de 2 px est donc couverte aux trois quarts environ.
	//              Le cartoon (trait 1,6) est imprime a titre de comparaison.
	double PartDeuxClasses(const NkVector<uint8> &img, int cx, int cy, int r) {
		uint32 hist[64] = {};
		uint32 n = 0;
		for (int y = cy - r; y <= cy + r; ++y)
			for (int x = cx - r; x <= cx + r; ++x) {
				if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > r * r)
					continue;
				const int l = (int)Lum(img.Data() + ((uint32)y * kW + (uint32)x) * 4);
				hist[(l < 0 ? 0 : (l > 255 ? 255 : l)) / 4]++;
				n++;
			}
		uint32 m1 = 0, m2 = 0;
		for (int i = 0; i < 64; ++i) {
			if (hist[i] > m1) {
				m2 = m1;
				m1 = hist[i];
			} else if (hist[i] > m2)
				m2 = hist[i];
		}
		return n ? (double)(m1 + m2) / (double)n : 0.0;
	}

	int ScenePlanche(Banc &b, NkMaterialSystem *mats) {
		printf("\n--- scene planche (stylise) : realiste | cartoon | rihen ---\n");
		NkMeshHandle sph = b.meshes->GetSphere();
		NkMeshHandle cube = b.meshes->GetCube();
		const NkRenderStyle styles[3] = {NkRenderStyle::NK_STYLE_REALISTE, NkRenderStyle::NK_STYLE_CARTOON,
										 NkRenderStyle::NK_STYLE_RIHEN};
		const NkStyledMaterialDesc matieres[4] = {
			{NkMatiere::NK_MATIERE_PLASTIQUE, {0.8f, 0.15f, 0.12f}, -1.f},
			{NkMatiere::NK_MATIERE_METAL, {1.0f, 0.78f, 0.34f}, -1.f},
			{NkMatiere::NK_MATIERE_PEAU, {0.85f, 0.62f, 0.5f}, -1.f},
			{NkMatiere::NK_MATIERE_TISSU, {0.2f, 0.3f, 0.7f}, -1.f},
		};
		const float32 xs[4] = {-1.65f, -0.55f, 0.55f, 1.65f};
		NkVector<uint8> imgs[3];
		NkPostConfig &pp = b.r->GetPostProcess()->GetConfig();
		const NkPostConfig ppInit = pp;
		for (int s = 0; s < 3; ++s) {
			NkPostConfig style = ppInit;
			NkApplyRenderStyle(style, styles[s]);
			pp.colorGrading = style.colorGrading;
			pp.saturation = style.saturation;
			NkVector<NkDrawCall3D> dcs;
			for (int m = 0; m < 4; ++m) {
				NkStyledMaterial sm = NkCreateStyledMaterial(mats, styles[s], matieres[m]);
				if (!sm.instance) {
					printf("[ECHEC] materiau de style %s\n", NkRenderStyleName(styles[s]));
					return 1;
				}
				NkDrawCall3D dc = Sphere(sph, xs[m], 0.48f, {1.f, 1.f, 1.f}, 0.f, 0.5f);
				sm.ApplyTo(dc);
				dcs.PushBack(dc);
			}
			NkStyledMaterial solM = NkCreateStyledMaterial(
				mats, styles[s], {NkMatiere::NK_MATIERE_PLASTIQUE, {0.55f, 0.55f, 0.52f}, 0.8f});
			NkDrawCall3D sol;
			sol.mesh = cube;
			sol.transform = NkMat4f::Identity();
			sol.transform[0][0] = 8.f;
			sol.transform[1][1] = 0.1f;
			sol.transform[2][2] = 6.f;
			sol.transform[3][1] = -0.53f;
			sol.aabb = {{-4.f, -0.6f, -3.f}, {4.f, -0.45f, 3.f}};
			solM.ApplyTo(sol);
			// Le sol n'a pas de contour : il couperait l'image d'un trait.
			if (solM.instance && styles[s] != NkRenderStyle::NK_STYLE_REALISTE)
				solM.instance->SetHullOutline(0.f, {0.f, 0.f, 0.f});
			dcs.PushBack(sol);

			NkSceneContext ctx;
			ctx.camera.SetPosition({0.f, 1.1f, 4.2f});
			ctx.camera.SetTarget({0.f, -0.1f, 0.f});
			ctx.camera.SetUp({0.f, 1.f, 0.f});
			ctx.camera.SetFOV(40.f);
			ctx.camera.SetAspect(kW, kH);
			ctx.camera.SetNearFar(0.1f, 100.f);
			NkLightDesc soleil;
			soleil.type = NkLightType::NK_DIRECTIONAL;
			soleil.direction = {-0.45f, -0.8f, -0.4f};
			soleil.color = {1.f, 0.97f, 0.92f};
			soleil.intensity = 3.f;
			ctx.lights.PushBack(soleil);
			NkLightDesc remplissage;
			remplissage.type = NkLightType::NK_DIRECTIONAL;
			remplissage.direction = {0.6f, -0.3f, -0.5f};
			remplissage.color = {0.75f, 0.85f, 1.f};
			remplissage.intensity = 0.6f;
			remplissage.castShadow = false;
			ctx.lights.PushBack(remplissage);
			imgs[s].Resize(kW * kH * 4);
			if (!Rendre(b, ctx, dcs, imgs[s])) {
				printf("[ECHEC] relecture\n");
				return 1;
			}
			char nom[64];
			snprintf(nom, sizeof(nom), "planche_%s", NkRenderStyleName(styles[s]));
			Capturer(b, nom);
		}
		pp.colorGrading = ppInit.colorGrading;
		pp.saturation = ppInit.saturation;

		// Planche : les trois colonnes cote a cote, une seule image.
		const char *pre = getenv("NK_RL_CAPTURE");
		if (pre && pre[0]) {
			NkImage planche;
			if (planche.Create(kW * 3, kH, math::NkColor(0, 0, 0, 255), 4)) {
				for (int s = 0; s < 3; ++s)
					for (uint32 y = 0; y < kH; ++y)
						memcpy(planche.Pixels() + (y * kW * 3 + (uint32)s * kW) * 4, imgs[s].Data() + y * kW * 4,
							   kW * 4);
				char chemin[1024];
				snprintf(chemin, sizeof(chemin), "%s_planche.png", pre);
				printf("%s : %s\n", planche.Save(chemin) ? "planche ecrite" : "[ECHEC] planche", chemin);
			}
		}

		// Centre projete de la sphere de plastique : releve sur l'image realiste
		// (barycentre des pixels nettement rouges, R > G + 60), jamais suppose.
		double sx = 0.0, sy = 0.0;
		uint32 nr = 0;
		for (uint32 y = 0; y < kH; ++y)
			for (uint32 x = 0; x < kW / 2; ++x) {
				const uint8 *p = imgs[0].Data() + (y * kW + x) * 4;
				if ((int)p[0] > (int)p[1] + 60) {
					sx += x;
					sy += y;
					nr++;
				}
			}
		if (nr < 500) {
			printf("[ECHEC] sphere de plastique introuvable (%u px rouges)\n", nr);
			return 1;
		}
		const int cx = (int)(sx / nr + 0.5), cy = (int)(sy / nr + 0.5);
		const int rDisque = (int)(sqrt((double)nr / 3.14159) * 0.8);
		const double plat[3] = {PartDeuxClasses(imgs[0], cx, cy, rDisque), PartDeuxClasses(imgs[1], cx, cy, rDisque),
								PartDeuxClasses(imgs[2], cx, cy, rDisque)};
		printf("plastique (centre %d,%d, disque %d px) : part des 2 classes majeures realiste %.2f | cartoon %.2f | rihen %.2f\n",
			   cx, cy, rDisque, plat[0], plat[1], plat[2]);
		// Couronne hors silhouette realiste : pixels NON rouges dont un voisin a
		// 1..4 px est rouge.
		uint32 nC = 0, sombreCartoon = 0, sombreRihen = 0;
		for (int y = 4; y < (int)kH - 4; ++y)
			for (int x = 4; x < (int)kW / 2; ++x) {
				const uint8 *p = imgs[0].Data() + ((uint32)y * kW + (uint32)x) * 4;
				if ((int)p[0] > (int)p[1] + 60)
					continue;
				bool pres = false;
				for (int dy = -2; dy <= 2 && !pres; ++dy)
					for (int dx = -2; dx <= 2 && !pres; ++dx) {
						const uint8 *q = imgs[0].Data() + ((uint32)(y + dy) * kW + (uint32)(x + dx)) * 4;
						if ((int)q[0] > (int)q[1] + 60)
							pres = true;
					}
				if (!pres)
					continue;
				// Seulement la ou le fond realiste est CLAIR (le sol) : sur le
				// ciel sombre, un trait sombre ne peut pas etre plus sombre de 25.
				const double l0 = Lum(p);
				if (l0 < 100.0)
					continue;
				nC++;
				if (Lum(imgs[1].Data() + ((uint32)y * kW + (uint32)x) * 4) < l0 - 25.0)
					sombreCartoon++;
				if (Lum(imgs[2].Data() + ((uint32)y * kW + (uint32)x) * 4) < l0 - 25.0)
					sombreRihen++;
			}
		const double fC = nC ? (double)sombreCartoon / nC : 0.0, fR = nC ? (double)sombreRihen / nC : 0.0;
		printf("couronne hors silhouette (%u px) : plus sombre que le realiste -- cartoon %.2f | rihen %.2f\n", nC, fC,
			   fR);
		int rouge = 0;
		if (!(plat[1] >= 0.60 && plat[0] < 0.40)) {
			printf("[ROUGE] planche : le cartoon n'est pas en aplats (ou le realiste l'est)\n");
			rouge = 2;
		}
		if (!(fR >= 0.50)) {
			printf("[ROUGE] planche : pas de contour autour de la matiere realiste du style rihen\n");
			rouge = 2;
		}
		if (rouge == 0)
			printf("[ ok ] planche : aplats du cartoon, contour du style rihen sur matiere realiste\n");
		return rouge;
	}

} // namespace

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	const char *envApi = getenv("NK_RL_API");
	NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_DX11;
	const char *nomApi = "DX11";
	if (envApi && envApi[0]) {
		if (strcmp(envApi, "gl") == 0) {
			api = NkGraphicsApi::NK_GFX_API_OPENGL;
			nomApi = "OpenGL";
		} else if (strcmp(envApi, "vk") == 0) {
			api = NkGraphicsApi::NK_GFX_API_VULKAN;
			nomApi = "Vulkan";
		} else if (strcmp(envApi, "dx12") == 0) {
			api = NkGraphicsApi::NK_GFX_API_DX12;
			nomApi = "DX12";
		} else if (strcmp(envApi, "dx11") != 0) {
			printf("[ECHEC] NK_RL_API inconnu : %s (dx11|dx12|gl|vk)\n", envApi);
			return 1;
		}
	}
	const char *envScene = getenv("NK_RL_SCENE");
	const char *scene = (envScene && envScene[0]) ? envScene : "tout";
	printf("\n=== NkRenduLivresProbe — dorsal %s, scene %s ===\n", nomApi, scene);

	NkDeviceInitInfo di;
	di.api = api;
	di.width = 0; // pas de surface -> sans fenetre
	di.height = 0;
	NkIDevice *device = NkDeviceFactory::Create(di);
	if (!device || !device->IsValid()) {
		printf("[ECHEC] peripherique %s sans surface non cree\n", nomApi);
		return 1;
	}
	// Temoin d'identite : on LIT l'API obtenue. Une retombee silencieuse sur un
	// autre dorsal ferait mesurer deux fois le meme chemin.
	if (device->GetApi() != api) {
		printf("[ECHEC] le peripherique obtenu n'est pas %s\n", nomApi);
		return 1;
	}

	NkRendererConfig cfg = NkRendererConfig::ForGame(api, kW, kH);
	cfg.postProcess.bloom = false;
	{
		// NK_RL_SSAO=1 : la SSAO seule, pour la scene « ssao » (T39).
		const char *v = getenv("NK_RL_SSAO");
		cfg.postProcess.ssao = (v && v[0] == '1');
	}
	cfg.postProcess.hbao = false;
	cfg.postProcess.fxaa = false;
	cfg.postProcess.taa = false;
	cfg.postProcess.autoExposureStrength = 0.f;
	// Ambiante a zero : chaque scene mesure la lumiere DIRECTE, et une ombre
	// portee doit y etre noire (T5).
	cfg.ibl.iblStrength = 0.f;
	cfg.vsync = false;

	NkRenderer *r = NkRenderer::Create(device, cfg);
	if (!r || !r->Initialize()) {
		printf("[ECHEC] NkRenderer::Initialize\n");
		return 1;
	}
	Banc b;
	b.device = device;
	b.r = r;
	b.meshes = r->GetMeshSystem();
	b.r3d = r->GetRender3D();
	NkTextureLibrary *texLib = r->GetTextures();
	if (!texLib || !b.meshes || !b.r3d) {
		printf("[ECHEC] sous-systemes absents\n");
		return 1;
	}

	NkOffscreenDesc od;
	od.width = kW;
	od.height = kH;
	od.hasDepth = true;
	// UNORM et non le defaut sRGB : OpenGL n'encode pas en sRGB comme les trois
	// autres dorsaux (mesure du 13/09) ; les relations restent comparables.
	od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
	od.readable = true;
	od.readback = true;
	od.name = NkString("NkRenduLivresProbe");
	NkOffscreenTarget cible;
	if (!cible.Init(device, texLib, od)) {
		printf("[ECHEC] cible hors ecran\n");
		return 1;
	}
	b.cible = &cible;
	r->SetRenderSizeOverride(kW, kH);
	r->SetFinalColorTarget(texLib->GetRHIHandle(cible.GetColorResult()));
	r->SetBackgroundColor({0.02f, 0.03f, 0.05f, 1.f});

	int rouge = 0;
	const bool tout = strcmp(scene, "tout") == 0;
	if (tout || strcmp(scene, "reflet") == 0)
		rouge |= SceneReflet(b);
	if (tout || strcmp(scene, "source") == 0)
		rouge |= SceneSource(b);
	if (tout || strcmp(scene, "peau") == 0)
		rouge |= ScenePeau(b, r->GetMaterials());
	if (tout || strcmp(scene, "tissu") == 0)
		rouge |= SceneTissu(b, r->GetMaterials());
	if (tout || strcmp(scene, "rampe") == 0)
		rouge |= SceneRampe(b, r->GetMaterials());
	if (tout || strcmp(scene, "planche") == 0)
		rouge |= ScenePlanche(b, r->GetMaterials());
	// La SSAO s'allume a l'initialisation : scene a part, jamais dans « tout ».
	if (strcmp(scene, "ssao") == 0)
		rouge |= SceneSsao(b);

	NkRenderer::Destroy(r);
	NkDeviceFactory::Destroy(device);
	printf("\n=== verdict : %s ===\n", rouge == 0 ? "VERT" : (rouge & 1 ? "ECHEC DU BANC" : "ROUGE"));
	return rouge;
}
