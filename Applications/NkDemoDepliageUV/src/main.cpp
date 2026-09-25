// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Applications/NkDemoDepliageUV/src/main.cpp
// -----------------------------------------------------------------------------
// NkDemoDepliageUV — LE DEPLIAGE UV, A L'ECRAN.
//
// CE QU'ELLE MONTRE : une sphere couverte d'un DAMIER. Le damier est le seul
// juge honnete d'un depliage : ses carreaux sont carres et de meme taille dans
// l'atlas, donc tout ce qu'on voit d'etire, d'ecrase ou de hache vient du
// depliage, pas du dessin.
//    [BRUT]    les UV spheriques naturelles -- la reference ;
//    [DEPLIE]  les UV calculees par `NkUVUnwrapAuto` (coutures automatiques,
//              de-soudure, LSCM).
// ESPACE bascule. La difference doit se VOIR.
//
// -- POURQUOI UNE SPHERE CONSTRUITE EN CODE, ET PAS UN OBJET GENERE ------------
// TRANCHE LE 26/09 : la demo FABRIQUE son sujet, elle ne le charge pas. La
// version precedente cherchait `logs_genia3d/crea/...`, un dossier NON VERSIONNE
// qui n'existe que dans l'arbre ou il a ete produit. Chez Rodolf elle ne trouvait
// rien. C'est le troisieme defaut de cette forme en une journee -- `NkRihenLogo.h`
// cassait trois applications chez tout le monde sauf son auteur.
// UNE DEMO DONT L'ACTIF N'EST PAS DANS LE DEPOT N'EST PAS LIVRABLE.
// Et la sphere n'est pas un pis-aller : c'est une surface FERMEE, de genre 0,
// aux sommets SOUDES -- exactement ce qu'un maillage importe presente au
// deplieur. Elle exerce la chaine entiere, et elle est identique partout.
//
// -- TROIS CHEMINS DANS UNE SEULE FENETRE, TROIS TEMOINS -----------------------
// Cette fenetre melange trois piles qui ne se ressemblent pas :
//    (1) rectangles `NkRender2D`   -- enregistres en direct dans le tampon ;
//    (2) texte                     -- par `NkOverlayRenderer`, dans sa passe ;
//    (3) geometrie 3D              -- par le GRAPHE de rendu, passe `Geometry`.
// J'ai deja paye deux fois de les confondre : « le bandeau est peint donc le
// dorsal marche » ne dit rien de (2) ni de (3). Chaque chemin a donc son temoin,
// et chaque temoin se mute seul. Un temoin qui couvre deux chemins ne dira
// jamais lequel a lache.
//
// -- CE QUI PROUVERAIT QUE C'EST CASSE (a chercher, pas a admirer) -------------
//   * le carre MAGENTA en bas a gauche manque      -> chemin (1) mort ;
//   * le texte manque alors que le carre est la    -> chemin (2) mort ;
//   * la sphere GRISE de gauche manque             -> chemin (3) mort, et le
//     defaut n'est pas dans mon maillage : cette sphere-la est construite par le
//     MOTEUR, sans materiau de moi ;
//   * ma sphere damier manque alors que la grise est la -> le defaut est dans
//     MON maillage ou MON materiau, pas dans la scene ;
//   * [BRUT] et [DEPLIE] donnent la meme image     -> les UV calculees
//     n'arrivent pas jusqu'au GPU ;
//   * le damier est un confetti de carreaux sans continuite -> le depliage a
//     eclate le maillage en un ilot par triangle. C'est l'ETAT MESURE
//     aujourd'hui, c'est attendu, et c'est ce que la strategie de coupe doit
//     corriger.
//
// Le journal `logs/app.log` (lignes `[demo-uv]`) dit ce que la demo CROIT
// dessiner. Lancer depuis la RACINE de l'arbre.
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
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Mesh/NkUVUnwrap.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Text/NkTextRenderer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace nkentseu;
using namespace nkentseu::renderer;

static void ConfigureAppData(NkAppData &d) {
	d.appName = "NkDemoDepliageUV";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

// -- LE DAMIER, GENERE ---------------------------------------------------------
// 16 x 16 carreaux sur 512 px, deux gris bien separes, plus un lisere orange
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
				b = 40;
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

// -- LA SPHERE, CONSTRUITE EN CODE ---------------------------------------------
// Sommets SOUDES le long des paralleles, comme un import : c'est justement la
// condition que le deplieur doit affronter (coins soudes, surface fermee).
static void FabriquerSphere(NkVector<NkVertex3D> &v, NkVector<uint32> &idx, uint32 piles,
							uint32 tranches, float32 rayon) {
	v.Clear();
	idx.Clear();
	for (uint32 i = 0; i <= piles; ++i) {
		const float32 phi = 3.14159265f * (float32)i / (float32)piles;
		for (uint32 j = 0; j <= tranches; ++j) {
			const float32 th = 6.28318531f * (float32)j / (float32)tranches;
			NkVertex3D p{};
			p.normal = {sinf(phi) * cosf(th), cosf(phi), sinf(phi) * sinf(th)};
			p.pos = {p.normal.x * rayon, p.normal.y * rayon, p.normal.z * rayon};
			// UV SPHERIQUES : la reference de [BRUT]. Elles sont propres, donc si
			// [DEPLIE] est pire, c'est mesurable a l'oeil nu.
			p.uv = {(float32)j / (float32)tranches, (float32)i / (float32)piles};
			p.color = 0xFFFFFFFFu;
			v.PushBack(p);
		}
	}
	for (uint32 i = 0; i < piles; ++i) {
		for (uint32 j = 0; j < tranches; ++j) {
			const uint32 a = i * (tranches + 1u) + j;
			const uint32 b = a + tranches + 1u;
			idx.PushBack(a);
			idx.PushBack(b);
			idx.PushBack(a + 1u);
			idx.PushBack(a + 1u);
			idx.PushBack(b);
			idx.PushBack(b + 1u);
		}
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
	NkOverlayRenderer *overlay = renderer->GetOverlay();
	NkMeshSystem *meshes = renderer->GetMeshSystem();
	NkMaterialSystem *mats = renderer->GetMaterials();
	NkFontHandle police = text ? text->GetDefaultFont() : NkFontHandle{};

	static uint8 damier[512 * 512 * 4];
	FabriquerDamier(damier, 512u, 16u);
	NkTextureCreateDesc td;
	td.pixels = damier;
	td.width = 512;
	td.height = 512;
	td.srgb = true;
	td.debugName = "damier_uv";
	NkTexHandle texDamier = textures->Create(td);

	// -- LE SUJET : ma sphere, brute puis depliee -------------------------------
	NkVector<NkVertex3D> sv;
	NkVector<uint32> si;
	FabriquerSphere(sv, si, 24u, 32u, 1.f);

	NkMeshHandle mBrut, mDeplie;
	{
		NkMeshDesc md;
		md.layout = renderer::NkVertexLayout::Default3D();
		md.vertices = sv.Data();
		md.vertexCount = (uint32)sv.Size();
		md.indices = si.Data();
		md.indexCount = (uint32)si.Size();
		mBrut = meshes->Create(md);
	}

	char ligne1[256] = {0}, ligne2[256] = {0}, ligne3[256] = {0};
	{
		NkEditMesh em;
		em.BuildFromIndexed(sv.Data(), (uint32)sv.Size(), si.Data(), (uint32)si.Size(), false);
		NkUVUnwrapParams pr;
		pr.packIslands = true;
		pr.packMargin = 0.02f;
		NkUVResult res;
		NkUVAutoBilan bilan;
		nkentseu::NkChrono chrono;
		const bool ok = NkUVUnwrapAuto(em, pr, res, &bilan);
		const double ms = chrono.Elapsed().ToMilliseconds();
		if (!ok) {
			snprintf(ligne1, sizeof(ligne1), "DEPLIAGE REFUSE : %s", NkUVRefusName(res.refus));
			snprintf(ligne2, sizeof(ligne2), "euler trouve = %d   coins soudes = %u   (%.0f ms)",
					 res.refusEuler, res.weldedCorners, ms);
			snprintf(ligne3, sizeof(ligne3), "ce n'est pas une panne : le module refuse et dit pourquoi");
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
			const float32 tpi =
				res.islandCount ? (float32)res.distortion.triCount / (float32)res.islandCount : 0.f;
			snprintf(ligne1, sizeof(ligne1), "DEPLIE en %.0f ms   ilots = %u   %.1f triangle(s)/ilot", ms,
					 res.islandCount, (double)tpi);
			snprintf(ligne2, sizeof(ligne2), "aire moy %.3f max %.1f   angle moy %.1f max %.1f deg",
					 (double)res.distortion.areaMean, (double)res.distortion.areaMax,
					 (double)res.distortion.angleMean, (double)res.distortion.angleMax);
			snprintf(ligne3, sizeof(ligne3), "recouvrement %u paire(s)   coutures %u/%u retrouvees",
					 bilan.pairesRecouvrement, bilan.couturesRetrouvees, bilan.coutures);
		}
	}

	// -- LE TEMOIN DU CHEMIN 3D : une sphere DU MOTEUR, sans materiau de moi ----
	// Elle separe « la scene ne se peint pas » de « MON maillage ne se peint
	// pas ». Si elle apparait et que la mienne non, le defaut est chez moi ; si
	// aucune des deux n'apparait, il est dans la scene ou la camera.
	NkMeshHandle mTemoin = meshes->CreateSphereMesh(16u, 24u);

	NkMaterial *mat = NkMaterial::Create(mats, NkMaterialType::NK_PBR_METALLIC);
	if (mat && texDamier.IsValid())
		mat->SetTexture("albedo_map", texDamier);

	logger.Info("[demo-uv] --- ce que la demo croit dessiner ---\n");
	logger.Info("[demo-uv] sujet        : sphere construite en code, {0} sommets {1} indices\n",
				(uint32)sv.Size(), (uint32)si.Size());
	logger.Info("[demo-uv] police       : {0}\n", police.IsValid() ? "VALIDE" : "INVALIDE");
	logger.Info("[demo-uv] surcouche    : {0}\n", overlay ? "presente" : "NULLE -- aucun texte possible");
	logger.Info("[demo-uv] damier       : {0}\n", texDamier.IsValid() ? "VALIDE" : "INVALIDE");
	logger.Info("[demo-uv] materiau     : {0}\n", mat ? "cree" : "NUL");
	logger.Info("[demo-uv] maillage brut: {0}   deplie: {1}   temoin moteur: {2}\n",
				mBrut.IsValid() ? "VALIDE" : "INVALIDE", mDeplie.IsValid() ? "VALIDE" : "absent",
				mTemoin.IsValid() ? "VALIDE" : "INVALIDE");
	logger.Info("[demo-uv] bandeau L1   : {0}\n", ligne1);
	logger.Info("[demo-uv] bandeau L2   : {0}\n", ligne2);
	logger.Info("[demo-uv] bandeau L3   : {0}\n", ligne3);

	bool deplie = false;
	float32 angle = 0.f;
	bool running = true;
	uint32 tracees = 0u;
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

		angle += 0.004f;
		if (!renderer->BeginFrame())
			continue;
		NkICommandBuffer *cmd = renderer->GetCmd();
		const uint32 W = window.GetSize().x, H = window.GetSize().y;

		// -- (3) LA SCENE 3D ----------------------------------------------------
		// On ne flushe PAS : c'est le graphe qui appelle `Flush` dans sa passe
		// `Geometry` (NkRendererImpl.cpp:979). Flusher ici viderait la liste de
		// dessin hors de toute passe -- la faute d'hier.
		const NkMeshHandle courant = (deplie && mDeplie.IsValid()) ? mDeplie : mBrut;
		NkSceneContext sctx;
		sctx.camera.SetFOV(50.f);
		sctx.camera.SetAspect(W, H ? H : 1u);
		sctx.camera.SetNearFar(0.05f, 60.f);
		sctx.camera.SetPosition({3.2f * sinf(angle), 1.2f, 3.2f * cosf(angle)});
		sctx.camera.SetTarget({0.f, 0.f, 0.f});
		sctx.ambientIntensity = 0.35f;
		NkLightDesc L;
		L.type = NkLightType::NK_DIRECTIONAL;
		L.direction = {-0.4f, -0.8f, -0.45f};
		L.color = {1.f, 1.f, 1.f};
		L.intensity = 3.f;
		sctx.lights.PushBack(L);

		r3d->ResetFrame();
		r3d->BeginScene(sctx);

		// -- LA BOITE ENGLOBANTE N'EST PAS UNE OPTION ---------------------------
		// VOILA POURQUOI RIEN NE SE PEIGNAIT. `NkRender3D::Submit` fait un culling
		// camera sur `dc.aabb`, et `NkAABB` naît INVERSEE :
		//     min = {+1e30, +1e30, +1e30}   max = {-1e30, -1e30, -1e30}
		// Une boite vide n'est jamais visible : chaque soumission etait REJETEE,
		// en silence, sans un avertissement. Le maillage etait valide, le
		// materiau valide, la scene ouverte, le graphe complet avec ses 22
		// passes -- et la liste de dessin restait vide parce que le culling avait
		// tout ecarte avant.
		// Un champ dont la valeur par defaut est « rien n'est visible » se
		// comporte comme un interrupteur eteint qu'aucun appelant ne voit.
		auto boite = [](const NkVec3f &c, float32 r) {
			NkAABB b;
			b.min = {c.x - r, c.y - r, c.z - r};
			b.max = {c.x + r, c.y + r, c.z + r};
			return b;
		};

		NkDrawCall3D dc;
		dc.mesh = courant;
		if (mat)
			dc.material = mat->GetInstHandle();
		dc.transform = NkMat4f::Translate({1.3f, 0.f, 0.f});
		// LE NEGATIF, DANS LA DEMO ELLE-MEME. `NK_DEMO_UV_SANS_BOITE=1` laisse la
		// boite a sa valeur par defaut -- celle du defaut d'hier. Le compteur de
		// culling doit alors passer de « ecartes=0 » a « ecartes=2 », et l'ecran
		// redevenir vide. Un temoin qui ne peut pas rougir ne prouve rien : celui-ci
		// se retourne d'une variable d'environnement.
		const bool sansBoite = (std::getenv("NK_DEMO_UV_SANS_BOITE") != nullptr);
		if (!sansBoite)
			dc.aabb = boite({1.3f, 0.f, 0.f}, 1.15f); // EN MONDE, comme le dit le champ
		dc.roughness = 0.85f;
		r3d->Submit(dc);
		// LE TEMOIN 3D, a cote, SANS materiau : un defaut de materiau ne peut pas
		// les eteindre tous les deux.
		if (mTemoin.IsValid()) {
			NkDrawCall3D t;
			t.mesh = mTemoin;
			t.transform = NkMat4f::Translate({-1.3f, 0.f, 0.f});
			if (!sansBoite)
				t.aabb = boite({-1.3f, 0.f, 0.f}, 1.15f);
			t.tint = {0.75f, 0.75f, 0.78f};
			t.roughness = 0.6f;
			r3d->Submit(t);
		}

		// -- (1) LE TEMOIN DU RECTANGLE ----------------------------------------
		r2d->Begin(cmd, W, H);
		r2d->FillRect({0.f, 0.f, (float32)W, 96.f}, {0.04f, 0.33f, 0.37f, 0.92f});
		// Carre MAGENTA en bas a gauche : il ne sert qu'a temoigner du chemin (1).
		r2d->FillRect({16.f, (float32)H - 56.f, 40.f, 40.f}, {1.f, 0.f, 1.f, 1.f});
		// Reglure SOUS le texte : si elle apparait et que le texte non, c'est le
		// chemin (2) qui a lache, pas la position ni la couleur.
		r2d->FillRect({12.f, 52.f, 360.f, 2.f}, {1.f, 1.f, 1.f, 0.35f});
		r2d->End();

		// -- (2) LE TEXTE, PAR LA SURCOUCHE ------------------------------------
		// `NkOverlayRenderer` est le chemin qu'une application qui affiche
		// vraiment du texte emprunte (cf. NKARDemo) : il ouvre sa passe, dessine,
		// la ferme. `NkTextRenderer::DrawText` appele en vrac n'etait rattache a
		// aucune passe -- meme faute que le `Flush` 3D d'hier, sur une autre pile.
		if (overlay) {
			overlay->BeginOverlay(cmd, W, H);
			overlay->DrawText({14.f, 10.f}, "%s",
							  deplie ? "[DEPLIE]  -  ESPACE pour revenir au brut"
									 : "[BRUT]  -  ESPACE pour voir le depliage");
			overlay->DrawText({14.f, 32.f}, "%s", ligne1);
			overlay->DrawText({14.f, 54.f}, "%s", ligne2);
			overlay->DrawText({14.f, 76.f}, "%s", ligne3);
			overlay->DrawText({16.f, (float32)H - 76.f},
							  "temoin (1) carre magenta  |  temoin (3) sphere grise a gauche");
			overlay->EndOverlay();
		}

		if (tracees < 2u) {
			// APRES COUP, et par chemin. « je vais dessiner » ne prouve rien.
			logger.Info("[demo-uv] image {0} : 3D scene_ouverte={1} maillage={2} materiau={3} | "
						"2D rectangles emis | texte par surcouche={4} | vue {5}x{6}\n",
						tracees, r3d->IsInScene() ? "oui" : "NON",
						courant.IsValid() ? "valide" : "INVALIDE",
						dc.material.IsValid() ? "valide" : "INVALIDE", overlay ? "oui" : "NON", W, H);
			// LE CHIFFRE QUI AURAIT TOUT DIT EN UNE LIGNE, et que je n'avais pas
			// demande : combien de soumissions ont SURVECU au culling. « soumis 2,
			// ecartes 2 » nomme le defaut sans rien d'autre.
			const NkRender3D::NkCullStats &cs = r3d->GetCullStats();
			logger.Info("[demo-uv] image {0} : culling -> soumis={1} ecartes={2}\n", tracees,
						cs.opaqueSubmitted, cs.opaqueCulled);
			++tracees;
		}

		renderer->Present();
		renderer->EndFrame();
	}

	NkRenderer::Destroy(renderer);
	NkDeviceFactory::Destroy(device);
	window.Close();
	return 0;
}
