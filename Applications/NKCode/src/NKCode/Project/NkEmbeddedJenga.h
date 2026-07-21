#pragma once
// =============================================================================
// NkEmbeddedJenga.h — Jenga IN-PROCESS (Phase 12 ROADMAP) : execute le build
//   via un interpreteur CPython EMBARQUE (pybind11) au lieu de spawner
//   `jenga.exe` en sous-processus. Zero dependance a un Python systeme :
//   l'interpreteur est STRICTEMENT isole (PyConfig isolated, home pointe sur
//   le runtime embarque, sys.path explicite) — voir NkEmbeddedJenga.cpp.
//
//   Surface volontairement COMPATIBLE NkProcess (Start/Running/Done/ExitCode/
//   Drain) pour se brancher dans NkCodeState (PumpQueue/PollBuild) avec un
//   minimum de changements. Difference clef : en plus des LIGNES de transcript
//   (identiques a stdout), Drain remonte des EVENEMENTS STRUCTURES de
//   progression (Jenga/Core/Embed.py + sink Reporter) -> plus de scraping de
//   texte pour les barres de progression/voyants d'erreur.
//
//   CONCURRENCE : UN SEUL thread worker possede l'interpreteur pour toute sa
//   vie (init/finalize sur le meme thread OS, exigence CPython) et traite UNE
//   requete a la fois — ce qui correspond exactement aux globals non
//   reentrants du DSL Jenga (Core/Api.py) ET au slot de build unique de
//   NkCodeState. Le thread UI ne touche JAMAIS Python : communication
//   uniquement via mutex + files (meme patron que NkProcess).
// =============================================================================
#include "NKThreading/NkThread.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;
		using namespace nkentseu::threading;

		// Evenement de progression structure (miroir du sink PascalCase de
		// Jenga/Core/Embed.py). `kind` pilote quels champs sont significatifs.
		struct NkJengaProgressEvent {
				enum Kind {
					PROJECT_TOTAL = 0, // total       (Build Order (N projects))
					PROJECT_DONE,	   // ok          (un projet fini)
					FILE_TOTAL,		   // project, total
					FILE_DONE,		   // project, index, total, file, ok, warned
					COMPILE_ERROR,	   // project, file, message
					LINK_ERROR		   // project, file, message
				};
				int32 kind = 0;
				NkString project, file, message;
				int32 total = 0, index = 0;
				bool ok = true, warned = false;
		};

		class NkEmbeddedJenga {
			public:
				// Une requete = un appel Embed.Build/Rebuild (kind "build"/"rebuild").
				struct Request {
						NkString kind;		// "build" | "rebuild"
						NkString jengaFile; // chemin absolu du .jenga workspace ("" = cwd)
						NkString target;	// projet cible ("" = tout)
						NkString config;	// "Debug"/"Release"
						NkString platform;	// "Windows-x86_64"... ("" = hote)
						NkString toolchain; // nom force ("" = auto)
				};

				static NkEmbeddedJenga &Get(); // singleton : UN interpreteur par process

				// Memorise les chemins (exeDir -> tools/python-embed, tools/jenga-src).
				// N'initialise PAS Python (paresseux, au premier Start).
				static void Configure(const NkString &exeDir);

				// Runtime embarque ET sources Jenga trouves sur disque ? (sinon le
				// feature-flag retombe automatiquement sur le chemin sous-processus.)
				static bool Available();

				bool Start(const Request &req); // false si deja en cours
				bool Running() const;
				bool Done() const;
				int ExitCode() const;
				// Recupere lignes de transcript + evenements structures (thread-safe).
				void Drain(NkVector<NkString> &outLines, NkVector<NkJengaProgressEvent> &outEvents);

				// Arret propre (join du worker -> Py_FinalizeEx sur SON thread).
				// A appeler UNE fois a la fermeture de NKCode.
				static void Shutdown();

				// — interne (worker/pybind), publics pour le .cpp uniquement —
				void PushLine(const NkString &s);
				void PushEvent(const NkJengaProgressEvent &e);

			private:
				NkEmbeddedJenga() = default;
				~NkEmbeddedJenga() = default;
				NkEmbeddedJenga(const NkEmbeddedJenga &) = delete;
				NkEmbeddedJenga &operator=(const NkEmbeddedJenga &) = delete;

				void WorkerMain(); // boucle du thread worker (vit dans le .cpp)

				NkThread mThread;
				mutable NkMutex mMutex;
				NkVector<NkString> mLines;				  // protege par mMutex
				NkVector<NkJengaProgressEvent> mEvents;	  // protege par mMutex
				Request mPending;						  // requete deposee (protegee par mMutex)
				bool mHasPending = false;
				bool mRunning = false; // une requete est en cours d'execution
				bool mDone = false;	   // la DERNIERE requete est terminee
				bool mQuit = false;	   // sentinel d'arret du worker
				bool mWorkerStarted = false;
				int mExit = 0;
		};

	} // namespace nkcode
} // namespace nkentseu
