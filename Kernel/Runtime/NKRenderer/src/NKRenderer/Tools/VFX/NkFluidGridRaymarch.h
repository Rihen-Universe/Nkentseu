#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkFluidGridRaymarch.h — RENDU DE VOLUME d'une NkFluidGrid par MARCHE DE RAYON.
//
// MÉTHODE : intégration de l'équation du transfert radiatif le long du rayon,
// en avant-vers-arrière (front-to-back), telle qu'elle est écrite chez
// James T. KAJIYA & Brian P. VON HERZEN, « Ray Tracing Volume Densities »,
// SIGGRAPH 1984, p. 165-174 : à chaque échantillon, une ABSORPTION
// (transmittance exp(-sigma_t ds)) et une DIFFUSION simple (single scattering)
// éclairée par UNE lumière, dont l'ombre interne est obtenue par une SECONDE
// marche vers la lumière (le « shadow ray » du même article). La marche vers la
// lumière est faite (`shadowMarch`, vraie par défaut) et son pas est plus grand
// que celui de la marche principale — dit, pas caché.
//
// ⚠️ CE QUE CE FICHIER EST, ET CE QU'IL N'EST PAS. C'est le rendu de RÉFÉRENCE,
// sur CPU : il n'ouvre ni fenêtre ni device, il rend dans un tampon RGBA8 que
// l'appelant écrit où il veut. C'est ce qui permet à un banc de JUGER DES PIXELS
// sans GPU. Le portage sur NKRHI/NkSL (texture 3D + compute) n'est PAS fait ;
// il est nommé, pas sous-entendu.
//
// ZÉRO STL : NkVector, NKMath. Aucun std::.
// =============================================================================
#include "NkFluidGrid.h"

namespace nkentseu {
	namespace renderer {

		struct NkFluidRaymarchParams {
				uint32 width = 480;
				uint32 height = 360;

				math::NkVec3f cameraPos = {0.f, 0.9f, 2.2f};
				math::NkVec3f cameraTarget = {0.f, 0.7f, 0.f};
				math::NkVec3f up = {0.f, 1.f, 0.f};
				float32 fovDegrees = 40.f;

				// Pas de marche (m). 0 = moitié d'une cellule (critère de Nyquist sur la
				// grille : deux échantillons par cellule).
				float32 stepSize = 0.f;
				// Pas de la marche d'OMBRE, en multiples du pas principal.
				float32 shadowStepFactor = 3.f;
				bool shadowMarch = true;
				// Longueur MAXIMALE de la marche d'ombre (m). 0 = jusqu'a la sortie de la
				// boite. MESURE le 05/09 : sans borne, l'ombre coute 5 fois la marche
				// principale (14,6 M echantillons contre 2,9 M a 480x360).
				float32 shadowMaxDistance = 0.f;

				// Coefficients d'extinction, par unité de densité et par mètre (1/m).
				float32 absorption = 8.f;
				float32 scattering = 22.f;
				// Albédo de la fumée (couleur diffusée).
				math::NkVec3f albedo = {0.92f, 0.92f, 0.95f};

				// Lumière directionnelle : direction VERS la lumière.
				math::NkVec3f lightDir = {-0.5f, 0.8f, 0.35f};
				math::NkVec3f lightColor = {1.35f, 1.30f, 1.20f};
				math::NkVec3f ambient = {0.16f, 0.18f, 0.22f};

				math::NkVec3f background = {0.05f, 0.055f, 0.07f};

				// On arrête le rayon quand il ne reste plus rien à voir.
				float32 transmittanceCutoff = 0.004f;

				// ── FEU (palier ③) : émission par la TEMPÉRATURE, couleur du corps noir.
				// Fausse par défaut : une grille de fumée pure ne doit rien émettre.
				bool emission = false;
				// En dessous de ce seuil (K), on n'émet rien du tout.
				float32 emissionMinTemperature = 800.f;
				// Facteur d'échelle de la luminance émise (sans unité).
				float32 emissionStrength = 1.f;
				// L'émission suit-elle la densité de suie, ou seulement la température ?
				bool emissionUsesDensity = false;
		};

		struct NkFluidRaymarchStats {
				uint32 rays = 0;
				uint32 raysHit = 0;		 // rayons qui coupent la boîte du volume
				uint64 samples = 0;		 // échantillons de la marche principale
				uint64 shadowSamples = 0; // échantillons de la marche d'ombre
				float32 ms = 0.f;
		};

		// Rend dans `rgba` (4 octets par pixel, ligne du haut en premier).
		// `rgba` est redimensionné à width*height*4.
		//
		// `maxTempOut`, s'il est non nul, reçoit la TEMPÉRATURE MAXIMALE rencontrée le
		// long du rayon de chaque pixel (un float32 par pixel, 0 si le rayon manque le
		// volume). Ce n'est pas un extra : c'est ce qui permet à un banc de juger
		// « la couleur suit la température » SANS refaire le lancer de rayon de son
		// côté — une sonde qui rejoue une partie de la stratégie du code mesure autre
		// chose que ce code.
		void NkFluidRaymarchRender(const NkFluidGrid &grid, const NkFluidRaymarchParams &p, NkVector<uint8> &rgba,
								   NkFluidRaymarchStats &stats, NkVector<float32> *maxTempOut = nullptr);

		// ── TABLE DU CORPS NOIR (palier ③) ───────────────────────────────────────
		// La couleur d'un corps noir à la température T n'est PAS inventée ici :
		// elle est CALCULÉE, à chaque appel, à partir
		//   1. de la LOI DE PLANCK — Max PLANCK, « Über das Gesetz der
		//      Energieverteilung im Normalspectrum », Annalen der Physik 4, 1901,
		//      p. 553-563 :
		//          B(lambda, T) = (2 h c^2 / lambda^5) / (exp(h c / (lambda k T)) - 1)
		//      avec h, c, k les constantes SI exactes du BIPM (SI 2019) ;
		//   2. des FONCTIONS COLORIMÉTRIQUES CIE 1931 2°, approchées par les
		//      GAUSSIENNES MULTI-LOBES de Chris WYMAN, Peter-Pike SLOAN & Peter
		//      SHIRLEY, « Simple Analytic Approximations to the CIE XYZ Color
		//      Matching Functions », Journal of Computer Graphics Techniques (JCGT),
		//      vol. 2, n° 2, 2013, p. 1-11 — équations (2), (3), (4) et leur tableau ;
		//   3. de la matrice XYZ -> sRGB linéaire de la norme IEC 61966-2-1 (sRGB,
		//      primaires Rec. 709, blanc D65), puis de l'encodage gamma sRGB de cette
		//      même norme.
		// Le résultat est NORMALISÉ (la composante maximale vaut 1) : c'est une
		// CHROMATICITÉ, pas une luminance — la luminance vient de `emissionStrength`.
		// Le banc vérifie ce chemin contre trois faits INDÉPENDANTS de la table :
		// la loi de Wien, la monotonie de la teinte, et le blanc de D65.
		math::NkVec3f NkBlackBodyColor(float32 kelvin);
		// La luminance relative (Stefan-Boltzmann, sigma T^4), normalisée à 1 pour
		// `reference` kelvins. Josef STEFAN 1879 / Ludwig BOLTZMANN 1884.
		float32 NkBlackBodyRelativeRadiance(float32 kelvin, float32 reference);
		// La longueur d'onde du maximum de Planck (m) — loi du déplacement de WIEN,
		// constante b = 2,897 771 955e-3 m.K (CODATA).
		float32 NkWienPeakWavelength(float32 kelvin);

		// ── INSTRUMENTS DE CONTROLE du chemin colorimetrique (le banc les appelle) ──
		// La chromaticite CIE xy d'un corps noir : ce que le banc compare a D65.
		void NkBlackBodyChromaticity(float32 kelvin, float32 &x, float32 &y);
		// La chromaticite d'un spectre d'ENERGIE EGALE : doit tomber sur x = y = 1/3
		// (point blanc E). Controle des fonctions colorimetriques SEULES, sans Planck.
		void NkEqualEnergyChromaticity(float32 &x, float32 &y);
		// Le maximum de Planck trouve PAR BALAYAGE, a comparer a la loi de Wien :
		// controle de l'implementation de Planck SEULE, sans la colorimetrie.
		float32 NkPlanckPeakWavelengthScanned(float32 kelvin, float32 nmMin, float32 nmMax, float32 nmStep);

	} // namespace renderer
} // namespace nkentseu
