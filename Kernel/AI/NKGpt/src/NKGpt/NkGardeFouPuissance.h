// =============================================================================
// NKGpt/NkGardeFouPuissance.h — plafond de puissance GPU applique par le produit
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
//
// POURQUOI CE FICHIER EXISTE
// --------------------------
// Le 2026-09-17 a 21:23:38 la machine s'est eteinte SANS verification de bogue
// (Kernel-Power 41 avec BugcheckCode = 0, EventLog 6008 « arret imprevu »), trois
// minutes apres le depart d'une course d'entrainement. Le releve de puissance,
// echantillonne toutes les 20 s, montre 19,3 W au repos puis 63,9 -> 85,2 -> 91,9 W
// en quarante secondes ; le dernier echantillon est a quatre secondes de
// l'extinction, et la courbe montait encore. La batterie de la machine est
// electriquement morte (RemainingCapacity 0, ni charge ni decharge, secteur
// present) : elle ne tamponne plus aucun pic.
//
// La limite materielle n'est PAS reglable sur cette carte :
//   nvidia-smi -pl 90  ->  « Changing power management limit is not supported in
//                            current scope »
// Le plafond doit donc vivre dans NOTRE code, et c'est ce que fait ce fichier.
//
// CE QU'IL EST, ET CE QU'IL N'EST PAS
// -----------------------------------
// C'est un PANSEMENT. Il reduit le risque, il ne repare rien. Le vrai correctif
// est materiel : remplacer la batterie morte, ou alimenter la machine avec un
// adaptateur qui encaisse le pic. Aucun message de ce fichier ne doit laisser
// croire le contraire.
//
// ⚠️ Il ne protege pas non plus des deux AUTRES coupures du 17/09 (18:14 et 19:34),
// qui portaient toutes deux la verification de bogue 0x119
// (VIDEO_SCHEDULER_INTERNAL_ERROR) : celles-la sont des plantages du pilote, un
// defaut different, instruit ailleurs dans echanges/ilyana-etat.questions.md.
//
// LE REGLAGE EST ECRIT, PAS CACHE
// -------------------------------
// Toutes les valeurs viennent d'un fichier texte `garde_fou_puissance.conf`,
// cherche dans cet ordre :
//   1. le chemin donne par la variable d'environnement NK_ILYANA_GARDE_FOU_CONF
//   2. <dossier du checkpoint de sauvegarde>/garde_fou_puissance.conf
//   3. ./garde_fou_puissance.conf
// Le journal imprime le chemin retenu, chaque valeur retenue ET sa provenance
// (fichier ou defaut). Une valeur dont on ne sait pas d'ou elle vient est une
// constante cachee, meme quand elle est juste.
//
// ⚠️ LE PLAFOND PAR DEFAUT N'EST PAS DERIVE D'UNE MESURE DU SEUIL DE COUPURE,
// parce que ce seuil N'EST PAS MESURE. 70 W, c'est 76 % du dernier point releve
// avant l'extinction (91,86 W) et 56 % de la limite par defaut de la carte
// (125 W). C'est un reglage a ajuster par l'experience — le descendre tant que
// les coupures reviennent — et c'est exactement pour cela qu'il est dans un
// fichier et non dans ce code.
//
// LA MUTATION
// -----------
// NK_ILYANA_GARDE_FOU=0 desarme completement le garde-fou et l'annonce dans le
// journal. Elle sert a prouver que ce code deplace vraiment la consommation : si
// la course desarmee ne consomme PAS davantage, le garde-fou est un placebo et il
// faut le retirer. CONDITION DE RETRAIT de l'interrupteur : le jour ou la machine
// aura une alimentation saine et ou ce plafond n'aura plus lieu d'etre.
// =============================================================================

#pragma once

#ifndef NKENTSEU_AI_GPT_NKGARDEFOUPUISSANCE_H
#define NKENTSEU_AI_GPT_NKGARDEFOUPUISSANCE_H

#include "NKCore/NkTypes.h"
#include "NKTime/NkChrono.h"

namespace nkentseu {
	namespace ai {
		namespace gpt {

			// Un echantillon de l'etat electrique et thermique de la carte.
			struct NkEtatCarte {
					double watts = 0.0;
					double celsius = 0.0;
					bool valide = false;
			};

			class NkGardeFouPuissance {
				public:
					NkGardeFouPuissance();
					~NkGardeFouPuissance();

					// Lit le reglage, ouvre NVML, et JOURNALISE tout ce qu'il a retenu
					// (chemin du fichier, chaque valeur et sa provenance, l'etat de NVML).
					// `cheminSauvegarde` sert uniquement a deduire le dossier ou chercher
					// le fichier de reglage ; il peut etre nul.
					void Ouvrir(const char *cheminSauvegarde);

					// Vrai seulement si le garde-fou peut REELLEMENT freiner : reglage lu,
					// NVML ouvert, interrupteur non desarme. Faux se dit dans le journal,
					// jamais en silence.
					bool Actif() const {
						return mActif;
					}

					// A appeler a chaque frontiere de micro-lot. Echantillonne, met a jour la
					// moyenne glissante, ajuste le rapport de repos, et DORT si necessaire.
					// Rend le nombre de millisecondes de repos reellement appliquees (0 si
					// aucun freinage).
					double FreinerSiNecessaire();

					// Bilan de fin de course : ce qui a ete mesure, et ce que le garde-fou a
					// coute. Il dit explicitement « aucun freinage » quand c'est le cas —
					// c'est le zero, et il doit se lire.
					// Appele aussi par le destructeur si personne ne l'a fait : ENUMERER LES
					// SORTIES plutot que se fier a l'appelant. Fit() a un `return` anticipe
					// (arret propre) qui sauterait ce bilan.
					void Resume();

					double MoyenneWatts() const;
					double PicWatts() const {
						return mPicWatts;
					}
					double PicCelsius() const {
						return mPicCelsius;
					}
					double ReposCumuleMs() const {
						return mReposCumuleMs;
					}
					int64 NombreEchantillons() const {
						return mNbEchantillons;
					}
					int64 NombreFreinages() const {
						return mNbFreinages;
					}
					double PlafondWatts() const {
						return mPlafondWatts;
					}
					// Cout propre du garde-fou quand il ne freine pas : le temps passe a lire
					// NVML. Mesure, pas estime.
					double CoutLecturesMs() const {
						return mCoutLecturesMs;
					}

				private:
					NkEtatCarte Lire();
					void ChargerReglage(const char *cheminSauvegarde);
					bool OuvrirNvml();
					void FermerNvml();

					// --- reglage (tout vient du fichier, tout est journalise) ---
					double mPlafondWatts = 70.0;
					double mTemperatureMaxC = 80.0;
					double mMargeWatts = 5.0;
					double mReposMaxRatio = 3.0;	 // repos <= 3x le temps de calcul
					double mPasMontee = 0.20;		 // + 20 points de repos par depassement
					double mPasDescente = 0.05;		 // - 5 points quand on est sous le plafond
					double mFenetreSecondes = 10.0;	 // constante de temps de la moyenne glissante
					double mJournalSecondes = 30.0;	 // cadence du rappel dans le journal
					// ATTENTION -- BORNE SUR UNE PAUSE. Le premier micro-lot d'un pas mesure AUSSI la mise
					// a jour d'Adam, la journalisation et le repos deja pris : sa duree n'est
					// plus celle d'un micro-lot, et la pause qu'on en deduit explose. Mesure le
					// 17/09 sur la course A3 : quatre pauses de 9,3 a 22,8 s. Un micro-lot dure
					// ~1 s ; au rapport maximal de 3,0 la pause legitime vaut ~3 s.
					double mPauseMaxMs = 3000.0;

					// --- etat ---
					bool mActif = false;
					bool mDesarme = false;
					double mRatioRepos = 0.0;

					// ⚠️ MOYENNE PONDEREE PAR LE TEMPS, et ce n'est pas un detail.
					// Les echantillons NE tombent PAS a cadence reguliere : un par micro-lot
					// pendant le calcul (~1,8 s), un par tranche pendant le repos (~25 ms).
					// Une moyenne « sur les N derniers echantillons » serait donc composee
					// presque uniquement de points pris PENDANT le repos — elle verrait 25 W,
					// conclurait qu'il n'y a rien a freiner, et relacherait le frein. Le
					// garde-fou oscillerait en mesurant son propre effet a l'envers.
					// La moyenne exponentielle ponderee par la duree ecoulee (constante de
					// temps mFenetreSecondes) est, elle, la meme grandeur que ce qu'un
					// echantillonneur externe a cadence fixe mesure. C'est la condition pour
					// que « avant/apres en watts » compare deux fois la meme chose.
					double mMoyenneGlissante = 0.0;
					double mSecondesAccumulees = 0.0;

					// --- mesures ---
					double mSommeWattsTemps = 0.0;	// integrale watts x secondes
					double mDureeMesureeS = 0.0;
					int64 mNbEchantillons = 0;
					double mPicWatts = 0.0;
					double mPicCelsius = 0.0;
					double mReposCumuleMs = 0.0;
					int64 mNbFreinages = 0;
					double mCoutLecturesMs = 0.0;
					double mDernierRatioJournalise = -1.0;
					bool mResumeFait = false;

					NkChrono mChronoIntervalle;	 // duree du dernier micro-lot
					NkChrono mChronoJournal;	 // cadence du rappel
					bool mPremierAppel = true;

					// --- NVML, charge dynamiquement ---
					void *mDll = nullptr;
					void *mDevice = nullptr;
					void *mFnPower = nullptr;
					void *mFnTemp = nullptr;
					void *mFnShutdown = nullptr;
			};

		} // namespace gpt
	}	  // namespace ai
} // namespace nkentseu

#endif // NKENTSEU_AI_GPT_NKGARDEFOUPUISSANCE_H
