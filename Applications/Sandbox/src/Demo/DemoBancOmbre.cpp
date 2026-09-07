// =============================================================================
// DemoBancOmbre.cpp — BANC D'OMBRE ET DE CIEL, entierement pilote par
// l'environnement, pour trancher DEUX questions sans un seul clic.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ── POURQUOI CE FICHIER EXISTE ──────────────────────────────────────────────
// Le 2026-09-06, le grain que Rodolf voit sur ses modeles a ete mesure sur ses
// captures. Deux faits en sont sortis :
//   * le soudage des sommets n'y peut RIEN (les normales coincidentes sont deja
//     d'accord a 0,04 degre) ;
//   * la MEME dalle plate est 27x plus bruitee DANS une ombre que hors d'elle,
//     avec 23,32 % de pixels isoles contre 0,00 % — donc une passe PAR PIXEL
//     fabrique du grain independamment du maillage.
// Le candidat nomme est le tramage de Bayer 4x4 de `shadowalpha.frag.nksl`,
// arme des qu'un materiau a une opacite < 1 (le mode 1 est LE DEFAUT de
// `mTransShadowMode`). Il n'a pas pu etre innocente depuis une capture : le
// tramage vit en espace CARTE D'OMBRE, sa periode n'arrive pas a 4 pixels a
// l'ecran.
//
// 🔑 CE BANC EXISTE POUR ARMER ET DESARMER LA CAUSE SOI-MEME. Une dalle plate,
// un occultant, une source. On change UN SEUL parametre entre deux captures —
// le mode d'ombre transparente — a opacite EGALE. Si le sel apparait et
// disparait avec lui, la cause est tenue. S'il reste, le candidat tombe, et
// c'est le resultat le plus utile.
//
// ⚠️ POURQUOI LA DALLE N'A PAS D'INSTANCE DE MATERIAU, ET C'EST DELIBERE.
// `NkRender3D.cpp:2797` lit `matInst ? matInst->mTransShadowMode : 0u` : SANS
// instance, le mode vaut 0 et le tramage ne peut pas s'armer. La dalle est donc
// hors de cause PAR CONSTRUCTION — ce qu'on mesure dessus ne peut venir que de
// l'ombre qu'elle RECOIT, jamais de celle qu'elle projette.
//
// ── LE CONTRAT D'ENVIRONNEMENT ──────────────────────────────────────────────
//   NK_BANC_VUE        0 = la dalle et son ombre (defaut) | 1 = le CIEL seul
//   NK_BANC_OPACITE    opacite de l'occultant, defaut 0.12 (celle de Rodolf)
//   NK_BANC_OMBRE_MODE 0 = pleine | 1 = proportionnelle (tramage) | 2 = aucune
//                      defaut 1, qui est AUSSI le defaut du moteur
//   NK_BANC_SOLEIL     elevation du soleil en DEGRES au-dessus de l'horizon.
//                      Defaut 70 en vue 0 (ombre courte et large), 90 en vue 1.
//                      Une valeur NEGATIVE met le soleil sous l'horizon.
//   NK_BANC_CIEL       modele de ciel : 0 degrade, 1 Preetham, 2 Rayleigh+Mie
//                      (defaut, celui du projet de Rodolf), 3 Hosek, 4 Prague
//
// Se capture avec les crochets deja presents dans `main.cpp` :
//   NK_MAXFRAMES=40 NK_CAPTURE=30 NK_CAPTURE_PATH=... renderdemo --demo=21
//
// ⚠️ LA CAMERA EST FIXE, ET C'EST LA CONDITION DE TOUT LE BANC. Deux captures
// qui ne cadrent pas la meme chose ne se soustraient pas. Aucune orbite, aucune
// dependance au temps ecoule : la meme image a la meme frame, toujours.
// =============================================================================
#include "DemoCommon.h"
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Environment/NkEnvironmentSystem.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKPlatform/NkEnv.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h" // sonde du cache : cle + version du generateur
#include <cstring>

#include <cmath>

namespace nkentseu {
	namespace demo {

		struct BancOmbreState {
				NkMaterial *matOccultant = nullptr;
				int32 vue = 0;
				float32 opacite = 0.12f;
				uint32 modeOmbre = 1u;
				float32 soleilDeg = 70.f;
				int32 modeleCiel = 2;
				float32 intensiteSoleil = 0.14f;
				bool journalFait = false;
		};

		// ── LECTURE DE L'ENVIRONNEMENT ──────────────────────────────────────────
		// ⚠️ On teste le `const char *` AVANT de le convertir. `GetEnvVar` rend
		// `nullptr` quand la variable est absente ; le glisser dans un `NkString`
		// rendrait une chaine VIDE et confondrait « absente » avec « posee a vide ».
		// Le depot a deja paye ce piege le 19/08 sur `NK_GPU_RECYCLE_CMD`.
		static float32 BancFloat(const char *nom, float32 defaut) {
			const char *v = ::nkentseu::env::GetEnvVar(nom);
			if (!v || !v[0])
				return defaut;
			return NkString(v).ToFloat32(defaut);
		}

		static int32 BancInt(const char *nom, int32 defaut) {
			const char *v = ::nkentseu::env::GetEnvVar(nom);
			if (!v || !v[0])
				return defaut;
			return NkString(v).ToInt32(defaut);
		}

		// Direction de PROPAGATION du soleil pour une elevation donnee, dans le
		// plan XZ d'azimut fixe. Convention du moteur : un soleil au zenith
		// DESCEND, donc (0,-1,0) — la meme que `NkLightDesc::direction` et que
		// `NkSkyParams::sunDirection`. Les deux doivent recevoir la MEME valeur,
		// sinon le ciel et l'eclairage racontent deux soleils differents.
		static NkVec3f BancDirSoleil(float32 elevationDeg) {
			const float32 e = elevationDeg * 3.14159265f / 180.f;
			const float32 ce = cosf(e);
			const float32 se = sinf(e);
			// Azimut fixe : le soleil est du cote -Z, donc il pousse la lumiere
			// vers +Z. L'ombre part vers l'observateur, qui regarde depuis +Z.
			return NkVec3f{0.f, -se, ce};
		}

		// =====================================================================
		// SONDE : UN SHADER QUI NE COMPILE PAS DOIT RENDRE UN HANDLE INVALIDE.
		//
		// 🔴 CE QU'ELLE GARDE. Le 2026-09-07, un shader refuse par le pilote DX11
		// (`error X3004`) a produit un handle VALIDE : `CreateShader` journalisait
		// l'echec puis faisait `continue`, et la creation de pipeline annoncait
		// `pipeline_valid=1`. Resultat : l'ombre proportionnelle etait morte sur
		// DX11 et RIEN ne le disait — Rodolf reglait un parametre inerte. Le
		// defaut de generation s'est corrige en cinquante lignes ; ce mensonge-la
		// est la raison pour laquelle personne ne l'avait vu, et il protegeait
		// tous les suivants.
		//
		// ⚠️ Elle n'interroge pas un JOURNAL, elle interroge la VALEUR RENDUE a
		// l'appelant — c'est d'elle que depend tout le reste. Un journal se lit
		// apres coup ; un handle decide.
		static bool BancSondeShader(DemoCtx &ctx) {
			if (!ctx.device)
				return true;
			// Un identifiant qui n'existe dans aucun dialecte : c'est exactement la
			// FORME du defaut reel (`gl_fragcoord` non declare), pas une faute de
			// syntaxe grossiere qu'un analyseur attraperait bien plus tot.
			static const char *kHlslCasse = "float4 main() : SV_Target {\n"
											"    return float4(nk_identifiant_qui_nexiste_pas, 0, 0, 1);\n"
											"}\n";
			static const char *kGlslCasse = "#version 430 core\n"
											"out vec4 oColor;\n"
											"void main() {\n"
											"    oColor = vec4(nk_identifiant_qui_nexiste_pas, 0, 0, 1);\n"
											"}\n";
			const bool hlsl =
				(ctx.api == NkGraphicsApi::NK_GFX_API_DX11 || ctx.api == NkGraphicsApi::NK_GFX_API_DX12);
			// ⚠️ QUALIFICATION COMPLETE : `NkShaderHandle` existe dans DEUX espaces de
			// noms visibles ici (`nkentseu::` et `nkentseu::renderer::`) et le
			// compilateur la declare ambigue. Le depot connait deja ce piege sur
			// `NkShaderStage`, avec la meme parade.
			::nkentseu::NkShaderDesc d;
			d.debugName = "BancSondeShaderCasse";
			if (hlsl)
				d.AddHLSL(::nkentseu::NkShaderStage::NK_FRAGMENT, kHlslCasse);
			else
				d.AddGLSL(::nkentseu::NkShaderStage::NK_FRAGMENT, kGlslCasse);

			::nkentseu::NkShaderHandle h = ctx.device->CreateShader(d);
			const bool valide = h.IsValid();
			if (valide)
				ctx.device->DestroyShader(h);

			if (valide) {
				logger.Errorf("[BancSondeShader] ROUGE : un shader REFUSE par le pilote a rendu un handle "
							  "VALIDE. Un echec qui preserve le succes est un mensonge, pas un repli.\n");
				return false;
			}
			logger.Infof("[BancSondeShader] VERTE : un shader refuse rend bien un handle invalide.\n");
			return true;
		}

		// =====================================================================
		// SONDE : CHANGER DE GENERATEUR DOIT INVALIDER LE CACHE.
		//
		// 🔴 CE QU'ELLE GARDE. Le 2026-09-07, un correctif du generateur HLSL a ete
		// livre, verifie, present dans le binaire — et **invisible chez Rodolf** :
		// le cache servait le texte de la veille. `ComputeKey` hachait la source,
		// l'etage et la cible, jamais le GENERATEUR. Consequence : tout correctif
		// futur de NkSL etait invisible chez quiconque possede un cache, et tout
		// banc comparant un avant a un apres mesurait deux fois l'avant.
		//
		// Elle ecrit une entree sous la version courante, redemande la MEME source
		// sous une version differente, et exige que la lecture REFUSE. Elle exerce
		// l'arithmetique de cle DU MOTEUR, pas une copie.
		static bool BancSondeCacheShader() {
			auto &cache = ::nkentseu::NkShaderCache::Global();
			const NkString src("// source temoin de la sonde de cache\nvoid main() {}\n");
			const NkString cible("temoin_sonde_cache");
			const ::nkentseu::NkSLStage etage = ::nkentseu::NkSLStage::NK_FRAGMENT;

			const uint64 cleV = ::nkentseu::NkShaderCache::ComputeKey(src, etage, cible);
			const uint64 cleAutre = ::nkentseu::NkShaderCache::ComputeKeyPourVersion(
				src, etage, cible, ::nkentseu::kNkSLGeneratorVersion + 1u);

			::nkentseu::NkShaderConvertResult ecrit;
			ecrit.success = true;
			const char *charge = "TEMOIN_SONDE_CACHE";
			const uint32 n = (uint32)strlen(charge);
			ecrit.binary.Resize(n);
			memcpy(ecrit.binary.Data(), charge, n);
			if (!cache.Save(cleV, ecrit)) {
				logger.Errorf("[BancSondeCache] ROUGE : impossible d'ecrire l'entree temoin — "
							  "la sonde ne peut rien prouver.\n");
				return false;
			}

			// ⚠️ LE CONTROLE POSITIF D'ABORD. Sans lui, un cache qui refuse TOUT
			// rendrait cette sonde verte pour la pire des raisons : elle
			// verifierait qu'un cache mort refuse, et appellerait ca une
			// invalidation.
			const bool relit = cache.Load(cleV).success;
			const bool refuse = !cache.Load(cleAutre).success;
			cache.Invalidate(cleV);
			cache.Invalidate(cleAutre);

			if (!relit) {
				logger.Errorf("[BancSondeCache] ROUGE : l'entree que je viens d'ecrire ne se relit pas. "
							  "Le cache ne fonctionne pas — la sonde ne prouve RIEN.\n");
				return false;
			}
			if (!refuse) {
				logger.Errorf("[BancSondeCache] ROUGE : une version de generateur DIFFERENTE relit "
							  "l'entree de la precedente. Tout correctif de NkSL sera invisible chez "
							  "quiconque possede un cache.\n");
				return false;
			}
			logger.Infof("[BancSondeCache] VERTE : l'entree se relit sous sa version (controle positif) "
						 "et une AUTRE version la refuse. cle %llx contre %llx\n",
						 (unsigned long long)cleV, (unsigned long long)cleAutre);
			return true;
		}

		bool DemoBancOmbre_Init(DemoCtx &ctx) {
			{
				const char *v = ::nkentseu::env::GetEnvVar("NK_BANC_SONDE_CACHE");
				if (v && v[0] && v[0] != '0' && !BancSondeCacheShader())
					return false; // ROUGE -> code de sortie non nul
			}
			// La sonde passe AVANT tout le reste : si le dorsal ment sur la
			// validite d'un shader, rien de ce que ce banc mesure ensuite ne peut
			// etre cru.
			//
			// ⚠️ ET ELLE SE DISTINGUE PAR LE CODE DE SORTIE. Premiere ecriture :
			// `return BancSondeShader(ctx) ? false : false;` — elle rendait faux
			// dans LES DEUX cas, donc l'application sortait en erreur qu'elle soit
			// verte ou rouge. *Une sonde qui echoue toujours ne vaut pas mieux
			// qu'une sonde qui ne peut pas echouer.* ROUGE -> `false`, code de
			// sortie non nul ; VERTE -> on poursuit, le banc rend son image et
			// sort a zero.
			{
				const char *v = ::nkentseu::env::GetEnvVar("NK_BANC_SONDE_SHADER");
				if (v && v[0] && v[0] != '0' && !BancSondeShader(ctx))
					return false;
			}

			auto *st = new BancOmbreState();
			ctx.userData = st;

			st->vue = BancInt("NK_BANC_VUE", 0);
			st->opacite = BancFloat("NK_BANC_OPACITE", 0.12f);
			st->modeOmbre = (uint32)BancInt("NK_BANC_OMBRE_MODE", 1);
			st->soleilDeg = BancFloat("NK_BANC_SOLEIL", st->vue == 1 ? 90.f : 70.f);
			st->modeleCiel = BancInt("NK_BANC_CIEL", 2);
			// ⚠️ 0,14 PAR DEFAUT, ET CE N'EST PAS UN GOUT. A l'intensite 1, le ciel
			// sort ECRETE — 214/213/212 uniformes sur tout le champ, mesure du
			// 06/09. Un signal sature ne peut porter AUCUN degrade : le temoin
			// aurait rendu « pas d'inversion » sur n'importe quel ciel. 0,14 est
			// aussi la valeur du projet de Rodolf (`soleilIntensite`), donc le banc
			// mesure ce qu'il voit, lui.
			st->intensiteSoleil = BancFloat("NK_BANC_INTENSITE", 0.14f);
			if (st->opacite < 0.f)
				st->opacite = 0.f;
			if (st->opacite > 1.f)
				st->opacite = 1.f;
			if (st->modeOmbre > 2u)
				st->modeOmbre = 1u;

			auto *matSys = ctx.renderer->GetMaterials();
			if (!matSys) {
				logger.Errorf("[BancOmbre] pas de systeme de materiaux — le banc ne peut RIEN prouver\n");
				delete st;
				ctx.userData = nullptr;
				return false;
			}

			// L'occultant porte une INSTANCE : c'est elle, et elle seule, qui peut
			// armer le tramage. Sans instance le mode vaut 0 cote rendu.
			// ⚠️ PAR LE TYPE, PAS PAR LE NOM. La surcharge par chaine
			// (`Create(sys, "PBR")`) a rendu une instance INVALIDE au premier
			// essai — le nom de gabarit n'est pas « PBR ». La garde ci-dessous a
			// mordu et le banc a refuse de tourner, ce qui etait le bon
			// comportement : sans instance le mode d'ombre vaut 0 cote rendu, et
			// le banc aurait mesure exactement le CONTRAIRE de ce qu'il annonce.
			// La surcharge par `NkMaterialType` passe par `sys->DefaultPBR()` et
			// ne depend d'aucune chaine.
			st->matOccultant = NkMaterial::Create(matSys, NkMaterialType::NK_PBR_METALLIC);
			if (!st->matOccultant || !st->matOccultant->IsValid()) {
				logger.Errorf("[BancOmbre] instance de materiau introuvable — "
							  "sans elle le mode d'ombre vaut 0 et le banc mesurerait le contraire\n");
				delete st;
				ctx.userData = nullptr;
				return false;
			}
			st->matOccultant->SetTransShadowMode(st->modeOmbre);

			// 🔴 UNE TEXTURE D'ALBEDO EST OBLIGATOIRE ICI, ET CE N'EST PAS
			// COSMETIQUE. Sans elle, le pilote NVIDIA plante dans
			// `glDrawElementsBaseVertex` des que l'occultant passe dans la file
			// TRANSPARENTE (mesure du 06/09 : SIGSEGV dans `nvoglv64.dll`,
			// reproductible a opacite 0,12, absent a opacite 1,0). Le jeu de
			// descripteurs de l'instance est incomplet et le shader echantillonne
			// quand meme `tAlbedo` — `shadowalpha.frag.nksl` le fait des sa
			// premiere ligne, avant tout tramage.
			// C'est une DETTE du moteur, pas du banc : une instance de materiau
			// sans texture devrait rendre une texture par defaut, jamais planter.
			// Elle est nommee dans le rapport ; ici on la contourne pour pouvoir
			// mesurer ce qu'on est venu mesurer.
			if (auto *texLib = ctx.renderer->GetTextures()) {
				const NkTexHandle blanc = texLib->GetWhite1x1();
				if (blanc.IsValid()) {
					st->matOccultant->SetTexture("albedo_map", blanc);
					st->matOccultant->SetTexture("albedo", blanc);
				}
			}

			// ⚠️ ON RELIT L'EFFET, ON NE CROIT PAS LE SETTER. Un mode ecrit et non
			// relu est exactement le « parametre declare qui n'est pas honore » que
			// ce depot a paye huit fois. Si la relecture ne rend pas ce qu'on a
			// pose, le banc REFUSE de tourner : un banc qui mesure un autre reglage
			// que celui qu'il annonce est pire qu'aucun banc.
			const uint32 relu = st->matOccultant->GetTransShadowMode();
			if (relu != st->modeOmbre) {
				logger.Errorf("[BancOmbre] REFUS : mode pose=%u, relu=%u — le reglage n'est pas honore\n",
							  st->modeOmbre, relu);
				NkMaterial::Destroy(st->matOccultant);
				delete st;
				ctx.userData = nullptr;
				return false;
			}

			logger.Infof("[BancOmbre] vue=%d opacite=%.3f modeOmbre=%u (relu %u) "
						 "soleil=%.1f deg ciel=%d\n",
						 st->vue, st->opacite, st->modeOmbre, relu, st->soleilDeg, st->modeleCiel);
			return true;
		}

		void DemoBancOmbre_Frame(DemoCtx &ctx, float32 dt) {
			(void)dt;
			auto *st = (BancOmbreState *)ctx.userData;
			if (!st)
				return;
			if (!ctx.renderer->BeginFrame())
				return;
			auto *r3d = ctx.renderer->GetRender3D();
			auto *meshSys = ctx.renderer->GetMeshSystem();
			if (!r3d || !meshSys) {
				ctx.renderer->Present();
				ctx.renderer->EndFrame();
				return;
			}

			const NkVec3f dirSoleil = BancDirSoleil(st->soleilDeg);

			// ── CAMERA FIXE ─────────────────────────────────────────────────────
			NkCamera3DData cd;
			if (st->vue == 3) {
				// Regard PLONGEANT sur le point du monde situe juste sous la
				// lumiere. Meme cadrage dans les deux conditions : c'est ce qui
				// permet de mesurer le MEME point.
				cd.position = {0.f, 4.f, 4.f};
				cd.target = {0.f, 0.f, 0.f};
				cd.fovY = 50.f;
				cd.farPlane = 4000.f;
			} else if (st->vue == 1) {
				// VUE CIEL : l'oeil a hauteur d'homme, l'horizon au milieu de
				// l'image. C'est le cadrage qui rend le profil vertical lisible —
				// la moitie haute est du ciel, du zenith vers l'horizon.
				cd.position = {0.f, 1.6f, 0.f};
				cd.target = {0.f, 1.6f, -10.f};
				cd.fovY = 70.f;
				cd.farPlane = 500.f;
			} else {
				// VUE OMBRE : la dalle occupe la moitie basse, l'ombre de
				// l'occultant tombe au centre.
				cd.position = {0.f, 5.5f, 8.f};
				cd.target = {0.f, 0.f, 0.f};
				cd.fovY = 50.f;
				cd.farPlane = 200.f;
			}
			cd.up = {0.f, 1.f, 0.f};
			cd.aspect = (float32)ctx.width / (float32)ctx.height;
			cd.nearPlane = 0.05f;
			NkCamera3D cam(cd);

			NkSceneContext sctx;
			sctx.camera = cam;
			// TEMPS FIGE : le ciel temps reel anime nuages et etoiles. Deux
			// captures prises a des temps differents differeraient pour une raison
			// qui n'a rien a voir avec ce qu'on mesure.
			sctx.time = 0.f;

			if (st->vue == 3) {
				// UNE SEULE source, ponctuelle, exactement au-dessus de l'origine :
				// tout point du plan a y=0 et x=z=0 est donc a distance `hauteur`,
				// quelle que soit l'echelle du plan. La distance est CONSTANTE entre
				// les deux conditions — c'est la condition du temoin.
				NkLightDesc pt;
				pt.type = NkLightType::NK_POINT;
				pt.position = {0.f, BancFloat("NK_BANC_LUM_HAUTEUR", 3.f), 0.f};
				pt.color = {1.f, 1.f, 1.f};
				pt.intensity = BancFloat("NK_BANC_LUM_PUISSANCE", 100.f);
				pt.range = BancFloat("NK_BANC_LUM_PORTEE", 10.f);
				pt.castShadow = false;
				sctx.lights.PushBack(pt);
				sctx.ambientIntensity = 0.f; // AUCUNE ambiante : une seule variable
				r3d->SetSkyboxEnabled(false);
				r3d->BeginScene(sctx);
			} else {
			NkLightDesc soleil;
			soleil.type = NkLightType::NK_DIRECTIONAL;
			soleil.direction = dirSoleil;
			soleil.color = {1.f, 0.97f, 0.92f};
			soleil.intensity = 3.0f;
			soleil.castShadow = true;
			sctx.lights.PushBack(soleil);
			// Ambiante BASSE et non nulle : a zero, l'interieur de l'ombre serait
			// noir pur et un ecart-type local y vaudrait zero quoi qu'il arrive —
			// l'instrument serait aveugle exactement la ou on veut voir.
			sctx.ambientIntensity = 0.12f;

			// ── LE CIEL ─────────────────────────────────────────────────────────
			r3d->SetSkyboxEnabled(true);
			NkSkyParams sky;
			sky.model = (NkSkyModel)st->modeleCiel;
			sky.sunDirection = dirSoleil; // MEME valeur que la lumiere : un seul soleil
			sky.turbidity = 2.5f;
			sky.sunDisc = true;
			sky.sunIntensity = st->intensiteSoleil;
			sky.clouds = false;
			sky.starIntensity = 0.f;
			r3d->SetSkyParams(sky);

			r3d->BeginScene(sctx);
			}

			// ── VUE 3 : LE TEMOIN DE LUMIERE ────────────────────────────────────
			//
			// 🔴 LA QUESTION : le sol du viseur et un maillage recoivent-ils la MEME
			// echelle de lumiere ? Sur la capture de Rodolf, le sol eclaire est a
			// 242 de luminance (93 % de ses pixels satures) et le plateau du bureau
			// a 34 — sept fois d'ecart sous la meme lumiere.
			//
			// ⚠️ MAIS CES DEUX ZONES N'ONT NI LA MEME DISTANCE A LA LUMIERE NI LE
			// MEME ALBEDO. Comparer un sol clair juste sous une source a un plateau
			// sombre plus loin, c'est melanger trois variables. Ce temoin n'en
			// laisse qu'UNE : meme point du monde, meme couleur, meme rugosite,
			// meme metallique, meme distance — seule change la FACON dont la
			// surface est soumise.
			//
			//   NK_BANC_LUM_GRAND=0 : un plan de 4x4, comme un maillage ordinaire
			//   NK_BANC_LUM_GRAND=1 : le MEME plan a l'echelle 1500, exactement
			//                         comme `NkDemo3D` soumet son sol (l. 7270)
			// Les deux surfaces passent par le MEME point du monde sous la lumiere.
			// Le temoin EXIGE la meme luminance a quelques pour cent pres.
			if (st->vue == 3) {
				const float32 hauteur = BancFloat("NK_BANC_LUM_HAUTEUR", 3.f);
				const float32 puissance = BancFloat("NK_BANC_LUM_PUISSANCE", 100.f);
				const bool grand = BancInt("NK_BANC_LUM_GRAND", 0) != 0;
				const float32 ech = grand ? 1500.f : 4.f;

				NkDrawCall3D dc;
				dc.mesh = meshSys->GetPlane();
				dc.transform = NkMat4f::Scale({ech, 1.f, ech});
				dc.aabb = {{-ech, -0.01f, -ech}, {ech, 0.01f, ech}};
				// Les reglages EXACTS du panneau « Sol » de Rodolf.
				dc.tint = {0.62f, 0.62f, 0.64f};
				dc.alpha = 1.f;
				dc.roughness = 0.90f;
				dc.metallic = 0.f;
				dc.castShadow = false;
				dc.receiveShadow = true;
				r3d->Submit(dc);
				if (!st->journalFait) {
					st->journalFait = true;
					logger.Infof("[BancLumiere] echelle=%.0f hauteur=%.2f puissance=%.1f "
								 "(tint 0.62 rough 0.90 metal 0.00) — la seule variable est l'ECHELLE\n",
								 ech, hauteur, puissance);
				}
				ctx.renderer->Present();
				ctx.renderer->EndFrame();
				return;
			}

			// ── LA DALLE PLATE ──────────────────────────────────────────────────
			// Aucune instance de materiau : elle ne PEUT pas armer le tramage.
			// Elle recoit l'ombre, elle n'en projette pas.
			{
				NkDrawCall3D sol;
				sol.mesh = meshSys->GetPlane();
				sol.transform = NkMat4f::Scale({40.f, 1.f, 40.f});
				sol.aabb = {{-40.f, -0.01f, -40.f}, {40.f, 0.01f, 40.f}};
				sol.tint = {0.62f, 0.62f, 0.64f};
				sol.alpha = 1.f;
				sol.roughness = 0.9f;
				sol.metallic = 0.f;
				sol.castShadow = false;
				sol.receiveShadow = true;
				r3d->Submit(sol);
			}

			// ── L'OCCULTANT ─────────────────────────────────────────────────────
			// Absent de la vue CIEL : il n'y a rien a occulter, et sa presence
			// mettrait une ombre dans le cadre qu'on mesure.
			if (st->vue != 1 && st->matOccultant && st->matOccultant->IsValid()) {
				NkDrawCall3D occ;
				occ.mesh = meshSys->GetCube();
				occ.transform = NkMat4f::Translate({0.f, 2.6f, 0.f}) * NkMat4f::Scale({3.2f, 0.18f, 3.2f});
				occ.aabb = {{-1.7f, 2.4f, -1.7f}, {1.7f, 2.8f, 1.7f}};
				occ.material = st->matOccultant->GetInstHandle();
				occ.tint = {0.85f, 0.85f, 0.9f};
				// C'EST CE CHAMP QUI DECIDE. `NkRender3D` traite le caster comme
				// semi-transparent des que `alpha < 0.999`, et c'est cette valeur
				// qui arrive dans `uObj.tint.w`, le seuil du tramage.
				occ.alpha = st->opacite;
				occ.roughness = 0.4f;
				occ.castShadow = true;
				occ.receiveShadow = true;
				r3d->Submit(occ);
			}

			if (!st->journalFait) {
				st->journalFait = true;
				// Le journal porte la CONFIGURATION de l'image, pas une intention :
				// une capture dont on ne peut plus dire sous quel reglage elle est
				// sortie ne prouve rien un jour plus tard.
				logger.Infof("[BancOmbre] image rendue — vue=%d opacite=%.3f mode=%u soleil=%.1f ciel=%d "
							 "cam=(%.2f,%.2f,%.2f)\n",
							 st->vue, st->opacite, st->modeOmbre, st->soleilDeg, st->modeleCiel, cd.position.x,
							 cd.position.y, cd.position.z);
			}

			ctx.renderer->Present();
			ctx.renderer->EndFrame();
		}

		void DemoBancOmbre_Shutdown(DemoCtx &ctx) {
			auto *st = (BancOmbreState *)ctx.userData;
			if (!st)
				return;
			if (st->matOccultant)
				NkMaterial::Destroy(st->matOccultant);
			delete st;
			ctx.userData = nullptr;
		}

	} // namespace demo
} // namespace nkentseu
