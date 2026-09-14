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
// ── VUE 3 : LE TEMOIN DE LUMIERE ────────────────────────────────────────────
//   NK_BANC_LUM_PUISSANCE  intensite de l'unique source ponctuelle (defaut 100)
//   NK_BANC_LUM_HAUTEUR    sa hauteur au-dessus du plan (defaut 3)
//   NK_BANC_LUM_PORTEE     sa portee (defaut 10)
//   NK_BANC_LUM_GRAND      0 = plan 4x4 | 1 = MEME plan a l'echelle 1500
//   NK_BANC_LUM_ALBEDO     clarte de la surface (defaut 0.62, celle du panneau)
//   NK_BANC_LUM_ENV        1 = l'ambiance vient de l'ENVIRONNEMENT (le ciel
//                          eclaire), comme la case << Source >> de Rodolf
//   NK_BANC_AMBIANTE       ce que la SCENE declare comme ambiante. Mis a
//                          l'epreuve le 07/09 : de 0 a 10, l'image ne bouge PAS
//                          d'un octet. Le champ est MORT ; l'ambiante reelle
//                          vient de NK_BANC_IBL.
//   NK_BANC_IBL            force de l'ambiante IBL -- reglee dans `main.cpp`
//                          car c'est un parametre de CREATION du renderer.
//                          Defaut moteur 0.05 ; le viseur du modeleur met 1.1.
//   NK_BANC_MARQUE         1 = pose un rectangle magenta au coin HAUT-GAUCHE en
//                          coordonnees d'ECRAN. Reference d'orientation qui ne
//                          traverse ni camera ni projection ni scene.
//
// ── VUE 6 : LE TEMOIN DE HALO (12/09) ───────────────────────────────────────
//   Une source COMPACTE, BRILLANTE, DECENTREE sur fond UNI : ni ciel, ni sol,
//   ni occultant. Un cube face camera, eclaire par une directionnelle qui
//   arrive DE la camera (N.L = 1 sur la face vue), rugosite 1, sans ombre.
//   ⚠️ Le fond n'est PAS noir : c'est la couleur d'effacement du moteur, mesuree
//   a 63 en LDR (~0,05 HDR) -- loin sous le seuil de 3,62, et elle s'annule
//   dans ON - OFF. Je l'avais ecrit « noir » avant la premiere capture.
//   NK_BANC_SOURCE         teinte du cube (multiplicateur d'albedo, defaut 1).
//                          ⚠️ NkDrawCall3D n'a AUCUN champ emissif : la radiance
//                          HDR de la source n'est PAS ce nombre, c'est
//                          teinte x eclairement x BRDF. Elle se LIT dans la
//                          capture (bloom eteint, ACES inverse sur les points
//                          non satures, blanc a 7,24), elle ne se pose pas.
//   NK_BANC_SOURCE_LUM     intensite de la directionnelle (defaut 3)
//   NK_BANC_SOURCE_TAILLE  arete du cube en unites monde (defaut 1). A la
//                          profondeur par defaut (12), 70 deg de champ, 720 px :
//                          1 unite = ~43 px, au-dessus de l'empreinte des 13
//                          taps de la passe brillante (~4x4 texels HDR).
//                          0.05 = ~2 px, SOUS l'empreinte : c'est la taille qui
//                          fait parler la moyenne de Karis.
//   NK_BANC_SOURCE_POS     "x,y,z" (defaut "3,6,-12" : le point du cube temoin,
//                          decentre dans les DEUX axes -- sans quoi un halo en
//                          miroir retomberait sur sa source et ne se verrait pas)
//   NK_BANC_BLOOM=1 (main.cpp) allume la passe a la CREATION, seuil 0,5 ;
//   NK_BANC_POST=bloom l'eteint ENSUITE par SetPostConfig. ON et OFF partagent
//   donc la meme creation ; seule la passe change entre les deux captures.
//
//   CE QUE LA COURBE DOIT DEPARTAGER, ECRIT AVANT LA MESURE (Q192). Profil
//   radial I(r) = moyenne de (ON - OFF) par anneau de 2 px autour du centroide
//   de la SOURCE, pour une famille de radiances S/seuil croissantes, deux
//   tailles, quatre dorsaux. Chaque cause laisse une trace DIFFERENTE, et
//   plusieurs peuvent etre presentes a la fois :
//     * COUPURE DURE SANS GENOU (SoftThreshold : 0 exact sous le seuil,
//       lineaire jusqu'a 5 seuils, quadratique au-dela, contribution > 1 au-dela
//       de ~5,8 seuils) : I = 0 sous le seuil ; au franchissement un halo qui
//       apparait deja LARGE ; amplitude en (S - seuil) a forme constante, puis
//       qui croit PLUS VITE que S.
//     * PYRAMIDE TROP COURTE (6 mips W/2..W/64, tente de 3 texels) : le rayon ou
//       I tombe sous 1/255 SATURE vers ~192 px quel que soit S, avec un bord net.
//     * REMONTEE ADDITIVE NON NORMALISEE : l'energie par octave de rayon (somme
//       de I(r)·2·pi·r sur [r, 2r]) est CONSTANTE d'une octave a l'autre -- un
//       profil en 1/r^2, plat et etendu : « une grosse eponge ».
//     * SATURATION EN AMONT : plateau a 255 dont la LARGEUR croit avec S alors
//       que la queue ne bouge pas ; ou, pour la source de 2 px seulement, une
//       amplitude qui plafonne quand S croit (Karis borne a ~1,4 toute source
//       plus petite que l'empreinte).
//   POSITION (sites `isVK ? -1 : +1` des sous-passes bloom, NkPostProcessStack
//   l. 1202 et 1235, jamais juges) : centroide du halo contre centroide de la
//   source dans la MEME image ; un retournement d'une sous-passe mettrait le
//   halo a (H - y_s), soit |H - 2 y_s| de sa source.
//
// Se capture avec les crochets deja presents dans `main.cpp` :
//   NK_MAXFRAMES=40 NK_CAPTURE=30 NK_CAPTURE_PATH=... renderdemo --demo=21
//
// ⚠️ LA CAMERA EST FIXE, ET C'EST LA CONDITION DE TOUT LE BANC. Deux captures
// qui ne cadrent pas la meme chose ne se soustraient pas. Aucune orbite, aucune
// dependance au temps ecoule : la meme image a la meme frame, toujours.
// =============================================================================
#include "DemoCommon.h"
#include <cstdio>
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Environment/NkEnvironmentSystem.h"
#include "NKRenderer/Tools/Reflection/NkPlanarReflectionSystem.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKPlatform/NkEnv.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h" // sonde du cache : cle + version du generateur
#include <cstring>

#include <cmath>

namespace nkentseu {
	namespace demo {

		struct BancOmbreState {
				NkMaterial *matOccultant = nullptr;
				NkMaterial *matMiroir = nullptr; // vue 5 : le sol reflechissant
				NkMaterial *matSol = nullptr; // vue 3 : le MEME plan, mais par instance de materiau
				renderer::NkPlanarReflectionHandle reflHandle{};
				// TEMOIN HORS ECRAN : rtA recoit la 3D, rtB recoit rtA ECHANTILLONNEE.
				renderer::NkOffscreenTarget rtA;
				renderer::NkOffscreenTarget rtB;
				bool heInit = false;
				bool heFait = false;

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

			// ── LE SOL MIROIR DE LA VUE 5 ───────────────────────────────────────
			// Cree SEULEMENT en vue 5 : un materiau reflechissant fait travailler la
			// passe miroir a chaque image, et le banc du grain n'en veut pas.
			// ⚠️ ON RELIT ICI AUSSI. Si le systeme de reflexion refuse le plan, le
			// temoin ne mesurerait qu'un sol ordinaire et le dirait VERT sans avoir
			// rien reflechi. Le banc REFUSE plutot que de mesurer a cote.
			// ── VUE 3 : LE MEME PLAN, MAIS PAR INSTANCE DE MATERIAU ─────────────
			// 🔴 LA DERNIERE VARIABLE JAMAIS ISOLEE. Le temoin de lumiere avait deja
			// refute l'ECHELLE (147,11 contre 147,11) et l'ALBEDO (+29 %, pas x7).
			// Reste le CHEMIN : le sol du viseur porte une INSTANCE DE MATERIAU,
			// un maillage ordinaire porte un simple `dc.tint`. On donne donc au plan
			// un materiau aux reglages IDENTIQUES -- meme albedo, meme rugosite, meme
			// metallique -- pour que la SEULE difference soit la facon dont la
			// surface est soumise. Toute divergence est alors imputable au chemin.
			if (st->vue == 3 && BancInt("NK_BANC_LUM_MAT", 0) != 0) {
				st->matSol = NkMaterial::Create(matSys, NkMaterialType::NK_PBR_METALLIC);
				if (!st->matSol || !st->matSol->IsValid()) {
					logger.Errorf("[BancOmbre] REFUS : materiau du sol invalide\n");
					NkMaterial::Destroy(st->matOccultant);
					delete st;
					ctx.userData = nullptr;
					return false;
				}
				const float32 alb = BancFloat("NK_BANC_LUM_ALBEDO", 0.62f);
				st->matSol->SetAlbedo({alb, alb, alb})->SetRoughness(0.90f)->SetMetallic(0.f);
				logger.Infof("[BancLumiere] plan par INSTANCE DE MATERIAU, albedo %.3f\n", alb);
			}

			if (st->vue == 5) {
				st->matMiroir = NkMaterial::Create(matSys, NkMaterialType::NK_REFL_FLOOR);
				if (!st->matMiroir || !st->matMiroir->IsValid()) {
					logger.Errorf("[BancOmbre] REFUS : materiau ReflFloor invalide\n");
					NkMaterial::Destroy(st->matOccultant);
					delete st;
					ctx.userData = nullptr;
					return false;
				}
				st->matMiroir->SetAlbedo({0.55f, 0.55f, 0.60f})->SetRoughness(0.05f);
				auto *refl = ctx.renderer->GetPlanarReflection();
				if (!refl) {
					logger.Errorf("[BancOmbre] REFUS : aucun systeme de reflexion plane\n");
					delete st;
					ctx.userData = nullptr;
					return false;
				}
				renderer::NkPlanarReflectionDesc rd;
				rd.normal = {0.f, 1.f, 0.f};
				rd.point = {0.f, 0.f, 0.f};
				rd.rtWidth = (ctx.width / 2 > 0) ? ctx.width / 2 : 512;
				rd.rtHeight = (ctx.height / 2 > 0) ? ctx.height / 2 : 256;
				rd.hdr = true;
				rd.debugName = NkString("BancOmbre_Miroir");
				rd.targetMaterial = st->matMiroir->GetInstHandle();
				st->reflHandle = refl->Register(rd);
				if (!st->reflHandle.IsValid()) {
					logger.Errorf("[BancOmbre] REFUS : le plan miroir n'a pas ete enregistre\n");
					delete st;
					ctx.userData = nullptr;
					return false;
				}
				logger.Infof("[BancMiroir] plan y=0 enregistre, RT %ux%u\n", rd.rtWidth, rd.rtHeight);
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
			// NK_BANC_POST="tonemap,bloom,ssao,fxaa" : eteint des passes de post-
			// traitement sur le banc, une par une -- le pendant de NK_AGENT_POST du
			// modeleur, et pour la MEME question : FXAA AGIT-elle ici ?
			//
			// ⚠️ ELLE ECRIT DANS LA CONFIGURATION QUE LE GRAPHE LIT. Il y en a deux :
			// NkPostProcessStack::GetConfig() (la pile) et NkRenderer::GetConfig()
			// .postProcess (le graphe, via SetPostConfig). Ecrire dans la premiere
			// imprimait « applique » et ne changeait rien -- mesure : eteindre TOUT,
			// tonemap compris, ne bougeait l'image que de 0.019. Le controle positif
			// de ce levier est donc « tonemap » : eteint, l'image DOIT etre bouleversee.
			{
				static bool sPostPose = false;
				if (!sPostPose) {
					sPostPose = true;
					if (const char *pv = ::nkentseu::env::GetEnvVar("NK_BANC_POST")) {
						const bool tout = std::strstr(pv, "tout") != nullptr;
						renderer::NkPostConfig c = ctx.renderer->GetConfig().postProcess;
						// « +fxaa » ALLUME, « fxaa » ETEINT. Le banc cree son renderer
						// avec fxaa=false (main.cpp, cas 20 : « un anticrenelage
						// EFFACERAIT ce qu'on vient mesurer »). Pour que le banc juge
						// yFlipUV avec ses temoins absolus, il faut pouvoir l'allumer.
						// Controle d'armement : la sonde NK_AGENT_TONELDR n'imprime
						// « ARMEE » que si la passe FXAA_Final tourne reellement.
						auto veut = [&](const char *nom, bool &champ) {
							char plus[16] = {'+', 0};
							std::strncat(plus, nom, sizeof(plus) - 2);
							if (std::strstr(pv, plus)) champ = true;
							else if (tout || std::strstr(pv, nom)) champ = false;
						};
						veut("tonemap", c.toneMapping);
						veut("bloom", c.bloom);
						veut("ssao", c.ssao);
						veut("fxaa", c.fxaa);
						ctx.renderer->SetPostConfig(c);
						logger.Infof("[BancOmbre] NK_BANC_POST=%s : tonemap=%d bloom=%d ssao=%d fxaa=%d\n",
									 pv, c.toneMapping ? 1 : 0, c.bloom ? 1 : 0, c.ssao ? 1 : 0, c.fxaa ? 1 : 0);
					}
				}
			}
			// NK_BANC_RESIZE=<w>x<h> : appelle ctx.renderer->OnResize(w, h) UNE fois, a la
			// 5e trame, SANS override. Ce qu'il separe : l'override a taille EGALE a la
			// chaine (1280x720) retourne DX sous FXAA (cube a 549.1), alors qu'un
			// rebuild du graphe seul (SetPostConfig) ne retourne rien. Or
			// SetRenderSizeOverride == ApplyRenderSize(touchDevice=false) == OnResize
			// des sous-systemes + WaitIdle + RebuildRenderGraph. Si un OnResize a taille
			// egale, hors override, retourne DX aussi, la variable est « OnResize des
			// sous-systemes apres l'init » -- et un redimensionnement de fenetre le
			// ferait dans le modeleur. Sans la variable, rien ne change.
			{
				static bool sResizeFait = false;
				if (!sResizeFait && ctx.frame >= 5) {
					sResizeFait = true;
					if (const char *rv = ::nkentseu::env::GetEnvVar("NK_BANC_RESIZE")) {
						int32 rw = 0, rh = 0;
						if (std::sscanf(rv, "%dx%d", &rw, &rh) == 2 && rw > 0 && rh > 0) {
							ctx.renderer->OnResize((uint32)rw, (uint32)rh);
							std::printf("[BancOmbre] OnResize(%d, %d) a la trame %u, sans override\n", rw, rh, ctx.frame);
							std::fflush(stdout);
						}
					}
				}
			}
			// NK_BANC_SURTAILLE=<w>x<h> : pose SetRenderSizeOverride, comme le
			// modeleur, qui rend sa vue 3D a 1064x566 dans une fenetre 1616x939.
			//
			// ⚠️ POURQUOI CE LEVIER. Le banc rend JUSTE sur les quatre dorsaux, en
			// absolu (marque magenta a 4800 px en (4,4), cube derive a x=768.9
			// y=169.9 pour 768.6 / 171.4 attendus). Le modeleur, meme dorsal, meme
			// generateur, meme chemin de soumission (GetRender3D + Submit), sort
			// INVERSE sur DX. La difference est donc dans le CHEMIN, et on la cherche
			// une a la fois. Celle-ci est la premiere des trois listees.
			{
				static bool sSurtaillePosee = false;
				if (!sSurtaillePosee) {
					sSurtaillePosee = true;
					if (const char *sv = ::nkentseu::env::GetEnvVar("NK_BANC_SURTAILLE")) {
						int32 sw = 0, sh = 0;
						if (std::sscanf(sv, "%dx%d", &sw, &sh) == 2 && sw > 0 && sh > 0) {
							ctx.renderer->SetRenderSizeOverride((uint32)sw, (uint32)sh);
							logger.Infof("[BancOmbre] SetRenderSizeOverride(%d, %d) pose" 
										 " (fenetre %ux%u)\n", sw, sh, ctx.width, ctx.height);
						}
					}
				}
			}
			auto *r3d = ctx.renderer->GetRender3D();
			auto *meshSys = ctx.renderer->GetMeshSystem();
			if (!r3d || !meshSys) {
				ctx.renderer->Present();
				ctx.renderer->EndFrame();
				return;
			}

			// ── TEMOIN HORS ECRAN ───────────────────────────────────────────────
			//
			// 🔴 CE QU'IL EXERCE, ET QUE MES SIX AUTRES TEMOINS N'ONT JAMAIS TOUCHE.
			// Le modeleur rend la 3D HORS ECRAN puis publie cette cible comme une
			// TEXTURE D'INTERFACE que le GUI dessine. Ma capture, elle, RELIT la
			// cible vers le CPU. Deux consommateurs differents de la meme cible :
			// l'un ECHANTILLONNE, l'autre RELIT -- et seule la relecture etait
			// mesuree. Six temoins verts n'ont donc rien pu dire du chemin que
			// Rodolf regarde.
			//
			// EN DEUX TRAMES, et c'est oblige : on ne peut pas echantillonner une
			// cible pendant qu'on ecrit dedans.
			//   trame paire   -> la 3D rend dans rtA
			//   trame impaire -> rtA est echantillonnee plein ecran dans rtB,
			//                    puis rtB est relue sur disque
			const bool heArme = BancInt("NK_BANC_HORSECRAN", 0) != 0;
			if (heArme && !st->heInit) {
				st->heInit = true;
				renderer::NkOffscreenDesc od;
				od.width = ctx.width;
				od.height = ctx.height;
				od.hdr = false;
				od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
				od.hasDepth = true;
				od.readable = true;
				od.readback = true;
				od.name = "BancHE_A";
				const bool okA = st->rtA.Init(ctx.renderer->GetDevice(), ctx.renderer->GetTextures(), od);
				od.name = "BancHE_B";
				const bool okB = st->rtB.Init(ctx.renderer->GetDevice(), ctx.renderer->GetTextures(), od);
				// ⚠️ REFUS PLUTOT QU'UNE MESURE A COTE : sans les deux cibles, le
				// temoin mesurerait le rendu direct en se croyant hors ecran.
				if (!okA || !okB) {
					logger.Errorf("[BancHorsEcran] REFUS : cible hors ecran refusee (A=%d B=%d)\n",
								  okA ? 1 : 0, okB ? 1 : 0);
					ctx.renderer->Present();
					ctx.renderer->EndFrame();
					return;
				}
				logger.Infof("[BancHorsEcran] deux cibles %ux%u pretes\n", ctx.width, ctx.height);
			}
			if (heArme) {
				auto *tl = ctx.renderer->GetTextures();
				if ((ctx.frame & 1u) == 0u) {
					// trame PAIRE : la 3D part dans rtA.
					ctx.renderer->SetFinalColorTarget(tl->GetRHIHandle(st->rtA.GetColorResult()));
				} else {
					// trame IMPAIRE : rtA ECHANTILLONNEE plein ecran dans rtB.
					ctx.renderer->SetFinalColorTarget(tl->GetRHIHandle(st->rtB.GetColorResult()));
					if (auto *r2 = ctx.renderer->GetRender2D()) {
						r2->Begin(ctx.renderer->GetCmd(), ctx.width, ctx.height);
						// DrawOffscreen et non DrawSprite : rtA EST une cible hors ecran,
						// et son orientation stockee depend du dorsal. Mesure avant ce
						// changement, marque posee en haut a gauche : opengl la sortait a
						// y=695.5 sur 720, dx11 et dx12 a 23.5.
						r2->DrawOffscreen(NkRectF{0.f, 0.f, (float32)ctx.width, (float32)ctx.height},
										  st->rtA.GetColorResult());
						r2->End();
					}
					ctx.renderer->Present();
					ctx.renderer->EndFrame();
					if (!st->heFait && ctx.frame >= 5) {
						st->heFait = true;
						const char *sortie = ::nkentseu::env::GetEnvVar("NK_BANC_HE_SORTIE");
						const bool ok = st->rtB.Capture(sortie && sortie[0] ? sortie : "banc_he.png");
						logger.Infof("[BancHorsEcran] relecture de rtB : %s\n", ok ? "OK" : "ECHEC");
					}
					return;
				}
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
			} else if (st->vue == 1 || st->vue == 6) {
				// VUE CIEL : l'oeil a hauteur d'homme, l'horizon au milieu de
				// l'image. C'est le cadrage qui rend le profil vertical lisible —
				// la moitie haute est du ciel, du zenith vers l'horizon.
				// VUE 6 (halo) : le MEME cadrage, pour que la source tombe la ou
				// tombait le cube temoin (x ~769, y ~170 sur 1280x720).
				cd.position = {0.f, 1.6f, 0.f};
				cd.target = {0.f, 1.6f, -10.f};
				cd.fovY = 70.f;
				cd.farPlane = 500.f;
			} else if (st->vue == 5) {
				// VUE MIROIR : l'oeil BAS et le regard presque horizontal, pour que le
				// plan y=0 coupe l'image et qu'on voie le cube ET son reflet. Un
				// regard plongeant les aurait empiles ; un regard rasant aurait ecrase
				// le reflet contre l'horizon.
				cd.position = {0.f, 1.8f, 7.f};
				cd.target = {0.f, 1.2f, 0.f};
				cd.fovY = 55.f;
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
				// NK_BANC_AMBIANTE : ce que la SCENE declare comme ambiante.
				// Defaut 0 -- et c'est justement ce qu'on vient mettre a l'epreuve :
				// si le rendu ne bouge pas quand cette valeur passe de 0 a 10, alors
				// le champ est MORT et toutes les applications qui l'ecrivent se
				// racontent une histoire.
				sctx.ambientIntensity = BancFloat("NK_BANC_AMBIANTE", 0.f);
				// NK_BANC_IBL_VOL : la MEME force d'ambiante, mais posee EN VOL, par
				// le meme appel que le curseur << Ambiance > Intensite >> du panneau
				// de Rodolf (Demo3DHostSetAmbient -> NkRender3D::SetIBLStrength).
				// NK_BANC_IBL, lui, passe par la CONFIG, donc par la creation du
				// renderer. Si les deux chemins rendent la meme image, alors un appel
				// en cours de vie est honore -- et le curseur de Rodolf agit.
				{
					const char *vv = ::nkentseu::env::GetEnvVar("NK_BANC_IBL_VOL");
					if (vv && vv[0])
						r3d->SetIBLStrength(BancFloat("NK_BANC_IBL_VOL", 0.05f));
				}
				// NK_BANC_LUM_ENV : reproduit la case << Source : environnement >> de
				// Rodolf (ambianceParEnv). Le ciel ECLAIRE alors les objets ; c'est un
				// mecanisme SEPARE de sa visibilite en fond.
				const bool envAmb = BancInt("NK_BANC_LUM_ENV", 0) != 0;
				r3d->SetIBLUseEnv(envAmb);
				if (envAmb) {
					NkSkyParams sk;
					sk.model = (NkSkyModel)st->modeleCiel;
					sk.sunDirection = dirSoleil;
					sk.turbidity = 2.5f;
					sk.sunDisc = true;
					sk.sunIntensity = st->intensiteSoleil;
					sk.clouds = false;
					sk.starIntensity = 0.f;
					r3d->SetSkyParams(sk);
				}
				r3d->SetSkyboxEnabled(envAmb);
				r3d->BeginScene(sctx);
			} else if (st->vue == 6) {
				// ── VUE 6 : FOND UNI, UNE DIRECTIONNELLE QUI VIENT DE LA CAMERA ───
				// Propagation vers -Z : la lumiere ARRIVE du cote de l'oeil (z=0)
				// et frappe la face +Z du cube, celle que la camera voit, avec
				// N.L = 1. Aucune ombre, aucun ciel : tout ce qui depasse le seuil
				// de la passe brillante est la source, et rien d'autre. (Pas de
				// `sctx.ambientIntensity` ici : le champ est MORT, le moteur ne le
				// lit pas -- l'ambiante reelle est l'IBL de la config, 0,05.)
				NkLightDesc lum;
				lum.type = NkLightType::NK_DIRECTIONAL;
				lum.direction = {0.f, 0.f, -1.f};
				lum.color = {1.f, 1.f, 1.f};
				lum.intensity = BancFloat("NK_BANC_SOURCE_LUM", 3.f);
				lum.castShadow = false;
				sctx.lights.PushBack(lum);
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
				// NK_BANC_LUM_ALBEDO : la clarte de la surface. Le motif du sol de
				// `NkDemo3D` genere une texture a 208/255 ; le panneau, lui, affiche
				// 0.62. Un albedo n'est pas un detail : il MULTIPLIE l'ambiante.
				{
					const float32 alb = BancFloat("NK_BANC_LUM_ALBEDO", 0.62f);
					dc.tint = {alb, alb, alb};
				}
				dc.alpha = 1.f;
				dc.roughness = 0.90f;
				dc.metallic = 0.f;
				dc.castShadow = false;
				dc.receiveShadow = true;
				// Le chemin MATERIAU, quand il est arme. Les reglages sont les memes :
				// seule la porte change.
				if (st->matSol && st->matSol->IsValid())
					dc.material = st->matSol->GetInstHandle();
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

			// ── VUE 5 : LE TEMOIN DE REFLET PLAN ────────────────────────────────
			//
			// 🔴 CE QU'IL TRANCHE : la compensation #4, le flip Y de `mirrorViewProj`
			// sur DirectX. Un reflet plan est le seul chemin qui ECHANTILLONNE une
			// cible rendue par le pipeline 3D ; c'est donc le seul ou une convention
			// d'orientation se paie deux fois.
			//
			// ⚠️ IL SE MESURE RELATIVEMENT AU PLAN, jamais en absolu -- sur DX toute
			// l'image est retournee, un reflet qui bouge ne prouverait rien. Le cube
			// est a une hauteur CONNUE au-dessus de y=0 ; son reflet doit tomber a la
			// MEME distance du plan, de l'autre cote. C'est cette distance qu'on
			// compare entre dorsaux, pas la position a l'ecran.
			if (st->vue == 5) {
				if (st->matMiroir && st->matMiroir->IsValid()) {
					NkDrawCall3D sol;
					sol.mesh = meshSys->GetPlane();
					sol.transform = NkMat4f::Scale({30.f, 1.f, 30.f});
					sol.aabb = {{-30.f, -0.01f, -30.f}, {30.f, 0.01f, 30.f}};
					sol.material = st->matMiroir->GetInstHandle();
					sol.alpha = 1.f;
					sol.castShadow = false;
					sol.receiveShadow = false;
					r3d->Submit(sol);
				}
				// Le cube du temoin, pose HAUT et A DROITE au-dessus du miroir. Meme
				// regle que partout : asymetrique dans les deux axes.
				if (BancInt("NK_BANC_TEMOIN_3D", 0) != 0) {
					const float32 h = BancFloat("NK_BANC_MIROIR_HAUTEUR", 2.f);
					NkDrawCall3D cube;
					cube.mesh = meshSys->GetCube();
					cube.transform = NkMat4f::Translate({1.5f, h, 0.f});
					cube.aabb = {{1.f, h - 0.5f, -0.5f}, {2.f, h + 0.5f, 0.5f}};
					cube.tint = {1.f, 0.f, 0.f};
					cube.alpha = 1.f;
					cube.roughness = 1.f;
					cube.metallic = 0.f;
					cube.castShadow = false;
					cube.receiveShadow = false;
					r3d->Submit(cube);
				}
				ctx.renderer->Present();
				ctx.renderer->EndFrame();
				return;
			}

			// ── LE TEMOIN 3D : UN CUBE A POSITION CONNUE ────────────────────────
			//
			// 🔴 CE QU'IL TRANCHE. Le ciel s'inverse sur DirectX. Deux causes
			// possibles, et une seule est vraie :
			//   A -- TOUTE la passe 3D ecrit a l'envers ;
			//   B -- seule la reconstruction du RAYON DU CIEL est retournee
			//        (le ciel est le seul consommateur de `yFlipNDC` avec la
			//        grille infinie), et la geometrie, elle, sort a l'endroit.
			//
			// Un cube pose HAUT et A DROITE est asymetrique dans les DEUX axes,
			// comme la marque d'ecran -- et il traverse, LUI, la camera et la
			// projection, ce que la marque ne fait pas. Les deux ensemble donnent
			// la reference et l'objet dans la meme image.
			//
			// (3, 6, -12) avec l'oeil a (0, 1.6, 0) et un champ de 70 degres : le
			// cube monte a ~52 %% de la demi-hauteur au-dessus du centre et se pose
			// a ~20 %% de la demi-largeur a droite. Aucun doute de lecture possible.
			// Sa position dans l'image se DEDUIT par difference avec la meme image
			// sans lui : aucun jugement d'oeil, aucune couleur a reconnaitre.
			if (BancInt("NK_BANC_TEMOIN_3D", 0) != 0) {
				NkDrawCall3D cube;
				cube.mesh = meshSys->GetCube();
				cube.transform = NkMat4f::Translate({3.f, 6.f, -12.f});
				cube.aabb = {{2.5f, 5.5f, -12.5f}, {3.5f, 6.5f, -11.5f}};
				// NK_BANC_TEMOIN_ECLAT : la clarte du cube. Rouge a 1.0 (il se
				// distingue du ciel), BLANC et vif au-dela -- c'est ce qui le fait
				// passer le seuil du halo. La source du halo doit etre le cube et RIEN
				// d'autre : sinon on mesurerait le halo du soleil, qui ne traverse pas
				// la geometrie et ne dirait rien de la compensation qu'on vise.
				{
					const float32 ec = BancFloat("NK_BANC_TEMOIN_ECLAT", 1.f);
					cube.tint = (ec > 1.001f) ? NkVec3f{ec, ec, ec} : NkVec3f{1.f, 0.f, 0.f};
				}

				// NK_BANC_TEMOIN_ALPHA : l'opacite du cube temoin. Il n'a AUCUNE
				// instance de materiau, contrairement a l'occultant. Sous 0.999 il
				// entre dans la file transparente (NkRender3D::Submit) : c'est la
				// variable qui separe << la transparence seule >> de << la
				// transparence AVEC instance de materiau >>.
				cube.alpha = BancFloat("NK_BANC_TEMOIN_ALPHA", 1.f);
				cube.roughness = 1.f;
				cube.metallic = 0.f;
				cube.castShadow = false;
				cube.receiveShadow = false;
				r3d->Submit(cube);
			}

			// ── LA DALLE PLATE ──────────────────────────────────────────────────
			// Aucune instance de materiau : elle ne PEUT pas armer le tramage.
			// Elle recoit l'ombre, elle n'en projette pas.
			//
			// NK_BANC_SANS_SOL=1 la retire. En vue CIEL elle est la SEULE geometrie :
			// tant qu'elle est la, une bande sombre en haut de l'image peut venir du
			// sol autant que du ciel, et la lecture ne tranche rien.
			// ── VUE 6 : LA SOURCE DU HALO ───────────────────────────────────────
			// Un cube, pas un plan : `GetPlane()` est horizontal et la camera le
			// verrait par la tranche. La face +Z du cube regarde l'oeil et recoit
			// la directionnelle de plein fouet. Sa radiance HDR vaut
			// teinte x intensite x BRDF (Lambert, rugosite 1) : elle se LIT sur la
			// capture bloom eteint, elle n'est pas posee ici.
			if (st->vue == 6) {
				float32 px = 3.f, py = 6.f, pz = -12.f;
				if (const char *pv = ::nkentseu::env::GetEnvVar("NK_BANC_SOURCE_POS")) {
					float a = 0.f, b = 0.f, c = 0.f;
					if (pv[0] && std::sscanf(pv, "%f,%f,%f", &a, &b, &c) == 3) {
						px = a;
						py = b;
						pz = c;
					}
				}
				const float32 taille = BancFloat("NK_BANC_SOURCE_TAILLE", 1.f);
				const float32 teinte = BancFloat("NK_BANC_SOURCE", 1.f);
				NkDrawCall3D src;
				src.mesh = meshSys->GetCube();
				src.transform = NkMat4f::Translate({px, py, pz}) * NkMat4f::Scale({taille, taille, taille});
				src.aabb = {{px - taille, py - taille, pz - taille}, {px + taille, py + taille, pz + taille}};
				src.tint = {teinte, teinte, teinte};
				src.alpha = 1.f;
				src.roughness = 1.f;
				src.metallic = 0.f;
				src.castShadow = false;
				src.receiveShadow = false;
				r3d->Submit(src);
				// Par printf : le `logger` du banc n'atteint pas un stdout redirige
				// (Q188), et une capture dont on ne sait plus le reglage ne prouve rien.
				static bool sSourceDite = false;
				if (!sSourceDite) {
					sSourceDite = true;
					std::printf("[BancHalo] source teinte=%.3f taille=%.3f pos=(%.2f,%.2f,%.2f) lum=%.3f\n",
								teinte, taille, px, py, pz, BancFloat("NK_BANC_SOURCE_LUM", 3.f));
					std::fflush(stdout);
				}
			}

			// (vue 6 : ni sol ni occultant -- une surface etendue passerait le seuil
			// et c'est le halo du SOL qu'on mesurerait, comme en Q190.)
			if (st->vue != 6 && BancInt("NK_BANC_SANS_SOL", 0) == 0) {
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
			if (st->vue != 1 && st->vue != 6 && st->matOccultant && st->matOccultant->IsValid()) {
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
				// NK_BANC_OCC_OMBRE : l'occultant PROJETTE-T-IL une ombre ? (defaut 1)
				//
				// ⚠️ POURQUOI CETTE MANETTE EXISTE, ET CE QU'ELLE CORRIGE CHEZ MOI :
				// `NK_BANC_OMBRE_MODE` N'A AUCUN EFFET SUR UN OCCULTANT OPAQUE. Mesure
				// du 08/09 : a opacite 1.0, les modes 0 et 2 rendent la MEME image, au
				// pixel pres, sur les quatre dorsaux. C'est correct -- le mode ne
				// gouverne que le chemin TRANSPARENT -- mais j'ai perdu une course a
				// l'attendre. Qui posera le mode 0 sur un occultant opaque en attendra
				// un effet, comme moi.
				//
				// Elle sert a ISOLER LES DEUX CHEMINS D'ECRITURE de l'atlas d'ombres,
				// que le mode ne sait pas separer :
				//   opacite 1.00 -> `Shadow`      depth-only, JAMAIS negate
				//   opacite 0.12 -> `ShadowAlpha` varyings,   negate
				// Avec l'ombre eteinte pour reference, chaque chemin se soustrait
				// SEUL -- et l'echantillonnage, lui, ne bouge pas d'une course a
				// l'autre : un ecart ne peut donc venir que de l'ECRITURE.
				occ.castShadow = BancInt("NK_BANC_OCC_OMBRE", 1) != 0;
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

			// ── LA MARQUE : un motif ASYMETRIQUE en coordonnees ECRAN ───────────
			//
			// 🔑 UNE ORIENTATION SE MESURE AVEC UN MOTIF ASYMETRIQUE, PAS AVEC UNE
			// LECTURE DE CODE. Cinq etages ont ete elimines par comparaison de
			// configurations (API, sous-systeme hors ecran, descripteur de cible,
			// projection) sans jamais trouver ou les deux chemins divergent.
			//
			// Cette marque ne depend d'AUCUNE scene : un carre opaque pose au coin
			// HAUT-GAUCHE de l'ecran, en coordonnees d'ecran. S'il ressort en bas a
			// gauche, la cible est retournee — et le verdict ne peut pas etre
			// confirme par hasard, contrairement a un critere de bande saturee qui
			// confondrait un sol clair et un zenith sombre.
			//
			// ⚠️ Elle remplace mon critere precedent, dont j'avais signale la
			// fragilite avant de m'en servir ailleurs.
			{
				const char *mv = ::nkentseu::env::GetEnvVar("NK_BANC_MARQUE");
				if (mv && mv[0] && mv[0] != '0') {
					if (auto *r2d = ctx.renderer->GetRender2D()) {
						r2d->Begin(ctx.renderer->GetCmd(), ctx.width, ctx.height);
						// HAUT-GAUCHE, opaque, et un rectangle NON carre pour que
						// meme un retournement horizontal se voie.
						r2d->FillRect(NkRectF{4.f, 4.f, 120.f, 40.f}, NkVec4f{1.f, 0.f, 1.f, 1.f});
						r2d->End();
					}
				}
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
