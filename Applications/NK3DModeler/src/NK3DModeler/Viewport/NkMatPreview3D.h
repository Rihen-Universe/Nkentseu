#pragma once
// -----------------------------------------------------------------------------
// @File    NkMatPreview3D.h
// @Brief   APERCU DE MATERIAU RENDU PAR LE MOTEUR : une mini-scene de studio
//          (sol + objet) rendue hors ecran avec le VRAI pipeline, et publiee
//          comme une image d'interface.
//
// POURQUOI REMPLACER LE RENDU ANALYTIQUE
//   L'apercu etait calcule par formules, pixel par pixel (HostMatPreviewRender).
//   Assez juste pour un PBR ordinaire, mais il ne connaissait QUE cela : le
//   verre, l'emissif et le toon s'y affichaient comme des plastiques, puisque
//   aucun de leurs shaders n'etait execute. Passer par le moteur, c'est montrer
//   la matiere telle qu'elle sera rendue -- et non telle qu'une approximation
//   l'imagine. « Il faut aussi les vrais modeles » (Rihen, 13 aout 2026).
//
// COMMENT IL S'INSERE DANS LA FRAME
//   L'editeur possede la frame device et le command buffer ; ce module n'ouvre
//   donc PAS de frame a lui. Il rejoue ce qu'un BeginFrame ferait pour son
//   propre renderer (reconstruction du graphe, reset du pool d'UBO, upload des
//   materiaux), soumet ses deux draw calls, puis execute son graphe DANS le
//   command buffer qu'on lui passe. C'est exactement la mecanique de la vue 3D,
//   et elle impose une contrainte : tout cela doit se faire AVANT que la passe
//   backbuffer ne s'ouvre, une passe de rendu ne pouvant pas en contenir une
//   autre.
//
// CE QU'IL NE FAIT PAS
//   Il ne rend qu'UNE image par frame, celle du grand apercu du panneau. Les
//   vignettes des cartes du navigateur ne passent pas par ici : elles recoivent
//   une CAPTURE de cette meme image, figee au moment ou le materiau est
//   enregistre (choix de Rihen). Rendre soixante-quatre scenes pour des
//   vignettes de quarante pixels couterait cher pour un gain invisible.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Core/NkSceneContext.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Materials/NkMaterialCollection.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKGui/NkGuiRHIBackend.h"
#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"
// Pour l'identifiant de texture, partage avec le panneau (qui, lui, ne connait
// pas NKRenderer). Ce header n'apporte aucun type NKRenderer -- c'est sa regle.
#include "NK3DModeler/Viewport/NkDemo3DHost.h"

namespace nkentseu {
	namespace nk3d {
		namespace matprev {

			using namespace nkentseu::renderer;

			/// Identifiant de la texture d'apercu dans le backend d'interface.
			/// Declare dans NkDemo3DHost.h, que le PANNEAU inclut deja : il pose
			/// l'image sans avoir a connaitre NKRenderer. Distinct des 4400+
			/// (vignettes analytiques des cartes), qui coexistent tant que la
			/// capture a l'enregistrement n'est pas en place.
			constexpr uint32 kPreviewTexId = demo::kNkMatPreviewTexId;

			/// Les sept formes, dans l'ordre des valeurs serialisees `prevShape`.
			/// Les trois premieres viennent du moteur (NkMeshSystem en fournit deja
			/// les primitives) ; les quatre autres sont des modeles charges.
			enum class NkPrevMesh : int32 {
				Plan = 0,
				Sphere = 1,
				Cube = 2,
				Liquide = 3,
				Cheveux = 4,
				Tissu = 5,
				Tete = 6,
				Count = 7
			};

			struct NkMatPreviewState {
					NkRenderer *rd = nullptr;
					NkOffscreenTarget *rt = nullptr;
					uint32 w = 260, h = 150;
					bool tried = false;
					bool ok = false;
					const char *err = nullptr;
					/// Les sept maillages. Ceux qui restent invalides sont remplaces
					/// par la sphere : mieux vaut un apercu approximatif qu'un trou
					/// noir, et le journal dit lequel manque.
					NkMeshHandle mesh[(int32)NkPrevMesh::Count];
					NkMeshHandle meshSol;
					/// Le sol a son propre materiau, porteur du damier : sans lui il
					/// faudrait passer par les surcharges du draw call, qui ne savent
					/// pas echantillonner une texture.
					NkMaterial *matSol = nullptr;
					NkTexHandle texDamier;
			};

			inline NkMatPreviewState &St() {
				static NkMatPreviewState s;
				return s;
			}

			// ── LES QUATRE MODELES SCULPTES ─────────────────────────────────────
			// Charges A LA DEMANDE, pas au demarrage : ce sont trois megaoctets
			// chacun, et rien ne dit qu'on affichera un jour la forme « cheveux ».
			// Ralentir tous les demarrages pour des formes qu'on ne regarde pas
			// serait un mauvais marche.
			//
			// Le chemin suit celui des icones : relatif au dossier de travail, donc
			// l'application se lance depuis la racine du depot.
			inline NkMeshHandle ChargerModele(int32 forme) {
				NkMatPreviewState &s = St();
				auto *ms = s.rd ? s.rd->GetMeshSystem() : nullptr;
				if (!ms)
					return NkMeshHandle{};
				static const char *const kFichier[(int32)NkPrevMesh::Count] = {
					nullptr, nullptr, nullptr, "liquide", "cheveux", "tissu", "mascotte"};
				const char *nom =
					(forme >= 0 && forme < (int32)NkPrevMesh::Count) ? kFichier[forme] : nullptr;
				if (!nom)
					return NkMeshHandle{};
				const char *kDossiers[2] = {"Applications/NK3DModeler/data/previews/",
											"data/previews/"};
				for (int32 d = 0; d < 2; ++d) {
					NkString chemin = NkString(kDossiers[d]) + nom + ".obj";
					if (!NkFile::Exists(chemin.CStr()))
						continue;
					// `Import` fait tout : lecture OBJ et montage GPU. SANS les
					// materiaux du .mtl -- c'est LE materiau de l'utilisateur qu'on
					// vient juger, pas celui livre avec le modele.
					NkMeshHandle h = ms->Import(chemin, false);
					if (h.IsValid()) {
						const NkAABB &bb = ms->GetBounds(h);
						NkLog::Instance().Info("[apercu] modele '{0}' charge, boite {1}x{2}x{3}",
											   nom, bb.max.x - bb.min.x, bb.max.y - bb.min.y,
											   bb.max.z - bb.min.z);
						return h;
					}
					NkLog::Instance().Info("[apercu] modele '{0}' : import refuse", nom);
					return NkMeshHandle{};
				}
				NkLog::Instance().Info("[apercu] modele '{0}.obj' introuvable", nom);
				return NkMeshHandle{};
			}

			/// Transform qui POSE un modele dans le cadre : mis a l'echelle pour que
			/// sa plus grande dimension tienne, centre en X/Z, base sur le sol.
			/// Indispensable -- un modele exporte arrive a une echelle quelconque, et
			/// sans cela il serait soit microscopique, soit hors champ.
			inline NkMat4f CadrerModele(const NkAABB &bb, float32 hVoulue, float32 lMax) {
				const float32 dx = bb.max.x - bb.min.x;
				const float32 dy = bb.max.y - bb.min.y;
				const float32 dz = bb.max.z - bb.min.z;
				// DEUX CONTRAINTES, ET LA PLUS SEVERE GAGNE. Mettre a l'echelle sur
				// la plus grande dimension -- ce que je faisais -- donne un objet
				// minuscule des qu'il est profond ou large : c'est sa PROFONDEUR qui
				// decidait de sa hauteur a l'ecran. On vise donc une hauteur, et on
				// ne rentre que si la largeur le permet aussi (Rihen : « la mascotte
				// doit etre plus proche ET rester dans le champ »).
				const float32 large = dx > dz ? dx : dz;
				const float32 kH = (dy > 1e-6f) ? (hVoulue / dy) : 1.f;
				const float32 kL = (large > 1e-6f) ? (lMax / large) : kH;
				const float32 k = kH < kL ? kH : kL;
				const float32 cx = (bb.min.x + bb.max.x) * 0.5f;
				const float32 cz = (bb.min.z + bb.max.z) * 0.5f;
				// Translation D'ABORD, echelle ENSUITE (l'ordre compte) : le modele
				// est ramene sur l'origine, base au sol, puis mis a l'echelle.
				return NkMat4f::Scale({k, k, k}) * NkMat4f::Translate({-cx, -bb.min.y, -cz});
			}

			// ── CREATION ────────────────────────────────────────────────────────
			// Un SECOND renderer sur le MEME device : c'est deja ce que fait la vue
			// 3D, et c'est la seule facon d'avoir une scene independante sans ouvrir
			// une seconde pile GPU dans la fenetre -- ce que le depot interdit.
			inline bool Init(NkIDevice *device, uint32 w, uint32 h) {
				NkMatPreviewState &s = St();
				if (s.tried)
					return s.ok;
				s.tried = true;
				if (!device || !device->IsValid()) {
					s.err = "device partage absent";
					NkLog::Instance().Info("[apercu] ECHEC : {0}", s.err);
					return false;
				}
				s.w = w < 32u ? 32u : w;
				s.h = h < 32u ? 32u : h;

				NkRendererConfig cfg = NkRendererConfig::ForGame(device->GetApi(), s.w, s.h);
				// UNE SEULE CASCADE D'OMBRE : la scene tient dans trois unites.
				cfg.shadow.cascadeCount = 1;
				cfg.Enable(NK_SS_OFFSCREEN);
				// Ni SSAO ni bloom : un apercu montre une MATIERE. Un halo ou un
				// depot sombre au pied de l'objet changerait la lecture de la
				// rugosite, ce qui est precisement ce qu'on vient y juger.
				cfg.postProcess.ssao = false;
				cfg.postProcess.bloom = false;
				// ANTICRENELAGE : un apercu est une petite image FIXE, ou chaque
				// marche d'escalier se voit -- « le modele doit etre lisse »
				// (Rihen, 13 aout). FXAA lisse les contours pour presque rien, et
				// le SURECHANTILLONNAGE (rendu a 2x, reduit a l'affichage) fait le
				// reste : sur une image immobile, c'est ce qui distingue un rendu
				// propre d'un rendu d'apercu.
				cfg.postProcess.fxaa = true;
				s.rd = NkRenderer::Create(device, cfg);
				if (!s.rd) {
					s.err = "creation du renderer d'apercu refusee";
					NkLog::Instance().Info("[apercu] ECHEC : {0}", s.err);
					return false;
				}

				NkOffscreenDesc od;
				od.width = s.w;
				od.height = s.h;
				od.hdr = false;
				od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
				od.hasDepth = true;
				od.readable = true;
				// readback : c'est par la que la vignette d'une carte sera capturee
				// a l'enregistrement du materiau.
				od.readback = true;
				od.name = "NK3DModelerMatPreview";
				s.rt = s.rd->CreateOffscreen(od);
				if (!s.rt || !s.rt->IsValid()) {
					s.err = "cible hors ecran d'apercu refusee";
					NkLog::Instance().Info("[apercu] ECHEC : {0}", s.err);
					return false;
				}
				// Sans l'override, le graphe rendrait a la taille de la FENETRE dans
				// une cible bien plus petite.
				s.rd->SetRenderSizeOverride(s.w, s.h);
				if (auto *texLib = s.rd->GetTextures())
					s.rd->SetFinalColorTarget(texLib->GetRHIHandle(s.rt->GetColorResult()));

				// ── LES FORMES ──────────────────────────────────────────────────
				// Les trois primitives sont celles du moteur : les livrer en
				// fichiers ferait deux verites sur ce qu'est un cube.
				if (auto *ms = s.rd->GetMeshSystem()) {
					s.meshSol = ms->GetPlane();
					s.mesh[(int32)NkPrevMesh::Plan] = ms->GetPlane();
					// La sphere de l'apercu est PLUS FINE que la primitive commune : celle-ci
					// est taillee pour une scene ou elle fait quelques dizaines de pixels,
					// alors que l'apercu la montre en gros plan -- ses facettes s'y voient.
					s.mesh[(int32)NkPrevMesh::Sphere] = ms->CreateSphereMesh(64u, 96u);
					if (!s.mesh[(int32)NkPrevMesh::Sphere].IsValid())
						s.mesh[(int32)NkPrevMesh::Sphere] = ms->GetSphere();
					s.mesh[(int32)NkPrevMesh::Cube] = ms->GetCube();
				}

				// ── LE SOL EN DAMIER ────────────────────────────────────────────
				// Un sol UNI ne dit rien de la matiere posee dessus. Le damier,
				// lui, se reflete, se deforme dans le verre et se voit au travers
				// d'un objet transparent : c'est un instrument de mesure autant
				// qu'un fond, et c'est pour cela que Blender en met un (Rihen,
				// 13 aout : « est-ce possible que le sol soit totalement a grille »).
				//
				// La texture est GENEREE, pas chargee : un damier est deux boucles,
				// et un fichier de plus serait un fichier a livrer, a trouver et a
				// ne pas perdre.
				{
					static const uint32 kT = 256u; // taille de la texture
					static const uint32 kCase = 8u; // cote d'une case : 32 cases par cote
					static uint8 px[kT * kT * 4u];
					for (uint32 y = 0; y < kT; ++y)
						for (uint32 x = 0; x < kT; ++x) {
							const bool clair = (((x / kCase) ^ (y / kCase)) & 1u) != 0u;
							// Contraste FRANC, comme celui de Blender : les cases
							// sombres etaient trop proches des claires et le damier se
							// lisait a peine (Rihen : « les cases noires doivent etre
							// plus foncees »). C'est lui qui rend lisibles le reflet
							// et la refraction -- un damier qu'on ne voit pas ne
							// mesure rien.
							const uint8 v = clair ? 78u : 26u;
							uint8 *o = px + (y * kT + x) * 4u;
							o[0] = o[1] = o[2] = v;
							o[3] = 255u;
						}
					if (auto *tl = s.rd->GetTextures()) {
						NkTextureCreateDesc td;
						td.pixels = px;
						td.width = kT;
						td.height = kT;
						td.srgb = true; // c'est une COULEUR, pas un parametre
						td.genMips = true; // sinon le damier fourmille en profondeur
						td.mipLevels = 0;
						td.debugName = "NkMatPreviewDamier";
						s.texDamier = tl->Create(td);
					}
					if (auto *matS = s.rd->GetMaterials()) {
						s.matSol = NkMaterial::Create(matS, NkMaterialType::NK_PBR_METALLIC);
						if (s.matSol) {
							s.matSol->SetAlbedo({1.f, 1.f, 1.f}, 1.f)
								->SetMetallic(0.f)
								->SetRoughness(0.94f);
							if (auto *tl = s.rd->GetTextures())
								if (s.texDamier.IsValid())
									s.matSol->SetAlbedoMap(s.texDamier);
						}
					}
				}
				s.ok = true;
				NkLog::Instance().Info(
					"[apercu] pret : {0}x{1}, meshes plan={2} sphere={3} cube={4}", s.w, s.h,
					s.mesh[(int32)NkPrevMesh::Plan].IsValid() ? 1 : 0,
					s.mesh[(int32)NkPrevMesh::Sphere].IsValid() ? 1 : 0,
					s.mesh[(int32)NkPrevMesh::Cube].IsValid() ? 1 : 0);
				return true;
			}

			/// LE RENDERER DE L'APERCU, pour que l'hote y construise ses instances
			/// de materiau. Nul tant qu'Init n'a pas reussi.
			inline NkRenderer *Renderer() {
				return St().ok ? St().rd : nullptr;
			}

			/// Redimensionne la cible si le panneau a change de largeur. Rendre a la
			/// taille exacte d'affichage evite l'etirement ET le flou.
			inline void EnsureSize(uint32 w, uint32 h) {
				NkMatPreviewState &s = St();
				if (!s.ok || !s.rt)
					return;
				if (w < 32u)
					w = 32u;
				if (h < 32u)
					h = 32u;
				if (w == s.w && h == s.h)
					return;
				if (!s.rt->Resize(w, h))
					return;
				s.w = w;
				s.h = h;
				s.rd->SetRenderSizeOverride(w, h);
				if (auto *texLib = s.rd->GetTextures())
					s.rd->SetFinalColorTarget(texLib->GetRHIHandle(s.rt->GetColorResult()));
			}

			// ── UNE IMAGE ───────────────────────────────────────────────────────
			// `mat` est l'INSTANCE MOTEUR du materiau du projet : c'est elle qui
			// porte le type (verre, toon, emissif...) et donc le shader. Passer par
			// elle est tout l'interet de ce module -- les surcharges par draw call
			// ne decrivent qu'un PBR.
			// `matiere` porte les SURCHARGES du materiau (teinte, opacite, metal,
			// rugosite, vernis, diffusion). Elles ne sont pas dans l'instance : dans
			// ce moteur, les facades de materiau n'ecrivent que l'etat, et c'est le
			// draw call qui porte la matiere jusqu'au shader. L'hote les remplit avec
			// la MEME fonction que la vue 3D (HostMatSlotToDC) -- sans quoi l'apercu
			// montrerait autre chose que la scene.
			// `avecSol` : le grand apercu pose l'objet sur un damier -- c'est un
			// instrument de mesure, il se reflete et se voit au travers. Une
			// VIGNETTE de carte, elle, s'en passe : a 40 pixels le damier devient
			// un bruit qui mange la sphere, et une liste doit se lire d'un coup
			// d'oeil (Rihen, 14 aout : « elle ne doit pas avoir le grillage »).
			inline void RenderOne(NkICommandBuffer *cmd, NkMatInstHandle mat, int32 shape,
								  uint32 w, uint32 h, float32 time,
								  const NkDrawCall3D &matiere, bool avecSol = true) {
				NkMatPreviewState &s = St();
				if (!s.ok || !cmd || !s.rd)
					return;
				EnsureSize(w, h);
				auto *r3d = s.rd->GetRender3D();
				if (!r3d) {
					static bool sDit = false;
					if (!sDit) {
						sDit = true;
						NkLog::Instance().Info("[apercu] pas de Render3D");
					}
					return;
				}
				{
					static int32 sVu = -2;
					if (sVu != shape) {
						sVu = shape;
						NkLog::Instance().Info("[apercu] rendu forme={0} taille={1}x{2}", shape,
											   s.w, s.h);
					}
				}
				// L'editeur possede la frame : on rejoue ce que ferait un BeginFrame
				// pour NOTRE renderer, pas un de plus.
				s.rd->FlushGraphRebuilds();
				r3d->ResetFrame();
				if (auto *mc = s.rd->GetMaterialCollection())
					mc->Upload();

				// ── CAMERA DE STUDIO ────────────────────────────────────────────
				// Fixe et legerement plongeante : on voit le dessus de l'objet et sa
				// pose sur le sol. L'aspect suit la LARGEUR de l'image -- c'est ce
				// qui elargit le champ sans grossir l'objet quand le panneau
				// s'agrandit.
				// LA DISTANCE EST CALCULEE, PAS CHOISIE. Une position figee marchait
				// pour la sphere et coupait la robe : la camera visait y=0.25 alors
				// que les objets montent a 1.55, et le haut sortait du cadre (Rihen :
				// « la camera de la previz ne permet pas de bien voir les elements »).
				//
				// On cadre une boite de kCadreH de haut sur kCadreL de large, en
				// prenant la contrainte la PLUS SEVERE des deux : quand le panneau
				// est etroit, c'est la largeur qui decide, et reculer est le seul
				// moyen de tout garder dans le champ.
				static const float32 kCadreH = 1.85f; // hauteur a garder visible
				static const float32 kCadreL = 2.10f; // largeur a garder visible
				static const float32 kFovY = 32.f;
				const float32 aspect = (float32)s.w / (float32)s.h;
				const float32 demiFov = kFovY * 0.5f * 3.14159265f / 180.f;
				const float32 tanV = math::NkTan(demiFov);
				const float32 dH = (kCadreH * 0.5f) / tanV;			  // pour la hauteur
				const float32 dL = (kCadreL * 0.5f) / (tanV * aspect); // pour la largeur
				const float32 dist = (dH > dL ? dH : dL) * 1.12f;	  // + une marge d'air

				NkCamera3DData camData;
				camData.up = {0.f, 1.f, 0.f};
				camData.fovY = kFovY;
				camData.aspect = aspect;
				camData.nearPlane = 0.05f;
				camData.farPlane = 60.f;
				NkCamera3D cam(camData);
				// Visee A MI-HAUTEUR de l'objet, et non pres du sol : c'est ce qui le
				// centre au lieu de le pousser vers le haut du cadre. Legere plongee
				// (la camera est un peu plus haute que sa cible) pour qu'on voie la
				// pose sur le sol -- sans elle, l'objet flotte.
				const float32 yCible = kCadreH * 0.42f;
				cam.SetPosition({0.f, yCible + dist * 0.22f, dist});
				cam.SetTarget({0.f, yCible, 0.f});

				NkSceneContext sctx;
				sctx.camera = cam;
				sctx.time = time;
				sctx.ambientIntensity = 0.22f;
				// Deux lumieres : une CLE qui sculpte, un APPOINT froid a l'oppose
				// pour que la face d'ombre garde de la matiere. Sans l'appoint, une
				// moitie de la sphere est noire et la rugosite ne s'y lit plus.
				NkLightDesc key;
				key.type = NkLightType::NK_DIRECTIONAL;
				key.direction = {-0.45f, -0.78f, -0.44f};
				key.color = {1.f, 0.97f, 0.92f};
				key.intensity = 3.1f;
				sctx.lights.PushBack(key);
				NkLightDesc fill;
				fill.type = NkLightType::NK_DIRECTIONAL;
				fill.direction = {0.62f, -0.35f, 0.70f};
				fill.color = {0.72f, 0.80f, 1.f};
				fill.intensity = 0.9f;
				sctx.lights.PushBack(fill);

			// LE CIEL NE SE DESSINE QUE POUR LE GRAND APERCU. Une vignette de carte
				// le recevait en fond uni gris-violet, alors que les autres cartes ont
				// un damier : deux fonds differents dans une meme liste se remarquent
				// aussitot (Rihen, 14 aout). Sans ciel, le fond garde la couleur de
				// nettoyage -- et son ALPHA, ce qui permet de composer le damier apres
				// coup. L'IBL, lui, reste actif : c'est l'eclairage, pas le decor.
				r3d->SetSkyboxEnabled(avecSol);
				r3d->BeginScene(sctx);

				// ── LE SOL ──────────────────────────────────────────────────────
				// Il recoit les ombres sans en projeter : un sol qui alimente la
				// cascade la gaspille pour une surface qu'on ne voit qu'a plat.
				// ── LE FOND DE LA VIGNETTE : UN PANNEAU DAMIER ─────────────────
				// Pas de sol en perspective pour une icone de liste, mais pas de
				// fond uni non plus : « capturer sans fond et mettre sur le meme
				// fond que le materiau de droite » (Rihen, 14 aout). Un panneau
				// VERTICAL derriere la sphere donne exactement le damier plat du
				// rendu analytique -- et il le donne DANS le rendu, sans dependre
				// de ce que le post-traitement laisse de l'alpha. Deviner le fond
				// apres coup sur la couleur des pixels ne marchait pas : c'est ce
				// que je venais d'essayer.
				if (!avecSol && s.meshSol.IsValid()) {
					NkDrawCall3D fond;
					fond.mesh = s.meshSol;
					// Le plan du moteur est horizontal : un quart de tour sur X le
					// dresse face a la camera.
					fond.transform = NkMat4f::Translate({0.f, 0.9f, -1.6f}) *
									 NkMat4f::RotationX(NkAngle::FromRad(1.5707963f)) *
									 NkMat4f::Scale({7.f, 1.f, 7.f});
					fond.aabb = {{-4.f, -3.f, -1.7f}, {4.f, 4.f, -1.5f}};
					if (s.matSol)
						fond.material = s.matSol->GetInstHandle();
					fond.tint = {1.f, 1.f, 1.f};
					fond.roughness = 1.f;
					fond.metallic = 0.f;
					fond.castShadow = false;
					fond.receiveShadow = false; // un fond de vignette ne porte pas d'ombre
					r3d->Submit(fond);
				}
				if (avecSol && s.meshSol.IsValid()) {
					NkDrawCall3D dc;
					dc.mesh = s.meshSol;
					dc.transform = NkMat4f::Scale({6.f, 1.f, 6.f});
					dc.aabb = {{-6.f, -0.01f, -6.f}, {6.f, 0.01f, 6.f}};
					// Le materiau porte le damier ; la teinte reste blanche pour ne
					// pas le salir (elle MULTIPLIE l'echantillon).
					if (s.matSol)
						dc.material = s.matSol->GetInstHandle();
					dc.tint = {1.f, 1.f, 1.f};
					dc.roughness = 0.94f;
					dc.metallic = 0.f;
					dc.castShadow = false;
					r3d->Submit(dc);
				}

				// ── L'OBJET ─────────────────────────────────────────────────────
				const int32 si = (shape < 0 || shape >= (int32)NkPrevMesh::Count) ? 1 : shape;
				// CHARGEMENT A LA DEMANDE, une seule tentative : un modele manquant
				// ne doit pas etre recherche soixante fois par seconde. Le temoin est
				// l'essai, pas le succes.
				static bool sEssaye[(int32)NkPrevMesh::Count] = {};
				if (!s.mesh[si].IsValid() && !sEssaye[si]) {
					sEssaye[si] = true;
					s.mesh[si] = ChargerModele(si);
				}
				NkMeshHandle mh = s.mesh[si];
				bool modeleCharge = true;
				if (!mh.IsValid()) {
					mh = s.mesh[(int32)NkPrevMesh::Sphere]; // repli : jamais de trou noir
					modeleCharge = false;
				}
				if (mh.IsValid()) {
					NkDrawCall3D dc;
					dc.mesh = mh;
					dc.material = mat; // LE VRAI MATERIAU, donc le vrai shader
					// Le PLAN se couche et s'elargit ; les autres gardent leur echelle.
				// LE CUBE TOURNE DE 45 DEGRES (Rihen) : de face il ne montre qu'un
					// carre plat, ou l'on ne lit ni l'arete ni le passage d'une face a
					// l'autre -- c'est pourtant la que se juge un reflet.
					if (si == (int32)NkPrevMesh::Plan)
						dc.transform = NkMat4f::Translate({0.f, 0.02f, 0.f}) *
									   NkMat4f::Scale({1.5f, 1.f, 1.5f});
					else if (si == (int32)NkPrevMesh::Cube)
						dc.transform = NkMat4f::Translate({0.f, 0.5f, 0.f}) *
									   NkMat4f::RotationY(NkAngle::FromRad(0.7853982f)); // 45 deg
					else if (modeleCharge) {
						// UN MODELE SCULPTE ARRIVE A UNE ECHELLE QUELCONQUE -- celle
						// du logiciel qui l'a exporte. On le CADRE depuis sa boite :
						// sans cela il serait microscopique ou hors champ, et le
						// regler a la main modele par modele serait a refaire au
						// premier reexport.
						auto *ms = s.rd->GetMeshSystem();
						// Hauteur visee 1.55, largeur bornee a 1.9 : l'objet occupe
						// franchement le cadre sans en sortir.
						NkMat4f cadre = ms ? CadrerModele(ms->GetBounds(mh), 1.55f, 1.9f)
										   : NkMat4f::Translate({0.f, 0.5f, 0.f});
						// LES CHEVEUX DE TROIS QUARTS, comme le cube : une meche vue
						// de face se lit comme un trait, alors que sa courbure est
						// justement ce qui montre le reflet (Rihen).
						if (si == (int32)NkPrevMesh::Cheveux)
							cadre = cadre * NkMat4f::RotationY(NkAngle::FromRad(0.7853982f));
						dc.transform = cadre;
					} else
						dc.transform = NkMat4f::Translate({0.f, 0.5f, 0.f});
					dc.aabb = {{-1.6f, -0.1f, -1.6f}, {1.6f, 1.6f, 1.6f}};
					// LA MATIERE, telle que la vue 3D l'appliquerait.
					dc.tint = matiere.tint;
					dc.alpha = matiere.alpha;
					dc.metallic = matiere.metallic;
					dc.roughness = matiere.roughness;
					dc.clearcoat = matiere.clearcoat;
					dc.clearcoatRough = matiere.clearcoatRough;
					dc.subsurface = matiere.subsurface;
					dc.subsurfaceColor = matiere.subsurfaceColor;
					// PAS D'OMBRE SUR UNE VIGNETTE (Rihen, 14 aout) : a quarante pixels
					// elle salit le fond sans rien apprendre de la matiere. Le grand
					// apercu la garde -- une ombre portee y dit la pose sur le sol.
					dc.castShadow = avecSol;
					dc.receiveShadow = avecSol;
					r3d->Submit(dc);
				}

				// Presenter = executer le graphe DANS le command buffer de l'editeur.
				if (auto *graph = s.rd->GetRenderGraph())
					graph->Execute(cmd);
			}

			/// Publie la texture d'apercu aupres du backend d'interface, sous
			/// `kPreviewTexId`. A appeler apres le rendu, comme la vue 3D.
			inline void RegisterInto(void *guiBackend) {
				NkMatPreviewState &s = St();
				if (!s.ok || !s.rt || !guiBackend)
					return;
				auto *b = (nkentseu::nkgui::NkGuiRHIBackend *)guiBackend;
				if (auto *texLib = s.rd->GetTextures())
					b->RegisterTexture(kPreviewTexId, texLib->GetRHIHandle(s.rt->GetColorResult()));
			}

			/// Lit la derniere image rendue (pour la vignette figee d'une carte).
			inline bool Readback(uint8 *dst) {
				NkMatPreviewState &s = St();
				return s.ok && s.rt && dst && s.rt->ReadbackPixels(dst, s.w * 4u);
			}

		} // namespace matprev
	} // namespace nk3d
} // namespace nkentseu
