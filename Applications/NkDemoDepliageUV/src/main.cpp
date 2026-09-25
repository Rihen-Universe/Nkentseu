// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Applications/NkDemoDepliageUV/src/main.cpp
// -----------------------------------------------------------------------------
// NkDemoDepliageUV — LE DEPLIAGE UV, A L'ECRAN, SUR UN OBJET GENERE.
//
// CE QU'ELLE MONTRE : un objet produit par la chaine de generation, couvert d'un
// DAMIER. Le damier est le seul juge honnete d'un depliage : ses carreaux sont
// carres et de meme taille dans l'atlas, donc tout ce qu'on voit d'etire,
// d'ecrase ou de hache sur la surface vient du depliage, pas du dessin.
//
// CE QU'ON FAIT : la touche ESPACE bascule entre
//    [BRUT]    les UV telles que le fichier les porte (souvent aucune : un seul
//              carreau etire sur tout l'objet, ou rien du tout) ;
//    [DEPLIE]  les UV calculees par `NkUVUnwrapAuto` -- coutures automatiques,
//              de-soudure, depliage LSCM.
// La difference doit se VOIR. Si elle ne se voit pas, la chaine ne sert a rien.
//
// CE QUI PROUVERAIT QUE C'EST CASSE (a chercher, pas a admirer) :
//   * [DEPLIE] et [BRUT] donnent la MEME image -> les UV calculees n'arrivent
//     pas jusqu'au rendu ; le cablage est coupe entre le solveur et le GPU ;
//   * le damier est un confetti de carreaux minuscules et sans continuite ->
//     le depliage a eclate le maillage en un ilot par triangle. C'est l'etat
//     MESURE aujourd'hui sur nos maillages (2 115 ilots pour 2 156 triangles) ;
//   * l'ecran affiche un REFUS avec un nom et un chiffre d'Euler -> le maillage
//     n'est pas depliable tel quel. Ce n'est PAS une panne de la demo : c'est le
//     module qui refuse, et il dit pourquoi.
//
// ATTENTION -- CETTE DEMO EST HONNETE, PAS FLATTEUSE. Au 25/09/2026 le depliage
// automatique ne passe PAS les criteres : sur un maillage TripoSR il produit
// 151 % de recouvrement en 42 s, et sur un maillage de famille il rend un ilot
// par triangle. La demo existe pour que Rodolf VOIE cet etat, pas pour lui
// cacher. Le solveur n'est pas en cause : c'est la STRATEGIE DE COUPE qui
// manque, et c'est le prochain lot.
//
// AUCUN ARGUMENT : la demo cherche elle-meme un objet genere dans les
// emplacements connus, et si elle n'en trouve aucun elle le DIT a l'ecran.
// -----------------------------------------------------------------------------
#include <NKWindow/Core/NkMain.h>
#include <NKWindow/Core/NkWindow.h>
#include <NKEvent/NkEventDispatcher.h>
#include <NKRHI/Core/NkDeviceFactory.h>
#include <NKRenderer/NkRenderer.h>

#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKLogger/NkLog.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKTime/NkChrono.h"
#include "NKWindow/Core/NkWindowConfig.h"

#ifdef DrawText
#undef DrawText
#endif

#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkSceneContext.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Mesh/NkEditMesh.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Mesh/NkOBJLoader.h"
#include "NKRenderer/Mesh/NkUVUnwrap.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Text/NkTextRenderer.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

using namespace nkentseu;
using namespace nkentseu::renderer;

static void ConfigureAppData(NkAppData &d) {
	d.appName = "NkDemoDepliageUV";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

// -- LE DAMIER -----------------------------------------------------------------
// Genere, pas charge : une demo qui depend d'un fichier d'assets se casse des
// qu'on la lance d'ailleurs, et la regle dit « sans fichier a preparer ».
// 16 x 16 carreaux sur 512 px, deux gris bien separes plus un liseré orange
// tous les quatre carreaux : l'oeil suit la DIRECTION de l'etirement, pas
// seulement sa presence.
static void FabriquerDamier(uint8 *px, uint32 n, uint32 carreaux) {
	const uint32 taille = n / carreaux;
	for (uint32 y = 0; y < n; ++y) {
		for (uint32 x = 0; x < n; ++x) {
			const uint32 cx = x / taille, cy = y / taille;
			const bool clair = ((cx + cy) & 1u) != 0u;
			const bool reglure = (cx % 4u == 0u) || (cy % 4u == 0u);
			uint8 r, g, b;
			if (reglure) {
				r = 247;
				g = 154;
				b = 40; // orange Rihen : la grille de reference
			} else if (clair) {
				r = g = b = 222;
			} else {
				r = g = b = 58;
			}
			uint8 *p = px + ((usize)y * n + x) * 4u;
			p[0] = r;
			p[1] = g;
			p[2] = b;
			p[3] = 255;
		}
	}
}

// -- TROUVER UN OBJET GENERE, SANS RIEN DEMANDER --------------------------------
// La demo s'ouvre et montre. Si rien n'est trouve, elle le DIT a l'ecran au lieu
// de se fermer : une fenetre qui disparait ne dit pas pourquoi.
// -- LA GEOMETRIE DE REPLI : UN CUBE, CONSTRUIT EN CODE -------------------------
// UNE DEMO NE DOIT PAS POUVOIR NE RIEN MONTRER. Si aucun objet genere n'est
// trouve -- et c'est arrive : les chemins etaient relatifs au repertoire
// courant, donc dependants de l'endroit d'ou on lance -- la fenetre restait un
// aplat uni, ce qui ne se distingue pas d'un rendu casse. Avec ce cube, une
// fenetre vide ne peut plus signifier qu'une chose : le rendu 3D ne passe pas.
// Le repli est ANNONCE a l'ecran et au journal : un repli muet serait pire que
// le vide, il ferait croire qu'on regarde l'objet genere.
//
// 24 sommets (4 par face) : chaque face porte ses propres UV, donc le damier y
// tombe droit et sans couture -- c'est la reference contre laquelle juger un
// depliage.
static void FabriquerCube(NkVector<NkVertex3D> &v, NkVector<uint32> &idx) {
	v.Clear();
	idx.Clear();
	static const float32 kN[6][3] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0},
									 {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
	static const float32 kU[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, -1},
									 {0, 0, 1}, {1, 0, 0}, {1, 0, 0}};
	static const float32 kV[6][3] = {{0, 1, 0}, {0, 1, 0}, {0, 1, 0},
									 {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
	for (uint32 f = 0; f < 6u; ++f) {
		const uint32 base = (uint32)v.Size();
		for (uint32 c = 0; c < 4u; ++c) {
			const float32 su = (c == 1u || c == 2u) ? 1.f : -1.f;
			const float32 sv = (c >= 2u) ? 1.f : -1.f;
			NkVertex3D p{};
			p.pos = {kN[f][0] + kU[f][0] * su + kV[f][0] * sv,
					 kN[f][1] + kU[f][1] * su + kV[f][1] * sv,
					 kN[f][2] + kU[f][2] * su + kV[f][2] * sv};
			p.normal = {kN[f][0], kN[f][1], kN[f][2]};
			p.uv = {su * 0.5f + 0.5f, sv * 0.5f + 0.5f};
			p.color = 0xFFFFFFFFu;
			v.PushBack(p);
		}
		idx.PushBack(base + 0u);
		idx.PushBack(base + 1u);
		idx.PushBack(base + 2u);
		idx.PushBack(base + 0u);
		idx.PushBack(base + 2u);
		idx.PushBack(base + 3u);
	}
}

static const char *TrouverObjet(char *buf, usize cap) {
	// LES CHEMINS ETAIENT RELATIFS AU REPERTOIRE COURANT, et c'est la seconde
	// moitie de la demo vide : lancee depuis le dossier du binaire (un double
	// clic, par exemple) elle ne trouvait rien. On remonte donc jusqu'a six
	// niveaux, ce qui couvre `Build/Bin/Release-Windows/<app>/` depuis la
	// racine de l'arbre. Chaque essai part au journal : si rien n'est trouve,
	// on saura d'ou la demo a cherche.
	static const char *const kRelatifs[] = {
		"logs_genia3d/crea/ajust/000.obj",
		"logs_genia3d/crea/q14fid/cand_tripo/table.glb",
	};
	char prefixe[64] = {0};
	for (uint32 niveau = 0; niveau < 7u; ++niveau) {
		for (uint32 i = 0; i < (uint32)(sizeof(kRelatifs) / sizeof(kRelatifs[0])); ++i) {
			snprintf(buf, cap, "%s%s", prefixe, kRelatifs[i]);
			FILE *fp = fopen(buf, "rb");
			if (fp) {
				fclose(fp);
				logger.Info("[demo-uv] objet trouve : {0}\n", buf);
				return buf;
			}
			logger.Info("[demo-uv]   essai sans succes : {0}\n", buf);
		}
		strncat(prefixe, "../", sizeof(prefixe) - strlen(prefixe) - 1u);
	}
	return nullptr;
}

static bool ChargerMaillage(const char *chemin, NkGLTFMeshData &out) {
	const usize n = chemin ? strlen(chemin) : 0u;
	const bool obj = n > 4 && (strcmp(chemin + n - 4, ".obj") == 0 || strcmp(chemin + n - 4, ".OBJ") == 0);
	return obj ? LoadOBJ(NkString(chemin), out) : LoadGLTF(NkString(chemin), out);
}

// LES INDICES PASSENT PAR LES SOUS-MAILLAGES, jamais bruts : chaque sous-mesh
// porte son `baseVertex`, et les lire a plat donnerait une geometrie melangee
// qui a l'air plausible. Meme calcul que le temoin, pour que les deux ne
// puissent pas diverger.
static void IndicesGlobaux(const NkGLTFMeshData &data, NkVector<uint32> &out) {
	out.Clear();
	const uint32 iTotal = (uint32)data.indices.Size();
	for (uint32 s = 0; s < (uint32)data.subMeshes.Size(); ++s) {
		const NkSubMesh &sm = data.subMeshes[s];
		for (uint32 i = 0; i < sm.indexCount && sm.firstIndex + i < iTotal; ++i)
			out.PushBack(data.indices[sm.firstIndex + i] + sm.baseVertex);
	}
}

int nkmain(const NkEntryState &state) {
	NkWindowConfig winCfg;
	winCfg.title = "NkDemoDepliageUV — ESPACE : brut / deplie";
	winCfg.width = 1280;
	winCfg.height = 720;
	winCfg.centered = true;
	winCfg.resizable = true;
	winCfg.vsync = true;
	NkWindow window(winCfg);
	if (!window.IsValid())
		return -1;

	NkDeviceInitInfo devInit{};
	devInit.surface = window.GetSurfaceDesc();
	devInit.width = window.GetSize().x;
	devInit.height = window.GetSize().y;
	NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInit);
	if (!device)
		return -1;

	NkRendererConfig rendCfg = NkRendererConfig::ForGame(devInit.api, devInit.width, devInit.height);
	NkRenderer *renderer = NkRenderer::Create(device, rendCfg);
	if (!renderer || !renderer->Initialize()) {
		NkDeviceFactory::Destroy(device);
		return -1;
	}

	NkTextureLibrary *textures = renderer->GetTextures();
	NkTextRenderer *text = renderer->GetTextRenderer();
	NkRender2D *r2d = renderer->GetRender2D();
	NkRender3D *r3d = renderer->GetRender3D();
	NkMeshSystem *meshes = renderer->GetMeshSystem();
	NkMaterialSystem *mats = renderer->GetMaterials();
	// -- LA POLICE EST EMBARQUEE, PAS CHARGEE ----------------------------------
	// MA FAUTE, ET ELLE A COUTE UNE DEMO VIDE. J'avais ecrit « sans fichier a
	// preparer » en en-tete, puis demande `assets/font.ttf` -- un fichier qui
	// n'existe nulle part dans ce depot. `LoadFont` rendait une poignee
	// invalide, `DrawText` ne dessinait rien, et Rodolf a vu un bandeau peint et
	// VIDE. Le cadre se peignait (NkRender2D), le texte non : exactement la
	// signature d'une police absente.
	// `GetDefaultFont` ne depend d'aucun fichier : la police est dans le binaire.
	NkFontHandle police = text->GetDefaultFont();

	// -- LE DAMIER --------------------------------------------------------------
	static uint8 damier[512 * 512 * 4];
	FabriquerDamier(damier, 512u, 16u);
	NkTextureCreateDesc td;
	td.pixels = damier;
	td.width = 512;
	td.height = 512;
	td.srgb = true;
	td.debugName = "damier_uv";
	NkTexHandle texDamier = textures->Create(td);

	// -- CHARGER, DEPLIER, MESURER ----------------------------------------------
	char cheminBuf[512];
	// AUCUN ARGUMENT : `NkEntryState` n'expose pas argv, et la regle dit de
	// toute facon « sans argument obligatoire ». La demo cherche elle-meme.
	const char *chemin = TrouverObjet(cheminBuf, sizeof(cheminBuf));

	char ligne1[256] = {0}, ligne2[256] = {0}, ligne3[256] = {0};
	bool aObjet = false;
	NkMeshHandle mBrut, mDeplie;
	NkVec3f centre{0.f, 0.f, 0.f};
	float32 rayon = 1.f;

	bool repliCube = false;
	if (!chemin) {
		// AUCUN OBJET TROUVE : on montre le CUBE, et on le DIT. Une fenetre vide
		// ne dit pas si le fichier manque ou si le rendu est casse ; un cube qui
		// apparait separe les deux d'un coup d'oeil.
		repliCube = true;
		NkVector<NkVertex3D> cv;
		NkVector<uint32> ci;
		FabriquerCube(cv, ci);
		NkMeshDesc md;
		md.layout = renderer::NkVertexLayout::Default3D();
		md.vertices = cv.Data();
		md.vertexCount = (uint32)cv.Size();
		md.indices = ci.Data();
		md.indexCount = (uint32)ci.Size();
		mBrut = meshes->Create(md);
		mDeplie = mBrut; // le cube porte deja des UV par face : brut = deplie
		centre = {0.f, 0.f, 0.f};
		rayon = 1.8f;
		aObjet = mBrut.IsValid();
		snprintf(ligne1, sizeof(ligne1), "AUCUN OBJET GENERE TROUVE -- cube de repli affiche");
		snprintf(ligne2, sizeof(ligne2),
				 "lancer depuis la racine de l'arbre pour voir un maillage genere");
		snprintf(ligne3, sizeof(ligne3),
				 "si ce cube damier apparait, le rendu 3D fonctionne : c'est le fichier qui manque");
	} else {
		NkGLTFMeshData data;
		if (!ChargerMaillage(chemin, data) || !data.IsValid()) {
			snprintf(ligne1, sizeof(ligne1), "LE CHARGEUR REFUSE : %s", chemin);
		} else {
			NkVector<uint32> idx;
			IndicesGlobaux(data, idx);

			// Boite englobante : la camera se place dessus, l'objet n'a pas a etre
			// a une echelle connue.
			NkVec3f mn = data.vertices[0].pos, mx = mn;
			for (uint32 i = 1; i < (uint32)data.vertices.Size(); ++i) {
				const NkVec3f &p = data.vertices[i].pos;
				mn = {mn.x < p.x ? mn.x : p.x, mn.y < p.y ? mn.y : p.y, mn.z < p.z ? mn.z : p.z};
				mx = {mx.x > p.x ? mx.x : p.x, mx.y > p.y ? mx.y : p.y, mx.z > p.z ? mx.z : p.z};
			}
			centre = {(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f};
			const NkVec3f d{mx.x - mn.x, mx.y - mn.y, mx.z - mn.z};
			rayon = 0.5f * sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
			if (rayon < 1e-4f)
				rayon = 1.f;

			// [BRUT] : le maillage tel que le fichier le donne.
			{
				NkMeshDesc md;
				md.layout = renderer::NkVertexLayout::Default3D();
				md.vertices = data.vertices.Data();
				md.vertexCount = (uint32)data.vertices.Size();
				md.indices = idx.Data();
				md.indexCount = (uint32)idx.Size();
				mBrut = meshes->Create(md);
			}

			// [DEPLIE] : la chaine complete. On MESURE le temps, parce qu'une
			// demo qui fige au demarrage sans rien dire est une demo qui ment.
			NkEditMesh em;
			em.BuildFromIndexed(data.vertices.Data(), (uint32)data.vertices.Size(), idx.Data(),
								(uint32)idx.Size(), false);
			NkUVUnwrapParams pr;
			pr.packIslands = true;
			pr.packMargin = 0.02f;
			NkUVResult res;
			NkUVAutoBilan bilan;
			nkentseu::NkChrono chrono;
			const bool ok = NkUVUnwrapAuto(em, pr, res, &bilan);
			const double ms = chrono.Elapsed().ToMilliseconds();

			if (!ok) {
				// LE REFUS S'AFFICHE, AVEC SON NOM ET SON CHIFFRE. `refusEuler`
				// rend la valeur TROUVEE, pas seulement « != 1 » : c'est assez
				// rare pour qu'on s'en serve.
				snprintf(ligne1, sizeof(ligne1), "DEPLIAGE REFUSE : %s", NkUVRefusName(res.refus));
				snprintf(ligne2, sizeof(ligne2), "euler trouve = %d   ilot = %u   coins soudes = %u",
						 res.refusEuler, res.refusIsland, res.weldedCorners);
				snprintf(ligne3, sizeof(ligne3),
						 "ce n'est pas une panne : le maillage n'est pas depliable tel quel (%.0f ms)",
						 ms);
			} else {
				NkVector<NkVertex3D> ov;
				NkVector<uint32> oi;
				NkVector<NkEmId> otf;
				em.Triangulate(ov, oi, otf);
				NkMeshDesc md;
				md.layout = renderer::NkVertexLayout::Default3D();
				md.vertices = ov.Data();
				md.vertexCount = (uint32)ov.Size();
				md.indices = oi.Data();
				md.indexCount = (uint32)oi.Size();
				mDeplie = meshes->Create(md);

				const float32 triParIlot =
					res.islandCount ? (float32)res.distortion.triCount / (float32)res.islandCount : 0.f;
				snprintf(ligne1, sizeof(ligne1), "DEPLIE en %.0f ms   ilots = %u   %.1f triangle(s)/ilot",
						 ms, res.islandCount, (double)triParIlot);
				snprintf(ligne2, sizeof(ligne2),
						 "distorsion aire moy %.3f max %.1f   angle moy %.1f max %.1f deg",
						 (double)res.distortion.areaMean, (double)res.distortion.areaMax,
						 (double)res.distortion.angleMean, (double)res.distortion.angleMax);
				snprintf(ligne3, sizeof(ligne3), "recouvrement : %u paire(s)   coutures %u/%u retrouvees",
						 bilan.pairesRecouvrement, bilan.couturesRetrouvees, bilan.coutures);
			}
			aObjet = mBrut.IsValid();
		}
	}

	// -- LE MATERIAU : le damier en albedo --------------------------------------
	NkMaterial *mat = NkMaterial::Create(mats, NkMaterialType::NK_PBR_METALLIC);
	if (mat && texDamier.IsValid())
		mat->SetTexture("albedo_map", texDamier);

	// -- CE QUE LA DEMO CROIT DESSINER -----------------------------------------
	// UNE TRACE SE PLACE PAR CE QU'ELLE DOIT VOIR. « la fenetre s'ouvre et
	// tient » n'est pas un critere de demo : c'est le critere d'un processus qui
	// ne plante pas. Ces lignes disent, sans jamais regarder l'ecran, si le
	// defaut est en AMONT (rien de valide a soumettre) ou en AVAL (tout est
	// valide et l'ecran reste vide).
	logger.Info("[demo-uv] --- ce que la demo croit dessiner ---\n");
	logger.Info("[demo-uv] fichier      : {0}\n", chemin ? chemin : "(aucun -- cube de repli)");
	logger.Info("[demo-uv] police       : {0}\n", police.IsValid() ? "VALIDE (embarquee)" : "INVALIDE -- aucun texte ne sera peint");
	logger.Info("[demo-uv] damier       : {0}\n", texDamier.IsValid() ? "texture VALIDE" : "texture INVALIDE");
	logger.Info("[demo-uv] materiau     : {0}\n", mat ? "cree" : "NUL -- le drawcall n'aura pas de materiau");
	logger.Info("[demo-uv] maillage brut: {0}\n", mBrut.IsValid() ? "VALIDE" : "INVALIDE -- rien a soumettre");
	logger.Info("[demo-uv] maillage depl: {0}\n", mDeplie.IsValid() ? "VALIDE" : "absent (depliage refuse ou non calcule)");
	logger.Info("[demo-uv] cadrage      : centre({0} {1} {2}) rayon {3}\n", centre.x, centre.y, centre.z, rayon);
	logger.Info("[demo-uv] bandeau L1   : {0}\n", ligne1);
	logger.Info("[demo-uv] bandeau L2   : {0}\n", ligne2);
	logger.Info("[demo-uv] bandeau L3   : {0}\n", ligne3);

	bool deplie = false; // on ouvre sur [BRUT] : la reference d'abord
	uint32 imagesTracees = 0u;
	float32 angle = 0.f;
	bool running = true;
	math::NkVec2u taille(renderer->GetWidth(), renderer->GetHeight());

	while (running && window.IsOpen()) {
		NkEvent *ev;
		while (NkEvents().PollEvent(ev)) {
			if (auto *rz = ev->As<NkWindowResizeEvent>()) {
				taille = math::NkVec2u(rz->GetWidth(), rz->GetHeight());
			} else if (ev->As<NkWindowCloseEvent>()) {
				running = false;
			} else if (auto *k = ev->As<NkKeyPressEvent>()) {
				if (k->GetKey() == NkKey::NK_ESCAPE)
					running = false;
				else if (k->GetKey() == NkKey::NK_SPACE)
					deplie = !deplie;
			}
		}
		if (!running)
			break;
		if (taille.width > 0 && taille.height > 0 &&
			(renderer->GetWidth() != taille.width || renderer->GetHeight() != taille.height))
			renderer->OnResize(taille.width, taille.height);

		angle += 0.004f; // rotation lente : l'etirement se lit en tournant
		if (!renderer->BeginFrame())
			continue;
		NkICommandBuffer *cmd = renderer->GetCmd();
		const uint32 W = window.GetSize().x, H = window.GetSize().y;

		// -- LA SCENE 3D --------------------------------------------------------
		const NkMeshHandle courant = (deplie && mDeplie.IsValid()) ? mDeplie : mBrut;
		if (aObjet && courant.IsValid()) {
			// NkCamera est ABSTRAITE : c'est NkCamera3D qu'on instancie, et c'est
			// aussi le type que porte `NkSceneContext::camera`.
			const float32 dist = rayon * 3.2f;
			NkSceneContext sctx;
			sctx.camera.SetFOV(50.f);
			sctx.camera.SetAspect(W, H ? H : 1u);
			sctx.camera.SetNearFar(rayon * 0.01f, rayon * 20.f);
			sctx.camera.SetPosition({centre.x + dist * sinf(angle), centre.y + rayon * 0.7f,
									 centre.z + dist * cosf(angle)});
			sctx.camera.SetTarget(centre);
			sctx.ambientIntensity = 0.35f;
			// UNE lumiere, franche : le damier doit rester lisible, pas etre
			// noye dans l'ombrage.
			NkLightDesc L;
			L.type = NkLightType::NK_DIRECTIONAL;
			L.direction = {-0.4f, -0.8f, -0.45f};
			L.color = {1.f, 1.f, 1.f};
			L.intensity = 3.f;
			sctx.lights.PushBack(L);

			r3d->ResetFrame();
			r3d->BeginScene(sctx);
			NkDrawCall3D dc;
			dc.mesh = courant;
			if (mat)
				dc.material = mat->GetInstHandle();
			dc.roughness = 0.85f;
			dc.metallic = 0.f;
			r3d->Submit(dc);
			// -- ON NE FLUSHE PAS ICI, ET C'EST LE DEFAUT QUI A FAIT L'ECRAN VIDE --
			// `NkRenderer::Present()` execute le GRAPHE DE RENDU, et c'est le
			// graphe qui appelle `mRender3D->Flush(cmd)` DANS sa passe de
			// geometrie (NkRendererImpl.cpp:979). En le faisant moi-meme, je
			// vidais la liste de dessin HORS de toute passe : le travail partait
			// dans un tampon sans cible, et la passe du graphe ne trouvait plus
			// rien a peindre. Le 2D, lui, se dessine bien en direct
			// (`r2d->Begin/End`) -- c'est pour cela que le bandeau se peignait
			// pendant que l'objet restait invisible. Deux chemins differents,
			// une seule fenetre : le bandeau prouvait le dorsal, pas la scene.
			if (imagesTracees < 2u) {
				// LA PREMIERE IMAGE EST TRACEE APRES COUP, pas avant : dire
				// « je vais soumettre » ne prouve rien, dire « j'ai soumis, avec
				// cette poignee et ce materiau » situe le defaut de part et
				// d'autre du Flush.
				// LA POIGNEE, PAS LE POINTEUR. Dire « materiau present » parce que
				// `mat != nullptr` ne prouve rien : c'est `dc.material` que le
				// rendu consomme, et elle peut etre invalide sur un materiau
				// parfaitement construit.
				logger.Info("[demo-uv] image {0} : maillage {1}, poignee materiau {2}, vue {3}x{4}\n",
							imagesTracees, courant.IsValid() ? "valide" : "INVALIDE",
							dc.material.IsValid() ? "valide" : "INVALIDE", W, H);
				++imagesTracees;
			}
		}

		// -- LE BANDEAU ---------------------------------------------------------
		r2d->Begin(cmd, W, H);
		r2d->FillRect({0.f, 0.f, (float32)W, 96.f}, {0.04f, 0.33f, 0.37f, 0.92f});
		r2d->End();

		const uint32 blanc = NkColor(1.f, 1.f, 1.f, 1.f).ToUint32A();
		const uint32 orange = NkColor(0.97f, 0.60f, 0.16f, 1.f).ToUint32A();
		text->DrawText({14.f, 10.f}, deplie ? "[DEPLIE]  -  ESPACE pour revenir au brut"
											: "[BRUT]  -  ESPACE pour voir le depliage",
					   police, 18.f, orange);
		text->DrawText({14.f, 34.f}, ligne1, police, 15.f, blanc);
		text->DrawText({14.f, 54.f}, ligne2, police, 15.f, blanc);
		text->DrawText({14.f, 74.f}, ligne3, police, 15.f, blanc);
		// LE FICHIER MONTRE EST NOMME A L'ECRAN : sans cela, « ca marche » ne dit
		// pas SUR QUOI, et la demo pourrait montrer un objet pendant qu'on croit
		// en voir un autre.
		if (chemin)
			text->DrawText({(float32)W - 520.f, 10.f}, chemin, police, 13.f, blanc);
		if (deplie && !mDeplie.IsValid())
			text->DrawText({14.f, (float32)H - 30.f},
						   "aucune UV calculee : l'image affichee reste celle du brut", police, 15.f,
						   orange);

		renderer->Present();
		renderer->EndFrame();
	}

	NkRenderer::Destroy(renderer);
	NkDeviceFactory::Destroy(device);
	window.Close();
	return 0;
}
