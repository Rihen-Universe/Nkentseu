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
//
// -----------------------------------------------------------------------------
// LA SUITE, LE MEME JOUR : (t2) N'EST PLUS ROUGE, ET LA CAUSE ETAIT ECRITE DANS
// LE NUANCEUR DEPUIS LE DEBUT.
//
// Deux hypotheses ont ete mises a l'epreuve, attendus ecrits avant chaque course.
//
//   H1, le retournement decorrelerait l'historique -> REFUTEE. Avec
//   NK_TAA_YFLIP=-1 (image droite), le rapport passe de 0,915 a 0,9219 : il ne
//   descend pas, il monte. Le nuanceur de sommets calcule vUV UNE fois et les
//   TROIS echantillonnages s'en servent, donc le retournement est COHERENT et ne
//   casse pas la correspondance courant/historique. Le retournement reste un vrai
//   defaut, mais ce n'est pas celui-ci.
//
//   H2, les matrices de reprojection portent le JITTER -> CONFIRMEE. Le nuanceur
//   ecrit en tete : « les deux matrices sont DE-JITTREES : le jitter ne doit pas
//   entrer dans la correspondance geometrique ». Le C++ lui envoyait les matrices
//   jittees. La sonde NK_TAA_DEBUG=3, qui mesure |prevUV - vUV| x 20 sur une scene
//   et une camera IMMOBILES, rendait 6,33 la ou l'identite aurait rendu 0 --
//   l'attendu derive avant la course etait « 0 si de-jitte, 5 a 11 si jitte ».
//
// LE CORRECTIF ET CE QU'IL DEPLACE (meme binaire, un seul levier) :
//   sonde 3 (|prevUV - vUV|) :        6,33  ->  0,00
//   somme des ecarts, blend 0,9 :    33273  ->  4589
//   rapport a l'image non accumulee : 0,915 ->  0,1262
// L'attendu, derive AVANT par D(n) = 0,1 E(n) + 0,9 D(n-1), valait 0,136 pour la
// frequence dominante du cycle de Halton a 8 phases. Le correctif tombe dessus.
//
// CAMERA MOBILE (NK_TEMPOREL_ROT=1.5), parce qu'un correctif de reprojection
// valide sur une camera immobile serait valide hors de son cas d'usage : aucune
// trainee introduite -- 6307 pixels de silhouette apres correctif, 6307 avant,
// 6317 sans TAA du tout. Les deux captures sont visuellement identiques.
//
// NK_TAA_DEJITTER=0 restitue l'ancien comportement sans recompiler : ce correctif
// reste refutable.
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

#include <cmath>
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
	printf("    (peripherique SANS SURFACE ; sur OpenGL seul, NKRHI cree lui-meme une\n"
		   "     fenetre CACHEE de 1x1 pour son contexte, qu'il detruit au Shutdown)\n\n");

	// ── NK_TEMPOREL_API=dx11|dx12|gl|vk ─────────────────────────────────────
	// Un correctif entierement C++ cote appelant DEVRAIT profiter aux six dorsaux.
	// « Devrait » n'est pas mesure : la correspondance geometrique du TAA repose sur
	// des conventions de repere qui, elles, DIFFERENT par dorsal (yFlipUV, ndcYSign,
	// correction clip-Z). Ce banc doit donc pouvoir changer d'API.
	// ⚠️ Sur OpenGL SEULEMENT, NKRHI cree lui-meme une fenetre CACHEE de 1x1 parce
	// que GL exige une surface pour son contexte (NkOpenglDevice.cpp:388-399,
	// WS_POPUP, detruite au Shutdown). Ce n'est pas moi qui l'ouvre et je ne la ferme
	// pas a la main -- mais je ne pretendrai pas que zero fenetre a ete creee.
	const char *envApi = getenv("NK_TEMPOREL_API");
	NkGraphicsApi apiVoulue = NkGraphicsApi::NK_GFX_API_DX11;
	const char *nomApi = "DX11";
	if (envApi && envApi[0]) {
		if (strcmp(envApi, "gl") == 0) {
			apiVoulue = NkGraphicsApi::NK_GFX_API_OPENGL;
			nomApi = "OpenGL";
		} else if (strcmp(envApi, "vk") == 0) {
			apiVoulue = NkGraphicsApi::NK_GFX_API_VULKAN;
			nomApi = "Vulkan";
		} else if (strcmp(envApi, "dx12") == 0) {
			apiVoulue = NkGraphicsApi::NK_GFX_API_DX12;
			nomApi = "DX12";
		} else if (strcmp(envApi, "dx11") != 0) {
			printf("[ECHEC] NK_TEMPOREL_API inconnu : %s (dx11|dx12|gl|vk)\n", envApi);
			return 1;
		}
	}
	printf("    dorsal demande : %s\n", nomApi);

	NkDeviceInitInfo di;
	di.api = apiVoulue;
	di.width = 0; // pas de surface -> headless
	di.height = 0;
	NkIDevice *device = NkDeviceFactory::Create(di);
	if (!device || !device->IsValid()) {
		printf("[ECHEC] peripherique %s headless non cree\n", nomApi);
		return 1;
	}
	// ⚠️ TEMOIN D'IDENTITE, et il compte DOUBLE depuis qu'on compare deux dorsaux.
	// On LIT l'API obtenue au lieu de croire celle qu'on a demandee. Sans lui, une
	// retombee silencieuse sur DX11 ferait mesurer DEUX FOIS LE MEME CHEMIN, et
	// l'egalite parfaite des deux courses passerait pour une confirmation alors
	// qu'elle serait la signature d'un instrument.
	if (device->GetApi() != apiVoulue) {
		printf("[ECHEC] le peripherique obtenu n'est PAS %s : la fabrique est retombee\n"
			   "        sur une autre API. Mesurer ici comparerait un dorsal a lui-meme.\n",
			   nomApi);
		return 1;
	}

	// ForGame, puis on RALLUME le TAA explicitement : le profil HIGH l'eteint
	// (NkRendererConfig.h:718). C'est justement le fait mesure en R1.
	// NK_TEMPOREL_PROFIL=editor : le profil REEL du viseur du modeleur
	// (NkViewport3D.cpp:726) et de NkAnimaEditor. Il sert a repondre a une
	// question qui ne se deduit pas : aujourd'hui, l'image d'un viseur d'editeur
	// sort-elle DROITE ? Ces trois applications redirigent toutes leur sortie par
	// SetFinalColorTarget, et cinq dorsaux retournent l'image des qu'un blit d'ECRAN
	// ecrit dans une cible hors ecran.
	const char *envProfil = getenv("NK_TEMPOREL_PROFIL");
	const bool profilEditeur = (envProfil && envProfil[0] == 'e');
	NkRendererConfig cfg = profilEditeur ? NkRendererConfig::ForEditor(apiVoulue, kW, kH)
										 : NkRendererConfig::ForGame(apiVoulue, kW, kH);
	printf("    profil : %s\n", profilEditeur ? "ForEditor" : "ForGame");
	cfg.postProcess.taa = true;
	// NK_TEMPOREL_FXAA=1 : FXAA au lieu du TAA. C'est le candidat C1 du chantier
	// « retournement DX11 » -- FXAA n'insere QU'UNE passe plein ecran apres le
	// tonemap, la ou le TAA en insere TROIS. Si l'image est droite avec une et
	// retournee avec trois, la faute est dans l'enchainement et non dans yFlipUV.
	// ⚠️ ABSENTE, LA VARIABLE NE TOUCHE PLUS A RIEN. Elle ECRASAIT le `fxaa` du
	// preset, donc le profil editeur etait mesure sans son FXAA -- c'est-a-dire
	// pas le profil editeur. Une option de banc qui modifie silencieusement la
	// configuration qu'on croit mesurer est un instrument qui ment.
	// NK_TEMPOREL_BLOOM=0/1 : le bloom, dont les deux passes portent l'ancienne
	// forme du signe Y. Absente, la variable ne touche pas au preset.
	if (const char *v = getenv("NK_TEMPOREL_BLOOM"))
		if (v[0])
			cfg.postProcess.bloom = (v[0] != '0');
	if (const char *v = getenv("NK_TEMPOREL_FXAA"))
		if (v[0])
			cfg.postProcess.fxaa = (v[0] != '0');
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

	// NK_TEMPOREL_NOOVERRIDE=1 : candidat C2. Mon banc pose une taille de rendu
	// independante ; les applications ne le font pas toutes. Si le retournement
	// disparait sans l'override, c'est LUI la condition.
	{
		const char *v = getenv("NK_TEMPOREL_NOOVERRIDE");
		if (!(v && v[0] && v[0] != '0'))
			r->SetRenderSizeOverride(kW, kH);
	}
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
	// ── NK_TEMPOREL_MOVE=<unites monde par image> : L'OBJET BOUGE ────────────
	// Camera posee sur l'axe Z, objet qui glisse en X. Attendu DERIVE de la
	// matrice de projection reellement construite par NkCamera3D (fovY VERTICAL,
	// mProj[0][0] = 1/(aspect x tanHalf), cf. NkCamera.cpp:146) :
	//     duv_x = 0,5 x dx / (aspect x tanHalfFovY x d)
	// ⚠️ Une premiere version de cet attendu supposait un fov HORIZONTAL et
	// oubliait l'aspect : elle etait fausse d'un facteur 1,3333, assez pour
	// declarer ROUGE un moteur correct et envoyer chercher un defaut inexistant.
	// « fov » ne dit pas quel axe.
	const char *envMove = getenv("NK_TEMPOREL_MOVE");
	const float32 deplacementParImage = (envMove && envMove[0]) ? (float32)atof(envMove) : 0.f;
	const float32 kDistCam = 4.28f;
	// NK_TEMPOREL_OFFSET_Y : decale l'objet VERTICALEMENT, sans le faire bouger.
	// Indispensable au critere du bloom : un objet CENTRE rendrait « halo droit » et
	// « halo retourne » indiscernables, et le critere ne pourrait pas echouer.
	const char *envOffY = getenv("NK_TEMPOREL_OFFSET_Y");
	const float32 decalageY = (envOffY && envOffY[0]) ? (float32)atof(envOffY) : 0.f;
	const char *envAxe = getenv("NK_TEMPOREL_MOVE_AXIS");
	const bool axeVertical = (envAxe && (envAxe[0] == 'y' || envAxe[0] == 'Y'));

	// Camera sur l'axe Z quand l'objet doit glisser : voir NK_TEMPOREL_MOVE.
	if (deplacementParImage != 0.f)
		ctx.camera.SetPosition({0.f, 0.f, kDistCam});
	else
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
	// NK_TEMPOREL_LUM : le bloom ne se declenche qu'au-dessus d'un seuil de
	// brillance HDR. A l'intensite nominale, la scene reste sous ce seuil et le
	// bloom n'ajoute RIEN -- mesure : image identique au bit avec et sans lui,
	// alors que ses onze passes tournent. Mesurer un halo demande donc d'abord de
	// FABRIQUER un halo.
	{
		const char *v = getenv("NK_TEMPOREL_LUM");
		soleil.intensity = (v && v[0]) ? (float32)atof(v) : 3.f;
	}
	ctx.lights.PushBack(soleil);

	// Le plan de fond : un quad mis a l'echelle, place DERRIERE l'objet et
	// perpendiculaire a l'axe de vue quand la camera est sur l'axe Z.
	NkMeshHandle planFond = meshes->GetQuad();
	NkMat4f planXform = NkMat4f::Identity();
	planXform[0][0] = 24.f;
	planXform[1][1] = 24.f;
	planXform[3][2] = -2.5f;

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
	dc.aabb = {{-1.5f, -1.5f + decalageY, -1.5f}, {1.5f, 1.5f + decalageY, 1.5f}};
	if (decalageY != 0.f)
		dc.transform[3][1] = decalageY;

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

	// -- NK_TEMPOREL_ROT=<degres par image> : LA CAMERA BOUGE -----------------
	// Un TAA se juge surtout EN MOUVEMENT : c'est la que la reprojection sert, et
	// c'est la qu'un TAA casse laisse une TRAINEE. Valider un correctif de
	// reprojection sur une camera immobile, ce serait le valider hors de son cas
	// d'usage. ATTENDU, ECRIT AVANT : une trainee ETALE la silhouette, donc elle
	// GONFLE le nombre de pixels non-fond. Un TAA sain garde ce nombre proche de
	// celui obtenu SANS TAA sur la MEME trajectoire.
	const char *envRot = getenv("NK_TEMPOREL_ROT");
	const float32 rotParImage = (envRot && envRot[0]) ? (float32)atof(envRot) : 0.f;
	const float32 rayonCam = 4.28f;  // |(2.6, 3.4)| dans le plan XZ
	const float32 azimutDep = 0.6529f; // atan2(2.6, 3.4), l'azimut de depart

	for (uint32 i = 0; i < kImages; ++i) {
		const uint64 t0 = ::nkentseu::NkChrono::Now().nanoseconds;
		if (!r->BeginFrame())
			continue;
		if (rotParImage != 0.f) {
			const float32 a = azimutDep + (float32)i * rotParImage * 3.14159265f / 180.f;
			ctx.camera.SetPosition({rayonCam * (float32)::sin((double)a), 2.0f,
									rayonCam * (float32)::cos((double)a)});
			ctx.camera.SetTarget({0.f, 0.f, 0.f});
		}
		// ── L'OBJET QUI TRAVERSE A VITESSE CONNUE (critere m1) ───────────────
		// La camera est posee sur l'axe Z et l'objet glisse en X : le deplacement
		// est alors exactement PERPENDICULAIRE a l'axe de vue, donc la profondeur
		// de vue `d` reste CONSTANTE quand l'objet glisse. C'est ce qui rend
		// l'attendu calculable exactement, au lieu de varier avec la position.
		if (deplacementParImage != 0.f) {
			NkMat4f mPrec = dc.transform;
			NkMat4f mCour = NkMat4f::Identity();
			// Depart a gauche : l'objet doit TRAVERSER, pas sortir. Le champ a z=0
			// fait 2 x d x tan(fovY/2) x aspect = 4,73 unites, soit +/- 2,36.
			// L'AXE compte, et c'est tout l'objet de (v2). Un mouvement HORIZONTAL
			// est independant de la convention Y -- c'est pourquoi (m1) l'employait.
			// Au branchement du TAA, c'est justement l'axe Y qui trompe : il faut
			// donc pouvoir le mesurer. NK_TEMPOREL_MOVE_AXIS=y.
			if (axeVertical)
				mCour[3][1] = -1.0f + (float32)i * deplacementParImage;
			else
				mCour[3][0] = -1.0f + (float32)i * deplacementParImage;
			dc.transform = mCour;
			// La pose PRECEDENTE est fournie explicitement. Sans elle, le moteur
			// traiterait l'objet comme statique -- ce qui est son defaut, et c'est
			// justement ce que (m0) verifie par ailleurs.
			dc.prevTransform = (i == 0) ? mCour : mPrec;
			dc.hasPrevTransform = true;
			// L'AABB suit l'objet, sinon le culling le rejette des qu'il sort de la
			// boite d'origine -- un rejet SILENCIEUX, deja paye une fois sur ce banc.
			const float32 cx = mCour[3][0], cy = mCour[3][1];
			dc.aabb = {{cx - 1.5f, cy - 1.5f, -1.5f}, {cx + 1.5f, cy + 1.5f, 1.5f}};
		}
		r3d->BeginScene(ctx);
		// ── LE PLAN DE FOND, ET IL EST INDISPENSABLE ─────────────────────────
		// Sans lui, le « fond » est le ciel : la porte `depth >= 0.9999` du
		// nuanceur TAA l'ecarte AVANT toute lecture d'historique, donc il
		// n'accumule RIEN. Un critere de trainee mesure alors zero avec ET sans
		// vecteurs, et on lirait ce double zero comme une reussite.
		// Il faut de la VRAIE geometrie derriere l'objet pour que le fond ait un
		// historique, donc pour qu'il puisse se degrader -- et donc pour qu'on
		// puisse verifier qu'il ne se degrade pas.
		if (planFond.IsValid()) {
			NkDrawCall3D bg;
			bg.mesh = planFond;
			bg.transform = planXform;
			bg.aabb = {{-20.f, -20.f, -2.6f}, {20.f, 20.f, -2.4f}};
			bg.tint = {0.12f, 0.22f, 0.45f};
			bg.roughness = 0.9f;
			bg.castShadow = false;
			r3d->Submit(bg);
		}
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
	// ── LECTURE DES VECTEURS DE MOUVEMENT (criteres m0 et m1) ────────────────
	// L'encodage de la sonde est exactement inversible :
	//     r = 0,5 + motion.x x A   ->   motion.x = (r/255 - 0,5) / A
	// On prend la MEDIANE et non la moyenne : la profondeur de vue varie sur la
	// silhouette d'un cube (faces avant et arriere), donc les vecteurs varient
	// autour de leur valeur centrale, et une moyenne se ferait tirer par les bords
	// de la silhouette ou l'antialiasing melange objet et fond.
	if (const char *amp = getenv("NK_MOTION_DEBUG")) {
		if (amp[0] && amp[0] != '0') {
			float32 A = (float32)atof(amp);
			if (A <= 1.f)
				A = 16.f; // meme regle que le moteur : « 1 » veut dire « allume »
			// ⚠️ LE TEMOIN DU CANAL BLEU, D'ABORD. Il vaut 0,5 partout et toujours
			// dans l'encodage. S'il s'ecarte de 128, ce n'est pas le vecteur qui est
			// faux, c'est le CHEMIN DE LECTURE (format, tonemap reste branche,
			// espace colorimetrique). Sans ce temoin, un banc ne peut pas distinguer
			// « le moteur ecrit un mauvais vecteur » de « je lis mal ».
			uint32 bleuHorsNorme = 0;
			for (uint32 k = 0; k < nPix; ++k)
				if (imgB[k * 4 + 2] < 126 || imgB[k * 4 + 2] > 130)
					bleuHorsNorme++;
			printf("---- (m0/m1) VECTEURS DE MOUVEMENT ----\n");
			printf("     amplification          : %.1f\n", (double)A);
			printf("     TEMOIN canal bleu      : %u pixels hors de 128 +/- 2 sur %u\n", bleuHorsNorme, nPix);
			if (bleuHorsNorme * 100u > nPix * 2u) {
				printf("     [ECHEC] le canal temoin n'est pas a 128 : c'est le chemin de LECTURE\n");
				printf("             qui est en cause, pas les vecteurs. Aucun chiffre ci-dessous ne vaut.\n");
			} else {
				// Les pixels qui ont un vecteur NON NUL. Le seuil de 1 niveau est la
				// quantification, pas un reglage : en dessous, rien n'est distinguable
				// du fond par construction.
				NkVector<float32> vx, vy;
				for (uint32 k = 0; k < nPix; ++k) {
					const int r8 = (int)imgB[k * 4 + 0];
					const int g8 = (int)imgB[k * 4 + 1];
					if (r8 > 129 || r8 < 127 || g8 > 129 || g8 < 127) {
						vx.PushBack(((float32)r8 / 255.f - 0.5f) / A);
						vy.PushBack(((float32)g8 / 255.f - 0.5f) / A);
					}
				}
				printf("     pixels a vecteur NON NUL : %u\n", (uint32)vx.Size());
				if (vx.Size() == 0) {
					printf("     mediane duv_x          : 0 (aucun pixel ne bouge)\n");
				} else {
					// Tri par insertion : quelques milliers d'elements, et une
					// dependance de moins.
					for (uint32 a1 = 1; a1 < (uint32)vx.Size(); ++a1) {
						float32 kx = vx[a1], ky = vy[a1];
						uint32 b1 = a1;
						while (b1 > 0 && vx[b1 - 1] > kx) {
							vx[b1] = vx[b1 - 1];
							b1--;
						}
						vx[b1] = kx;
						(void)ky;
					}
					for (uint32 a1 = 1; a1 < (uint32)vy.Size(); ++a1) {
						float32 ky = vy[a1];
						uint32 b1 = a1;
						while (b1 > 0 && vy[b1 - 1] > ky) {
							vy[b1] = vy[b1 - 1];
							b1--;
						}
						vy[b1] = ky;
					}
					const float32 medx = vx[(uint32)vx.Size() / 2];
					const float32 medy = vy[(uint32)vy.Size() / 2];
					printf("     MEDIANE duv_x          : %.6f   (soit %.3f pixels)\n", (double)medx,
						   (double)medx * (double)kW);
					printf("     MEDIANE duv_y          : %.6f\n", (double)medy);
					if (deplacementParImage != 0.f) {
						const double aspect = (double)kW / (double)kH;
						const double tanHalf = 0.41421356; // tan(45/2 degres)
						const double attendu = 0.5 * (double)deplacementParImage / (aspect * tanHalf * (double)kDistCam);
						const double ecart = (attendu != 0.0) ? (double)medx / attendu : 0.0;
						printf("     ATTENDU derive         : %.6f   (rapport mesure/attendu : %.4f)\n", attendu,
							   ecart);
						printf("     critere (m1)           : %s   [tolerance +/- 15 %%]\n",
							   (ecart > 0.85 && ecart < 1.15) ? "[ ok ]" : "[ECHEC]");
						const double rapportY = (medx != 0.f) ? (double)medy / (double)medx : 0.0;
						printf("     |duv_y| / |duv_x|      : %.4f   %s   [doit rester sous 0,1]\n", rapportY,
							   (rapportY < 0.1 && rapportY > -0.1) ? "[ ok ]" : "[ECHEC]");
					}
				}
			}
		}
	}

	// -- (v1) L'ERREUR SUR L'OBJET MOBILE, CONTRE UNE REFERENCE --------------
	// NK_TEMPOREL_REF=<fichier> : absent du disque, le banc l'ECRIT (c'est la
	// course de reference) ; present, il le LIT et mesure l'ecart.
	//
	// LA REFERENCE EST LA COURSE SANS TAA, et c'est le seul choix defendable :
	// elle n'accumule rien, donc elle ne peut ni trainer ni delaver. Comparer deux
	// courses TAA entre elles ne dirait que « elles different », pas laquelle est
	// juste.
	//
	// Le classement objet / fond vient de la REFERENCE, pas de la course mesuree :
	// un pixel est « objet » si la reference l'y voit. Le deduire de la course
	// mesuree serait circulaire -- une course qui delave l'objet retrecirait la
	// silhouette sur laquelle on mesure son delavage.
	if (const char *refPath = getenv("NK_TEMPOREL_REF")) {
		if (refPath[0]) {
			FILE *f = fopen(refPath, "rb");
			if (!f) {
				FILE *w = fopen(refPath, "wb");
				if (w) {
					fwrite(imgB.Data(), 1, nPix * 4, w);
					fclose(w);
					printf("---- (v1) REFERENCE ECRITE : %s ----\n", refPath);
				} else {
					printf("---- (v1) [ECHEC] impossible d'ecrire la reference %s ----\n", refPath);
				}
			} else {
				NkVector<uint8> ref;
				ref.Resize(nPix * 4);
				const size_t lus = fread(ref.Data(), 1, nPix * 4, f);
				fclose(f);
				if (lus != (size_t)nPix * 4) {
					printf("---- (v1) [ECHEC] reference tronquee (%u octets sur %u) ----\n", (uint32)lus, nPix * 4);
				} else {
					const uint8 rr = ref[0], rg = ref[1], rb = ref[2];
					uint64 errObjet = 0, errFond = 0;
					uint32 nObjet = 0, nFond = 0;
					for (uint32 k = 0; k < nPix; ++k) {
						const uint8 *pr = ref.Data() + k * 4;
						const uint8 *pm = imgB.Data() + k * 4;
						uint32 d = 0;
						for (int c = 0; c < 3; ++c)
							d += (uint32)(pr[c] > pm[c] ? pr[c] - pm[c] : pm[c] - pr[c]);
						const bool estObjet = (pr[0] != rr || pr[1] != rg || pr[2] != rb);
						if (estObjet) {
							errObjet += d;
							nObjet++;
						} else {
							errFond += d;
							nFond++;
						}
					}
					printf("---- (v1) ERREUR CONTRE LA REFERENCE ----\n");
					printf("     ERR_OBJET              : %llu   sur %u pixels\n", (unsigned long long)errObjet,
						   nObjet);
					printf("     ERR_FOND               : %llu   sur %u pixels\n", (unsigned long long)errFond,
						   nFond);
				}
			}
		}
	}

	// -- (v3) LE COMPTE DES REJETS DU TAA (NK_TAA_DEBUG=5) -------------------
	// Un repli qu'on ne peut pas compter est un repli qu'on ne saura jamais trop
	// frequent. La sonde 5 peint une couleur par cas ; ce bloc les compte.
	//   rouge  = VECTEUR    : le deplacement a servi
	//   vert   = PROFONDEUR : repli geometrique (fond, ou objet statique)
	//   autre  = SORTI TOT  : ciel, derriere la camera, hors ecran, sans histoire
	if (const char *dbg = getenv("NK_TAA_DEBUG")) {
		if (dbg[0] == '5') {
			uint32 nVecteur = 0, nProfondeur = 0, nSortiTot = 0;
			for (uint32 k = 0; k < nPix; ++k) {
				const uint8 *p = imgB.Data() + k * 4;
				if (p[0] > 200 && p[1] < 60)
					nVecteur++;
				else if (p[1] > 200 && p[0] < 60)
					nProfondeur++;
				else
					nSortiTot++;
			}
			printf("---- (v3) COMPTE DES REJETS ----\n");
			printf("     VECTEUR    : %6u  (%.1f %%)\n", nVecteur, 100.0 * nVecteur / (double)nPix);
			printf("     PROFONDEUR : %6u  (%.1f %%)\n", nProfondeur, 100.0 * nProfondeur / (double)nPix);
			printf("     SORTI TOT  : %6u  (%.1f %%)\n", nSortiTot, 100.0 * nSortiTot / (double)nPix);
			printf("     total      : %6u  (doit valoir %u : une decomposition qui ne se\n",
				   nVecteur + nProfondeur + nSortiTot, nPix);
			printf("                          referme pas sur son total mesure autre chose)\n");
		}
	}

	// -- (b1) LE CENTRE DE MASSE DE CE QUE LE BLOOM AJOUTE -------------------
	// L'image AVEC bloom moins l'image SANS bloom donne exactement ce que le bloom
	// ajoute. Un halo entoure sa source : le centre de masse de cet ajout doit donc
	// tomber sur celui de l'objet. Si une passe du bloom est retournee, il tombe en
	// H - y. C'est pourquoi l'objet est DECENTRE : centre, les deux hypotheses
	// donneraient le meme nombre et le critere ne pourrait pas echouer.
	if (getenv("NK_TEMPOREL_BLOOMCM")) {
		FILE *f = fopen("Captures/temporel/bloom_ref.raw", "rb");
		if (!f) {
			FILE *w = fopen("Captures/temporel/bloom_ref.raw", "wb");
			if (w) {
				fwrite(imgB.Data(), 1, nPix * 4, w);
				fclose(w);
				printf("---- (b1) REFERENCE BLOOM ECRITE ----\n");
			}
		} else {
			NkVector<uint8> ref;
			ref.Resize(nPix * 4);
			const size_t lus = fread(ref.Data(), 1, nPix * 4, f);
			fclose(f);
			if (lus == (size_t)nPix * 4) {
				double sommeY = 0.0, sommeP = 0.0;
				uint32 nDiff = 0;
				double cmObjY = 0.0, cmObjP = 0.0;
				for (uint32 y = 0; y < kH; ++y)
					for (uint32 x = 0; x < kW; ++x) {
						const uint32 k = y * kW + x;
						const uint8 *pr = ref.Data() + k * 4;
						const uint8 *pm = imgB.Data() + k * 4;
						int d = 0;
						for (int c = 0; c < 3; ++c)
							d += (int)pm[c] - (int)pr[c];
						if (d > 3) { // ce que le bloom AJOUTE, pas ce qu'il retire
							sommeY += (double)y * (double)d;
							sommeP += (double)d;
							nDiff++;
						}
						// centre de masse de l'OBJET, pris sur la reference :
						// luminance au-dessus du fond.
						const double lum = ((double)pr[0] + pr[1] + pr[2]) / 3.0;
						if (lum > 60.0) {
							cmObjY += (double)y * lum;
							cmObjP += lum;
						}
					}
				printf("---- (b1) CENTRE DE MASSE ----\n");
				printf("     pixels ajoutes par le bloom : %u\n", nDiff);
				printf("     CM_OBJET   (y, pixels)      : %.1f\n", cmObjP > 0 ? cmObjY / cmObjP : -1.0);
				printf("     CM_AJOUT   (y, pixels)      : %.1f\n", sommeP > 0 ? sommeY / sommeP : -1.0);
				printf("     miroir de CM_OBJET          : %.1f   (ce que donnerait un halo RETOURNE)\n",
					   cmObjP > 0 ? (double)kH - cmObjY / cmObjP : -1.0);
			}
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
	// ⚠️ ET CETTE GARDE NON PLUS NE SE PRONONCE PAS HORS DE SA CONDITION. Sous
	// NK_MOTION_DEBUG, l'image finale N'EST PAS UN RENDU : c'est une carte de
	// vecteurs, uniformement grise la ou rien ne bouge. « Aucun pixel de
	// geometrie » y est le comportement NORMAL, et la garde criait donc ROUGE sur
	// un resultat juste. Dans ce mode, le temoin qui vaut est le CANAL BLEU, qui
	// est verifie plus haut.
	const bool modeCarteDeVecteurs = [] {
		const char *v = getenv("NK_MOTION_DEBUG");
		return v && v[0] && v[0] != '0';
	}();
	if (c.pixelsGeometrie == 0 && !modeCarteDeVecteurs) {
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
	// ⚠️ (t0) NE SE PRONONCE QUE SI RIEN NE BOUGE. Il affirme « deux images
	// consecutives sont identiques » ; sous NK_TEMPOREL_MOVE ou NK_TEMPOREL_ROT,
	// quelque chose bouge PAR CONSTRUCTION et le critere crierait ROUGE sur un
	// moteur parfaitement correct. Un temoin qui se prononce hors de sa condition
	// de validite ne mesure plus rien -- il fabrique du bruit qu'on finit par
	// apprendre a ignorer, et c'est ainsi qu'un vrai rouge passe inapercu.
	const bool sceneImmobile = (deplacementParImage == 0.f && rotParImage == 0.f);
	if ((!envTaa || envTaa[0] == '0') && sceneImmobile) {
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
