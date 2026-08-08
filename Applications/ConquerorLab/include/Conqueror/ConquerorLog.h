#pragma once
// =============================================================================
// ConquerorLog.h — faire remonter le journal d'un module dans l'atelier.
//
// LE PROBLEME
// -----------
// Un module est lie STATIQUEMENT a sa propre copie de Nkentseu. Son `logger`
// n'est donc pas celui de l'atelier : un logger.Infof() depuis un module ecrit
// dans un journal que personne ne lit. Ce n'est pas un oubli — c'est la
// consequence directe de l'isolation qui fait tout l'interet du systeme. Mais
// laisser le stagiaire sans retour serait absurde : deboguer un moteur de regles
// sans pouvoir afficher un etat, c'est deboguer a l'aveugle.
//
// LA SOLUTION, EN UNE LIGNE
// -------------------------
// Ecrivez ceci UNE FOIS dans votre .cpp, apres vos exports :
//
//     NKC_MODULE_LOGGING(rules)      // dans un module de REGLES
//     NKC_MODULE_LOGGING(ai)         // dans un module d'IA
//
// C'est tout. A partir de la :
//
//     NKC_LOG_INFO("case %d retournee, cascade x%d", index, n);
//     logger.Infof("... marche aussi, si vous incluez NKLogger");
//
// s'affichent dans le panneau « Sortie » de l'atelier, avec le nom de votre
// module, le niveau, et l'horodatage.
//
// SANS ATELIER
// ------------
// Si personne n'injecte de puits — banc d'essai, test en ligne de commande —
// tout part sur stderr. Vos traces ne disparaissent jamais silencieusement.
//
// THREADS
// -------
// Une IA journalise depuis son thread worker. Le puits de l'atelier est fait
// pour : il verrouille et met en file. Ne supposez PAS l'ordre entre deux
// threads, seulement que rien ne se perd et que rien ne s'entremele.
//
// COUT
// ----
// NKC_LOG_* formate AVANT de savoir si quelqu'un ecoute. Dans une boucle chaude
// d'IA — un rollout MCTS, par exemple — ne journalisez pas : mesurez avec
// NkcAIResult, ou remontez par GetDebugJson. Une trace par coup, pas par noeud.
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"

#include <cstdio>
#include <cstdarg>

namespace nkentseu {
	namespace conqueror {

		/// Puits courant. Un par module, puisque chaque module a sa propre copie
		/// de ce header — c'est exactement ce qu'on veut : deux modules charges
		/// en meme temps journalisent chacun sous SON nom.
		struct NkcLogState {
				NkcLogFn fn	  = nullptr;
				void	*user = nullptr;
				char	 name[64] = {'m', 'o', 'd', 'u', 'l', 'e', '\0'};
		};

		inline NkcLogState &NkcLogGet() noexcept {
			static NkcLogState s;
			return s;
		}

		/// Appelee par l'atelier au chargement. `name` peut etre nullptr.
		inline void NkcLogInstall(NkcLogFn fn, void *user, const char *name) noexcept {
			NkcLogState &s = NkcLogGet();
			s.fn   = fn;
			s.user = user;
			if (name && name[0]) {
				usize i = 0;
				for (; i + 1 < sizeof(s.name) && name[i]; ++i) s.name[i] = name[i];
				s.name[i] = '\0';
			}
		}

		/// Ecrit une ligne. Repli stderr si l'atelier n'a rien injecte.
		inline void NkcLogWrite(NkcLogLevel level, const char *fmt, ...) noexcept {
			char	buf[1024];
			va_list ap;
			va_start(ap, fmt);
			std::vsnprintf(buf, sizeof(buf), fmt, ap);
			va_end(ap);

			NkcLogState &s = NkcLogGet();
			if (s.fn) {
				s.fn(s.user, level, s.name, buf);
				return;
			}
			// Sans hote : stderr, pour que rien ne disparaisse en silence.
			static const char *kTag[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
			const int32 li = static_cast<int32>(level);
			std::fprintf(stderr, "[%s] %s: %s\n",
						 (li >= 0 && li <= 5) ? kTag[li] : "?", s.name, buf);
		}

	} // namespace conqueror
} // namespace nkentseu

#define NKC_LOG_TRACE(...) ::nkentseu::conqueror::NkcLogWrite(::nkentseu::conqueror::NkcLogLevel::Trace, __VA_ARGS__)
#define NKC_LOG_DEBUG(...) ::nkentseu::conqueror::NkcLogWrite(::nkentseu::conqueror::NkcLogLevel::Debug, __VA_ARGS__)
#define NKC_LOG_INFO(...)  ::nkentseu::conqueror::NkcLogWrite(::nkentseu::conqueror::NkcLogLevel::Info,  __VA_ARGS__)
#define NKC_LOG_WARN(...)  ::nkentseu::conqueror::NkcLogWrite(::nkentseu::conqueror::NkcLogLevel::Warn,  __VA_ARGS__)
#define NKC_LOG_ERROR(...) ::nkentseu::conqueror::NkcLogWrite(::nkentseu::conqueror::NkcLogLevel::Error, __VA_ARGS__)

// -----------------------------------------------------------------------------
// NKC_MODULE_LOGGING(kind) — kind vaut `rules` ou `ai`.
//
// Definit le symbole que l'atelier cherche au chargement. Le passage par
// NKC_MODULE_LOGGING_ est le detour classique pour que `kind` soit developpe
// AVANT le collage : sans lui on obtiendrait le symbole `nkc_kind_set_logger`.
// -----------------------------------------------------------------------------
#define NKC_MODULE_LOGGING_(kind)                                                  \
	NKC_MODULE_EXPORT void nkc_##kind##_set_logger(                                \
		::nkentseu::conqueror::NkcLogFn fn, void *user, const char *name) {        \
		::nkentseu::conqueror::NkcLogInstall(fn, user, name);                      \
		NKC_MODULE_LOGGING_BRIDGE();                                               \
	}

#define NKC_MODULE_LOGGING(kind) NKC_MODULE_LOGGING_(kind)

// -----------------------------------------------------------------------------
// PONT VERS NKLogger — actif SEULEMENT si le module a inclus NKLogger.
//
// Il faut que `logger.Infof(...)` marche aussi : c'est le reflexe de quelqu'un
// qui connait deja Nkentseu, et lui dire « pas ici » serait une regle de plus a
// retenir pour rien. On branche donc un NkISink sur le logger PRIVE du module,
// qui repousse tout vers le puits de l'atelier.
//
// La detection se fait sur NKENTSEU_NKLOG_H, pose par NkLog.h : si le stagiaire
// n'inclut pas NKLogger, ce fichier reste sans dependance, et
// NKC_MODULE_LOGGING compile quand meme.
//
// CONSEQUENCE D'ORDRE : ce header doit etre inclus APRES NKLogger. Incluez-le
// en dernier — le probleme ne se pose pour aucun autre en-tete du contrat.
// -----------------------------------------------------------------------------
#if defined(NKENTSEU_NKLOG_H)
#	define NKC_MODULE_LOGGING_BRIDGE() ::nkentseu::conqueror::NkcLogBridgeInstall()

namespace nkentseu {
	namespace conqueror {

		/// Renvoie vers l'atelier ce que le module ecrit dans SON logger.
		class NkcLogBridgeSink : public NkISink {
			public:
				void Log(const NkLogMessage &m) override {
					NkcLogLevel lv = NkcLogLevel::Info;
					switch (m.level) {
						case NkLogLevel::NK_TRACE:	  lv = NkcLogLevel::Trace; break;
						case NkLogLevel::NK_DEBUG:	  lv = NkcLogLevel::Debug; break;
						case NkLogLevel::NK_WARN:	  lv = NkcLogLevel::Warn;  break;
						case NkLogLevel::NK_ERROR:	  lv = NkcLogLevel::Error; break;
						case NkLogLevel::NK_CRITICAL:
						case NkLogLevel::NK_FATAL:	  lv = NkcLogLevel::Fatal; break;
						default:					  lv = NkcLogLevel::Info;  break;
					}
					NkcLogWrite(lv, "%s", m.message.CStr());
				}
				void Flush() override {}
		};

		inline void NkcLogBridgeInstall() noexcept {
			static bool done = false;
			if (done) return;
			done = true;
			// `logger` est une MACRO qui developpe en NkLog::Instance().Source(...) :
			// on passe donc par l'instance directement, sans le decor de source.
			NkLog::Instance().AddSink(memory::NkMakeShared<NkcLogBridgeSink>());
		}

	} // namespace conqueror
} // namespace nkentseu

#else
#	define NKC_MODULE_LOGGING_BRIDGE() ((void)0)
#endif
