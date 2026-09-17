// =============================================================================
// NKGpt/NkGardeFouPuissance.cpp — plafond de puissance GPU applique par le produit
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
// Le pourquoi, le reglage, la mutation et la limite de ce garde-fou sont ecrits
// en tete de NkGardeFouPuissance.h. Ce fichier n'en repete que ce qui explique
// une ligne de code.
// =============================================================================

#include "NKGpt/NkGardeFouPuissance.h"

#include "NKLogger/NkLog.h"
#include "NKMath/NkFunctions.h" // NkExp
#include "NKPlatform/NkEnv.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(_WIN32)
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	include <windows.h>
#endif

using namespace nkentseu;
using namespace nkentseu::math;

namespace nkentseu {
	namespace ai {
		namespace gpt {

			// -----------------------------------------------------------------
			// NVML, charge dynamiquement. On ne se lie a rien a la compilation :
			// une machine sans carte NVIDIA doit continuer de construire et de
			// tourner, en DISANT que le garde-fou est inactif.
			// -----------------------------------------------------------------
			typedef int(__cdecl *Fn_nvmlInit)(void);
			typedef int(__cdecl *Fn_nvmlShutdown)(void);
			typedef int(__cdecl *Fn_nvmlHandleByIndex)(unsigned int, void **);
			typedef int(__cdecl *Fn_nvmlPower)(void *, unsigned int *);
			typedef int(__cdecl *Fn_nvmlTemp)(void *, int, unsigned int *);

			NkGardeFouPuissance::NkGardeFouPuissance() {
			}

			NkGardeFouPuissance::~NkGardeFouPuissance() {
				// Un chemin de sortie qui oublie le bilan est un chemin qui efface la
				// mesure. Fit() en a un (`return` de l'arret propre).
				if (!mResumeFait)
					Resume();
				FermerNvml();
			}

			// -----------------------------------------------------------------
			// Le reglage : lu dans un fichier, et chaque valeur dit d'ou elle vient.
			// -----------------------------------------------------------------
			static bool LireLigneReglage(const char *ligne, char *cle, int cleMax, double *valeur) {
				// Format : `cle = valeur`, '#' commence un commentaire, lignes vides ignorees.
				int i = 0;
				while (ligne[i] == ' ' || ligne[i] == '\t')
					++i;
				if (ligne[i] == '#' || ligne[i] == '\0' || ligne[i] == '\n' || ligne[i] == '\r')
					return false;
				int k = 0;
				while (ligne[i] != '\0' && ligne[i] != '=' && ligne[i] != ' ' && ligne[i] != '\t' && k < cleMax - 1)
					cle[k++] = ligne[i++];
				cle[k] = '\0';
				while (ligne[i] == ' ' || ligne[i] == '\t')
					++i;
				if (ligne[i] != '=')
					return false;
				++i;
				while (ligne[i] == ' ' || ligne[i] == '\t')
					++i;
				if (ligne[i] == '\0')
					return false;
				*valeur = atof(ligne + i);
				return k > 0;
			}

			void NkGardeFouPuissance::ChargerReglage(const char *cheminSauvegarde) {
				char chemin[1024];
				chemin[0] = '\0';

				const char *parEnv = nkentseu::env::GetEnvVar("NK_ILYANA_GARDE_FOU_CONF");
				if (parEnv != nullptr && parEnv[0] != '\0') {
					snprintf(chemin, sizeof(chemin), "%s", parEnv);
				} else if (cheminSauvegarde != nullptr && cheminSauvegarde[0] != '\0') {
					// dossier du checkpoint de sauvegarde
					snprintf(chemin, sizeof(chemin), "%s", cheminSauvegarde);
					int coupe = -1;
					for (int i = 0; chemin[i] != '\0'; ++i)
						if (chemin[i] == '/' || chemin[i] == '\\')
							coupe = i;
					if (coupe >= 0)
						snprintf(chemin + coupe + 1, sizeof(chemin) - (size_t)coupe - 1, "garde_fou_puissance.conf");
					else
						snprintf(chemin, sizeof(chemin), "garde_fou_puissance.conf");
				} else {
					snprintf(chemin, sizeof(chemin), "garde_fou_puissance.conf");
				}

				FILE *f = fopen(chemin, "rb");
				if (f == nullptr && parEnv == nullptr) {
					// dernier recours : le repertoire courant
					snprintf(chemin, sizeof(chemin), "garde_fou_puissance.conf");
					f = fopen(chemin, "rb");
				}

				if (f == nullptr) {
					// ⚠️ PAS DE REPLI MUET. On dit le chemin cherche ET le fait que toutes
					// les valeurs sont des defauts du code.
					logger.Info("[garde-fou] AUCUN fichier de reglage trouve (dernier chemin essaye : {0}). "
								"Toutes les valeurs ci-dessous sont des DEFAUTS DU CODE, pas un reglage ecrit.",
								chemin);
					logger.Info("[garde-fou] reglage retenu : plafond {0} W | temperature max {1} C | marge {2} W | "
								"repos max {3}x | fenetre {4} s   (tous par DEFAUT)",
								mPlafondWatts, mTemperatureMaxC, mMargeWatts, mReposMaxRatio, mFenetreSecondes);
					return;
				}

				bool vuPlafond = false, vuTemp = false, vuMarge = false, vuRepos = false, vuFenetre = false;
				bool vuMontee = false, vuDescente = false, vuJournal = false, vuPause = false;
				char ligne[512];
				while (fgets(ligne, sizeof(ligne), f) != nullptr) {
					char cle[128];
					double v = 0.0;
					if (!LireLigneReglage(ligne, cle, (int)sizeof(cle), &v))
						continue;
					if (strcmp(cle, "plafond_watts") == 0) {
						mPlafondWatts = v;
						vuPlafond = true;
					} else if (strcmp(cle, "temperature_max_c") == 0) {
						mTemperatureMaxC = v;
						vuTemp = true;
					} else if (strcmp(cle, "marge_watts") == 0) {
						mMargeWatts = v;
						vuMarge = true;
					} else if (strcmp(cle, "repos_max_ratio") == 0) {
						mReposMaxRatio = v;
						vuRepos = true;
					} else if (strcmp(cle, "fenetre_secondes") == 0) {
						mFenetreSecondes = v;
						vuFenetre = true;
					} else if (strcmp(cle, "pas_montee") == 0) {
						mPasMontee = v;
						vuMontee = true;
					} else if (strcmp(cle, "pas_descente") == 0) {
						mPasDescente = v;
						vuDescente = true;
					} else if (strcmp(cle, "pause_max_ms") == 0) {
						mPauseMaxMs = v;
						vuPause = true;
					} else if (strcmp(cle, "journal_secondes") == 0) {
						mJournalSecondes = v;
						vuJournal = true;
					} else {
						logger.Info("[garde-fou] cle inconnue ignoree dans {0} : '{1}'", chemin, cle);
					}
				}
				fclose(f);

				logger.Info("[garde-fou] reglage lu dans : {0}", chemin);
				logger.Info("[garde-fou]   plafond_watts     = {0} ({1})", mPlafondWatts,
							vuPlafond ? "FICHIER" : "defaut");
				logger.Info("[garde-fou]   temperature_max_c = {0} ({1})", mTemperatureMaxC,
							vuTemp ? "FICHIER" : "defaut");
				logger.Info("[garde-fou]   marge_watts       = {0} ({1})", mMargeWatts, vuMarge ? "FICHIER" : "defaut");
				logger.Info("[garde-fou]   repos_max_ratio   = {0} ({1})", mReposMaxRatio,
							vuRepos ? "FICHIER" : "defaut");
				logger.Info("[garde-fou]   fenetre_secondes  = {0} ({1})", mFenetreSecondes,
							vuFenetre ? "FICHIER" : "defaut");
				logger.Info("[garde-fou]   pas_montee        = {0} ({1})", mPasMontee, vuMontee ? "FICHIER" : "defaut");
				logger.Info("[garde-fou]   pas_descente      = {0} ({1})", mPasDescente,
							vuDescente ? "FICHIER" : "defaut");
				logger.Info("[garde-fou]   journal_secondes  = {0} ({1})", mJournalSecondes,
							vuJournal ? "FICHIER" : "defaut");
				logger.Info("[garde-fou]   pause_max_ms      = {0} ({1})", mPauseMaxMs,
							vuPause ? "FICHIER" : "defaut");

				// Garde-fous du garde-fou : un reglage absurde doit se voir, pas s'appliquer.
				if (mPlafondWatts < 15.0) {
					logger.Info("[garde-fou] ⚠️ plafond_watts = {0} est SOUS la consommation au repos de la carte "
								"(~20 W) : il serait impossible a tenir. Ramene a 15 W.",
								mPlafondWatts);
					mPlafondWatts = 15.0;
				}
				if (mFenetreSecondes < 0.5)
					mFenetreSecondes = 0.5;
				if (mReposMaxRatio < 0.0)
					mReposMaxRatio = 0.0;
			}

			// -----------------------------------------------------------------
			bool NkGardeFouPuissance::OuvrirNvml() {
#if defined(_WIN32)
				::HMODULE dll = ::LoadLibraryA("nvml.dll");
				if (dll == nullptr)
					dll = ::LoadLibraryA("C:\\Windows\\System32\\nvml.dll");
				if (dll == nullptr) {
					logger.Info("[garde-fou] nvml.dll INTROUVABLE : impossible de lire la puissance de la carte. "
								"Le garde-fou est INACTIF pour cette course, et il le dit plutot que de se taire.");
					return false;
				}
				Fn_nvmlInit init = (Fn_nvmlInit)(void *)::GetProcAddress(dll, "nvmlInit_v2");
				if (init == nullptr)
					init = (Fn_nvmlInit)(void *)::GetProcAddress(dll, "nvmlInit");
				Fn_nvmlHandleByIndex parIndex =
					(Fn_nvmlHandleByIndex)(void *)::GetProcAddress(dll, "nvmlDeviceGetHandleByIndex_v2");
				if (parIndex == nullptr)
					parIndex = (Fn_nvmlHandleByIndex)(void *)::GetProcAddress(dll, "nvmlDeviceGetHandleByIndex");
				Fn_nvmlPower power = (Fn_nvmlPower)(void *)::GetProcAddress(dll, "nvmlDeviceGetPowerUsage");
				Fn_nvmlTemp temp = (Fn_nvmlTemp)(void *)::GetProcAddress(dll, "nvmlDeviceGetTemperature");
				Fn_nvmlShutdown shut = (Fn_nvmlShutdown)(void *)::GetProcAddress(dll, "nvmlShutdown");

				if (init == nullptr || parIndex == nullptr || power == nullptr) {
					logger.Info("[garde-fou] nvml.dll chargee mais ses fonctions manquent (init={0} index={1} "
								"power={2}) : garde-fou INACTIF.",
								(long long)(init != nullptr), (long long)(parIndex != nullptr),
								(long long)(power != nullptr));
					::FreeLibrary(dll);
					return false;
				}
				const int rInit = init();
				if (rInit != 0) {
					logger.Info("[garde-fou] nvmlInit a rendu {0} (0 attendu) : garde-fou INACTIF.", (long long)rInit);
					::FreeLibrary(dll);
					return false;
				}
				void *dev = nullptr;
				const int rDev = parIndex(0u, &dev);
				if (rDev != 0 || dev == nullptr) {
					logger.Info("[garde-fou] nvmlDeviceGetHandleByIndex(0) a rendu {0} : garde-fou INACTIF.",
								(long long)rDev);
					if (shut != nullptr)
						shut();
					::FreeLibrary(dll);
					return false;
				}
				mDll = (void *)dll;
				mDevice = dev;
				mFnPower = (void *)power;
				mFnTemp = (void *)temp;
				mFnShutdown = (void *)shut;

				// PROUVER LE ZERO D'ABORD : on ne declare pas la source ouverte sur la foi
				// d'un code de retour, on lui demande une valeur et on l'imprime.
				const NkEtatCarte e = Lire();
				if (!e.valide) {
					logger.Info("[garde-fou] NVML ouvert mais la premiere lecture a echoue : garde-fou INACTIF.");
					FermerNvml();
					return false;
				}
				logger.Info("[garde-fou] NVML ouvert. Premiere lecture : {0} W, {1} C. C'est la valeur de depart, "
							"et elle prouve que l'instrument rend autre chose que zero.",
							e.watts, e.celsius);
				// Cette lecture de calibrage ne doit pas compter dans les mesures de la course.
				mNbEchantillons = 0;
				mSommeWattsTemps = 0.0;
				mDureeMesureeS = 0.0;
				mPicWatts = 0.0;
				mPicCelsius = 0.0;
				mMoyenneGlissante = e.watts;
				return true;
#else
				// ⚠️ LIMITE NOMMEE. CONDITION DE REOUVERTURE : le jour ou la campagne
				// tournera ailleurs que sur Windows.
				logger.Info("[garde-fou] lecture de la puissance non implementee sur cette plateforme : "
							"garde-fou INACTIF. Il le dit au lieu de se taire.");
				return false;
#endif
			}

			void NkGardeFouPuissance::FermerNvml() {
#if defined(_WIN32)
				if (mFnShutdown != nullptr)
					((Fn_nvmlShutdown)mFnShutdown)();
				if (mDll != nullptr)
					::FreeLibrary((::HMODULE)mDll);
#endif
				mDll = nullptr;
				mDevice = nullptr;
				mFnPower = nullptr;
				mFnTemp = nullptr;
				mFnShutdown = nullptr;
			}

			NkEtatCarte NkGardeFouPuissance::Lire() {
				NkEtatCarte e;
#if defined(_WIN32)
				if (mDevice == nullptr || mFnPower == nullptr)
					return e;
				NkChrono c;
				unsigned int mw = 0u;
				if (((Fn_nvmlPower)mFnPower)(mDevice, &mw) != 0)
					return e;
				e.watts = (double)mw / 1000.0;
				if (mFnTemp != nullptr) {
					unsigned int t = 0u;
					if (((Fn_nvmlTemp)mFnTemp)(mDevice, 0 /* NVML_TEMPERATURE_GPU */, &t) == 0)
						e.celsius = (double)t;
				}
				e.valide = true;
				mCoutLecturesMs += c.Elapsed().milliseconds;
#endif
				return e;
			}

			// -----------------------------------------------------------------
			void NkGardeFouPuissance::Ouvrir(const char *cheminSauvegarde) {
				const char *inter = nkentseu::env::GetEnvVar("NK_ILYANA_GARDE_FOU");
				if (inter != nullptr && inter[0] == '0') {
					mDesarme = true;
					mActif = false;
					logger.Info("*** SONDE DE MESURE *** NK_ILYANA_GARDE_FOU=0 : le garde-fou de puissance est "
								"DESARME. La carte tirera ce qu'elle veut. C'est la MUTATION : la consommation "
								"DOIT remonter par rapport a une course armee, sinon le garde-fou ne fait rien.");
					return;
				}
				ChargerReglage(cheminSauvegarde);
				mActif = OuvrirNvml();
				if (mActif) {
					logger.Info("[garde-fou] ARME : plafond {0} W (moyenne glissante sur {1} s), temperature max "
								"{2} C. Il FREINE en intercalant des pauses entre les micro-lots ; il ne change "
								"AUCUN calcul, aucun gradient, aucune trajectoire.",
								mPlafondWatts, mFenetreSecondes, mTemperatureMaxC);
					logger.Info("[garde-fou] ⚠️ C'est un PANSEMENT, pas une reparation : la batterie de cette "
								"machine est electriquement morte et la limite materielle de la carte n'est pas "
								"reglable. Le vrai correctif est materiel.");
					logger.Info("[garde-fou] ⚠️ CONSEQUENCE A NE PAS OUBLIER : une course BRIDEE n'est plus "
								"comparable a la reference d'erreurs pilote de R12 (0,121 erreur par heure de "
								"course). La carte ne travaille plus le meme nombre de secondes par heure.");
				}
				(void)mChronoIntervalle.Reset();
				(void)mChronoJournal.Reset();
				mPremierAppel = true;
			}

			double NkGardeFouPuissance::MoyenneWatts() const {
				return (mDureeMesureeS > 0.0) ? (mSommeWattsTemps / mDureeMesureeS) : 0.0;
			}

			// -----------------------------------------------------------------
			// Le coeur. Appele a chaque frontiere de micro-lot.
			// -----------------------------------------------------------------
			double NkGardeFouPuissance::FreinerSiNecessaire() {
				if (!mActif)
					return 0.0;

				const double dtS = mChronoIntervalle.Reset().seconds;
				if (mPremierAppel) {
					// Le premier intervalle contient le chargement du corpus et la compilation
					// des noyaux : il ne represente pas un micro-lot. On l'ecarte au lieu de
					// laisser un intervalle de plusieurs minutes empoisonner la moyenne.
					mPremierAppel = false;
					return 0.0;
				}
				const double dureeMicroLotS = (dtS > 0.0 && dtS < 300.0) ? dtS : 0.0;

				// --- echantillon de fin de micro-lot, pondere par la duree qu'il represente
				const NkEtatCarte e = Lire();
				if (!e.valide)
					return 0.0;
				++mNbEchantillons;
				if (e.watts > mPicWatts)
					mPicWatts = e.watts;
				if (e.celsius > mPicCelsius)
					mPicCelsius = e.celsius;
				if (dureeMicroLotS > 0.0) {
					mSommeWattsTemps += e.watts * dureeMicroLotS;
					mDureeMesureeS += dureeMicroLotS;
					const double alpha = 1.0 - NkExp(-dureeMicroLotS / mFenetreSecondes);
					mMoyenneGlissante += alpha * (e.watts - mMoyenneGlissante);
					mSecondesAccumulees += dureeMicroLotS;
				}

				// --- CONDITION DE VALIDITE : tant que la moyenne glissante n'a pas vu au
				// moins une constante de temps, elle ne dit rien de fiable. Un critere qui se
				// prononce hors de sa condition fabrique du bruit.
				if (mSecondesAccumulees < mFenetreSecondes)
					return 0.0;

				// --- regulation du rapport de repos
				const double ancien = mRatioRepos;
				const bool tropChaud = (e.celsius > 0.0 && e.celsius > mTemperatureMaxC);
				if (mMoyenneGlissante > mPlafondWatts || tropChaud) {
					mRatioRepos += mPasMontee;
					if (mRatioRepos > mReposMaxRatio)
						mRatioRepos = mReposMaxRatio;
				} else if (mMoyenneGlissante < mPlafondWatts - mMargeWatts) {
					mRatioRepos -= mPasDescente;
					if (mRatioRepos < 0.0)
						mRatioRepos = 0.0;
				}

				if (mRatioRepos <= 0.0) {
					if (ancien > 0.0) {
						logger.Info("[garde-fou] moyenne {0} W sous le plafond {1} W : freinage RELACHE (repos 0 %).",
									mMoyenneGlissante, mPlafondWatts);
						mDernierRatioJournalise = 0.0;
					}
					return 0.0;
				}

				// --- freinage reel : on dort, et la carte retombe
				double reposMs = dureeMicroLotS * 1000.0 * mRatioRepos;
				if (reposMs > mPauseMaxMs) {
					// NE PAS ECRETER EN SILENCE : un defaut qu'on borne sans le dire devient un
					// defaut muet. On imprime la valeur brute ET la valeur retenue.
					logger.Info("[garde-fou] pause de {0} ms deduite d'un intervalle de {1} s : ce n'est plus la "
						"duree d'un micro-lot (frontiere de pas : Adam, journal, repos precedent). "
						"Bornee a {2} ms.",
						reposMs, dureeMicroLotS, mPauseMaxMs);
					reposMs = mPauseMaxMs;
				}
				if (reposMs < 1.0)
					return 0.0;

				const bool changement = (mDernierRatioJournalise < 0.0) ||
										(mRatioRepos - mDernierRatioJournalise > 0.02) ||
										(mDernierRatioJournalise - mRatioRepos > 0.02);
				const bool cadence = (mChronoJournal.Elapsed().seconds >= mJournalSecondes);
				if (changement || cadence) {
					logger.Info("[garde-fou] FREINE : moyenne {0} W (pic {1} W, {2} C) contre un plafond de {3} W "
								"-> repos {4} % du temps de calcul, soit une pause de {5} ms par micro-lot.",
								mMoyenneGlissante, mPicWatts, e.celsius, mPlafondWatts, mRatioRepos * 100.0, reposMs);
					mDernierRatioJournalise = mRatioRepos;
					(void)mChronoJournal.Reset();
				}

				// La pause est decoupee : pendant qu'on dort, on continue d'echantillonner,
				// SINON la moyenne ne verrait que les phases de calcul et le garde-fou
				// mesurerait son propre effet a l'envers. Chaque echantillon reste pondere
				// par la duree qu'il represente (sa tranche), ce qui garde la moyenne
				// comparable a celle d'un echantillonneur externe a cadence fixe.
				const double trancheMs = 25.0;
				double resteMs = reposMs;
				while (resteMs > 0.0) {
					const double pasMs = (resteMs > trancheMs) ? trancheMs : resteMs;
					NkChrono::SleepMilliseconds((int64)(pasMs + 0.5));
					const NkEtatCarte er = Lire();
					if (er.valide) {
						++mNbEchantillons;
						const double s = pasMs / 1000.0;
						mSommeWattsTemps += er.watts * s;
						mDureeMesureeS += s;
						const double a = 1.0 - NkExp(-s / mFenetreSecondes);
						mMoyenneGlissante += a * (er.watts - mMoyenneGlissante);
					}
					resteMs -= pasMs;
				}

				mReposCumuleMs += reposMs;
				++mNbFreinages;
				// L'intervalle repart APRES la pause : la duree du micro-lot suivant ne doit
				// pas inclure le repos qu'on vient de prendre.
				(void)mChronoIntervalle.Reset();
				return reposMs;
			}

			// -----------------------------------------------------------------
			void NkGardeFouPuissance::Resume() {
				mResumeFait = true;
				if (mDesarme) {
					logger.Info("[garde-fou] DESARME pendant toute la course (NK_ILYANA_GARDE_FOU=0). Aucune mesure, "
								"aucun freinage. C'est la course de MUTATION.");
					return;
				}
				if (!mActif) {
					logger.Info("[garde-fou] INACTIF pendant toute la course (voir la raison plus haut). "
								"Aucun freinage n'a pu avoir lieu.");
					return;
				}
				logger.Info("[garde-fou] BILAN : {0} lectures, moyenne ponderee par le temps = {1} W, pic {2} W, "
							"pic {3} C, sur {4} s mesurees.",
							(long long)mNbEchantillons, MoyenneWatts(), mPicWatts, mPicCelsius, mDureeMesureeS);
				if (mNbFreinages == 0) {
					// LE ZERO, et il doit se lire comme un resultat, pas comme un silence.
					logger.Info("[garde-fou] AUCUN FREINAGE : la moyenne glissante n'a jamais depasse le plafond de "
								"{0} W (pic instantane observe : {1} W). Cout du garde-fou sur cette course : {2} ms "
								"de lectures cumulees, soit {3} % du temps mesure.",
								mPlafondWatts, mPicWatts, mCoutLecturesMs,
								(mDureeMesureeS > 0.0) ? (mCoutLecturesMs / 10.0 / mDureeMesureeS) : 0.0);
				} else {
					logger.Info("[garde-fou] FREINAGES : {0} pauses, {1} ms de repos cumulees, soit {2} % du temps "
								"mesure passe a ne rien calculer. Cout des lectures : {3} ms.",
								(long long)mNbFreinages, mReposCumuleMs,
								(mDureeMesureeS > 0.0) ? (mReposCumuleMs / 10.0 / mDureeMesureeS) : 0.0,
								mCoutLecturesMs);
					logger.Info("[garde-fou] ⚠️ Cette course est BRIDEE : son debit en pas par heure et son taux "
								"d'erreurs pilote par heure ne se comparent PAS a ceux d'une course libre.");
				}
			}

		} // namespace gpt
	}	  // namespace ai
} // namespace nkentseu
