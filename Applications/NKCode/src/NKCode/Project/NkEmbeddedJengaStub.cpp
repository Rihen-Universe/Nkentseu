//
// NkEmbeddedJengaStub.cpp — implementation NEUTRE de l'embarquement Jenga,
// pour les plateformes ou CPython n'est pas vendorise.
//
// POURQUOI : la Phase 12 embarque un CPython dans NKCode pour que l'utilisateur
// n'ait pas a installer Python. Le paquet vendorise est aujourd'hui celui de
// WINDOWS (`Externals/Libs/PythonEmbed/sdk` contient un `pyconfig.h` Windows,
// qui reclame <io.h>). Compiler ce module sur Linux echouait donc des
// l'inclusion de pybind11.
//
// Ce fichier fournit la MEME surface publique, mais qui repond « indisponible ».
// L'appelant retombe alors sur le `jenga` du PATH — ce qui est raisonnable sous
// Linux, ou Python est presque toujours present, alors que c'est precisement
// l'hypothese qu'on ne pouvait pas faire sous Windows.
//
// A SUPPRIMER le jour ou un CPython Linux sera vendorise (python-build-standalone
// fait exactement ce travail) : il suffira alors de compiler le vrai module
// partout.
//
#include "NKCode/Project/NkEmbeddedJenga.h"

#if !defined(NKCODE_EMBED_PYTHON)

namespace nkentseu {
	namespace nkcode {

		namespace {
			// Raison affichee dans le panneau Sortie au demarrage : l'utilisateur
			// doit savoir POURQUOI le mode embarque est inactif, pas le deviner.
			NkString gExeDir;
		} // namespace

		NkEmbeddedJenga &NkEmbeddedJenga::Get() {
			static NkEmbeddedJenga inst;
			return inst;
		}

		void NkEmbeddedJenga::Configure(const NkString &exeDir) { gExeDir = exeDir; }

		// Jamais disponible : c'est tout l'objet de ce fichier.
		bool NkEmbeddedJenga::Available() { return false; }
		bool NkEmbeddedJenga::HasProdTools() { return false; }
		bool NkEmbeddedJenga::NeedsCompiler() { return false; }

		NkString NkEmbeddedJenga::CompilersDir() {
			return gExeDir.Empty() ? NkString() : (gExeDir + "/tools/compilers");
		}

		NkString NkEmbeddedJenga::DefaultCompilerBin() {
			const NkString d = CompilersDir();
			if (d.Empty())
				return NkString();
#if defined(_WIN32)
			return d + "/llvm-mingw/bin";
#else
			return d + "/zig";
#endif
		}

		bool NkEmbeddedJenga::Start(const Request &) { return false; }
		bool NkEmbeddedJenga::Running() const { return false; }
		bool NkEmbeddedJenga::Done() const { return true; }
		int NkEmbeddedJenga::ExitCode() const { return -1; }

		void NkEmbeddedJenga::Drain(NkVector<NkString> &, NkVector<NkJengaProgressEvent> &) {}

		void NkEmbeddedJenga::Shutdown() {}

	} // namespace nkcode
} // namespace nkentseu

#endif // !NKCODE_EMBED_PYTHON
