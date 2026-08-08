#pragma once
// =============================================================================
// NkcModuleLog — le journal des modules, cote ATELIER.
//
// POURQUOI CE FICHIER EXISTE
// --------------------------
// Un module est lie statiquement a sa propre copie de Nkentseu : son `logger`
// n'est pas celui de l'atelier. Sans canal explicite, un stagiaire qui veut
// afficher un etat pour comprendre son bug ecrit dans le vide.
//
// L'atelier injecte donc un puits dans chaque module au chargement
// (`nkc_rules_set_logger` / `nkc_ai_set_logger`), et ce fichier est l'autre bout
// du tuyau : il collecte, il borne, et le panneau « Sortie » l'affiche.
//
// TROIS CONTRAINTES QUI ONT FAÇONNE LE CODE
// -----------------------------------------
// 1. L'APPEL VIENT DE N'IMPORTE QUEL THREAD. Une IA journalise depuis son worker
//    pendant que l'interface dessine. Tout passe donc par un verrou, et le
//    panneau lit sous le meme verrou.
//
// 2. UNE BOUCLE CHAUDE PEUT INONDER. Un stagiaire qui journalise dans un rollout
//    MCTS produit des dizaines de milliers de lignes par seconde. Le tampon est
//    donc CIRCULAIRE et borne : on garde les dernieres, on compte les perdues,
//    et on l'affiche. Faire grossir un tableau sans limite tuerait l'atelier
//    juste au moment ou le stagiaire en a le plus besoin.
//
// 3. ON N'ALLOUE PAS PAR LIGNE. Les lignes vivent dans des tampons de taille
//    fixe : une allocation par message, sous verrou, depuis un thread d'IA,
//    serait un point de contention parfaitement evitable.
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"

#include "NKContainers/String/NkString.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"

#include <cstring>

namespace nkentseu {
	namespace conqueror {

		/// Bornes. 4096 lignes couvrent tres largement une session de debogage ;
		/// au-dela, ce n'est plus du journal, c'est de la mesure — et la mesure a
		/// son panneau.
		inline constexpr uint32 kLogCapacity   = 4096;
		inline constexpr uint32 kLogTextMax	   = 240;
		inline constexpr uint32 kLogModuleMax  = 48;

		struct NkcLogLine {
				char		text[kLogTextMax]	 = {};
				char		module[kLogModuleMax] = {};
				NkcLogLevel level				 = NkcLogLevel::Info;
				uint32		repeat				 = 1;  ///< lignes identiques fusionnees
		};

		// ---------------------------------------------------------------------
		class NkcModuleLog {
			public:
				/// Le puits, passe aux modules. `user` porte le `this`.
				static void Sink(void *user, NkcLogLevel level, const char *module,
								 const char *text) noexcept {
					if (!user) return;
					static_cast<NkcModuleLog *>(user)->Push(level, module, text);
				}

				void Push(NkcLogLevel level, const char *module, const char *text) noexcept {
					if (!text) return;
					threading::NkScopedLock<threading::NkMutex> lock(mMutex);

					// Repetition immediate : on incremente au lieu d'empiler. Une
					// boucle qui journalise la meme ligne mille fois reste lisible.
					if (mCount > 0) {
						NkcLogLine &last = mLines[(mHead + kLogCapacity - 1) % kLogCapacity];
						if (last.level == level && std::strncmp(last.text, text, kLogTextMax - 1) == 0) {
							if (last.repeat < 0xFFFFFFFFu) ++last.repeat;
							++mTotal;
							return;
						}
					}

					NkcLogLine &l = mLines[mHead];
					l.level	 = level;
					l.repeat = 1;
					Copy(l.text, kLogTextMax, text);
					Copy(l.module, kLogModuleMax, module ? module : "module");

					mHead = (mHead + 1) % kLogCapacity;
					if (mCount < kLogCapacity) ++mCount;
					else ++mDropped;   // le plus ancien vient d'etre ecrase
					++mTotal;
					++mVersion;
				}

				/// Parcours sous verrou, du plus ancien au plus recent.
				/// `fn` recoit (index affiche, ligne).
				template <typename F>
				void ForEach(F &&fn) const noexcept {
					threading::NkScopedLock<threading::NkMutex> lock(mMutex);
					const uint32 start = (mHead + kLogCapacity - mCount) % kLogCapacity;
					for (uint32 i = 0; i < mCount; ++i)
						fn(i, mLines[(start + i) % kLogCapacity]);
				}

				/// Du plus RECENT au plus ancien — c'est l'ordre d'affichage du
				/// panneau « Sortie » : NKGui n'offre pas de defilement
				/// programmatique, donc mettre le neuf en haut est le seul moyen
				/// de garantir qu'on voit la derniere ligne sans rien toucher.
				template <typename F>
				void ForEachNewestFirst(F &&fn) const noexcept {
					threading::NkScopedLock<threading::NkMutex> lock(mMutex);
					for (uint32 i = 0; i < mCount; ++i)
						fn(i, mLines[(mHead + kLogCapacity - 1 - i) % kLogCapacity]);
				}

				uint32 Count() const noexcept   { threading::NkScopedLock<threading::NkMutex> l(mMutex); return mCount; }
				uint32 Dropped() const noexcept { threading::NkScopedLock<threading::NkMutex> l(mMutex); return mDropped; }
				uint32 Total() const noexcept   { threading::NkScopedLock<threading::NkMutex> l(mMutex); return mTotal; }
				/// Change a chaque ligne nouvelle : le panneau s'en sert pour ne
				/// defiler automatiquement que quand il y a du nouveau.
				uint32 Version() const noexcept { threading::NkScopedLock<threading::NkMutex> l(mMutex); return mVersion; }

				void Clear() noexcept {
					threading::NkScopedLock<threading::NkMutex> lock(mMutex);
					mHead = mCount = mDropped = mTotal = 0;
					++mVersion;
				}

			private:
				static void Copy(char *dst, uint32 cap, const char *src) noexcept {
					uint32 i = 0;
					for (; i + 1 < cap && src[i]; ++i) dst[i] = src[i];
					dst[i] = '\0';
				}

				mutable threading::NkMutex mMutex;
				NkcLogLine		mLines[kLogCapacity];
				uint32			mHead	 = 0;
				uint32			mCount	 = 0;
				uint32			mDropped = 0;
				uint32			mTotal	 = 0;
				uint32			mVersion = 0;
		};

	} // namespace conqueror
} // namespace nkentseu
