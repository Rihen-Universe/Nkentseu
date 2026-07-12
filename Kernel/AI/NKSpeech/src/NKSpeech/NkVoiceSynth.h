// =============================================================================
// NKSpeech/NkVoiceSynth.h — synthèse vocale par FORMANTS (source-filtre) → onde.
//
// Brique TTS (Phase 8) : produire une voix AUDIBLE *from-scratch*, sans donnée ni
// modèle appris. Modèle source-filtre classique (à la Klatt), en DOMAINE TEMPOREL :
// une SOURCE (train d'impulsions glottiques à la fréquence fondamentale F0 pour les
// sons voisés, bruit large bande pour les non-voisés) excite trois RÉSONATEURS de
// FORMANTS en parallèle (F1/F2/F3, filtres numériques du 2nd ordre). Chaque résonateur
// « sonne » entre deux impulsions → un son SOUTENU (une vraie voyelle), pas un grain.
// (Le vocodeur Griffin-Lim NkGriffinLim reste une brique séparée, pour reconstruire une
// onde depuis un spectrogramme de magnitude — ex. mel produit par un futur modèle appris.)
//
// Petite échelle, pédagogique : ça « parle » des voyelles reconnaissables (a/e/i/o/u),
// pas une voix naturelle. Zero-STL, namespace nkentseu::ai.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {

		// Un « phone » à synthétiser : formants (Hz) + durée + voisement.
		struct NkPhone {
				float32 f1 = 0.0f, f2 = 0.0f, f3 = 0.0f; // formants (voyelle) ; 0 = pas de résonance
				float32 durationMs = 120.0f;
				bool voiced = true;   // true = source harmonique (F0) ; false = bruit (fricative)
				float32 gain = 1.0f;  // 0 = silence
		};

		// Profil de voix d'un PERSONNAGE : hauteur, timbre (longueur du conduit vocal) et
		// « naturel » (souffle, micro-variations). Deux personnages = deux profils.
		struct NkVoiceSynthConfig {
				int32 sampleRate = 16000;
				float32 f0 = 120.0f;			// fréquence fondamentale (hauteur de voix, Hz)
				float32 f0Jitter = 0.020f;		// micro-variation aléatoire de F0 (naturel, 0 = mécanique)
				float32 f0Drift = 0.06f;		// contour d'intonation (chute de hauteur en fin d'énoncé)
				float32 breathiness = 0.03f;	// souffle mêlé à la source voisée (0..~0.3)
				float32 vocalTractScale = 1.0f; // multiplicateur des FORMANTS : >1 conduit court (aigu/enfant), <1 grave/géant
				float32 rate = 1.0f;			// débit (multiplie la vitesse ; >1 = plus rapide)
				float32 openQuotient = 0.55f;	// largeur de l'impulsion glottique (fraction de période)
				float32 glideMs = 35.0f;		// durée d'interpolation des formants entre phones (coarticulation)
				int32 fftSize = 1024;			// (analyse du self-test seulement)
				int32 hopSize = 256;

				// Presets de personnages (timbre + hauteur). À ajuster librement.
				static NkVoiceSynthConfig Homme();  // voix grave d'homme
				static NkVoiceSynthConfig Femme();  // voix de femme
				static NkVoiceSynthConfig Enfant(); // voix d'enfant (conduit court, aigu)
				static NkVoiceSynthConfig Geant();  // voix de géant (grave, conduit long)
		};

		class NkVoiceSynth {
			public:
				// Voyelle française approximée : 'a','e','i','o','u','é' + 'w'=« ou » /u/ → NkPhone.
				// (Le 'u' est le /y/ français — antérieur arrondi, F2 haut — distinct de « ou ».)
				static NkPhone Vowel(char v, float32 durationMs);

				// Un PHONÈME (voyelle OU consonne) depuis un symbole ASCII (voir table dans le .cpp) :
				// voyelles a e i o u w(ou) E(é) · fricatives s f S(ch) v z Z(j) · nasales m n ·
				// liquides l r · plosives p t k b d g · silence '_'. Renvoie le NkPhone (formants,
				// voisement, gain). Les plosives sont approximées (brève occlusion + salve).
				static NkPhone Phoneme(const char *sym, float32 durationMs = 120.0f);

				// « Parle » une suite de phonèmes séparés par des espaces (ex. "p a p a", "s a l u",
				// "b o~ n j u r") → forme d'onde. Chaque token = un symbole de Phoneme(). Utilise
				// des durées par défaut (consonnes courtes, voyelles longues). Pas de vrai G2P
				// orthographique ici : on fournit les phonèmes (le G2P texte→phonèmes viendra).
				static NkVector<float32> Speak(const char *phonemes, const NkVoiceSynthConfig &cfg = NkVoiceSynthConfig{});

				// Synthétise une séquence de phones → forme d'onde mono [-1,1] (synthèse temporelle
				// source-filtre : impulsion glottique lissée + souffle + jitter → résonateurs de
				// formants interpolés). Renvoie les échantillons.
				static NkVector<float32> Synthesize(const NkVector<NkPhone> &phones,
													const NkVoiceSynthConfig &cfg = NkVoiceSynthConfig{});

				// Auto-test headless : synthétise la voyelle 'a' et vérifie que le spectre du signal
				// produit présente de l'énergie autour de F1/F2 et que le son est SOUTENU (pas des clics).
				static bool SelfTest();
		};

	} // namespace ai
} // namespace nkentseu
