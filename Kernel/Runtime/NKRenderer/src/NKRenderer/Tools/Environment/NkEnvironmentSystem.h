#pragma once
// =============================================================================
// NkEnvironmentSystem.h  — NKRenderer v5.0  (Tools/Environment/)
//
// Image-Based Lighting (IBL) ressources :
//   - Irradiance cubemap (diffuse pre-integrated, convolution Lambert)
//   - Prefilter cubemap  (specular GGX pre-integrated, mips par roughness)
//   - BRDF LUT 2D        (split-sum Karis)
//
// Etat actuel : convolution CPU complete (Phase D.2d livree). Le compute GPU
// pour les convolutions est reporté a Phase N v1 (gain init 0.5-2s -> <50ms).
//
// Source IBL parametrable via NkEnvironmentConfig::source :
//   - NK_ENV_PROCEDURAL : gradient sky (skyTop/horizon/ground), default
//   - NK_ENV_HDR_FILE   : .hdr equirect 360 charge depuis hdrPath
//   - NK_ENV_NONE       : pas d'auto-load, l'app appelle LoadProcedural()
//                          ou LoadFromHDR() explicitement plus tard
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace renderer {

		// Source de l'environnement IBL choisie au Init par l'application.
		enum class NkEnvSource : uint8 {
			NK_ENV_PROCEDURAL = 0, // gradient sky parametrable (default)
			NK_ENV_HDR_FILE = 1,   // charge un .hdr equirect 360 depuis hdrPath
			NK_ENV_NONE = 2,	   // pas d'auto-load (textures creées mais vides)
		};

		struct NkEnvironmentConfig {
				uint32 irradianceSize = 32; // taille du cubemap irradiance (D.2b : 32-64)
				uint32 prefilterSize = 128; // taille du cubemap specular (D.2b : 128-512)
				uint32 prefilterMips = 5;	// niveaux de mips du specular cubemap
				uint32 brdfLUTSize = 256;	// taille du LUT 2D (D.2b : 256x256)
				// Cache disque : active par defaut, fichiers dans le repertoire courant.
				// Mettre cacheDir = "" ou enableCache = false pour desactiver.
				bool enableCache = true;
				const char *cacheDir = ""; // "" = repertoire courant

				// Phase N v1 : convolutions irradiance + prefilter sur GPU
				// (compute NkSL, cf. NkIBLCompute). Fallback CPU automatique si
				// le backend n'a pas de compute ou si un kernel echoue.
				// Override runtime : NK_IBL_GPU=0 force le CPU ;
				// NK_IBL_VERIFY=1 execute AUSSI le CPU et loggue l'ecart max.
				bool gpuConvolution = true;

				// ── Source IBL (Phase N v0) ─────────────────────────────────────
				// L'app choisit comment Init() initialise l'IBL. Retro-compat :
				// default = PROCEDURAL avec les couleurs ci-dessous.
				NkEnvSource source = NkEnvSource::NK_ENV_PROCEDURAL;

				// Parametres du gradient sky (utilises si source == PROCEDURAL).
				NkVec3f skyTop = {0.40f, 0.55f, 0.80f};
				NkVec3f horizon = {0.45f, 0.48f, 0.52f};
				NkVec3f ground = {0.10f, 0.08f, 0.06f};

				// Chemin du .hdr equirect (utilise si source == HDR_FILE).
				// Le fichier doit etre une projection equirectangulaire 360 RGB96F.
				// Ex: "Resources/HDRI/studio.hdr" (PolyHaven ou similaire).
				NkString hdrPath = "";
		};

		// ── MODELES DE CIEL PROCEDURAL ──────────────────────────────────────────
		// Le degrade a trois couleurs etait le SEUL ciel disponible : pas de
		// soleil, pas de diffusion, pas de nuage. Ce n'etait pas casse, ce
		// n'etait pas implemente -- Rihen a demande les trois, au choix.
		enum class NkSkyModel : uint8 {
			// Degrade zenith -> horizon -> sol selon la hauteur du rayon.
			// C'est l'ancien comportement, et il reste le DEFAUT : les scenes
			// existantes ne doivent pas changer d'aspect sans qu'on le demande.
			NK_SKY_GRADIENT = 0,
			// Ciel physique analytique (Preetham et al., 1999). Un vrai modele
			// de diffusion atmospherique : la couleur depend de la position du
			// soleil et de la turbidite de l'air. C'est lui qui donne le bleu
			// profond au zenith, le blanchiment vers l'horizon et les teintes
			// chaudes au couchant -- gratuitement, parce qu'elles sortent du
			// modele et non d'un reglage.
			NK_SKY_PHYSICAL = 1,
			// Diffusion simple Rayleigh + Mie, integree le long du rayon. Ecrite
			// DEPUIS LA PHYSIQUE et non depuis une table de coefficients : chaque
			// terme se verifie, ce qui n'est pas le cas d'un modele tabule.
			//
			// Ce qu'elle apporte de plus que Preetham : le soleil SOUS L'HORIZON.
			// Preetham n'est pas defini pour un soleil couche (son terme en
			// exp(B/cos theta) diverge) ; ici l'integration continue de
			// fonctionner et donne le crepuscule, puis la nuit. C'est exactement
			// ce dont un cycle jour/nuit a besoin.
			NK_SKY_ATMOSPHERE = 2,
			// Hosek & Wilkie, SIGGRAPH 2012 — le modele MESURE. Ses tables de
			// coefficients (BSD 3-clauses, copie VERBATIM verifiee par empreinte
			// SHA-256 depuis la distribution officielle 1.4a) sont cuites cote
			// CPU en 9 coefficients par canal ; le shader n'evalue que la
			// formule. C'est le plus fidele des quatre en plein jour ; comme
			// Preetham, il n'est pas defini sous l'horizon (l'atmosphere
			// Rayleigh + Mie reste le modele des crepuscules).
			NK_SKY_HOSEK = 3,
			// Modele de PRAGUE (« A Fitted Radiance and Attenuation Model for
			// Realistic Atmospheres », SIGGRAPH 2021) — variante sol/XYZ, BSD
			// 3-clauses, jeu de donnees de 18,4 Mo copie verbatim (SHA-256
			// verifiee) dans Resources/NKRenderer/Sky/SkyModelDataset.dat.
			// C'est le successeur MESURE de Hosek par la meme equipe, et il est
			// defini jusqu'a -4,2 DEGRES SOUS L'HORIZON : les couchants mesures.
			// Trop couteux pour un pixel shader : il est CUIT en cubemap a la
			// regeneration (le shader l'affiche via le chemin cubemap, avec les
			// surcouches etoiles/lunes/nuages par-dessus).
			NK_SKY_PRAGUE = 4,
			// SOLEIL ALIEN — la fonctionnalite « Alien World » du code Hosek
			// officiel (cuisson SPECTRALE + corps noir a la temperature
			// demandee). Un soleil de 3 000 K rougit LE CIEL ENTIER, pas
			// seulement son disque : c'est le monde qui change d'etoile.
			// Cuit en cubemap comme Prague, memes surcouches animees.
			NK_SKY_ALIEN = 5,
		};

		// ── Coefficients cuits du modele Hosek-Wilkie ───────────────────────
		// Resultat de l'interpolation des tables pour UN triplet (turbidite,
		// albedo, elevation du soleil). C'est CE qui descend au GPU — dix vec4
		// — et non les tables elles-memes (65 Ko).
		struct NkHosekSkyCoeffs {
				NkVec3f coef[9];  // A..I de la formule etendue, par canal RGB
				NkVec3f radiance; // facteur de radiance par canal
		};

		// Cuisson : interpolation de Bezier quintique sur l'elevation, lineaire
		// sur la turbidite et l'albedo — adaptee du code officiel BSD (v1.4a,
		// ArHosekSkyModel.c), tables incluses telles quelles. L'elevation est
		// attendue >= 0 : le modele n'est pas defini sous l'horizon.
		void NkHosekCookRGB(float32 turbidity, const NkVec3f &albedo, float32 sunElevRad,
							NkHosekSkyCoeffs &out);
		// Evaluation CPU de la formule (meme calcul que le shader). Sert a la
		// NORMALISATION : on evalue une reference pour caler l'exposition sans
		// avoir a regler un facteur a l'oeil.
		NkVec3f NkHosekEvalRGB(const NkHosekSkyCoeffs &c, float32 cosTheta, float32 gamma);

		// Parametres complets du ciel procedural. La COUCHE DE NUAGES est
		// independante du modele de base : on peut couvrir aussi bien un degrade
		// stylise qu'un ciel physique.
		struct NkSkyParams {
				NkSkyModel model = NkSkyModel::NK_SKY_GRADIENT;

				// ── Degrade (model == GRADIENT) ─────────────────────────────
				NkVec3f skyTop = {0.40f, 0.55f, 0.80f};
				NkVec3f horizon = {0.45f, 0.48f, 0.52f};
				NkVec3f ground = {0.10f, 0.08f, 0.06f};

				// ── Soleil ──────────────────────────────────────────────────
				// DIRECTION DE PROPAGATION, comme pour les lumieres du moteur :
				// un soleil au zenith descend, donc (0,-1,0). Garder la meme
				// convention que NkLightDesc::direction evite d'avoir a se
				// demander laquelle s'applique quand on branchera le ciel sur
				// le soleil de la scene.
				NkVec3f sunDirection = {0.35f, -0.65f, 0.35f};
				// Turbidite : 1 = air de haute montagne, 2-3 = ciel clair,
				// 6-10 = atmosphere chargee/brumeuse. Au-dela le modele perd
				// son sens physique.
				float32 turbidity = 2.5f;
				// Le DISQUE solaire lui-meme. Sans lui le ciel physique n'a pas
				// de source visible, ce qui se remarque immediatement.
				bool sunDisc = true;
				float32 sunIntensity = 1.f;
				// TEINTE du soleil. Le modele physique deduit deja la couleur du
				// ciel de la position du soleil et de la turbidite ; celle-ci
				// s'applique au DISQUE, pour un soleil volontairement chaud ou
				// froid sans avoir a mentir sur la turbidite. Blanc = neutre.
				NkVec3f sunColor = {1.f, 1.f, 1.f};

				// ── Couche de nuages (optionnelle, par-dessus le modele) ────
				bool clouds = false;
				// Couverture : 0 = ciel degage, 1 = entierement couvert. C'est
				// un SEUIL sur le bruit, pas une opacite -- d'ou son nom.
				float32 cloudCoverage = 0.5f;
				// Densite : opacite des nuages une fois formes.
				float32 cloudDensity = 1.f;
				// Echelle : taille des motifs. Petit = gros nuages.
				float32 cloudScale = 2.f;
				NkVec3f cloudColor = {1.f, 1.f, 1.f};
				// VITESSE DE DEFILEMENT des nuages, en unites de bruit par seconde.
				// Sans effet sur la cuisson CPU (une cubemap est une image fixe) :
				// elle n'a de sens que pour le ciel evalue EN TEMPS REEL dans le
				// shader. Elle vit ici quand meme, parce que « ce qu'est ce ciel »
				// doit se decrire a UN seul endroit -- sinon les deux chemins
				// finissent par decrire deux ciels differents.
				float32 cloudSpeed = 0.02f;

				// ── Etoiles ─────────────────────────────────────────────────
				// Elles s'effacent TOUTES SEULES quand le ciel s'eclaire : leur
				// visibilite est l'inverse de la luminosite locale. Un cycle
				// jour/nuit les fera donc apparaitre et disparaitre sans qu'on
				// ait a les piloter. Comme cloudSpeed, elles n'ont de sens que
				// pour le ciel evalue en temps reel.
				float32 starIntensity = 0.f; // 0 = aucune
				float32 starDensity = 200.f; // plus grand = plus fines et nombreuses
				// Temperature de surface de l'etoile (modele SOLEIL ALIEN).
				// 5 778 K = notre soleil ; 3 000 K = naine rouge ; 15 000 K =
				// etoile bleue. La teinte du MONDE entier en decoule.
				float32 alienTempK = 5778.f;
				// ROTATION CELESTE, en radians par seconde. Le champ d'etoiles
				// tourne lentement, comme la voute vue du sol. A 0 il est fige.
				// Une valeur realiste est minuscule (2*PI / 86164 s) ; pour un
				// film on l'accelere sans complexe, c'est le but d'un reglage.
				float32 starRotation = 0.f;
				// ETOILES FILANTES : nombre d'apparitions par minute, en moyenne.
				// 0 = aucune. Elles sont TIREES DU TEMPS, pas d'un generateur
				// aleatoire : chaque creneau horaire produit toujours la meme
				// etoile filante, donc une capture se rejoue a l'identique.
				float32 shootingRate = 0.f;

				// ── Lunes ───────────────────────────────────────────────────
				// PLUSIEURS sont possibles : c'est un tableau, pas un cas
				// particulier. Une lune de plus ne coute qu'une iteration.
				//
				// LEUR PHASE N'EST PAS UN REGLAGE : elle se DEDUIT de la position
				// du soleil. Une lune est une sphere eclairee par lui ; le
				// croissant sort donc du calcul, et il change tout seul quand le
				// soleil descend. Un curseur de phase aurait permis d'afficher un
				// croissant incoherent avec l'eclairage de la scene -- exactement
				// le genre d'etat affiche sans rapport avec l'etat effectif qu'on
				// a passe la soiree a supprimer.
				static constexpr int32 kMaxMoons = 2;
				struct NkSkyMoon {
						// Direction VERS la lune (et non de propagation, contrairement
						// au soleil) : c'est « ou elle est dans le ciel », qui est la
						// facon dont on y pense.
						NkVec3f direction = {0.4f, 0.5f, -0.75f};
						float32 angularSize = 0.03f; // rayon apparent, en radians
						NkVec3f color = {1.f, 0.97f, 0.92f};
						float32 brightness = 1.f;
						// PHASE FORCEE, en option. Par defaut la phase se DEDUIT du
						// soleil : la lune est une sphere qu'il eclaire, le croissant
						// sort du calcul et change tout seul. C'est ce qu'on veut
						// quand on cherche la coherence.
						//
						// Mais un film n'a pas toujours ce besoin : forcer un
						// croissant est un CHOIX DE MISE EN SCENE legitime. On
						// l'autorise donc, explicitement -- ce n'est plus un etat
						// affiche par accident, c'est une intention declaree.
						bool manualPhase = false;
						// -1 = nouvelle lune a gauche, 0 = pleine, +1 = nouvelle a
						// droite. L'angle du terminateur, en somme.
						float32 phase = 0.25f;
				};
				int32 moonCount = 0; // 0 = aucune
				NkSkyMoon moons[kMaxMoons];
		};

		class NkEnvironmentSystem {
			public:
				NkEnvironmentSystem() = default;
				~NkEnvironmentSystem();

				bool Init(NkIDevice *device, const NkEnvironmentConfig &cfg = {});
				void Shutdown();

				// Genere une cubemap procedurale gradient sky -> horizon -> ground
				// et l'upload dans mIrradiance + mPrefilter. Utile comme placeholder
				// ou pour des scenes stylisees sans HDR realiste.
				void LoadProcedural(const NkVec3f &skyTop, const NkVec3f &horizon, const NkVec3f &ground);

				// Version COMPLETE : modele de ciel au choix (degrade ou physique),
				// disque solaire et couche de nuages. LoadProcedural ci-dessus s'y
				// ramene avec model = GRADIENT — les appels existants sont donc
				// inchanges, y compris le contenu genere.
				void LoadProceduralEx(const NkSkyParams &params);

				// ── MODELES CUITS EN QUASI TEMPS REEL (Prague, soleil alien) ─
				// Recuit la table du modele (mesure : ~11 ms pour Prague) et
				// reecrit la SEULE cubemap visible — sans les convolutions
				// d'eclairage, qui restent sur la regeneration complete. C'est
				// ce qui permet au ciel MESURE de suivre le soleil en continu :
				// la partie lente n'a jamais ete le modele, c'etait
				// l'irradiance. Rend faux si le modele n'est pas cuit ou si ses
				// donnees manquent.
				bool RefreshBakedSkyVisual(const NkSkyParams &params);

				// Phase N v0 : charge un .hdr equirectangulaire (360 RGB96F) et
				// l'utilise comme source pour les convolutions irradiance +
				// prefilter. CPU-side (tout comme LoadProcedural) ; future v1
				// portera la conversion en compute shader GPU. Retourne true
				// si le chargement et la convolution ont reussi.
				bool LoadFromHDR(const NkString &path);

				// Accesseurs RHI pour Render3D / NkMaterialSystem (binding 8/9/10 du shader PBR).
				NkTextureHandle GetIrradianceCubemap() const {
					return mIrradiance;
				}

				NkTextureHandle GetPrefilterCubemap() const {
					return mPrefilter;
				}

				NkTextureHandle GetBRDFLUT() const {
					return mBrdfLUT;
				}

				NkSamplerHandle GetEnvSampler() const {
					return mEnvSampler;
				}

				NkSamplerHandle GetLUTSampler() const {
					return mLutSampler;
				}

				// Phase N v1 : cubemap dedie au skybox (RGBA32F, mip 0, sans
				// Reinhard tonemap) pour preserver le vrai dynamic range HDR
				// dans le background. Le tEnvPrefilter (binding=9) reste
				// utilise par le shader PBR pour l'IBL specular (avec tonemap
				// necessaire pour eviter le clamp blanc des reflexions).
				// En source PROCEDURAL, ce cubemap est rempli avec le gradient
				// sky pour que le skybox reste utilisable sans HDR.
				NkTextureHandle GetSkyEnvCube() const {
					return mSkyEnvCube;
				}

			private:
				NkIDevice *mDevice = nullptr;
				NkEnvironmentConfig mCfg;

				NkTextureHandle mIrradiance; // samplerCube (binding 8)
				NkTextureHandle mPrefilter;	 // samplerCube (binding 9, mip-mapped)
				NkTextureHandle mBrdfLUT;	 // sampler2D   (binding 10)
				NkTextureHandle mSkyEnvCube; // samplerCube (binding 11, RGBA32F, HDR brut)

				NkSamplerHandle mEnvSampler; // linear clamp pour cubemaps
				NkSamplerHandle mLutSampler; // linear clamp pour BRDF LUT
		};

	} // namespace renderer
} // namespace nkentseu
