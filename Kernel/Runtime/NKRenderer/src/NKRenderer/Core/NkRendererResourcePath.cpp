// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkRendererResourcePath.cpp — cf. l'en-tete : pourquoi une troisieme racine.
// =============================================================================
#include "NkRendererResourcePath.h"
#include "NKFileSystem/NkPath.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			/// Barre finale, separateurs normalises en '/'.
			NkString AvecBarre(const NkString &s) {
				NkString r;
				for (usize i = 0; i < s.Size(); ++i)
					r.Append(s[i] == '\\' ? '/' : s[i]);
				if (!r.Empty() && r[r.Size() - 1u] != '/')
					r.Append('/');
				return r;
			}

			bool Existe(const NkString &p) {
				return !p.Empty() && (NkFile::Exists(p.CStr()) || NkDirectory::Exists(p.CStr()));
			}

			/// La remontee : le dossier de l'executable, puis ses parents.
			/// ⚠️ DOUZE NIVEAUX ET PAS PLUS : `Build/Bin/<cfg>/<app>/` est a
			///    quatre niveaux de la racine ; une remontee sans borne finirait a
			///    la racine du disque et pourrait y trouver le `Resources/` de
			///    n'importe qui.
			NkString Chercher() {
				NkPath d = NkPath::GetExecutableDirectory();
				for (int32 k = 0; k < 12; ++k) {
					const NkString s = AvecBarre(d.ToString());
					if (s.Empty())
						break;
					if (NkDirectory::Exists((s + "Resources/NKRenderer").CStr()))
						return s;
					const NkPath parent = d.GetParent();
					const NkString ps = parent.ToString();
					if (ps.Empty() || ps == d.ToString())
						break;
					d = parent;
				}
				return NkString();
			}
		} // namespace

		const NkString &NkRendererResourceRoot() noexcept {
			// Statique locale : initialisation unique et sure entre threads (C++11).
			static const NkString sRacine = Chercher();
			return sRacine;
		}

		NkString NkRendererResolvePath(const NkString &relatif) noexcept {
			if (relatif.Empty())
				return relatif;
			// 1. Tel quel : le repertoire courant d'abord, comme avant.
			if (Existe(relatif))
				return relatif;
			// Un chemin deja absolu n'a pas de seconde chance a la racine.
			if (NkPath(relatif.CStr()).IsAbsolute())
				return relatif;
			// 2. La racine trouvee en remontant depuis l'executable.
			const NkString &racine = NkRendererResourceRoot();
			if (!racine.Empty()) {
				const NkString c = racine + relatif;
				if (Existe(c))
					return c;
			}
			return relatif;
		}

		NkString NkRendererResourceSearchReport() noexcept {
			const NkString &racine = NkRendererResourceRoot();
			if (!racine.Empty())
				return NkString("racine trouvee en remontant depuis l'executable : ") + racine;
			return NkString("aucun dossier contenant Resources/NKRenderer en remontant depuis ") +
				   NkPath::GetExecutableDirectory().ToString();
		}

	} // namespace renderer
} // namespace nkentseu
