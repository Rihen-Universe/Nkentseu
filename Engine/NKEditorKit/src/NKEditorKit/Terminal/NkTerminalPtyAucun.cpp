// =============================================================================
// NkTerminalPtyAucun.cpp — NkPty la ou une application ne lance PAS de shell
// systeme : Android, iOS (et ses cousins), Web.
//
// Ce n'est pas un oubli : ces plateformes n'offrent pas de shell a une
// application (bac a sable iOS, pas de fork sous Web, et sous Android un
// /system/bin/sh sans utilisateur derriere). Le moteur REFUSE donc, avec une
// phrase que le panneau affiche telle quelle -- jamais un plantage, jamais un
// terminal vide qui laisse chercher.
// =============================================================================
#include "NKEditorKit/Terminal/NkTerminalPty.h"

#if defined(NKENTSEU_TERMINAL_AUCUN)

namespace nkentseu {
	namespace editorkit {

		struct NkPty::Impl {};

		NkPty::NkPty() = default;

		NkPty::~NkPty() = default;

		bool NkPty::Disponible() {
			return false;
		}

		const char *NkPty::NomMoteur() {
			return "aucun";
		}

		bool NkPty::Start(const NkString &cmdline, int16 cols, int16 rows, const NkString &cwd) {
			(void)cmdline;
			(void)cwd;
			mCols = cols < 1 ? 80 : cols;
			mRows = rows < 1 ? 24 : rows;
#if defined(__ANDROID__)
			mErreur = "Pas de shell systeme sous Android : une application n'y ouvre pas de terminal.";
#elif defined(__EMSCRIPTEN__)
			mErreur = "Pas de shell systeme dans un navigateur (Web) : le terminal est reserve aux editeurs de bureau.";
#else
			mErreur = "Pas de shell systeme sous iOS : une application n'y ouvre pas de terminal.";
#endif
			return false;
		}

		void NkPty::Write(const char *data, usize len) {
			(void)data;
			(void)len;
		}

		void NkPty::Resize(int16 cols, int16 rows) {
			mCols = cols;
			mRows = rows;
		}

		void NkPty::Drain(NkVector<char> &out) {
			(void)out;
		}

		bool NkPty::Running() const {
			return false;
		}

		int32 NkPty::CodeSortie() const {
			return -1;
		}

		void NkPty::Stop() {}

		void NkPty::ProcessusDuGroupe(NkVector<int64> &pids) const {
			pids.Clear();
		}

		void NkPty::Tuer(int64 pid) {
			(void)pid;
		}

		bool NkPty::ProcessusVivant(int64 pid) {
			(void)pid;
			return false;
		}

	} // namespace editorkit
} // namespace nkentseu

#endif // NKENTSEU_TERMINAL_AUCUN
