// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkTemporelProbe — LE TAA S'EXECUTE-T-IL, ET QU'EST-CE QUI BOUGE QUAND IL TOURNE ?
// -----------------------------------------------------------------------------
// POURQUOI CE BANC EXISTE.
//
// Le dépôt contient un TAA complet : décalage sous-pixel de Halton dans la
// projection (`NkRender3D.cpp:2490`), matrice de vue-projection de l'image
// précédente (`NkRendererImpl.h:261`), reprojection par la profondeur, clamp de
// voisinage 3x3, trois passes à cibles fixes. TOUT CELA EST LU. Un appel qui
// existe n'est pas un appel qui s'exécute, et la seule trace que le moteur en
// donne est une ligne de journal one-shot — qui prouve qu'on est passé par là,
// pas que les pixels ont changé.
//
// Ce banc rend SANS AUCUNE FENETRE (périphérique DX11 headless : `NkDeviceInitInfo`
// sans surface), redirige la sortie complète du graphe vers une cible hors écran
// (`SetFinalColorTarget`, le même chemin que le viewport du modeleur), et RELIT
// les pixels. Il compare des IMAGES, pas des intentions.
//
// -----------------------------------------------------------------------------
// LES ATTENDUS, ECRITS AVANT LA PREMIERE MESURE, ET DERIVES.
//
// La scène est FIXE et la caméra est FIXE. Rien, dans la scène, ne peut faire
// différer deux images consécutives — sauf le décalage sous-pixel du TAA.
//
//  (t0) LE ZERO, D'ABORD.  NK_TAA=0.
//       Deux images consécutives doivent être identiques : `diff == 0` pixel.
//       Si ce n'est pas zéro, ce banc ne mesure PAS le TAA mais une source de
//       variation que je n'ai pas identifiée, et TOUT ce qui suit est nul.
//
//  (t1) LE DECALAGE SOUS-PIXEL S'EXECUTE.  NK_TAA=1 NK_TAA_BLEND=0.
//       Blend nul = le nuanceur retourne `current` sans toucher à l'historique
//       (`if (blendHistory < 0.001)`), donc AUCUNE accumulation. Mais le jitter,
//       lui, est armé par le même booléen `taaOn` que la passe
//       (`NkRendererImpl.cpp:1166`). Deux images consécutives doivent donc
//       différer : `diff > 0`.
//       ORDRE DE GRANDEUR DERIVE : le décalage vaut au plus un demi-pixel dans
//       chaque sens. Un pixel ne change que là où l'image varie SPATIALEMENT —
//       les CONTOURS. L'intérieur du cube est un dégradé doux, le fond est un
//       aplat que le nuanceur sort d'ailleurs par la porte « ciel ». On attend
//       donc un nombre de pixels de l'ordre du PERIMETRE de la silhouette, pas
//       de sa SURFACE. Le banc imprime les deux pour que l'écart se juge.
//
//  (t2) L'ACCUMULATION S'EXECUTE.  NK_TAA=1 NK_TAA_BLEND=0.9.
//       `mix(current, history, 0.9)` remplace chaque pixel accumulé par
//       0.1*current + 0.9*history. L'écart entre deux images consécutives est
//       donc multiplié par (1 - 0.9) = 0.1 sur les pixels qui accumulent.
//       ATTENDU DERIVE : somme|delta|(t2) / somme|delta|(t1) ~ 0.1.
//       TOLERANCE LARGE, ET ELLE EST JUSTIFIEE : les pixels sortis par une des
//       quatre portes anticipées (ciel, derrière la caméra, hors écran, premier
//       passage) n'accumulent pas et gardent l'écart plein. Le rapport mesuré
//       est donc une MOYENNE entre 0.1 et 1. Je déclare le critère vert entre
//       0.02 et 0.60, et ROUGE à 1.00 +/- 0.05 — une valeur à 1 dirait que le
//       mélange n'a lieu nulle part, ce qui est exactement ce qu'il faut détecter.
//
//  (t3) L'INSTRUMENT SAIT DISTINGUER.  Le rapport (t2)/(t1) doit être
//       STRICTEMENT inférieur à 1 : si les deux courses rendaient la même chose,
//       (t1) et (t2) ne mesureraient pas deux états différents du moteur.
//
// LE NEGATIF EST DANS LE MEME BINAIRE : les trois courses sont trois exécutions
// du MEME exécutable, et seules deux variables d'environnement changent. Aucune
// reconstruction n'est intercalée, donc aucune autre différence ne peut s'y glisser.
//
// LA MUTATION QUI DOIT FAIRE ROUGIR (t1) : annuler le décalage dans
// `NkRender3D.cpp` (mettre jx = jy = 0). Elle se passe hors de ce fichier, et le
// rapport doit alors tomber à diff == 0 sous NK_TAA=1 NK_TAA_BLEND=0. Une
// mutation qui survit dirait que (t1) ne teste rien.
//
// LE SECOND INSTRUMENT, SANS CODE COMMUN avec le comptage de pixels : la trace
// one-shot du moteur `[TAA] useHistory=... | ids ldr=... hist=... depth=...`
// (`NkPostProcessStack.cpp:1805`). Elle vient du moteur, pas de ce banc, et elle
// dit en plus que les trois entrées ne pointent pas sur la même cible.
//
// -----------------------------------------------------------------------------
// CE QUE LA MESURE A DIT, LE 17/09/2026, SUR DX11 HORS ECRAN, 320x240, 32 IMAGES.
// Les attendus ci-dessus ne sont PAS réécrits : un attendu corrigé après coup ne
// peut plus contredire personne. Ce qui suit s'ajoute, et dit où je me suis trompé.
//
//   (t0) VERT.    NK_TAA=0 -> 0 pixel de différence, sur 5965 pixels de géométrie.
//   (t1) VERT.    NK_TAA=1 BLEND=0 -> 560 pixels, pour un périmètre dérivé à ~308
//                 et une surface de 6005. L'ordre de grandeur est celui du CONTOUR,
//                 pas de la surface : le décalage sous-pixel s'exécute.
//   (t2) ROUGE.   somme|delta| : 33273 (blend 0.9) contre 36363 (blend 0), soit un
//                 rapport de 0,915 là où la récurrence D(n) = 0,1*E(n) + 0,9*D(n-1)
//                 prédit entre 0,05 et 0,14 selon la fréquence. L'accumulation A
//                 LIEU (la sonde NK_TAA_DEBUG=1 montre un historique FLOU, donc
//                 accumulé) mais elle ne calme PAS l'image. Cause non établie.
//   (t4) RETIRE.  Mon attendu « montée depuis un historique vierge » était faux :
//                 voir le commentaire au point d'impression.
//
// ⚠️ ET UN DEFAUT TROUVE EN CHEMIN, QUI N'ETAIT PAS LE SUJET : sur DX11, le chemin
// TAA rend l'image RETOURNEE verticalement. Mesuré en A/B avec NK_TAA_YFLIP : à la
// valeur du code (+1 sur DX, NkPostProcessStack.cpp:1771) l'image est retournée,
// à -1 elle est droite. Le nuanceur de sommets écrit d'ailleurs, en commentaire,
// l'inverse de ce que le C++ pose : « yFlipUV = -1 sur VK et DX, +1 sur GL ».
// ⚠️ Le comptage de ce banc NE PEUT PAS voir ce défaut : il compare deux images
// consécutives, une grandeur invariante par retournement. Ce sont les CAPTURES
// qui l'ont trouvé — les images trouvent ce que les nombres validaient.
//
// ⚠️ AUCUNE FENETRE N'EST OUVERTE. Périphérique DX11 sans surface.
// =============================================================================
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkDeviceInitInfo.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKMemory/NKMemory.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {

	// Petit : on compte des pixels, pas une scène. 320x240 suffit à séparer un
	// périmètre d'une surface (facteur ~60 entre les deux).
	const uint32 kW = 320, kH = 240;
	// Assez d'images pour que l'accumulation du TAA ait convergé : le poids de
	// l'historique 0.9 laisse 0.9^N de l'image initiale, soit 3 % à N = 32.
	const uint32 kImages = 32;
	// Fenêtre ENTIEREMENT intérieure au cube, relevée sur la capture. Elle sert à
	// mesurer l'accumulation là où elle peut avoir lieu : sur des pixels dont la
	// profondeur est exploitable. Le banc VERIFIE qu'elle est bien dans le cube
	// (aucun pixel de la couleur du fond), sinon il refuse de conclure.
	const uint32 kBoiteX0 = 140, kBoiteX1 = 190, kBoiteY0 = 100, kBoiteY1 = 150;

	struct Course {
			uint32 diffPixels = 0;	  // pixels dont au moins un canal change
			uint64 sommeDeltas = 0;	  // somme des |delta| sur les 3 canaux
			uint32 pixelsGeometrie = 0; // pixels non-fond de la dernière image
			bool ok = false;
	};

	// Compte les différences entre deux relectures. DEUX grandeurs, parce
	// qu'elles répondent à deux questions : « combien de pixels bougent » (t1)
	// et « de combien ils bougent » (t2).
	void Comparer(const uint8 *a, const uint8 *b, uint32 n, uint32 &outPixels, uint64 &outSomme) {
		outPixels = 0;
		outSomme = 0;
		for (uint32 i = 0; i < n; ++i) {
			const uint8 *pa = a + i * 4, *pb = b + i * 4;
			uint32 d = 0;
			for (int c = 0; c < 3; ++c)
				d += (uint32)(pa[c] > pb[c] ? pa[c] - pb[c] : pb[c] - pa[c]);
			if (d != 0) {
				outPixels++;
				outSomme += d;
			}
		}
	}

	// Pixels dont la couleur n'est pas le fond exact. Sert à dériver l'ordre de
	// grandeur attendu de (t1) : le périmètre d'une silhouette de S pixels vaut
	// grossièrement 4*sqrt(S) pour une forme compacte.
	uint32 CompterNonFond(const uint8 *px, uint32 n, uint8 r, uint8 g, uint8 b) {
		uint32 c = 0;
		for (uint32 i = 0; i < n; ++i) {
			const uint8 *p = px + i * 4;
			if (p[0] != r || p[1] != g || p[2] != b)
				c++;
		}
		return c;
	}

	uint32 RacineEntiere(uint32 v) {
		uint32 r = 0;
		while ((r + 1) * (r + 1) <= v)
			r++;
		return r;
	}

} // namespace

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	const char *envTaa = getenv("NK_TAA");
	const char *envBlend = getenv("NK_TAA_BLEND");
	printf("\n=== NkTemporelProbe — NK_TAA=%s NK_TAA_BLEND=%s ===\n", envTaa ? envTaa : "(absent)",
		   envBlend ? envBlend : "(absent)");
	printf("    (aucune fenetre ouverte : peripherique DX11 sans surface)\n\n");

	NkDeviceInitInfo di;
	di.api = NkGraphicsApi::NK_GFX_API_DX11;
	di.width = 0; // pas de surface -> headless
	di.height = 0;
	NkIDevice *device = NkDeviceFactory::Create(di);
	if (!device || !device->IsValid()) {
		printf("[ECHEC] peripherique DX11 headless non cree\n");
		return 1;
	}
	// TEMOIN D'IDENTITE : on LIT l'API obtenue, on ne croit pas celle demandee.
	if (device->GetApi() != NkGraphicsApi::NK_GFX_API_DX11) {
		printf("[ECHEC] le peripherique obtenu n'est pas DX11\n");
		return 1;
	}

	// ForGame, puis on RALLUME le TAA explicitement : le profil HIGH l'eteint
	// (NkRendererConfig.h:718). C'est justement le fait mesure en R1.
	NkRendererConfig cfg = NkRendererConfig::ForGame(NkGraphicsApi::NK_GFX_API_DX11, kW, kH);
	cfg.postProcess.taa = true;
	cfg.postProcess.fxaa = false; // le TAA a priorite de toute facon ; on l'ecrit
	cfg.vsync = false;

	NkRenderer *r = NkRenderer::Create(device, cfg);
	if (!r || !r->Initialize()) {
		printf("[ECHEC] NkRenderer::Initialize\n");
		return 1;
	}

	NkTextureLibrary *texLib = r->GetTextures();
	NkMeshSystem *meshes = r->GetMeshSystem();
	NkRender3D *r3d = r->GetRender3D();
	if (!texLib || !meshes || !r3d) {
		printf("[ECHEC] sous-systemes absents (textures=%p meshes=%p render3d=%p)\n", (void *)texLib, (void *)meshes,
			   (void *)r3d);
		return 1;
	}

	NkOffscreenDesc od;
	od.width = kW;
	od.height = kH;
	od.hasDepth = true;
	// ⚠️ UNORM, PAS le defaut sRGB du struct : OpenGL n'encode pas en sRGB comme
	// les trois autres dorsaux, et un attendu cesserait d'etre comparable si ce
	// banc changeait un jour de dorsal.
	od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
	od.readable = true;
	od.readback = true;
	od.name = NkString("NkTemporelProbe");
	NkOffscreenTarget cible;
	if (!cible.Init(device, texLib, od)) {
		printf("[ECHEC] cible hors ecran\n");
		return 1;
	}

	r->SetRenderSizeOverride(kW, kH);
	r->SetFinalColorTarget(texLib->GetRHIHandle(cible.GetColorResult()));
	// ⚠️ LE FOND N'EST PAS NOIR, ET C'EST LE POINT. Un fond noir est INDISCERNABLE
	// d'une cible que personne n'a ecrite : le premier jet de ce banc a imprime
	// « 0 pixel de geometrie, 0 pixel different » et un [ ok ] t0 parfaitement vert
	// sur une image entierement vide. Un fond COLORE separe les deux causes : si la
	// relecture rend cette couleur, le graphe ecrit bien dans ma cible et seul le
	// cube manquerait ; si elle rend du noir, c'est la cible qui n'est pas ecrite.
	const NkVec4f kFond = {0.10f, 0.20f, 0.50f, 1.f};
	r->SetBackgroundColor(kFond);

	NkMeshHandle cube = meshes->GetCube();
	if (!cube.IsValid()) {
		printf("[ECHEC] maillage cube\n");
		return 1;
	}

	// ── La scene : FIXE. Un cube tourne d'un angle quelconque pour offrir des
	// aretes obliques (une arete alignee sur la grille de pixels ne bougerait
	// pas sous un decalage sous-pixel, et (t1) serait aveugle par construction).
	NkSceneContext ctx;
	ctx.camera.SetPosition({2.6f, 2.0f, 3.4f});
	ctx.camera.SetTarget({0.f, 0.f, 0.f});
	ctx.camera.SetUp({0.f, 1.f, 0.f});
	ctx.camera.SetFOV(45.f);
	ctx.camera.SetAspect(kW, kH);
	ctx.camera.SetNearFar(0.1f, 100.f);
	NkLightDesc soleil;
	soleil.type = NkLightType::NK_DIRECTIONAL;
	soleil.direction = {-0.4f, -0.8f, -0.45f};
	soleil.color = {1.f, 1.f, 1.f};
	soleil.intensity = 3.f;
	ctx.lights.PushBack(soleil);

	NkDrawCall3D dc;
	dc.mesh = cube;
	// Rotation FIXE : la meme a chaque image. Rien ne bouge dans la scene.
	{
		const float32 a = 0.62f; // radians, valeur quelconque non alignee
		const float32 ca = (float32)::cos((double)a), sa = (float32)::sin((double)a);
		NkMat4f m = NkMat4f::Identity();
		m[0][0] = ca;
		m[0][2] = sa;
		m[2][0] = -sa;
		m[2][2] = ca;
		dc.transform = m;
	}
	dc.tint = {0.85f, 0.80f, 0.35f};
	dc.roughness = 0.45f;
	// ⚠️ L'AABB EST OBLIGATOIRE, ET SON DEFAUT EST CONTRE NOUS. `NkAABB` naît
	// INVERSEE (min = +1e30, max = -1e30, cf. NkRendererTypes.h:188) : c'est le
	// neutre de `Expand`, donc une boîte VIDE. `NkRender3D::Submit` cull sur
	// `IsAABBVisible(dc.aabb)` et rejette silencieusement tout appel de dessin qui
	// n'en pose pas. Mon premier jet ne la posait pas : l'image sortait en aplat
	// de fond, sans un seul message, et le compteur (t0) verdissait dessus.
	// Large volontairement : une AABB trop grande ne fait que désarmer le culling,
	// ce qui est exactement ce qu'un banc veut.
	dc.aabb = {{-1.5f, -1.5f, -1.5f}, {1.5f, 1.5f, 1.5f}};

	const uint32 nPix = kW * kH;
	NkVector<uint8> imgA, imgB;
	imgA.Resize(nPix * 4);
	imgB.Resize(nPix * 4);
	memset(imgA.Data(), 0, nPix * 4);
	memset(imgB.Data(), 0, nPix * 4);

	// ── Chronometre : la PIRE image de la course, sans aucun seuil ───────────
	// Pas de moyenne : une moyenne se fait polluer par ce qu'elle doit detecter,
	// et le pic s'immunise lui-meme. On garde le maximum, des la premiere image.
	uint64 pireNs = 0;
	uint64 totalNs = 0;
	uint32 imagesMesurees = 0;
	double lumParImage[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	for (uint32 i = 0; i < kImages; ++i) {
		const uint64 t0 = ::nkentseu::NkChrono::Now().nanoseconds;
		if (!r->BeginFrame())
			continue;
		r3d->BeginScene(ctx);
		r3d->Submit(dc);
		r->Present();
		r->EndFrame();
		const uint64 dt = ::nkentseu::NkChrono::Now().nanoseconds - t0;
		if (dt > pireNs)
			pireNs = dt;
		totalNs += dt;
		imagesMesurees++;

		// On garde les DEUX dernieres images : l'avant-derniere dans imgA, la
		// derniere dans imgB. Relire A CHAQUE image plutot qu'aux deux dernieres
		// garantit que le couple compare est bien consecutif, et que la
		// relecture elle-meme ne s'intercale pas differemment entre les courses.
		imgA = imgB;
		if (!cible.ReadbackPixels(imgB.Data())) {
			printf("[ECHEC] relecture a l'image %u\n", i);
			return 1;
		}

		// ── (t4) LA MONTEE DEPUIS UN HISTORIQUE VIERGE ───────────────────────
		// L'instrument le plus direct de l'accumulation, et son attendu se DERIVE
		// sans rien supposer : à la première image l'historique est effacé à noir,
		// donc si le mélange a lieu, L(n) = Linf * (1 - blend^n). Le dépôt cite
		// lui-même cette loi avec ses chiffres (NkRendererImpl.cpp:556) :
		// « luminance 19,6 au lieu de 92,9 trois images après un rebuild, exactement
		// 92,9*(1-0,9^3) ». Si l'accumulation N'A PAS lieu, L(n) = Linf dès n = 1
		// et la suite est PLATE. Les deux formes ne se ressemblent pas.
		// ⚠️ SUR LE CUBE SEUL, ET C'EST UNE CORRECTION. Le premier jet moyennait
		// TOUTE l'image : le fond occupe 92 % des pixels et sort du TAA par la
		// porte « ciel » (depth >= 0.9999), donc il n'accumule JAMAIS et la
		// moyenne globale est PLATE par construction — elle aurait dit « pas
		// d'accumulation » même si l'accumulation était parfaite. L'instrument
		// mesurait une grandeur voisine. La fenêtre ci-dessous est ENTIEREMENT
		// dans le cube (vérifié sur la capture), donc chacun de ses pixels a une
		// profondeur exploitable.
		if (i < 8) {
			uint64 somme = 0;
			uint32 n = 0;
			for (uint32 y = kBoiteY0; y < kBoiteY1; ++y)
				for (uint32 x = kBoiteX0; x < kBoiteX1; ++x) {
					const uint8 *p = imgB.Data() + (y * kW + x) * 4;
					somme += (uint64)p[0] + p[1] + p[2];
					n++;
				}
			lumParImage[i] = n ? (double)somme / (3.0 * (double)n) : 0.0;
		}
	}

	Course c;
	Comparer(imgA.Data(), imgB.Data(), nPix, c.diffPixels, c.sommeDeltas);
	// Le fond RELU, pris au coin superieur gauche : le tonemap ACES transforme la
	// couleur demandee, donc on ne peut PAS comparer a kFond. On compare a ce que
	// le coin CONTIENT, et on l'imprime pour qu'il se juge.
	const uint8 fr = imgB[0], fg = imgB[1], fb = imgB[2];
	c.pixelsGeometrie = CompterNonFond(imgB.Data(), nPix, fr, fg, fb);
	c.ok = true;

	const uint32 perimetreAttendu = 4u * RacineEntiere(c.pixelsGeometrie);

	printf("images rendues         : %u (sur %u demandees)\n", imagesMesurees, kImages);
	printf("resolution             : %ux%u = %u pixels\n", kW, kH, nPix);
	printf("couleur du coin (fond) : (%u,%u,%u)   demandee (%.2f,%.2f,%.2f) avant tonemap\n", fr, fg, fb,
		   (double)kFond.x, (double)kFond.y, (double)kFond.z);
	printf("pixels de geometrie    : %u  (non-fond, derniere image)\n", c.pixelsGeometrie);
	printf("perimetre attendu ~    : %u  (4*racine(surface), forme compacte)\n", perimetreAttendu);
	printf("---- LA MESURE ----\n");
	printf("DIFF_PIXELS            : %u\n", c.diffPixels);
	printf("SOMME_DELTAS           : %llu\n", (unsigned long long)c.sommeDeltas);
	// ── (t4) LUMINANCE DES PREMIERES IMAGES, ET UN ATTENDU QUE J'AI DU RETIRER ──
	//
	// J'avais écrit ici : « si l'accumulation a lieu, L(n) = L(final)*(1-blend^n),
	// puisque l'historique part d'un effacement à noir ». C'ETAIT FAUX, et la
	// mesure ne l'a pas contredit — c'est la lecture du code qui l'a fait.
	//
	// A la PREMIERE image, `mTAAHasPrev` est faux, donc le CPU envoie blend = 0
	// (vérifié dans la trace du moteur : `image=1 useHistory=0 blend=0`). Le
	// nuanceur fait alors un passe-plat, et `TAA_Store` range cette image
	// COMPLETE dans l'historique. A l'image 2, l'historique vaut donc déjà
	// l'image finale : IL N'Y A AUCUNE MONTEE DEPUIS LE NOIR A MESURER.
	//
	// La montée « 19,6 puis 92,9 » que cite NkRendererImpl.cpp:556 décrit un cas
	// précis et DIFFERENT — un historique recréé vierge par un rebuild alors que
	// `mTAAHasPrev` serait resté vrai — et c'est exactement le cas que le
	// désarmement de `RebuildRenderGraph` empêche. J'avais transporté un chiffre
	// sans sa condition.
	//
	// Le relevé RESTE, sans attendu : une suite plate est ici le comportement
	// NORMAL, et le savoir évite à la prochaine session d'y voir une panne.
	printf("---- (t4) LUMINANCE DES 8 PREMIERES IMAGES (releve, SANS attendu) ----\n");
	printf("     une suite PLATE est NORMALE : l'image 1 est un passe-plat (blend=0)\n");
	printf("     et TAA_Store en fait l'historique, qui n'est donc jamais vierge.\n");
	{
		// L(final) est pris sur la DERNIERE image de la course, pas sur une image
		// choisie : une reference prise a un numero qu'on choisit est le piege le
		// plus cher du depot.
		double lumFinale = 0.0;
		uint32 fondDansLaBoite = 0;
		{
			uint64 s = 0;
			uint32 n = 0;
			for (uint32 y = kBoiteY0; y < kBoiteY1; ++y)
				for (uint32 x = kBoiteX0; x < kBoiteX1; ++x) {
					const uint8 *p = imgB.Data() + (y * kW + x) * 4;
					s += (uint64)p[0] + p[1] + p[2];
					n++;
					if (p[0] == fr && p[1] == fg && p[2] == fb)
						fondDansLaBoite++;
				}
			lumFinale = n ? (double)s / (3.0 * (double)n) : 0.0;
		}
		// La garde qui empeche de mesurer l'accumulation sur du fond.
		printf("     fenetre (%u,%u)-(%u,%u) : %u pixels de FOND dedans (doit valoir 0)\n", kBoiteX0, kBoiteY0,
			   kBoiteX1, kBoiteY1, fondDansLaBoite);
		printf("     L(finale, image %u) = %.2f\n", imagesMesurees, lumFinale);
		for (uint32 k = 0; k < 8 && k < imagesMesurees; ++k) {
			printf("     L(%u) = %7.2f\n", k + 1, lumParImage[k]);
		}
	}
	printf("---- LE TEMPS (pire image de la course, aucun seuil) ----\n");
	printf("PIRE_IMAGE_MS          : %.3f\n", (double)pireNs / 1.0e6);
	printf("MOYENNE_MS             : %.3f  (denominateur : %u images)\n",
		   imagesMesurees ? (double)totalNs / 1.0e6 / (double)imagesMesurees : 0.0, imagesMesurees);

	// ── LA GARDE QUI EMPECHE UN VERT SUR DU VIDE ─────────────────────────────
	// Elle passe AVANT tout verdict. Deux images vides sont identiques, donc
	// (t0) vaut 0 pixel et se déclare vert : c'est arrivé au premier jet de ce
	// banc, et l'image était un aplat. Aucun critère ne vaut si rien n'est peint.
	if (c.pixelsGeometrie == 0) {
		printf("\n[ECHEC] AUCUN PIXEL DE GEOMETRIE : l'image est un aplat de fond.\n");
		printf("        Aucun critere de ce banc ne vaut dans cet etat — deux images\n");
		printf("        VIDES sont identiques, donc (t0) verdirait sur du rien.\n");
		NkRenderer::Destroy(r);
		NkDeviceFactory::Destroy(device);
		return 2;
	}

	// Le verdict de (t0) est le seul que ce binaire puisse rendre seul : les
	// autres comparent DEUX courses, donc ils appartiennent au script qui les
	// enchaine. Ce banc imprime des nombres ; il ne pretend pas conclure a leur
	// place.
	if (!envTaa || envTaa[0] == '0') {
		printf("---- VERDICT (t0), LE ZERO ----\n");
		if (c.diffPixels == 0)
			printf("[ ok ] t0 : NK_TAA=0 -> deux images consecutives identiques (0 pixel)\n");
		else
			printf("[ECHEC] t0 : NK_TAA=0 mais %u pixels different. Ce banc ne mesure PAS le TAA.\n", c.diffPixels);
	}

	// LES IMAGES TROUVENT CE QUE LES NOMBRES VALIDAIENT. Le banc ECRIT sa derniere
	// image quand on le lui demande : un compteur vert sur une image vide est deja
	// arrive ici meme, au premier jet.
	if (const char *cap = getenv("NK_TEMPOREL_CAPTURE"))
		if (cap[0] && cible.Capture(cap))
			printf("capture ecrite         : %s\n", cap);

	NkRenderer::Destroy(r);
	NkDeviceFactory::Destroy(device);
	printf("\n");
	return 0;
}
