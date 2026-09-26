#pragma once
// =============================================================================
// Nkentseu/Core/NkProfiler.h — Profiler de performance
// =============================================================================
// CORRECTION : Ce fichier était incorrectement placé dans NkPrefab.h.
// Le profiler est un composant de la couche Core, pas des Prefabs.
//
// Usage :
//   NK_PROFILE_SCOPE("MovementSystem.Execute");
//
// =============================================================================
// ⚠️ ETAT REEL AU 2026-09-26 : CES MARQUEURS NE MESURENT RIEN.
// =============================================================================
// Ce fichier est un EN-TETE DE SPECIFICATION : 11 declarations pour 6 corps,
// et il n'existe AUCUN NkProfiler.cpp. Manquent en particulier :
//
//     Begin / End            le coeur -- sans eux, rien ne se mesure
//     GetStats               lit mStats
//     ExportToJSON / Reset
//     GetTimestampNs / GetCurrentThreadId   les deux sources de temps
//
// CONSEQUENCE, decouverte le 2026-09-26 : `NkEngineLayer.cpp` -- SEUL
// consommateur de `NK_PROFILE_SCOPE` dans tout le depot -- NE SE LIAIT PAS.
// Son `.obj` est bien dans `Noge.lib`, mais un objet de bibliotheque statique
// n'est lie que s'il est REFERENCE, et rien ne le referencait. *Compiler n'est
// pas se lier ; etre dans la bibliotheque n'est pas etre utilise.*
//
// CE QUI A ETE FAIT, ET RIEN DE PLUS (decision du 2026-09-26) :
// `NK_PROFILE_SCOPE` compile a RIEN par defaut, pour rendre la couche
// LIABLE -- pas pour obtenir des mesures que personne n'a demandees.
//
// ⚠️ ET L'INERTIE EST ECRITE, PAS SUBIE : definir
// `NKENTSEU_PROFILER_ENABLED=1` ECHOUE A LA COMPILATION tant qu'aucune
// implementation n'existe. Un profileur qui rendrait des ZEROS en silence
// serait le repli muet qu'on traque -- *un zero affiche la ou rien n'a ete
// mesure*. Mieux vaut refuser de compiler que mentir.
//
// CONDITION DE RETRAIT de tout ce bloc : le jour ou un `NkProfiler.cpp`
// definira les six methodes, il definira aussi `NKENTSEU_PROFILER_HAS_IMPL`,
// et cet avertissement partira avec le `#error`.
// =============================================================================
//   NK_PROFILE_SAMPLE("Raycast.Cast");
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {

	class NkProfiler {
		public:
			enum class MarkerType : uint8 { Frame, System, Query, Script, Custom };

			struct Marker {
					MarkerType type = MarkerType::Custom;
					const char *name = "";
					uint64 startTime = 0;
					uint64 endTime = 0;
					uint32 frameIndex = 0;
					uint32 threadId = 0;
					bool isValid = false;
			};

			struct Stats {
					uint32 sampleCount = 0;
					uint64 minTime = UINT64_MAX;
					uint64 maxTime = 0;
					uint64 totalTime = 0;

					[[nodiscard]] uint64 AvgTime() const noexcept {
						return (sampleCount > 0) ? (totalTime / sampleCount) : 0;
					}
			};

			[[nodiscard]] static NkProfiler &Global() noexcept {
				static NkProfiler instance;
				return instance;
			}

			void Begin(const char *name, MarkerType type = MarkerType::Custom) noexcept;
			void End(const char *name) noexcept;

			void Sample(const char *name, MarkerType type = MarkerType::Custom) noexcept {
				Begin(name, type);
				End(name);
			}

			void NextFrame() noexcept {
				++mCurrentFrame;
			}

			[[nodiscard]] Stats GetStats(const char *name) const noexcept;
			[[nodiscard]] bool ExportToJSON(char *buffer, uint32 bufSize) const noexcept;
			void Reset() noexcept;

		private:
			uint32 mCurrentFrame = 0;
			NkUnorderedMap<NkString, Stats> mStats;

			[[nodiscard]] static uint64 GetTimestampNs() noexcept;
			[[nodiscard]] static uint32 GetCurrentThreadId() noexcept;
	};

} // namespace nkentseu

// =============================================================================
// Macros utilitaires
// =============================================================================
// Par defaut : RIEN. Voir le bloc « ETAT REEL » en tete de fichier.
#if !defined(NKENTSEU_PROFILER_ENABLED)
#	define NKENTSEU_PROFILER_ENABLED 0
#endif

#if NKENTSEU_PROFILER_ENABLED
#	if !defined(NKENTSEU_PROFILER_HAS_IMPL)
#		error "NKENTSEU_PROFILER_ENABLED=1, mais NkProfiler::Begin/End/GetStats/ExportToJSON/Reset/GetTimestampNs/GetCurrentThreadId n'ont AUCUNE definition dans le depot (aucun NkProfiler.cpp). Le lien echouerait, ou pire : le profileur rendrait des zeros. Ecrire NkProfiler.cpp et y definir NKENTSEU_PROFILER_HAS_IMPL avant d'activer ce drapeau."
#	endif

#define NK_PROFILE_SCOPE(name)                                                                                         \
	::nkentseu::NkProfiler::Global().Begin((name));                                                                    \
	struct _NkProfilerEnd_##__LINE__ {                                                                                 \
			const char *_n;                                                                                            \
			_NkProfilerEnd_##__LINE__(const char *n) : _n(n) {                                                         \
			}                                                                                                          \
			~_NkProfilerEnd_##__LINE__() noexcept {                                                                    \
				::nkentseu::NkProfiler::Global().End(_n);                                                              \
			}                                                                                                          \
	} _profilerEnd_##__LINE__ {                                                                                        \
		(name)                                                                                                         \
	}

#define NK_PROFILE_SAMPLE(name) ::nkentseu::NkProfiler::Global().Sample((name))

#else // NKENTSEU_PROFILER_ENABLED == 0

// ⚠️ INERTES, ET C'EST ECRIT : aucune mesure n'est prise, aucun zero n'est
//    rendu. `(void)(name)` garde le nom REFERENCE, pour qu'une chaine
//    invalide ou une variable inexistante soit quand meme signalee par le
//    compilateur -- une macro vide laisserait passer du code faux.
#define NK_PROFILE_SCOPE(name) ((void)(name))
#define NK_PROFILE_SAMPLE(name) ((void)(name))

#endif // NKENTSEU_PROFILER_ENABLED
